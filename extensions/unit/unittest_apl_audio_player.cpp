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

#include "gtest/gtest.h"

#include "alexaext/extensionmessage.h"
#include "alexaext/APLAudioPlayerExtension/AplAudioPlayerExtension.h"

using namespace alexaext;
using namespace AudioPlayer;
using namespace rapidjson;

class TestAudioPlayerObserver : public AplAudioPlayerExtensionObserverInterface {
public:
    void onAudioPlayerPlay() override
    { mCommand = "PLAY"; }

    void onAudioPlayerPause() override
    { mCommand = "PAUSE"; }

    void onAudioPlayerNext() override
    { mCommand = "NEXT"; }

    void onAudioPlayerPrevious() override
    { mCommand = "PREVIOUS"; }

    void onAudioPlayerSeekToPosition(int offsetInMilliseconds) override
    {
        mCommand = "SEEK";
        mParaNum = offsetInMilliseconds;
    }

    void onAudioPlayerToggle(const std::string &name, bool checked) override
    {
        mCommand = "TOGGLE";
        mParamString = name;
        mParamBool = checked;
    };

    void onAudioPlayerLyricDataFlushed(const std::string &token,
                                       long durationInMilliseconds,
                                       const std::string &lyricData) override
    {
        mCommand = "FLUSHED";
        mParamString = token;
        mParamJson = lyricData;
        mParaNum = durationInMilliseconds;
    }

    void onAudioPlayerSkipForward() override
    { mCommand = "FORWARD"; }

    void onAudioPlayerSkipBackward() override
    { mCommand = "BACKWARD"; }

public:
    std::string mCommand;
    double mParaNum;
    bool mParamBool;
    std::string mParamJson; //  string representation of json
    std::string mParamString;
};

class TestAudioPlayerExtension : public AplAudioPlayerExtension {
public:
    explicit TestAudioPlayerExtension(std::shared_ptr<AplAudioPlayerExtensionObserverInterface> observer)
            : AplAudioPlayerExtension(observer)
    {}

    void updateLiveData() {
        AplAudioPlayerExtension::publishLiveData();
    }
};

class AplAudioPlayerExtensionTest : public ::testing::Test {
public:

    void SetUp() override
    {
        mObserver = std::make_shared<TestAudioPlayerObserver>();
        mExtension = std::make_shared<TestAudioPlayerExtension>(mObserver);
    }

    /**
     * Simple utility to create activity descriptors accross the tests
     */
    ActivityDescriptor createActivityDescriptor(std::string uri = URI) {
        // Create Activity
        SessionDescriptorPtr sessionPtr = SessionDescriptor::create("TestSessionId");
        ActivityDescriptor activityDescriptor(
            uri,
            sessionPtr);
        return activityDescriptor;
    }

