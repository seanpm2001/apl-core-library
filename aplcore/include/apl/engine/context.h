/*
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

#ifndef _APL_CONTEXT_H
#define _APL_CONTEXT_H

#include <yoga/Yoga.h>

#include "apl/common.h"
#include "apl/component/componentproperties.h"
#include "apl/content/metrics.h"
#include "apl/engine/contextobject.h"
#include "apl/engine/jsonresource.h"
#include "apl/engine/recalculatesource.h"
#include "apl/engine/recalculatetarget.h"
#include "apl/engine/styleinstance.h"
#include "apl/primitives/object.h"
#include "apl/primitives/textmeasurerequest.h"
#include "apl/utils/counter.h"
#include "apl/utils/localemethods.h"
#include "apl/utils/lrucache.h"
#include "apl/utils/noncopyable.h"
#include "apl/utils/path.h"

#ifdef SCENEGRAPH
#include "apl/scenegraph/common.h"
#endif // SCENEGRAPH

namespace apl {

class DataSourceConnection;
class Event;
class RootConfig;
class Sequencer;
class State;
class Styles;

class ExtensionManager;
class FocusManager;
class HoverManager;
class KeyboardManager;
class LayoutManager;
class LiveDataManager;
class UIDManager;

using DataSourceConnectionPtr = std::shared_ptr<DataSourceConnection>;

/*
 * The data-binding context holds information about the local environment, metrics, and resources.
 * Context objects should be heap-allocated with a shared pointer to their parent context.
 */
