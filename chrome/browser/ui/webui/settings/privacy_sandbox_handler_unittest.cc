// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using Topic = browsing_topics::Topic;
using ::testing::Return;

constexpr char kCallbackId1[] = "test-callback-id";
constexpr char kCallbackId2[] = "test-callback-id-2";

constexpr int kTestTaxonomyVersion = 1;

void ValidateFledgeInfo(content::TestWebUI* web_ui,
                        std::string expected_callback_id,
                        std::vector<std::string> expected_joining_sites,
                        std::vector<std::string> expected_blocked_sites) {
  const content::TestWebUI::CallData& data = *web_ui->call_data().back();
  EXPECT_EQ(expected_callback_id, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_dict());

  auto* blocked_sites = data.arg3()->GetDict().FindList("blockedSites");
  ASSERT_TRUE(blocked_sites);
  ASSERT_EQ(expected_blocked_sites.size(), blocked_sites->size());
  for (size_t i = 0; i < expected_blocked_sites.size(); i++) {
    EXPECT_EQ(expected_blocked_sites[i], (*blocked_sites)[i].GetString());
  }

  auto* joining_sites = data.arg3()->GetDict().FindList("joiningSites");
  ASSERT_TRUE(joining_sites);
  ASSERT_EQ(expected_joining_sites.size(), joining_sites->size());
  for (size_t i = 0; i < expected_joining_sites.size(); i++) {
    EXPECT_EQ(expected_joining_sites[i], (*joining_sites)[i].GetString());
  }
}

void ValidateTopicsInfo(
    std::vector<privacy_sandbox::CanonicalTopic> expected_topics,
    const base::Value::List& actual_topics) {
  ASSERT_EQ(expected_topics.size(), actual_topics.size());
  for (size_t i = 0; i < expected_topics.size(); i++) {
    const auto& actual_topic = actual_topics[i];
    const auto& expected_topic = expected_topics[i];
    ASSERT_TRUE(actual_topic.is_dict());
    const base::Value::Dict& actual_topic_dict = actual_topic.GetDict();
    ASSERT_EQ(expected_topic.topic_id().value(),
              actual_topic_dict.FindInt("topicId"));
    ASSERT_EQ(expected_topic.taxonomy_version(),
              actual_topic_dict.FindInt("taxonomyVersion"));
    ASSERT_EQ(
        expected_topic.GetLocalizedRepresentation(),
        base::UTF8ToUTF16(*actual_topic_dict.FindString("displayString")));
  }
}

}  // namespace

namespace settings {

class PrivacySandboxHandlerTest : public testing::Test {
 public:
  PrivacySandboxHandlerTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = std::make_unique<PrivacySandboxHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PrivacySandboxHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PrivacySandboxHandler> handler_;
};

class PrivacySandboxHandlerTestMockService : public PrivacySandboxHandlerTest {
 public:
  void SetUp() override {
    PrivacySandboxHandlerTest::SetUp();

    mock_privacy_sandbox_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  MockPrivacySandboxService* mock_privacy_sandbox_service() {
    return mock_privacy_sandbox_service_;
  }

 private:
  raw_ptr<MockPrivacySandboxService> mock_privacy_sandbox_service_;
};

TEST_F(PrivacySandboxHandlerTestMockService, SetFledgeJoiningAllowed) {
  // Confirm that the handler forward FLEDGE allowed changes to the service.
  const std::string kTestSite = "example.com";
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              SetFledgeJoiningAllowed(kTestSite, true));

  base::Value::List args;
  args.Append(kTestSite);
  args.Append(true);
  handler()->HandleSetFledgeJoiningAllowed(args);
}

TEST_F(PrivacySandboxHandlerTestMockService, GetFledgeState) {
  // Confirm that FLEDGE state is correctly returned. As FLEDGE state is
  // retrieved async, the handler must also support multiple requests in flight.
  using Callback = base::OnceCallback<void(std::vector<std::string>)>;
  Callback callback_one;
  Callback callback_two;

  EXPECT_CALL(*mock_privacy_sandbox_service(),
              GetFledgeJoiningEtldPlusOneForDisplay(testing::_))
      .Times(2)
      .WillOnce([&](Callback callback) { callback_one = std::move(callback); })
      .WillOnce([&](Callback callback) { callback_two = std::move(callback); });

  base::Value::List args;
  args.Append(kCallbackId1);
  handler()->HandleGetFledgeState(args);

  args.clear();
  args.Append(kCallbackId2);
  handler()->HandleGetFledgeState(args);

  // Provide different sets of information to each request to the FLEDGE
  // backend.
  const std::vector<std::string> kJoiningSites1 = {"e.com", "f.com"};
  const std::vector<std::string> kJoiningSites2 = {"g.com", "h.com"};
  const std::vector<std::string> kBlockedSites1 = {"a.com", "b.com"};
  const std::vector<std::string> kBlockedSites2 = {"c.com", "d.com"};

  EXPECT_CALL(*mock_privacy_sandbox_service(),
              GetBlockedFledgeJoiningTopFramesForDisplay())
      .Times(2)
      .WillOnce(testing::Return(kBlockedSites1))
      .WillOnce(testing::Return(kBlockedSites2));

  std::move(callback_one).Run(kJoiningSites1);
  ValidateFledgeInfo(web_ui(), kCallbackId1, kJoiningSites1, kBlockedSites1);

  std::move(callback_two).Run(kJoiningSites2);
  ValidateFledgeInfo(web_ui(), kCallbackId2, kJoiningSites2, kBlockedSites2);
}

TEST_F(PrivacySandboxHandlerTestMockService, SetTopicAllowed) {
  // Confirm that the handler correctly constructs the CanonicalTopic and
  // passes it to the PrivacySandboxService.
  const privacy_sandbox::CanonicalTopic kTestTopic(Topic(1),
                                                   kTestTaxonomyVersion);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              SetTopicAllowed(kTestTopic, false))
      .Times(1);
  base::Value::List args;
  args.Append(kTestTopic.topic_id().value());
  args.Append(kTestTopic.taxonomy_version());
  args.Append(false);
  handler()->HandleSetTopicAllowed(args);
}