    /**
     * Simple registration for testing event/command/data.
     */
    ::testing::AssertionResult registerExtension(const alexaext::ActivityDescriptor& activity)
    {
        Document settings(kObjectType);
        settings.AddMember("playbackStateName", Value("MyPlayBackState"), settings.GetAllocator());

        Document regReq = RegistrationRequest("1.0").uri(URI)
                                                    .settings(settings);
        auto registration = mExtension->createRegistration(activity, regReq);
        mExtension->onActivityRegistered(activity);
        auto method = GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, "Fail");
        if (std::strcmp("RegisterSuccess", method) != 0)
            return testing::AssertionFailure() << "Failed Registration:" << method;
        mClientToken = GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, "");
        if (mClientToken.length() == 0)
            return testing::AssertionFailure() << "Failed Token:" << mClientToken;

        return ::testing::AssertionSuccess();
    }

    inline
    ::testing::AssertionResult CheckLiveData(const Value &update, const std::string &operation,
                                             const std::string &key)
    {
        using namespace testing;

        if (!update.IsObject())
            return AssertionFailure() << "Invalid json object" << update.GetType();

        std::string op = GetWithDefault<const char *>(LiveDataMapOperation::TYPE(), update, "");
        if (op != operation)
            return AssertionFailure() << "Invalid operation - expected:" << operation << " actual:" << op;

        std::string kk = GetWithDefault<const char *>(LiveDataMapOperation::KEY(), update, "");
        if (kk != key)
            return AssertionFailure() << "Invalid key - expected:" << key << " actual:" << kk;

        return AssertionSuccess();
    }

    ::testing::AssertionResult CheckLiveData(const Value &update, const std::string &operation,
                                             const std::string &key, const char *item)
    {
        using namespace testing;

        const auto &preCheck = CheckLiveData(update, operation, key);
        if (!preCheck)
            return preCheck;

        const rapidjson::Value *itm = LiveDataMapOperation::ITEM().Get(update);
        if (!itm || !itm->IsString())
            return AssertionFailure() << "Invalid item type";

        // string compare
        const char *value = itm->GetString();
        if (std::strcmp(value, item) != 0)
            return AssertionFailure() << "Invalid item - expected:" << item << " actual:" << value;

        return AssertionSuccess();
    }

    template<typename T>
    ::testing::AssertionResult CheckLiveData(const Value &update, const std::string &operation,
                                             const std::string &key, T item)
    {

        using namespace testing;

        const auto &preCheck = CheckLiveData(update, operation, key);
        if (!preCheck)
            return preCheck;

        const rapidjson::Value *itm = LiveDataMapOperation::ITEM().Get(update);
        if (!itm || itm->IsNull() || !itm->Is<T>())
            return AssertionFailure() << "Invalid item type";

        T value = itm->Get<T>();
        if (item != value)
            return AssertionFailure() << "Invalid item - extected:" << item << " actual:" << value;

        return AssertionSuccess();
    }

    Value* findDataType (Value *types, const std::string& typeName) {
        for (auto& v : types->GetArray()) {
            auto name = GetWithDefault<const char *>(TypePropertySchema::NAME(), v, "");
            if (typeName.compare(name) == 0) {
                return &v;
            }
        }
        return nullptr;
    }

    inline
    ::testing::AssertionResult IsEqual(const Value &lhs, const Value &rhs)
    {

        if (lhs != rhs) {
            return ::testing::AssertionFailure() << "Documents not equal\n"
                                                 << "lhs:\n" << AsPrettyString(lhs)
                                                 << "\nrhs:\n" << AsPrettyString(rhs) << "\n";
        }
        return ::testing::AssertionSuccess();
    }

    std::shared_ptr<TestAudioPlayerObserver> mObserver;
    std::shared_ptr<TestAudioPlayerExtension>  mExtension;
    std::string mClientToken;
};

/**
 * Simple create test for sanity.
 */
TEST_F(AplAudioPlayerExtensionTest, CreateExtension)
{
    ASSERT_TRUE(mObserver);
    ASSERT_TRUE(mExtension);
    auto supported = mExtension->getURIs();
    ASSERT_EQ(1, supported.size());
    ASSERT_NE(supported.end(), supported.find(URI));
}

