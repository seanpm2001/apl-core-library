/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "apl/extension/extensionclient.h"

#include <rapidjson/stringbuffer.h>

#include "apl/component/componentpropdef.h"
#include "apl/component/componentproperties.h"
#include "apl/content/content.h"
#include "apl/document/coredocumentcontext.h"
#include "apl/engine/arrayify.h"
#include "apl/engine/binding.h"
#include "apl/engine/corerootcontext.h"
#include "apl/engine/evaluate.h"
#include "apl/extension/extensionmanager.h"
#include "apl/livedata/livearray.h"
#include "apl/livedata/livearrayobject.h"
#include "apl/livedata/livemap.h"
#include "apl/livedata/livemapobject.h"
#include "apl/utils/random.h"
#include "apl/utils/streamer.h"

namespace apl {

static const bool DEBUG_EXTENSION_CLIENT = false;

/// Simple "semi-unique" generator for command IDs.
id_type ExtensionClient::sCommandIdGenerator = 1000;

static const std::string IMPLEMENTED_INTERFACE_VERSION = "1.0";
static const std::string MAX_SUPPORTED_SCHEMA_VERSION = "1.1";

static const Bimap<ExtensionLiveDataUpdateType, std::string> sExtensionLiveDataUpdateTypeBimap = {
    {kExtensionLiveDataUpdateTypeInsert, "Insert"},
    {kExtensionLiveDataUpdateTypeUpdate, "Update"},
    {kExtensionLiveDataUpdateTypeSet,    "Set"},
    {kExtensionLiveDataUpdateTypeRemove, "Remove"},
    {kExtensionLiveDataUpdateTypeClear,  "Clear"},
};

static const Bimap<ExtensionMethod, std::string> sExtensionMethodBimap = {
    {kExtensionMethodRegister,         "Register"},
    {kExtensionMethodRegisterSuccess,  "RegisterSuccess"},
    {kExtensionMethodRegisterFailure,  "RegisterFailure"},
    {kExtensionMethodCommand,          "Command"},
    {kExtensionMethodCommandSuccess,   "CommandSuccess"},
    {kExtensionMethodCommandFailure,   "CommandFailure"},
    {kExtensionMethodEvent,            "Event"},
    {kExtensionMethodLiveDataUpdate,   "LiveDataUpdate"},
    {kExtensionMethodComponent,        "Component"},
    {kExtensionMethodComponentSuccess, "ComponentSuccess"},
    {kExtensionMethodComponentFailure, "ComponentFailure"},
    {kExtensionMethodComponentUpdate,  "ComponentUpdate"},
};

static const Bimap<ExtensionEventExecutionMode, std::string> sExtensionEventExecutionModeBimap = {
    {kExtensionEventExecutionModeNormal, "NORMAL"},
    {kExtensionEventExecutionModeFast,   "FAST"},
};

static const Bimap<ExtensionComponentResourceState, std::string> sExtensionComponentStateBimap = {
    {kResourcePending,  "Pending"},
    {kResourceReady,    "Ready"},
    {kResourceError,    "Error"},
    {kResourceReleased, "Released"}
};

ExtensionClientPtr
ExtensionClient::create(const RootConfigPtr& rootConfig, const std::string& uri)
{
    return create(rootConfig, uri, makeDefaultSession());
}

ExtensionClientPtr
ExtensionClient::create(const RootConfigPtr& rootConfig, const std::string& uri, const SessionPtr& session)
{
    if (rootConfig == nullptr) {
        LOG(LogLevel::kError) << "Can't create client with a null RootConfig.";
        return nullptr;
    }

    auto client = std::make_shared<ExtensionClient>(uri, session, rootConfig->getExtensionFlags(uri));

    // Auto-register client to support legacy non-mediator pathway
    rootConfig->registerLegacyExtensionClient(uri, client);

    return client;
}

ExtensionClient::ExtensionClient(const std::string& uri, const SessionPtr& session, const Object& flags)
    :  mRegistrationProcessed(false), mRegistered(false), mUri(uri), mSession(session), mFlags(flags),
       mInternalRootConfig(RootConfig::create())
{}

rapidjson::Value
ExtensionClient::createRegistrationRequest(rapidjson::Document::AllocatorType& allocator, Content& content) {
    const auto& settings = content.getExtensionSettings(mUri);
    return createRegistrationRequest(allocator, mUri, settings, mFlags);
}

rapidjson::Value
ExtensionClient::createRegistrationRequest(rapidjson::Document::AllocatorType& allocator,
        const std::string& uri, const Object& settings, const Object& flags) {
    auto request = std::make_shared<ObjectMap>();
    request->emplace("method", sExtensionMethodBimap.at(kExtensionMethodRegister));
    request->emplace("version", IMPLEMENTED_INTERFACE_VERSION);
    request->emplace("uri", uri);
    request->emplace("settings", settings);
    request->emplace("flags", flags);

    return Object(request).serialize(allocator);
}

std::string
ExtensionClient::getUri() const
{
    return mUri;
}

bool
ExtensionClient::registrationMessageProcessed() const
{
    return mRegistrationProcessed;
}

bool
ExtensionClient::registered() const
{
    return mRegistered;
}

bool
ExtensionClient::registrationFailed() const
{
    return mRegistrationProcessed && !mRegistered;
}

const ParsedExtensionSchema&
ExtensionClient::extensionSchema() const {
    return mSchema;
}

std::string
ExtensionClient::getConnectionToken() const
{
    return mConnectionToken;
}

void
ExtensionClient::bindContext(const CoreRootContextPtr& rootContext)
{
    if (!rootContext) {
        LOG(LogLevel::kError) << "Can't bind Client to non-existent RootContext.";
        return;
    }
    bindContextInternal(std::static_pointer_cast<CoreRootContext>(rootContext)->mTopDocument);
}

void
ExtensionClient::bindContextInternal(const CoreDocumentContextPtr& documentContext)
{
    mCachedContext = documentContext;
    flushPendingEvents(documentContext);
    LOG_IF(DEBUG_EXTENSION_CLIENT).session(documentContext) << "connection: " << mConnectionToken;
}

bool
ExtensionClient::processMessage(const RootContextPtr& rootContext, JsonData&& message)
{
    auto doc = rootContext ? std::static_pointer_cast<CoreRootContext>(rootContext)->mTopDocument : nullptr;
    return processMessageInternal(doc, std::move(message));
}

bool
ExtensionClient::processMessageInternal(const CoreDocumentContextPtr& documentContext, JsonData&& message)
{
    LOG_IF(DEBUG_EXTENSION_CLIENT).session(documentContext)
        << "Connection: " << mConnectionToken << " message: " << message.toString();

    if (!message) {
        CONSOLE(mSession).log("Malformed offset=%u: %s.", message.offset(), message.error());
        return false;
    }

    const auto& context = documentContext ? documentContext->context() : mInternalRootConfig->evaluationContext();
    auto evaluated = Object(std::move(message).get());
    auto method = propertyAsMapped<ExtensionMethod>(context, evaluated, "method", static_cast<ExtensionMethod>(-1), sExtensionMethodBimap);

    if (!mRegistered) {
        if (mRegistrationProcessed) {
            CONSOLE(mSession).log("Can't process message after failed registration.");
            return false;
        } else if (method != kExtensionMethodRegisterSuccess && method != kExtensionMethodRegisterFailure) {
            CONSOLE(mSession).log("Can't process message before registration.");
            return false;
        }
    }

    if (documentContext) {
        mCachedContext = documentContext;
    }

    auto version = propertyAsObject(context, evaluated, "version");
    if (version.isNull() || version.getString() != IMPLEMENTED_INTERFACE_VERSION) {
        CONSOLE(mSession) << "Interface version is wrong. Expected=" << IMPLEMENTED_INTERFACE_VERSION
                          << "; Actual=" << version.toDebugString();
        return false;
    }

    bool result = true;
    switch (method) {
        case kExtensionMethodRegisterSuccess:
            result = processRegistrationResponse(context, evaluated);
            // FALL_THROUGH
        case kExtensionMethodRegisterFailure:
            mRegistrationProcessed = true;
            break;
        case kExtensionMethodCommandSuccess: // FALL_THROUGH
        case kExtensionMethodCommandFailure:
            result = processCommandResponse(context, evaluated);
            break;
        case kExtensionMethodEvent:
            result = processEvent(context, evaluated);
            break;
        case kExtensionMethodLiveDataUpdate:
            result = processLiveDataUpdate(context, evaluated);
            break;
//        case kExtensionMethodRegister: // Outgoing commands should not be processed here.
//        case kExtensionMethodCommand:
        case kExtensionMethodComponentSuccess:
        case kExtensionMethodComponentFailure:
        case kExtensionMethodComponentUpdate:
            result = processComponentResponse(context, evaluated);
            break;
        default:
            CONSOLE(mSession).log("Unknown method");
            result = false;
            break;
    }

    return result;
}

bool
ExtensionClient::processRegistrationResponse(const Context& context, const Object& connectionResponse)
{
    if (mRegistered) {
        CONSOLE(mSession).log("Can't register extension twice.");
        return false;
    }

    auto connectionToken = propertyAsObject(context, connectionResponse, "token");
    auto schema = propertyAsObject(context, connectionResponse, "schema");
    if (connectionToken.isNull() || connectionToken.empty() || schema.isNull()) {
        CONSOLE(mSession).log("Malformed connection response message.");
        return false;
    }

    if (!readExtension(context, schema)) {
        CONSOLE(mSession).log("Malformed schema.");
        return false;
    }

    const auto& assignedToken = connectionToken.getString();
    if (assignedToken == "<AUTO_TOKEN>") {
        if (mConnectionToken.empty()) {
            mConnectionToken = Random::generateToken(mUri);
        }
    } else {
        mConnectionToken = assignedToken;
    }

    // The extension's environment ends up being available to the document author via
    // ${environment.extension.Fish} (where Fish is the name assigned to the extension).
    //
    // The value of Fish is what we're setting below. We want Fish to be truthy (either as a map of
    // values or "true") because that's how the author knows whether the extension is available.
    auto environment = propertyAsRecursive(context, connectionResponse, "environment");
    if (environment.isMap()) {
        // Use environment provided by extension, if it provided one 
        mSchema.environment = std::move(environment);
    } else {
        // Otherwise mark the environment as "true"
        mSchema.environment = Object::TRUE_OBJECT();
    }

    mRegistered = true;
    return true;
}

bool
ExtensionClient::processEvent(const Context& context, const Object& event)
{
    auto name = propertyAsObject(context, event, "name");
    if (!name.isString() || name.empty() || (mSchema.eventModes.find(name.getString()) == mSchema.eventModes.end())) {
        CONSOLE(mSession) << "Invalid extension event name for extension=" << mUri
                << " name:" << name.toDebugString();
        return false;
    }

    auto target = propertyAsObject(context, event, "target");
    if (!target.isString() || target.empty() || target.getString() != mUri) {
        CONSOLE(mSession) << "Invalid extension event target for extension=" << mUri;
        return false;
    }

    auto payload = propertyAsRecursive(context, event, "payload");
    if (!payload.isNull() && !payload.isMap()) {
        CONSOLE(mSession) << "Invalid extension event data for extension=" << mUri;
        return false;
    }

    auto resourceId = propertyAsString(context, event, "resourceId");
    invokeExtensionHandler(mUri,
                           name.getString(),
                           payload.isNull() ? Object::EMPTY_MAP().getMap() : payload.getMap(),
                           mSchema.eventModes.at(name.getString()) == kExtensionEventExecutionModeFast,
                           resourceId);
    return true;
}

rapidjson::Value
ExtensionClient::processCommand(rapidjson::Document::AllocatorType& allocator, const Event& event)
{
    if (kEventTypeExtension != event.getType()) {
        CONSOLE(mSession) << "Invalid extension command type for extension=" << mUri;
        return rapidjson::Value(rapidjson::kNullType);
    }

    auto extensionURI = event.getValue(kEventPropertyExtensionURI);
    if (!extensionURI.isString() || extensionURI.getString() != mUri) {
        CONSOLE(mSession) << "Invalid extension command target for extension=" << mUri;
        return rapidjson::Value(rapidjson::kNullType);
    }

    auto commandName = event.getValue(kEventPropertyName);
    if (!commandName.isString() || commandName.empty()) {
        CONSOLE(mSession) << "Invalid extension command name for extension=" << mUri
            << " command:" << commandName;
        return rapidjson::Value(rapidjson::kNullType);
    }

    auto resourceId = event.getValue(kEventPropertyExtensionResourceId);
    if (!resourceId.empty() && !resourceId.isString()) {
        CONSOLE(mSession) << "Invalid extension component handle for extension=" << mUri;
        return rapidjson::Value (rapidjson::kNullType);
    }

    auto result = std::make_shared<ObjectMap>();
    result->emplace("version", IMPLEMENTED_INTERFACE_VERSION);
    result->emplace("method", sExtensionMethodBimap.at(kExtensionMethodCommand));
    result->emplace("token", mConnectionToken);
    if (!resourceId.empty()) {
        result->emplace("resourceId", resourceId);
    }
    auto id = sCommandIdGenerator++;
    result->emplace("id", id  );

    auto actionRef = event.getActionRef();
    if (!actionRef.empty() && actionRef.isPending()) {
        auto weakSelf = std::weak_ptr<ExtensionClient>(shared_from_this());
        actionRef.addTerminateCallback([weakSelf, id](const TimersPtr&) {
            if (auto self = weakSelf.lock()) {
                self->mActionRefs.erase(id);
            }
        });
        mActionRefs.emplace(id, actionRef);
    }

    auto parameters = event.getValue(kEventPropertyExtension);
    result->emplace("name", commandName.getString());
    result->emplace("target", extensionURI.getString());
    result->emplace("payload", parameters);

    return Object(result).serialize(allocator);
}

bool
ExtensionClient::processCommandResponse(const Context& context, const Object& response)
{
    // Resolve any Action associated with a command response.  The action is resolved independent
    // of a Success or Failure in command execution
    auto id = propertyAsObject(context, response, "id");
    if (!id.isNumber() || id.getInteger() > sCommandIdGenerator) {
        CONSOLE(mSession) << "Invalid extension command response for extension=" << mUri << " id="
            << id.toDebugString() << " total pending=" << mActionRefs.size();
        return false;
    }

    if (!mActionRefs.count(id.getDouble())) {
        // no action ref is tracked.
        return true;
    }

    auto resultObj = propertyAsObject(context, response, "result");
    if (!resultObj.isNull()) {
        // TODO: We don't have resolves with arbitrary results.
    }

    auto ref = mActionRefs.find(id.getDouble());
    ref->second.resolve();
    mActionRefs.erase(ref);

    return true;
}

void
ExtensionClient::liveDataObjectFlushed(const std::string& key, LiveDataObject& liveDataObject)
{
    if (!mLiveData.count(key)) {
        LOG(LogLevel::kWarn).session(mSession) << "Received update for unhandled LiveData " << key;
        return;
    }

    auto ref = mLiveData.at(key);
    LOG_IF(DEBUG_EXTENSION_CLIENT).session(mSession) << " connection: " << mConnectionToken
                                   << ", key: " << key
                                   << ", ref.name: " << ref.name
                                   << ", type: " << ref.type;

    switch (ref.objectType) {
        case kExtensionLiveDataTypeArray:
            reportLiveArrayChanges(ref, LiveArrayObject::cast(liveDataObject).getChanges());
            break;
        case kExtensionLiveDataTypeObject:
            reportLiveMapChanges(ref, LiveMapObject::cast(liveDataObject).getChanges());
            break;
    }
    ref.hasPendingUpdate = false;
}

void
ExtensionClient::sendLiveDataEvent(const std::string& event,
        const Object& current, const Object& changed)
{
    ObjectMap parameters;
    parameters.emplace("current", current);
    if (!changed.isNull()) {
        parameters.emplace("changed", changed);
    }
    invokeExtensionHandler(mUri, event, parameters, true);
}

void
ExtensionClient::reportLiveMapChanges(const LiveDataRef& ref, const std::vector<LiveMapChange>& changes)
{
    std::set<std::string> updatedCollapsed;
    std::set<std::string> removedCollapsed;
    std::string updateTriggerEvent = ref.updateEvent.name;
    std::string removeTriggerEvent = ref.removeEvent.name;

    auto liveMap = std::static_pointer_cast<LiveMap>(ref.objectPtr);
    auto mapPtr = std::make_shared<ObjectMap>(liveMap->getMap());
    for (auto& change : changes) {
        auto key = change.key();
        auto changed = std::make_shared<ObjectMap>();
        switch (change.command()) {
            case LiveMapChange::Command::SET:
                if (ref.updateEvent.params.count(key)) {
                    if (ref.updateEvent.params.at(key)) {
                        updatedCollapsed.emplace(key);
                    } else {
                        changed->emplace(key, liveMap->get(key));
                        sendLiveDataEvent(updateTriggerEvent, mapPtr, changed);
                    }
                }
                break;
            case LiveMapChange::Command::REMOVE:
                if (ref.removeEvent.params.count(key)) {
                    if (ref.removeEvent.params.at(key)) {
                        removedCollapsed.emplace(key);
                    } else {
                        changed->emplace(key, Object::NULL_OBJECT());
                        sendLiveDataEvent(removeTriggerEvent, mapPtr, changed);
                    }
                }
                break;
            default:
                LOG(LogLevel::kWarn) << "Unknown LiveDataObject change type: " << change.command()
                                    << " for: " << ref.name;
                break;
        }
    }

    if (!updatedCollapsed.empty()) {
        auto changed = std::make_shared<ObjectMap>();
        for (auto& c : updatedCollapsed) {
            changed->emplace(c, liveMap->get(c));
        }
        sendLiveDataEvent(updateTriggerEvent, mapPtr, changed);
    }
    if (!removedCollapsed.empty()) {
        auto changed = std::make_shared<ObjectMap>();
        for (auto& c : removedCollapsed) {
            changed->emplace(c, liveMap->get(c));
        }
        sendLiveDataEvent(removeTriggerEvent, mapPtr, changed);
    }
}

void
ExtensionClient::reportLiveArrayChanges(const LiveDataRef& ref, const std::vector<LiveArrayChange>& changes)
{
    std::string addTriggerEvent;
    std::string updateTriggerEvent;
    std::string removeTriggerEvent;

    auto arrayPtr = std::make_shared<ObjectArray>(std::static_pointer_cast<LiveArray>(ref.objectPtr)->getArray());
    for (auto& change : changes) {
        switch (change.command()) {
            case LiveArrayChange::Command::INSERT :
                addTriggerEvent = ref.addEvent.name;
                break;
            case LiveArrayChange::Command::UPDATE:
                updateTriggerEvent = ref.updateEvent.name;
                break;
            case LiveArrayChange::Command::REMOVE:
                removeTriggerEvent = ref.removeEvent.name;
                break;
            default:
                LOG(LogLevel::kWarn) << "Unknown LiveDataObject change type: " << change.command()
                                    << " for: " << ref.name;
                break;
        }
    }

    if (!addTriggerEvent.empty()) {
        sendLiveDataEvent(addTriggerEvent, arrayPtr, Object::NULL_OBJECT());
    }
    if (!updateTriggerEvent.empty()) {
        sendLiveDataEvent(updateTriggerEvent, arrayPtr, Object::NULL_OBJECT());
    }
    if (!removeTriggerEvent.empty()) {
        sendLiveDataEvent(removeTriggerEvent, arrayPtr, Object::NULL_OBJECT());
    }
}

bool
ExtensionClient::processLiveDataUpdate(const Context& context, const Object& update)
{
    auto name = propertyAsObject(context, update, "name");
    if (!name.isString() || name.empty() || (mLiveData.find(name.getString()) == mLiveData.end())) {
        CONSOLE(mSession) << "Invalid LiveData name for extension=" << mUri;
        return false;
    }

    auto target = propertyAsObject(context, update, "target");
    if (!target.isString() || target.empty() || target.getString() != mUri) {
        CONSOLE(mSession) << "Invalid LiveData target for extension=" << mUri;
        return false;
    }

    auto operations = propertyAsRecursive(context, update, "operations");
    if (!operations.isArray()) {
        CONSOLE(mSession) << "Invalid LiveData operations for extension=" << mUri;
        return false;
    }

    auto& dataRef = mLiveData.at(name.getString());
    for (const auto& operation : operations.getArray()) {
        auto type = propertyAsMapped<ExtensionLiveDataUpdateType>(context, operation, "type",
                static_cast<ExtensionLiveDataUpdateType>(-1), sExtensionLiveDataUpdateTypeBimap);
        if (type == static_cast<ExtensionLiveDataUpdateType>(-1)) {
            CONSOLE(mSession) << "Wrong operation type for=" << name;
            return false;
        }

        bool result;
        switch (dataRef.objectType) {
            case kExtensionLiveDataTypeObject:
                result = updateLiveMap(type, dataRef, operation);
                break;
            case kExtensionLiveDataTypeArray:
                result = updateLiveArray(type, dataRef, operation);
                break;
            default:
                result = false;
                CONSOLE(mSession) << "Unknown LiveObject type=" << dataRef.objectType << " for " << dataRef.name;
                break;
        }

        if (!result) {
            CONSOLE(mSession) << "LiveMap operation failed=" << dataRef.name << " operation="
                                << sExtensionLiveDataUpdateTypeBimap.at(type);
        } else {
            dataRef.hasPendingUpdate = true;
        }
    }
    return true;
}

bool
ExtensionClient::updateLiveMap(ExtensionLiveDataUpdateType type, const LiveDataRef& dataRef, const Object& operation)
{
    std::string triggerEvent;
    auto keyObj = operation.opt("key", "");
    if (keyObj.empty()) {
        CONSOLE(mSession) << "Invalid LiveData key for=" << dataRef.name;
        return false;
    }
    const auto& key = keyObj.getString();

    // TODO: Why don't we verify the type? Should we?
//    auto typeDef = mSchema.types.at(dataRef.type);
    auto item = operation.get("item");

    auto liveMap = std::static_pointer_cast<LiveMap>(dataRef.objectPtr);
    auto result = true;

    switch (type) {
        case kExtensionLiveDataUpdateTypeSet:
            liveMap->set(key, item);
            break;
        case kExtensionLiveDataUpdateTypeRemove:
            result = liveMap->remove(key);
            break;
        default:
            CONSOLE(mSession) << "Unknown operation for=" << dataRef.name;
            return false;
    }

    return result;
}

bool
ExtensionClient::updateLiveArray(ExtensionLiveDataUpdateType type, const LiveDataRef& dataRef, const Object& operation)
{
    std::string triggerEvent;

    auto item = operation.get("item");
    if (item.isNull() && (type != kExtensionLiveDataUpdateTypeRemove && type != kExtensionLiveDataUpdateTypeClear)) {
        CONSOLE(mSession) << "Malformed items on LiveData update for=" << dataRef.name;
        return false;
    }

    auto indexObj = operation.opt("index", -1);
    if (!indexObj.isNumber() && type != kExtensionLiveDataUpdateTypeClear) {
        CONSOLE(mSession) << "Invalid LiveData index for=" << dataRef.name;
        return false;
    }
    auto index = indexObj.getInteger();
    auto count = operation.get("count");
    auto liveArray = std::static_pointer_cast<LiveArray>(dataRef.objectPtr);
    auto result = true;

    switch (type) {
        case kExtensionLiveDataUpdateTypeInsert:
            if(item.isArray()) {
                result = liveArray->insert(index, item.getArray().begin(), item.getArray().end());
            } else {
                result = liveArray->insert(index, item);
            }
            break;
        case kExtensionLiveDataUpdateTypeUpdate:
            if(item.isArray()) {
                result = liveArray->update(index, item.getArray().begin(), item.getArray().end());
            } else {
                result = liveArray->update(index, item);
            }
            break;
        case kExtensionLiveDataUpdateTypeRemove:
            result = liveArray->remove(index, count.isNumber() ? count.getDouble() : 1);
            break;
        case kExtensionLiveDataUpdateTypeClear:
            liveArray->clear();
            break;
        default:
            CONSOLE(mSession) << "Unknown operation for=" << dataRef.name;
            return false;
    }

    return result;
}

bool
ExtensionClient::readExtension(const Context& context, const Object& extension)
{
    // verify extension schema
    auto schema = propertyAsString(context, extension, "type");
    auto version = propertyAsString(context, extension, "version");
    if (schema != "Schema" || version.compare(MAX_SUPPORTED_SCHEMA_VERSION) > 0) {
        CONSOLE(mSession) << "Unsupported extension schema version:" << version;
        return false;
    }

    // register extension based on URI
    auto uriObj = propertyAsObject(context, extension, "uri");
    if (!uriObj.isString() || uriObj.empty()) {
        CONSOLE(mSession).log("Missing or invalid extension URI.");
        return false;
    }
    const auto& uri = uriObj.getString();
    mUri = uri;

    auto types = arrayifyPropertyAsObject(context, extension, "types");
    if (!readExtensionTypes(context, types)) {
        return false;
    }

    // register extension commands
    auto commands = arrayifyPropertyAsObject(context, extension, "commands");
    if (!readExtensionCommandDefinitions(context, commands)) {
        return false;
    }

    // register extension event handlers
    auto handlers = arrayifyPropertyAsObject(context, extension, "events");
    if (!readExtensionEventHandlers(context, handlers)) {
        return false;
    }

    // register extension live data
    auto liveData = arrayifyPropertyAsObject(context, extension, "liveData");
    if(!readExtensionLiveData(context, liveData)) {
        return false;
    }

    auto components = arrayifyPropertyAsObject(context, extension, "components");
    if (!readExtensionComponentDefinitions(context, components)) {
        return false;
    }

    return true;
}

bool
ExtensionClient::readExtensionTypes(const Context& context, const Object& types)
{
    if (!types.isArray()) {
        CONSOLE(mSession).log("The extension name=%s has a malformed 'commands' block", mUri.c_str());
        return false;
    }

    for (const auto& t : types.getArray()) {
        auto name = propertyAsObject(context, t, "name");
        auto props = propertyAsObject(context, t, "properties");
        if (!name.isString() || !props.isMap()) {
            CONSOLE(mSession).log("Invalid extension type for extension=%s",
                                    mUri.c_str());
            continue;
        }

        auto properties = std::make_shared<std::map<std::string, ExtensionProperty>>();
        auto extends = propertyAsObject(context, t, "extends");
        if (extends.isString()) {
            auto& extended = extends.getString();
            auto extendedType = mSchema.types.find(extended);
            if (extendedType != mSchema.types.end()) {
                properties->insert(extendedType->second->begin(), extendedType->second->end());
            } else {
                CONSOLE(mSession) << "Unknown type to extend=" << extended
                                    << " for type=" << name.getString()
                                    << " for extension=" << mUri.c_str();
            }
        }

        for (const auto &p : props.getMap()) {
            auto pname = p.first;
            auto ps = p.second;

            Object defValue = Object::NULL_OBJECT();
            BindingType ptype;
            auto preq = true;

            if (ps.isString()) {
                ptype = sBindingMap.get(ps.getString(), kBindingTypeAny);
            } else if (!ps.has("type")) {
                CONSOLE(mSession).log("Invalid extension property for type=%s extension=%s",
                        name.getString().c_str(), mUri.c_str());
                continue;
            } else {
                defValue = propertyAsRecursive(context, ps, "default");
                ptype = propertyAsMapped<BindingType>(context, ps, "type", kBindingTypeAny, sBindingMap);
                if (!sBindingMap.has(ptype)) {
                    ptype = kBindingTypeAny;
                }
                preq = propertyAsBoolean(context, ps, "required", false);
            }

            const auto& pfunc = sBindingFunctions.at(ptype);
            auto value = pfunc(context, defValue);

            properties->emplace(pname, ExtensionProperty{ptype, pfunc(context, defValue), preq});
        }
        mSchema.types.emplace(name.getString(), properties);
    }
    return true;
}

std::vector<ExtensionCommandDefinition>
ExtensionClient::readCommandDefinitionsInternal(const Context& context,const ObjectArray& commands)
{
    std::vector<ExtensionCommandDefinition> commandDefs;

    for (const auto& command : commands) {
        // create a command
        auto name = propertyAsObject(context, command, "name");
        if (!name.isString() || name.empty()) {
            CONSOLE(mSession).log("Invalid extension command for extension=%s", mUri.c_str());
            continue;
        }
        auto commandName = name.asString();
        commandDefs.emplace_back(mUri, commandName);
        auto& commandDef = commandDefs.back();
        // set required response
        auto req = propertyAsBoolean(context, command, "requireResponse", false);
        commandDef.requireResolution(req);
        auto fast = propertyAsBoolean(context, command, "allowFastMode", false);
        commandDef.allowFastMode(fast);

        // add command properties
        if (command.has("payload")) {
            // TODO: Work with references only now, maybe inlines later
            auto payload = command.get("payload");
            std::string type;
            if (payload.isString()) {
                type = payload.getString();
            } else if (payload.isMap()) {
                type = propertyAsString(context, payload, "type");
            }

            if (!mSchema.types.count(type)) {
                CONSOLE(mSession).log("The extension name=%s has a malformed `payload` block for command=%s",
                                        mUri.c_str(), commandName.c_str());
                continue;
            }

            auto props = mSchema.types.at(type);
            for (const auto& p : *props) {
                // add the property
                commandDef.property(p.first, p.second.btype, p.second.defvalue, p.second.required);
            }
        } // properties

        mSchema.commandDefinitions.emplace_back(std::move(commandDef));
    }
    return commandDefs;
}

bool
ExtensionClient::readExtensionCommandDefinitions(const Context& context, const Object& commands)
{
    if (!commands.isArray()) {
        CONSOLE(mSession).log("The extension name=%s has a malformed 'commands' block", mUri.c_str());
        return false;
    }
    readCommandDefinitionsInternal(context, commands.getArray());
    return true;
}

void
ExtensionClient::readExtensionComponentCommandDefinitions(const Context& context, const Object& commands,
                                                          ExtensionComponentDefinition& def)
{
    if (!commands.isArray()) {
        CONSOLE(mSession).log("The extension component name=%s has a malformed 'commands' block", mUri.c_str());
        return;
    }
    // TODO: Remove when customers stopped using it.
    readCommandDefinitionsInternal(context, commands.getArray());
}

bool
ExtensionClient::readExtensionEventHandlers(const Context& context, const Object& handlers)
{
    if (!handlers.isArray()) {
        CONSOLE(mSession).log("The extension name=%s has a malformed 'events' block", mUri.c_str());
        return false;
    }

    for (const auto& handler : handlers.getArray()) {
        auto name = propertyAsObject(context, handler, "name");
        if (!name.isString() || name.empty()) {
            CONSOLE(mSession).log("Invalid extension event handler for extension=%s", mUri.c_str());
            return false;
        } else {
            auto mode = propertyAsMapped<ExtensionEventExecutionMode>(context, handler, "mode",
                    kExtensionEventExecutionModeFast, sExtensionEventExecutionModeBimap);
            mSchema.eventModes.emplace(name.asString(), mode);
            mSchema.eventHandlers.emplace_back(ExtensionEventHandler(mUri, name.asString()));
        }
    }

    return true;
}

bool
ExtensionClient::readExtensionLiveData(const Context& context, const Object& liveData)
{
    if (!liveData.isArray()) {
        CONSOLE(mSession).log("The extension name=%s has a malformed 'dataBindings' block", mUri.c_str());
        return false;
    }

    for (const auto& binding : liveData.getArray()) {
        auto name = propertyAsObject(context, binding, "name");
        if (!name.isString() || name.empty()) {
            CONSOLE(mSession).log("Invalid extension data binding for extension=%s", mUri.c_str());
            return false;
        }

        auto typeDef = propertyAsObject(context, binding, "type");
        if (!typeDef.isString()) {
            CONSOLE(mSession).log("Invalid extension data binding type for extension=%s", mUri.c_str());
            return false;
        }

        auto type = typeDef.getString();
        size_t arrayDefinition = type.find("[]");
        bool isArray = arrayDefinition != std::string::npos;

        if (isArray) {
            type = type.substr(0, arrayDefinition);
        }

        if (!(mSchema.types.count(type) // Any LiveData may use defined complex types
          || (isArray && sBindingMap.has(type)))) { // Arrays also may use primitive types
            CONSOLE(mSession).log("Data type=%s, for LiveData=%s is invalid", type.c_str(), name.getString().c_str());
            continue;
        }

        ExtensionLiveDataType ltype;
        PropertyTriggerEvent addEvent;
        PropertyTriggerEvent updateEvent;
        PropertyTriggerEvent removeEvent;
        LiveObjectPtr live;

        auto initialData = propertyAsRecursive(context, binding, "data");

        if (isArray) {
            ltype = kExtensionLiveDataTypeArray;

            if (initialData.isArray()) {
                live = LiveArray::create(ObjectArray(initialData.getArray()));
            } else {
                live = LiveArray::create();
                if (!initialData.isNull()) {
                    CONSOLE(mSession).log("Initial data for LiveData=%s is of invalid type. Should be an Array.", name.getString().c_str());
                }
            }
        } else {
            ltype = kExtensionLiveDataTypeObject;

            if (initialData.isMap()) {
                live = LiveMap::create(ObjectMap(initialData.getMap()));
            } else {
                live = LiveMap::create();
                if (!initialData.isNull()) {
                    CONSOLE(mSession).log("Initial data for LiveData=%s is of invalid type. Should be a Map.", name.getString().c_str());
                }
            }
        }

        auto events = propertyAsObject(context, binding, "events");
        if (events.isMap()) {
            auto typeProps = mSchema.types.at(type);
            auto event = propertyAsObject(context, events, "add");
            if (event.isMap()) {
                auto propTriggers = propertyAsObject(context, event, "properties");
                auto triggers = readPropertyTriggers(context, typeProps, propTriggers);
                addEvent = {propertyAsString(context, event, "eventHandler"), triggers};
            }
            event = propertyAsObject(context, events, "update");
            if (event.isMap()) {
                auto propTriggers = propertyAsObject(context, event, "properties");
                auto triggers = readPropertyTriggers(context, typeProps, propTriggers);
                updateEvent = {propertyAsString(context, event, "eventHandler"), triggers};
            }
            event = propertyAsObject(context, events, "set");
            if (event.isMap()) {
                auto propTriggers = propertyAsObject(context, event, "properties");
                auto triggers = readPropertyTriggers(context, typeProps, propTriggers);
                updateEvent = {propertyAsString(context, event, "eventHandler"), triggers};
            }
            event = propertyAsObject(context, events, "remove");
            if (event.isMap()) {
                auto propTriggers = propertyAsObject(context, event, "properties");
                auto triggers = readPropertyTriggers(context, typeProps, propTriggers);
                removeEvent = {propertyAsString(context, event, "eventHandler"), triggers};
            }
        }

        LiveDataRef ldf = {name.getString(), ltype, type, live, false, addEvent, updateEvent, removeEvent};

        mLiveData.emplace(name.getString(), ldf);
        mSchema.liveData.emplace(name.getString(), live);
    }

    return true;
}

std::map<std::string, bool>
ExtensionClient::readPropertyTriggers(const Context& context, const TypePropertiesPtr& type, const Object& triggers)
{
    auto result = std::map<std::string, bool>();

    if (triggers.isNull()) {
        // Include all by default
        for (const auto& p : *type) {
            result.emplace(p.first, true);
        }
        return result;
    }

    auto requested = std::map<std::string, bool>();
    for (const auto& t : triggers.getArray()) {
        auto name = propertyAsString(context, t, "name");
        auto update = propertyAsBoolean(context, t, "update", false);
        auto collapse = propertyAsBoolean(context, t, "collapse", true);

        if (name == "*" && update) {
            // Include all
            for (const auto& p : *type) {
                requested.emplace(p.first, collapse);
            }
        } else if (update) {
            requested.emplace(name, collapse);
        } else {
            requested.erase(name);
        }
    }

    for (const auto& p : *type) {
        auto r = requested.find(p.first);
        if (r != requested.end()) {
            result.emplace(r->first, r->second);
        }
    }

    return result;
}

bool
ExtensionClient::readExtensionComponentEventHandlers(const Context& context,
                                                     const Object& handlers,
                                                     ExtensionComponentDefinition& def)
{
    if (!handlers.isNull()) {
        if (!handlers.isArray()) {
            CONSOLE(mSession).log("The extension name=%s has a malformed 'events' block",
                                    mUri.c_str());
            return false;
        }

        for (const auto& handler : handlers.getArray()) {
            auto name = propertyAsObject(context, handler, "name");
            if (!name.isString() || name.empty()) {
                CONSOLE(mSession).log("Invalid extension event handler for extension=%s",
                                        mUri.c_str());
                return false;
            }
            else {
                auto mode = propertyAsMapped<ExtensionEventExecutionMode>(
                    context, handler, "mode", kExtensionEventExecutionModeFast,
                    sExtensionEventExecutionModeBimap);
                mSchema.eventModes.emplace(name.asString(), mode);
                auto eventKey = sComponentPropertyBimap.append(name.asString());
                def.addEventHandler(eventKey, ExtensionEventHandler(mUri, name.asString()));
            }
        }
    }
    return true;
}

bool
ExtensionClient::readExtensionComponentDefinitions(const Context& context, const Object& components)
{
    if (!components.isArray()) {
        CONSOLE(mSession).log("The extension name=%s has a malformed 'components' block", mUri.c_str());
        return false;
    }

    for (const auto& component : components.getArray()) {
        auto name = propertyAsObject(context, component, "name");
        if (!name.isString() || name.empty()) {
            CONSOLE(mSession).log("Invalid extension component name for extension=%s", mUri.c_str());
            continue;
        }
        auto componentName = name.asString();

        auto componentDef = ExtensionComponentDefinition(mUri, componentName);
        auto rType = propertyAsObject(context, component, "resourceType");
        if(rType.isString()) {
            componentDef.resourceType(rType.asString());
        }

        auto visualContext = propertyAsString(context, component, "context");
        componentDef.visualContextType(visualContext);

        auto commands = propertyAsObject(context, component, "commands");
        if(!commands.isNull()) {
            readExtensionComponentCommandDefinitions(context, commands, componentDef);
        }

        auto events = propertyAsObject(context, component, "events");
        if (!readExtensionComponentEventHandlers(context, events, componentDef)) {
            return false;
        }

        auto props = propertyAsObject(context, component, "properties");
        if (!props.empty() && props.isMap()) {
            auto properties = std::make_shared<std::map<id_type, ExtensionProperty>>();
            for (const auto &p : props.getMap()) {
                auto pname = p.first;
                auto ps = p.second;
                Object defValue = Object::NULL_OBJECT();
                BindingType ptype;
                auto preq = true;

                if (ps.isString()) {
                    ptype = sBindingMap.get(ps.getString(), kBindingTypeAny);
                } else if (!ps.has("type")) {
                    CONSOLE(mSession).log("Invalid extension property extension=%s", mUri.c_str());
                    continue;
                } else {
                    defValue = propertyAsObject(context, ps, "default");
                    ptype = propertyAsMapped<BindingType>(context, ps, "type", kBindingTypeAny, sBindingMap);
                    if (!sBindingMap.has(ptype)) {
                        ptype = kBindingTypeAny;
                    }
                    preq = propertyAsBoolean(context, ps, "required", false);
                }

                const auto& pfunc = sBindingFunctions.at(ptype);
                auto value = pfunc(context, defValue);

                // returned property key is an incremented integer
                auto propertyKey = sComponentPropertyBimap.append(pname);
                properties->emplace(propertyKey, ExtensionProperty{ptype, std::move(value), preq});
            }
            componentDef.properties(properties);
        }

        mSchema.componentDefinitions.emplace_back(std::move(componentDef));
    }

    return true;
}

rapidjson::Value
ExtensionClient::createComponentChange(rapidjson::MemoryPoolAllocator<>& allocator, ExtensionComponent& component)
{
    auto extensionURI = component.getUri();
    if (extensionURI != mUri) {
        CONSOLE(mSession) << "Invalid extension command target for extension=" << mUri;
        return rapidjson::Value(rapidjson::kNullType);
    }

    auto result = std::make_shared<ObjectMap>();
    result->emplace("method", sExtensionMethodBimap.at(kExtensionMethodComponent));
    result->emplace("version", IMPLEMENTED_INTERFACE_VERSION);
    result->emplace("target", extensionURI);
    result->emplace("token", mConnectionToken);
    result->emplace("resourceId", component.getResourceID());
    auto state = static_cast<ExtensionComponentResourceState>(component.getCalculated(kPropertyResourceState).asInt());
    result->emplace("state", sExtensionComponentStateBimap.at(state));

    // State specific message content
    if (state == kResourceReady || state == kResourcePending) {

        // include the viewport
        const auto& contextref = component.getContext()->find("viewport");
        if (!contextref.empty()) {
            result->emplace("viewport", contextref.object().value());
        }

        // Fill the payload with all the dirty dynamic properties that are also kPropOut
        // on first message, this will be all properties
        auto payload = std::make_shared<ObjectMap>();
        auto& dirty = component.getDirty();
        for (auto& pd : component.propDefSet().dynamic()) {
            if (((pd.second.flags & kPropOut) != 0 ) &&
                    (state == kResourcePending || dirty.find(pd.first) != dirty.end())) {
                    payload->emplace(pd.second.names[0], component.getCalculated(pd.first));
            }
        }
        result->emplace("payload", payload);
    }

    LOG_IF(DEBUG_EXTENSION_CLIENT).session(mSession) << "Component: " << Object(result);
    return Object(result).serialize(allocator);
}


bool
ExtensionClient::processComponentResponse(const Context& context, const Object& response)
{
    auto method = propertyAsString(context, response, "method");

    auto componentId = propertyAsString(context, response, "resourceId");
    auto extensionComp = context.extensionManager().findExtensionComponent(componentId);

    if (extensionComp != nullptr) {
        if (method == sExtensionMethodBimap.at(kExtensionMethodComponentFailure)) {
            auto message = propertyAsString(context, response, "message");
            auto code = propertyAsInt(context, response, "code", -1);
            extensionComp->extensionComponentFail(code, message);
        } else if (method == sExtensionMethodBimap.at(kExtensionMethodComponentUpdate)) {
           // TODO apply component properties
        }
    } else {
        CONSOLE(mSession) << "Unable to find component associated with :" << componentId;
    }
    return true;
}

bool
ExtensionClient::handleDisconnection(const RootContextPtr& rootContext,
                                     int errorCode,
                                     const std::string& message)
{
    auto doc = rootContext ? std::static_pointer_cast<CoreRootContext>(rootContext)->mTopDocument : nullptr;
    return handleDisconnectionInternal(doc, errorCode, message);
}

bool
ExtensionClient::handleDisconnectionInternal(const CoreDocumentContextPtr& documentContext,
                                             int errorCode,
                                             const std::string& message)
{
    const auto& context = documentContext ? documentContext->context() : mInternalRootConfig->evaluationContext();

    if (errorCode != 0) {
        for (const auto& componentEntry : context.extensionManager().getExtensionComponents()) {
            auto extensionComponent = componentEntry.second;

            if (mUri == extensionComponent->getUri()) {
                extensionComponent->extensionComponentFail(errorCode, message);
            }
        }
    }
    return true;
}

rapidjson::Value
ExtensionClient::processComponentRequest(rapidjson::MemoryPoolAllocator<>& allocator,
                                         ExtensionComponent& component)
{
    return createComponentChange(allocator, component);
}

rapidjson::Value
ExtensionClient::processComponentRelease(rapidjson::MemoryPoolAllocator<>& allocator,
                                         ExtensionComponent& component)
{
    return createComponentChange(allocator, component);
}

rapidjson::Value
ExtensionClient::processComponentUpdate(rapidjson::MemoryPoolAllocator<>& allocator,
                                        ExtensionComponent& component)
{
    return createComponentChange(allocator, component);
}

void
ExtensionClient::invokeExtensionHandler(const std::string& uri, const std::string& name,
                            const ObjectMap& data, bool fastMode,
                            std::string resourceId)
{
    auto documentContext = mCachedContext.lock();
    if (documentContext) {
        documentContext->invokeExtensionEventHandler(uri, name, data, fastMode, resourceId);
    } else {
        LOG(LogLevel::kWarn).session(mSession) << "RootContext not available";
        ExtensionEvent event = {uri, name, data, fastMode, resourceId};
        mPendingEvents.emplace_back(std::move(event));
    }
}

void
ExtensionClient::flushPendingEvents(const CoreDocumentContextPtr& documentContext)
{
    LOG_IF(DEBUG_EXTENSION_CLIENT && (mPendingEvents.size() > 0))
            .session(documentContext) << "Flushing "
                                      << mPendingEvents.size()
                                      << " pending events for " << mConnectionToken;

    for (auto& event : mPendingEvents) {
        invokeExtensionHandler(event.uri, event.name, event.data, event.fastMode, event.resourceId);
    }

    // So if data was sent on a specific time before objects created it will still miss flush (changes not stored).
    // Go through data list and simulate those.
    for (auto& kv : mLiveData) {
        auto& ref = kv.second;
        if (!ref.hasPendingUpdate) continue;

        LOG_IF(DEBUG_EXTENSION_CLIENT).session(mSession) << "Simulate changes for " << ref.name << " in: " << mConnectionToken;

        // Generate changelist based on notion of nothing been there initially
        if (ref.objectType == kExtensionLiveDataTypeArray) {
            std::vector<LiveArrayChange> changes;
            auto& array = std::static_pointer_cast<LiveArray>(ref.objectPtr)->getArray();
            changes.emplace_back(LiveArrayChange::insert(0, array.size()));
            reportLiveArrayChanges(ref, changes);
        } else {
            std::vector<LiveMapChange> changes;
            auto& map = std::static_pointer_cast<LiveMap>(ref.objectPtr)->getMap();
            for (auto& item : map) {
                changes.emplace_back(LiveMapChange::set(item.first));
            }
            reportLiveMapChanges(ref, changes);
        }
        ref.hasPendingUpdate = false;
    }

    mPendingEvents.clear();
}

} // namespace apl