TEST_F(PrivacySandboxHandlerTestMockService, GetTopicsState) {
  const std::vector<privacy_sandbox::CanonicalTopic> kBlockedTopics = {
      privacy_sandbox::CanonicalTopic(Topic(1), kTestTaxonomyVersion),
      privacy_sandbox::CanonicalTopic(Topic(2), kTestTaxonomyVersion)};
  const std::vector<privacy_sandbox::CanonicalTopic> kTopTopics = {
      privacy_sandbox::CanonicalTopic(Topic(3), kTestTaxonomyVersion),
      privacy_sandbox::CanonicalTopic(Topic(4), kTestTaxonomyVersion)};

  EXPECT_CALL(*mock_privacy_sandbox_service(), GetCurrentTopTopics())
      .Times(1)
      .WillOnce(testing::Return(kTopTopics));
  EXPECT_CALL(*mock_privacy_sandbox_service(), GetBlockedTopics())
      .Times(1)
      .WillOnce(testing::Return(kBlockedTopics));

  base::Value::List args;
  args.Append(kCallbackId1);
  handler()->HandleGetTopicsState(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId1, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_dict());

  ValidateTopicsInfo(kTopTopics, *data.arg3()->GetDict().FindList("topTopics"));
  ValidateTopicsInfo(kBlockedTopics,
                     *data.arg3()->GetDict().FindList("blockedTopics"));
}

TEST_F(PrivacySandboxHandlerTestMockService, TopicsToggleChanged) {
  std::vector<bool> states = {true, false};
  for (bool state : states) {
    testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
    EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsToggleChanged(state));

    base::Value::List args;
    args.Append(state);
    handler()->HandleTopicsToggleChanged(args);
  }
}

// A mock implementation of the PrivacySandboxCountries interface for testing.
class MockPrivacySandboxCountries : public PrivacySandboxCountries {
 public:
  MockPrivacySandboxCountries() = default;
  ~MockPrivacySandboxCountries() override = default;
  MOCK_METHOD(bool, IsConsentCountry, (), (override));
  MOCK_METHOD(bool, IsRestOfWorldCountry, (), (override));
};

// A fake implementation of PrivacySandboxHandler that uses a mock for testing
// country-related functionality.
class FakePrivacySandboxHandler : public PrivacySandboxHandler {
 public:
  FakePrivacySandboxHandler() {
    mock_privacy_sandbox_countries_ =
        std::make_unique<MockPrivacySandboxCountries>();
  }

  ~FakePrivacySandboxHandler() override {
    mock_privacy_sandbox_countries_.reset();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerPrivacyGuideAdTopicsTest,
                           AdTopicsCardShownForUserInConsentCountry);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerPrivacyGuideAdTopicsTest,
                           AdTopicsCardNotShownForUserNotInConsentCountry);

  MockPrivacySandboxCountries* GetPrivacySandboxCountries() override {
    return mock_privacy_sandbox_countries_.get();
  }

  std::unique_ptr<MockPrivacySandboxCountries> mock_privacy_sandbox_countries_;
};

class PrivacySandboxHandlerPrivacyGuideAdTopicsTest : public testing::Test {
 public:
  PrivacySandboxHandlerPrivacyGuideAdTopicsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxPrivacyGuideAdTopics);
    auto handler = std::make_unique<FakePrivacySandboxHandler>();
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    static_cast<content::WebUIMessageHandler*>(handler_)
        ->AllowJavascriptForTesting();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  FakePrivacySandboxHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList feature_list_;
  content::TestWebUI web_ui_;
  raw_ptr<FakePrivacySandboxHandler> handler_;
};

// Verifies that the WebUI correctly responds to show the Ad Topics card when
// the user is in a consent country.
TEST_F(PrivacySandboxHandlerPrivacyGuideAdTopicsTest,
       AdTopicsCardShownForUserInConsentCountry) {
  ON_CALL(*handler()->GetPrivacySandboxCountries(), IsConsentCountry())
      .WillByDefault(Return(true));

  base::Value::List args;
  args.Append(kCallbackId1);
  web_ui()->ProcessWebUIMessage(
      GURL(), "privacySandboxPrivacyGuideShouldShowAdTopicsCard",
      std::move(args));

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(data.arg1()->GetString(), kCallbackId1);
  EXPECT_EQ(data.function_name(), "cr.webUIResponse");
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_TRUE(data.arg3()->GetBool());
}

// Verifies that the WebUI correctly responds to NOT show the Ad Topics card
// when the user is NOT in a consent country.
TEST_F(PrivacySandboxHandlerPrivacyGuideAdTopicsTest,
       AdTopicsCardNotShownForUserNotInConsentCountry) {
  ON_CALL(*handler()->GetPrivacySandboxCountries(), IsConsentCountry())
      .WillByDefault(Return(false));

  base::Value::List args;
  args.Append(kCallbackId1);
  web_ui()->ProcessWebUIMessage(
      GURL(), "privacySandboxPrivacyGuideShouldShowAdTopicsCard",
      std::move(args));

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(data.arg1()->GetString(), kCallbackId1);
  EXPECT_EQ(data.function_name(), "cr.webUIResponse");
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_FALSE(data.arg3()->GetBool());
}

}  // namespace settings