/**
 * Registration request with bad URI.
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationURIBad)
{
    SessionDescriptorPtr sessionPtr = SessionDescriptor::create("TestSessionId");
    ActivityDescriptor badActivity("aplext:audioplayer:BAD", sessionPtr);
    Document regReq = RegistrationRequest("aplext:audioplayer:BAD");
    auto registration = mExtension->createRegistration(badActivity, regReq);
    ASSERT_FALSE(registration.HasParseError());
    ASSERT_FALSE(registration.IsNull());
    ASSERT_STREQ("RegisterFailure",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
}

/**
 * Registration Success has required fields
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationSuccess)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    ASSERT_STREQ("aplext:audioplayer:10",
                 GetWithDefault<const char *>(RegistrationSuccess::URI(), registration, ""));
    auto schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);
    ASSERT_STREQ("aplext:audioplayer:10", GetWithDefault<const char *>("uri", *schema, ""));
    std::string token = GetWithDefault<const char *>(RegistrationSuccess::TOKEN(), registration, "");
    ASSERT_TRUE(token.rfind("AplAudioPlayerExtension") == 0);
}

/**
 * Environment registration has best practice of version
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationEnvironmentVersion)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *environment = RegistrationSuccess::ENVIRONMENT().Get(registration);
    ASSERT_TRUE(environment);
    ASSERT_STREQ("APLAudioPlayerExtension-1.0",
                 GetWithDefault<const char *>(Environment::VERSION(), *environment, ""));
}

/**
 * Commands are defined at registration.
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationCommands)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);

    Value *commands = ExtensionSchema::COMMANDS().Get(*schema);
    ASSERT_TRUE(commands);

    auto expectedCommandSet = std::set<std::string>();
    expectedCommandSet.insert("Play");
    expectedCommandSet.insert("Pause");
    expectedCommandSet.insert("Previous");
    expectedCommandSet.insert("Next");
    expectedCommandSet.insert("SeekToPosition");
    expectedCommandSet.insert("Toggle");
    expectedCommandSet.insert("AddLyricsViewed");
    expectedCommandSet.insert("AddLyricsDurationInMilliseconds");
    expectedCommandSet.insert("FlushLyricData");
    expectedCommandSet.insert("SkipForward");
    expectedCommandSet.insert("SkipBackward");
    ASSERT_TRUE(commands->IsArray() && commands->Size() == expectedCommandSet.size());

    for (const Value &com : commands->GetArray()) {
        ASSERT_TRUE(com.IsObject());
        auto name = GetWithDefault<const char *>(Command::NAME(), com, "MissingName");
        ASSERT_TRUE(expectedCommandSet.count(name) == 1) << "Unknown Command:" << name;
        expectedCommandSet.erase(name);
    }
    ASSERT_TRUE(expectedCommandSet.empty());
}

/**
 * Events are defined
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationEvents)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);

    Value *events = ExtensionSchema::EVENTS().Get(*schema);
    ASSERT_TRUE(events);

    // FullSet event handler for audio player
    auto expectedHandlerSet = std::set<std::string>();
    expectedHandlerSet.insert("OnPlayerActivityUpdated");
    ASSERT_TRUE(events->IsArray() && events->Size() == expectedHandlerSet.size());

    // should have all event handlers defined
    for (const Value &evt : events->GetArray()) {
        ASSERT_TRUE(evt.IsObject());
        auto name = GetWithDefault<const char *>(Event::NAME(), evt, "MissingName");
        ASSERT_TRUE(expectedHandlerSet.count(name) == 1);
        expectedHandlerSet.erase(name);
    }
    ASSERT_TRUE(expectedHandlerSet.empty());
}

/**
 * LiveData registration is not defined without settings.
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationSettingsEmpty)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);

    // no live data
    Value *liveData = ExtensionSchema::LIVE_DATA().Get(*schema);
    ASSERT_TRUE(liveData);
    ASSERT_TRUE(liveData->IsArray() && liveData->Empty());
}

/**
 * LiveData registration is defined with settings.
 */