class Context : public RecalculateTarget<std::string>,
                public RecalculateSource<std::string>,
                public std::enable_shared_from_this<Context>,
                public NonCopyable,
                public Counter<Context> {

public:
    /**
     * Create a context that is the child of another context.
     * @param parent The parent context.
     * @return The child context.
     */
    static ContextPtr createFromParent(const ContextPtr &parent) {
        return std::make_shared<Context>(parent);
    }

    /**
     * Create a top-level context for testing. Do not use this for non-testing code
     * @param metrics Display metrics
     * @param session The logging session
     * @return The context
     */
    static ContextPtr createTestContext(const Metrics& metrics, const SessionPtr& session);

    /**
     * Create a top-level context for testing. Do not use this for production.
     * @param metrics Display metrics.
     * @param config Root configuration
     * @return The context
     */
    static ContextPtr createTestContext(const Metrics& metrics, const RootConfig& config);

    /**
     * Create a top-level context for testing. Do not use this for production.
     * @param metrics Display metrics.
     * @param config Root configuration
     * @param session The logging session
     * @return The context
     */
    static ContextPtr createTestContext(const Metrics& metrics, const RootConfig& config, const SessionPtr& session);

    /**
     * Create a top-level context for the Content evaluation.
     * @param metrics Display metrics.
     * @param config Root configuration
     * @param theme Theme
     * @param session Session
     * @return The context
     */
    static ContextPtr createContentEvaluationContext(
        const Metrics& metrics,
        const RootConfig& config,
        const std::string& aplVersion,
        const std::string& theme,
        const SessionPtr& session);

    /**
     * Create a top-level context for extension definition.
     * @param config Root configuration
     * @param session Session
     * @return The context
     */
    static ContextPtr createTypeEvaluationContext(
        const RootConfig& config,
        const std::string& aplVersion,
        const SessionPtr& session);

    /**
     * Create a top-level context.  Only used by RootContext
     * @param metrics Display metrics
     * @param core Internal document data.
     * @return The context.
     */
    static ContextPtr createRootEvaluationContext(const Metrics& metrics,
                                                  const ContextDataPtr& core);

    /**
     * Create a "clean" context.  This shares the same root data, but does not contain any
     * of the built content.  It does contain the top-level resources.
     * Used when creating clean data-binding contexts for graphics.
     * @param other The context to build from.
     * @return The context.
     */
    static ContextPtr createClean(const ContextPtr& other);

    /**
     * Construct a child context. Do not call this method directly; use the ::create*() method instead.
     * @param parent The parent of this context.
     */
    explicit Context(const ContextPtr& parent)
        : mParent(parent), mTop(parent->top() ? parent->top() : parent), mCore(parent->mCore) {}

    /**
     * Construct a free-standing context.  Do not call this directly; use the ::create* method instead
     * @param metrics The display metrics.
     * @param core A pointer to the document data.
     */
    Context(const Metrics& metrics, const ContextDataPtr& core);

    /**
     * Standard destructor
     */
    ~Context() override = default;

    /**
     * Release local data bindings in this context.  Resources that hold context references (such as a GraphicPattern)
     * can set up a loop in the context system.  Calling this routine releases all locally defined data-bindings.
     */
    void release() {
        mMap.clear();
    }

    /**
     * Return a reference to an object in some context.  This is typically
     * used to find and retrieve objects when searching upwards through the context hierarchy.
     *
     * Note that we store raw pointers in this object.  This object should only be used as a
     * temporary when there is no chance of a ContextPtr going out of scope.
     */
    class ContextRef {
    public:
        ContextRef() = default;
        ContextRef(const Context& context, const ContextObject& object) : mContext(&context), mObject(&object) {}

        bool empty() const {
            return mContext == nullptr || mObject == nullptr;
        }

        const ContextObject& object() const {
            assert(mObject);
            return *mObject;
        }

        ContextPtr context() const {
            return mContext ? std::const_pointer_cast<Context>(mContext->shared_from_this()) : nullptr;
        }

    private:
        const Context *mContext = nullptr;
        const ContextObject *mObject = nullptr;
    };

    /**
     * Find a reference to an object in a context.  The returned object may be empty.
     * @param key The name to search for
     * @return The context reference object
     */
    ContextRef find(const std::string& key) const {
        auto it = mMap.find(key);
        if (it != mMap.end())
            return { *this, it->second };

        if (mParent)
            return mParent->find(key);

        return {};
    }

    /**
     * Look up a value in the context.  If the value doesn't exist, return null.
     * @param key The string name to look up.
     * @return The value or null.
     */
    Object opt(const std::string& key) const {
        auto cr = find(key);
        if (!cr.empty())
            return cr.object().value();

        return Object::NULL_OBJECT();
    }

    /**
     * Check to see if a value exists in the context.
     * @param key The string name to look up.
     * @return True if the value is defined somewhere in this context or an ancestor context.
     */
    bool has(const std::string& key) const {
        auto cr = find(key);
        return !cr.empty();
    }

    /**
     * Check to see if a value exists in the local context
     * @param key The string name to look up
     * @return True if the values is defined somewhere in this immediate context (not an ancestor)
     */
    bool hasLocal(const std::string& key) const {
        return mMap.find(key) != mMap.end();
    }

    /**
     * Check if a key exists somewhere in the context chain as an immutable value.
     * @param key The string name to look up.
     * @return True if the value is defined in the context chain and is immutable
     */
    bool hasImmutable(const std::string& key) const {
        auto cr = find(key);
        if (!cr.empty())
            return !cr.object().isMutable();
        return false;
    }

    /**
     * Find the first context containing a specific key.
     * @param key The key to search for.
     * @return The first context or nullptr if one can't be found.
     */
    ContextPtr findContextContaining(const std::string& key) const {
        auto cr = find(key);
        return cr.context();
    }

    void setValue(std::string key, const Object& value, bool) override {
        auto it = mMap.find(key);
        if (it == mMap.end())
            return;

        if (it->second.set(value))
            enqueueDownstream(key);
    }

    /**
     * Write a value in the current context.  This only works for user-writeable values.
     * It will fail if the value does not already exist.  This method will search in parent
     * contexts if the value is not found in the current context.
     * @param key The string key name.
     * @param value The value to store.
     * @param useDirtyFlag If true, mark changes downstream with a dirty flag
     * @return True if the key already exists in this context (it may not be changed)
     */
    bool userUpdateAndRecalculate(const std::string& key, const Object& value, bool useDirtyFlag);

    /**
     * Mutate a value in the current context.  This only works for user- and system-writeable values.
     * It will fail if the value does not already exist.  This method ONLY searches in the current context.
     * @param key The string key name.
     * @param value The value to store.
     * @param useDirtyFlag If true, mark changes downstream with a dirty flag
     * @return True if the key already exists in this context (it may not be changed)
     */
    bool systemUpdateAndRecalculate(const std::string& key, const Object& value, bool useDirtyFlag);

    /**
     * Store a value in the current context.  If the value already exists in the current
     * context, nothing is written.  The value is stored as a fixed property and may not be changed.
     * @param key The string key name.
     * @param value The value to store.
     */
    void putConstant(const std::string& key, const Object& value)
    {
        mMap.emplace(key, ContextObject(value));
    }

    /**
     * Store a value in the current context.  If the value already exists in the current
     * context, nothing is written.  The value may be changed with the userUpdateAndRecalculate method.
     * User-writeable value include component "bind" properties, "parameters" from a layout inflation,
     * and graphic "parameters".
     *
     * @param key The string key name
     * @param value The value to store.
     */
    void putUserWriteable(const std::string& key, const Object& value)
    {
        mMap.emplace(key, ContextObject(value).userWriteable());
    }

    /**
     * Store a value in the current context and mark it as mutable.  If the value already exists in the
     * current context, nothing is written.  The value may be changed with the systemUpdateAndRecalculate
     * method.  System-writeable values include the "width"/"height" properties assigned to a graphic
     * during layout.
     * @param key The string key name
     * @param value The value to store
     */
    void putSystemWriteable(const std::string& key, const Object& value)
    {
        mMap.emplace(key, ContextObject(value).systemWriteable());
    }

    /**
     * Store a resource and provenance path data in the current context.
     * Resources are allowed to overwrite an existing resource with the same name.
     *
     * @param key The string key name
     * @param value The value to store
     * @param path The path data to associate with this key
     */
    void putResource(const std::string& key, const Object& value, const Path& path) {
        // Toss away a resource if it already exists (we overwrite it)
        auto it = mMap.find(key);
        if (it != mMap.end())
            mMap.erase(it);

        mMap.emplace(key, ContextObject(value).provenance(path));
    }

    /**
     * Remove resource from the context.
     * @param key The string key name
     */
    void remove(const std::string& key) {
        auto it = mMap.find(key);
        if (it != mMap.end())
            mMap.erase(it);
    }

    /**
     * Return the provenance associated with this key.
     * @param key The string key name
     * @return The provenance path data, or an empty string if it can't be found
     */
    std::string provenance(const std::string& key) const {
        // The provenance for a key can only be used if the current map has that key entry
        auto it = mMap.find(key);
        if (it != mMap.end())
            return it->second.provenance().toString();

        if (mParent)
            return mParent->provenance(key);

        return "";
    }

    /**
     * Check if a value is mutable
     * @param key The string key name
     * @return True if the value is mutable.
     */
    bool isMutable(const std::string& key) const {
        auto it = mMap.find(key);
        if (it != mMap.end())
            return it->second.isMutable();

        if (mParent)
            return mParent->isMutable(key);

        return false;
    }

    /**
     * @return An iterator to the beginning of defined bindings
     */
    std::map<std::string, ContextObject>::const_iterator begin() const { return mMap.begin(); }

    /**
     * @return An iterator to the end of the defined bindings
     */
    std::map<std::string, ContextObject>::const_iterator end() const { return mMap.end(); }

    /**
     * @return The parent of this context or nullptr if there is no parent
     */
    ConstContextPtr parent() const { return mParent; }
    ContextPtr parent() { return mParent; }

    /**
     * @return The top context for data evaluation
     */
    ConstContextPtr top() const { return mTop ? mTop : shared_from_this(); }
    ContextPtr top() { return mTop ? mTop : shared_from_this(); }

    /**
     * Convert "vw" units to "dp"
     * @param vw The amount of vw units
     * @return The corresponding amount in dp
     */
    double vwToDp(double vw) const;

    /**
     * Convert "vh" units to "dp"
     * @param vh The amount of vh units
     * @return The corresponding amount in dp
     */
    double vhToDp(double vh) const;

    /**
     * Convert pixel units to dp
     * @param px The amount of px units
     * @return The corresponding amount in dp
     */
    double pxToDp(double px) const;

    /**
     * Convert dp to pixel units
     * @param dp The amount of dp units
     * @return The corresponding amount in px
     */
    double dpToPx(double dp) const;

    /**
     * @return The height of the viewport in dp
     */
    double width() const;

    /**
     * @return The width of the viewport in dp
     */
    double height() const;

    /**
     * @return The root configuration provided by the viewhost
     */
    const RootConfig& getRootConfig() const;

    friend streamer& operator<<(streamer& os, const Context& context);

    /**
     * Lookup and return a named layout
     * @param name The name of the layout
     * @return The JSON resource defining this layout.  May be null if the layout cannot be found
     */
    const JsonResource getLayout(const std::string& name) const;

    /**
     * Lookup and return a style by name
     * @param name The name of the style
     * @param state The operating state to use when evaluating the style.
     * @return An instance of the style or nullptr if the style cannot be found.
     */
    StyleInstancePtr getStyle(const std::string& name, const State& state);

    /**
     * Lookup and return a named command
     * @param name The name of the command
     * @return The JSON resource defining this command.  May be null if the command cannot be found.
     */
    const JsonResource getCommand(const std::string& name) const;

    /**
     * Lookup and return a graphic by name.
     * @param name The name of the graphic.
     * @return The JSON resource defining this graphic.  May be null if the graphic cannot be found
     */
    const JsonResource getGraphic(const std::string& name) const;

    /**
     * Find a component with the given id or uniqueId somewhere in the DOM of the document identified by documentId.
     * To find a component in the top-level document, use the variant accepting only id.
     * @param id The id or uniqueID to search for.
     * @param documentId The document to search in.
     * @return The component or nullptr if either documentId is not registered or id is not found within the registered document.
     */
    ComponentPtr findComponentById(const std::string& id, bool traverseHost = true) const;

    /**
     * @return The top component
     */
    ComponentPtr topComponent() const;

    /**
     * @return The current theme
     */
    std::string getTheme() const;

    /**
     * @return The language as a BCP-47 string (e.g., en-US)
     */
    std::string getLang() const;

    /**
     * @return The layout direction
     */
    LayoutDirection getLayoutDirection() const;

    /**
     * @return The locale methods
     */
    std::shared_ptr<LocaleMethods> getLocaleMethods() const;

    /**
     * @return The reinflation flag
     */
    bool getReinflationFlag() const;

    /**
     * @return The APL version requested by the document
     */
    std::string getRequestedAPLVersion() const;

    /**
     * @return The dpi of the screen.
     */
    int getDpi() const;

    /**
     * @return The screen shape
     */
    ScreenShape getScreenShape() const;

    /**
     * @return The SharedContextData.
     */
    SharedContextDataPtr getShared() const;

    /**
     * @return The human-readable mode of the viewport
     */
    ViewportMode getViewportMode() const;

    /**
     * Internal routine used by components to mark themselves as changed.
     * @param ptr The component to mark
     */
    void setDirty(const ComponentPtr& ptr);

    /**
     * Internal routine used by components to mark themselves as no longer dirty
     * @param ptr The component to unmark
     */
    void clearDirty(const ComponentPtr& ptr);

    /**
     * Internal routine used by components to mark when the visual context may have changed.
     * @param ptr The component to mark
     */
    void setDirtyVisualContext(const ComponentPtr& ptr);

    /**
     * Internal routine used by components to check the dirty state of the visual context
     * @param ptr The component to check
     */
    bool isVisualContextDirty(const ComponentPtr& ptr);

    /**
     * Internal routine used by dynamic datasources to mark/unmark/test when the datasource context may have changed.
     * @param ptr The data source connection to mark
     */
    void setDirtyDataSourceContext(const DataSourceConnectionPtr& ptr);

    /**
     * @return internal text measurement cache.
     */
    LruCache<TextMeasureRequest, YGSize>& cachedMeasures();

    /**
     * @return internal text measurement baseline cache.
     */
    LruCache<TextMeasureRequest, float>& cachedBaselines();

    /**
     * @return List of pending onMount handlers for recently inflated components.
     */
    WeakPtrSet<CoreComponent>& pendingOnMounts();

    /**
     * @return true if embedded document context, false if host one.
     */
    bool embedded() const;

    /**
     * @return Relevant DocumentContext.
     */
    DocumentContextPtr documentContext() const;

#ifdef SCENEGRAPH
    /**
     * @return A cache of TextProperties
     */
    sg::TextPropertiesCache& textPropertiesCache() const;
#endif // SCENEGRAPH

    void pushEvent(Event&& event);

#ifdef ALEXAEXTENSIONS
    void pushExtensionEvent(Event&& event);
#endif

    Sequencer& sequencer() const;
    FocusManager& focusManager() const;
    HoverManager& hoverManager() const;

    LiveDataManager& dataManager() const;
    ExtensionManager& extensionManager() const;
    LayoutManager& layoutManager() const;
    MediaManager& mediaManager() const;
    MediaPlayerFactory& mediaPlayerFactory() const;
    UIDManager& uniqueIdManager() const;
    DependantManager& dependantManager() const;

    std::shared_ptr<Styles> styles() const;

    const SessionPtr& session() const;

    YGConfigRef ygconfig() const;

    const TextMeasurementPtr& measure() const;

    void takeScreenLock() const;
    void releaseScreenLock() const;

    /**
     * Inflate raw JSON into a component.  This method assumes the presence of a document and
     * inflates the JSON using the layouts, resources, and styles defined by that document.
     * @param component The raw JSON component definition.
     * @return The inflated component or nullptr if the JSON is invalid.
     */
    ComponentPtr inflate(const rapidjson::Value& component);

    rapidjson::Value serialize(rapidjson::Document::AllocatorType& allocator);

protected:
    ContextPtr mParent;
    ContextPtr mTop;
    ContextDataPtr mCore;
    std::map<std::string, ContextObject> mMap;

private:
    /**
     * Initialize environment parameters for the context
     * @param metrics The display metrics.
     * @param core A pointer to the document data.
     */
    void init(const Metrics& metrics, const ContextDataPtr& core);
};

}  // namespace apl

#endif // _APL_CONTEXT_H