TEST_F(AplAudioPlayerExtensionTest, RegistrationSettingsHasLiveData)
{
    Document settings(kObjectType);
    settings.AddMember("playbackStateName", Value("MyPlayBackState"), settings.GetAllocator());

    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI)
                                                .settings(settings);
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);

    // live data defined
    Value *liveData = ExtensionSchema::LIVE_DATA().Get(*schema);
    ASSERT_TRUE(liveData);
    ASSERT_TRUE(liveData->IsArray() && liveData->Size() == 1);

    const Value& data = (*liveData)[0];
    ASSERT_TRUE(data.IsObject());
    auto name = GetWithDefault<const char *>(LiveDataSchema::NAME(), data, "");
    ASSERT_STREQ("MyPlayBackState", name);
    auto type = GetWithDefault<const char *>(LiveDataSchema::DATA_TYPE(), data, "");
    ASSERT_STREQ("playbackState", type);

    Value *types = ExtensionSchema::TYPES().Get(*schema);
    ASSERT_TRUE(types);
    ASSERT_TRUE(liveData->IsArray());

    Value *stateType = findDataType(types, "playbackState");
    ASSERT_NE(nullptr, stateType);
    ASSERT_TRUE(stateType->IsObject());

    rapidjson::Document expected;
    expected.Parse(R"(
        {
            "name": "playbackState",
            "properties": {
                "playerActivity": "string",
                "offset": "number"
            }
        }
    )");
    ASSERT_FALSE(expected.HasParseError());
    ASSERT_TRUE(IsEqual(expected, *stateType));

}

/**
* Invalid settings on registration are handled and defaults are used.
**/
TEST_F(AplAudioPlayerExtensionTest, RegistrationSettingsBad)
{
    auto activity = createActivityDescriptor();
    Document regReq = RegistrationRequest("1.0").uri(URI)
                                                .settings(Value());
    auto registration = mExtension->createRegistration(activity, regReq);
    ASSERT_STREQ("RegisterSuccess",
                 GetWithDefault<const char *>(RegistrationSuccess::METHOD(), registration, ""));
    Value *schema = RegistrationSuccess::SCHEMA().Get(registration);
    ASSERT_TRUE(schema);
    // live data available
    Value *liveData = ExtensionSchema::LIVE_DATA().Get(*schema);
    ASSERT_TRUE(liveData);
    ASSERT_TRUE(liveData->IsArray() && liveData->Empty());
}

/**
 * LiveData player activity is published when settings assigned.
 */
TEST_F(AplAudioPlayerExtensionTest, GetLiveDataObjectsSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    bool gotUpdate = false;
    mExtension->registerLiveDataUpdateCallback(
            [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
                gotUpdate = true;
                ASSERT_STREQ("LiveDataUpdate",
                             GetWithDefault<const char *>(RegistrationSuccess::METHOD(), liveDataUpdate, ""));
                const Value *ops = LiveDataUpdate::OPERATIONS().Get(liveDataUpdate);
                ASSERT_TRUE(ops);
                ASSERT_TRUE(ops->IsArray() && ops->Size() == 2);
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[0], "Set", "playerActivity", "STOPPED"));
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[1], "Set", "offset", 0));
            });
    mExtension->updateLiveData();
    ASSERT_TRUE(gotUpdate);
}

/**
 * Command Play calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandPlaySuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Play");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("PLAY", mObserver->mCommand);
}

/**
 * Command Pause calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandPauseSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Pause");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("PAUSE", mObserver->mCommand);
}

/**
 * Command Previous calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandPreviousSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Previous");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("PREVIOUS", mObserver->mCommand);
}

/**
 * Command Next calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandNextSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Next");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("NEXT", mObserver->mCommand);
}

/**
 * Command Seek handles missing params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandSeekToPositionMissingParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("SeekToPosition");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);
}

/**
 * Command Seek handles bad params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandSeekToPositionBadParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("SeekToPosition")
                                 .property("offset", "wrong");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);
}

/**
 * Command Seek calls observer with offset.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandSeekToPositionSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("SeekToPosition")
                                 .property("offset", 42);
    bool gotUpdate = false;
    mExtension->registerLiveDataUpdateCallback(
            [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
                gotUpdate = true;
                ASSERT_STREQ("LiveDataUpdate",
                             GetWithDefault<const char *>(RegistrationSuccess::METHOD(), liveDataUpdate, ""));
                ASSERT_STREQ("aplext:audioplayer:10",
                             GetWithDefault<const char *>(RegistrationSuccess::TARGET(), liveDataUpdate, ""));
                const Value *ops = LiveDataUpdate::OPERATIONS().Get(liveDataUpdate);
                ASSERT_TRUE(ops);
                ASSERT_TRUE(ops->IsArray() && ops->Size() == 2);
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[1], "Set", "offset", 42));
            });
    auto invoke = mExtension->invokeCommand(activity, command);
    
    ASSERT_TRUE(gotUpdate);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("SEEK", mObserver->mCommand);
    ASSERT_EQ(42, mObserver->mParaNum);
}

/**
 * Command SkipForward calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandSkipForwardSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("SkipForward");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("FORWARD", mObserver->mCommand);
}

/**
 * Command SkipBackward calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandSkipBackwardSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("SkipBackward");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("BACKWARD", mObserver->mCommand);
}

/**
 * Command Toggle handles missing params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandToggleMissingParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    // missing checked param
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Toggle")
                                 .property("name", "value");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);

    // missing name param
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("Toggle")
                            .property("checked", true);
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);
}

/**
 * Command Toggle handles invalid params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandToggleBadParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    // missing checked param
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Toggle")
                                 .property("name", 0)
                                 .property("checked", true);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);

    // missing name param
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("Toggle")
                            .property("name", "thumbsUp")
                            .property("checked", -10);
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
    ASSERT_EQ("", mObserver->mCommand);
}

/**
 * Command Toggle calls observer.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandToggleSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("Toggle")
                                 .property("name", "thumbsUp")
                                 .property("checked", true);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    ASSERT_EQ("TOGGLE", mObserver->mCommand);
    ASSERT_EQ("thumbsUp", mObserver->mParamString);
    ASSERT_EQ(true, mObserver->mParamBool);
}


const char *LINES = R"(
    {
      "lines": [
        {
          "text": "hello",
          "startTime": 0,
          "endTime": 500
        },
        {
          "text": "friend",
          "startTime": 500,
          "endTime": 1000
        }
      ]
    })";

/**
 * Command AddLyricsViewed handles missing params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsViewedMissingParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    // missing "lines"
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN");
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);

    // missing "token"
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("AddLyricsViewed")
                            .property("lines", lines["lines"].Move());
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
}

/**
 * Command AddLyricsViewed handles invalid params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsViewedBadParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    // bad token
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);

    // bad lines param handled in folllow-on test
}

/**
 * Add lyrics stores lyric data.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsViewedSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    mExtension->onActivityUnregistered(activity);
    lines.Parse(LINES);
    // Observer is notified of flush.
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(0, mObserver->mParaNum);
}

//  Invalid line data in additon to valid data from LINES
const char *BAD_LINES = R"(
    {
      "lines": [
        {
          "text": "badStart",
          "startTime": -100,
          "endTime": 500
        },
        {
          "text": "badEnd",
          "startTime": 500,
          "endTime": -100
        },
        {
          "text": "endBeforeStart",
          "startTime": 500,
          "endTime": 400
        },
        {
          "text": "",
          "startTime": 0,
          "endTime": 100
        },
        {
          "text": "hello",
          "startTime": 0,
          "endTime": 500
        },
        {
          "text": "friend",
          "startTime": 500,
          "endTime": 1000
        }
      ]
    })";

/**
 * Command AddLyricsViewed ignores invalid lines params and succeeds.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsIgnoreBadLines)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(BAD_LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    lines.Parse(LINES);
    // verify only good line data recorded
    mExtension->onActivityUnregistered(activity);
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(0, mObserver->mParaNum);
}

TEST_F(AplAudioPlayerExtensionTest, InvokeFlushLyricsSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    // Flush data
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("FlushLyricData")
                            .property("token", "SONG-TOKEN");
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    lines.Parse(LINES);
    // Observer is notified of flush.
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(0, mObserver->mParaNum);
}

TEST_F(AplAudioPlayerExtensionTest, ShouldFlushLyricsOnUnregister)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    // Unregister activity
    mExtension->onActivityUnregistered(activity);

    lines.Parse(LINES);
    // Observer is notified of flush.
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(0, mObserver->mParaNum);
}

TEST_F(AplAudioPlayerExtensionTest, ShouldFlushLyricsOnTokenChange)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                       .uri(URI)
                       .name("AddLyricsViewed")
                       .property("token", "SONG-TOKEN")
                       .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    // Changing token causes lyrics to get flushed
    lines.Parse(LINES);
    auto newCommand = Command("1.0").target(mClientToken)
                        .uri(URI)
                        .name("AddLyricsViewed")
                        .property("token", "OTHER_TOKEN")
                        .property("lines", lines["lines"]);
    invoke = mExtension->invokeCommand(activity, newCommand);
    ASSERT_TRUE(invoke);

    lines.Parse(LINES);
    // Observer is notified of flush.
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(0, mObserver->mParaNum);
}


/**
 * Command AddLyricsDurationInMilliseconds handles missing params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsDurationInMillisecondsMissingParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    // missing token
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsDurationInMilliseconds")
                                 .property("durationInMilliseconds", 100);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);

    // missing duration
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("AddLyricsDurationInMilliseconds")
                            .property("token", "SONG-TOKEN");
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
}

/**
 * Command AddLyricsDuratioInMilliseconds handles ivalid params and properly fails.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsDurationInMillisecondsBadParamFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    // bad token
    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsDurationInMilliseconds")
                                 .property("token", "")
                                 .property("durationInMilliseconds", 100);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);

    // bad duration
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("AddLyricsDurationInMilliseconds")
                            .property("token", "SONG-TOKEN")
                            .property("durationInMilliseconds", -1);
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_FALSE(invoke);
}

/**
 * Command AddLyricsDurationInMilliseconds duration update is sent to observer on flush.
 */
TEST_F(AplAudioPlayerExtensionTest, InvokeCommandAddLyricsDurationInMillisecondsSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    Document lines;
    lines.Parse(LINES);
    ASSERT_FALSE(lines.HasParseError());

    auto command = Command("1.0").target(mClientToken)
                                 .uri(URI)
                                 .name("AddLyricsViewed")
                                 .property("token", "SONG-TOKEN")
                                 .property("lines", lines["lines"]);
    auto invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);
    lines.Parse(LINES);

    // add duration
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("AddLyricsDurationInMilliseconds")
                            .property("token", "SONG-TOKEN")
                            .property("durationInMilliseconds", 53);
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);

    // Flush data
    command = Command("1.0").target(mClientToken)
                            .uri(URI)
                            .name("FlushLyricData")
                            .property("token", "SONG-TOKEN");
    invoke = mExtension->invokeCommand(activity, command);
    ASSERT_TRUE(invoke);


    // Observer is notified of flush.
    ASSERT_EQ("FLUSHED", mObserver->mCommand);
    ASSERT_EQ(AsString(lines["lines"]), mObserver->mParamJson);
    ASSERT_EQ(53, mObserver->mParaNum);
}

/**
 * Playback progress change updates live data.
 */
TEST_F(AplAudioPlayerExtensionTest, UpdatePlaybackProgressSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    bool gotUpdate = false;
    mExtension->registerLiveDataUpdateCallback(
            [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
                gotUpdate = true;
                ASSERT_STREQ("LiveDataUpdate",
                             GetWithDefault<const char *>(RegistrationSuccess::METHOD(), liveDataUpdate, ""));
                ASSERT_STREQ("aplext:audioplayer:10",
                           GetWithDefault<const char *>(RegistrationSuccess::TARGET(), liveDataUpdate, ""));
                const Value *ops = LiveDataUpdate::OPERATIONS().Get(liveDataUpdate);
                ASSERT_TRUE(ops);
                ASSERT_TRUE(ops->IsArray() && ops->Size() == 2);
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[0], "Set", "playerActivity", "STOPPED"));
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[1], "Set", "offset", 100));
            });

    mExtension->updatePlaybackProgress(100);
    ASSERT_TRUE(gotUpdate);
}

/**
 * Playback state change updates live data.
 */
TEST_F(AplAudioPlayerExtensionTest, UpdatePlayerActivityLiveDataSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    bool gotUpdate = false;
    mExtension->registerLiveDataUpdateCallback(
            [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
                gotUpdate = true;
                ASSERT_STREQ("LiveDataUpdate",
                             GetWithDefault<const char *>(LiveDataUpdate::METHOD(), liveDataUpdate, ""));
                ASSERT_STREQ("aplext:audioplayer:10",
                           GetWithDefault<const char *>(LiveDataUpdate::TARGET(), liveDataUpdate, ""));
                const Value *ops = LiveDataUpdate::OPERATIONS().Get(liveDataUpdate);
                ASSERT_TRUE(ops);
                ASSERT_TRUE(ops->IsArray() && ops->Size() == 2);
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[0], "Set", "playerActivity", "PLAYING"));
                ASSERT_TRUE(CheckLiveData(ops->GetArray()[1], "Set", "offset", 100));
            });

    mExtension->updatePlayerActivity("PLAYING", 100);
    ASSERT_TRUE(gotUpdate);
}

/**
 * Playback state change updates live data.
 */
TEST_F(AplAudioPlayerExtensionTest, UpdatePlayerActivityEventSuccess)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    bool gotUpdate = false;
    mExtension->registerEventCallback(
        [&](const std::string &uri, const rapidjson::Value &event) {
            gotUpdate = true;
            ASSERT_STREQ("Event",
                         GetWithDefault<const char *>(Event::METHOD(), event, ""));
            ASSERT_STREQ("aplext:audioplayer:10",
                         GetWithDefault<const char *>(Event::TARGET(), event, ""));
            auto payload = Event::PAYLOAD().Get(event);
            ASSERT_STREQ("PLAYING",
                         GetWithDefault<const char *>("playerActivity", payload, ""));
            ASSERT_EQ(100,
                      GetWithDefault<int>("offset", payload, -1));
        });

    mExtension->updatePlayerActivity("PLAYING", 100);
    ASSERT_TRUE(gotUpdate);
}

/**
 * Invalid updates to playback state and progress are ignored.
 */
TEST_F(AplAudioPlayerExtensionTest, UpdatePlayerActivityFailure)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    bool gotUpdate = false;
    mExtension->registerLiveDataUpdateCallback(
            [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
                gotUpdate = true;
            });

    mExtension->updatePlayerActivity("Invalid", 100);
    ASSERT_FALSE(gotUpdate);
    mExtension->updatePlayerActivity("", 100);
    ASSERT_FALSE(gotUpdate);
    mExtension->updatePlayerActivity("PAUSED", -100);
    ASSERT_TRUE(gotUpdate);
}

/**
 * Validates changing state always triggers an update.
 */
TEST_F(AplAudioPlayerExtensionTest, UpdatePlayerActivityStateChange)
{
    auto activity = createActivityDescriptor();
    ASSERT_TRUE(registerExtension(activity));

    int updateCount = 0;
    mExtension->registerLiveDataUpdateCallback(
        [&](const ActivityDescriptor& activity, const rapidjson::Value &liveDataUpdate) {
            updateCount++;
        });

    mExtension->updatePlayerActivity("PAUSED", 0);
    ASSERT_EQ(1, updateCount);
    mExtension->updatePlayerActivity("PLAYING", 0);
    ASSERT_EQ(2, updateCount);
    mExtension->updatePlayerActivity("PLAYING", 50);
    ASSERT_EQ(3, updateCount);
    mExtension->updatePlaybackProgress(100);
    ASSERT_EQ(4, updateCount);
}