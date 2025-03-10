// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_
#define CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "build/branding_buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
// Trigger identifiers currently used; duplicates not allowed.
extern const char kHatsSurveyTriggerAutofillAddress[];
extern const char kHatsSurveyTriggerAutofillAddressUserPerception[];
extern const char kHatsSurveyTriggerAutofillCreditCardUserPerception[];
extern const char kHatsSurveyTriggerAutofillPasswordUserPerception[];
extern const char kHatsSurveyTriggerAutofillCard[];
extern const char kHatsSurveyTriggerAutofillPassword[];
extern const char kHatsSurveyTriggerDevToolsIssuesCOEP[];
extern const char kHatsSurveyTriggerDevToolsIssuesMixedContent[];
extern const char kHatsSurveyTriggerDevToolsIssuesCookiesSameSite[];
extern const char kHatsSurveyTriggerDevToolsIssuesHeavyAd[];
extern const char kHatsSurveyTriggerDevToolsIssuesCSP[];
extern const char kHatsSurveyTriggerDownloadWarningBubbleBypass[];
extern const char kHatsSurveyTriggerDownloadWarningBubbleHeed[];
extern const char kHatsSurveyTriggerDownloadWarningBubbleIgnore[];
extern const char kHatsSurveyTriggerDownloadWarningPageBypass[];
extern const char kHatsSurveyTriggerDownloadWarningPageHeed[];
extern const char kHatsSurveyTriggerDownloadWarningPageIgnore[];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kHatsSurveyTriggerGetMostChrome[];
#endif
extern const char kHatsSurveyTriggerM1AdPrivacyPage[];
extern const char kHatsSurveyTriggerM1TopicsSubpage[];
extern const char kHatsSurveyTriggerM1FledgeSubpage[];
extern const char kHatsSurveyTriggerM1AdMeasurementSubpage[];
extern const char kHatsSurveyTriggerNtpModules[];
extern const char kHatsSurveyTriggerNtpPhotosModuleOptOut[];
extern const char kHatsSurveyTriggerPerformanceControlsPerformance[];
extern const char kHatsSurveyTriggerPerformanceControlsBatteryPerformance[];
extern const char kHatsSurveyTriggerPerformanceControlsMemorySaverOptOut[];
extern const char kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut[];
extern const char kHatsSurveyTriggerPrivacyGuide[];
extern const char kHatsSurveyTriggerRedWarning[];
extern const char kHatsSurveyTriggerSettings[];
extern const char kHatsSurveyTriggerSettingsPrivacy[];
extern const char kHatsSurveyTriggerSuggestedPasswordsExperiment[];
extern const char kHatsSurveyTriggerSettingsSecurity[];
extern const char kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept[];
extern const char kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline[];
extern const char kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk[];
extern const char kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings[];
extern const char kHatsSurveyTriggerTrustSafetyPrivacySettings[];
extern const char kHatsSurveyTriggerTrustSafetyTrustedSurface[];
extern const char kHatsSurveyTriggerTrustSafetyTransactions[];
extern const char kHatsSurveyTriggerTrustSafetyV2BrowsingData[];
extern const char kHatsSurveyTriggerTrustSafetyV2ControlGroup[];
extern const char kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI[];
extern const char kHatsSurveyTriggerTrustSafetyV2PasswordCheck[];
extern const char kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI[];
extern const char kHatsSurveyTriggerTrustSafetyV2SafetyCheck[];
extern const char kHatsSurveyTriggerTrustSafetyV2SafetyHubNotification[];
extern const char kHatsSurveyTriggerTrustSafetyV2SafetyHubInteraction[];
extern const char kHatsSurveyTriggerTrustSafetyV2TrustedSurface[];
extern const char kHatsSurveyTriggerTrustSafetyV2PrivacyGuide[];
extern const char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept[];
extern const char
    kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline[];
extern const char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk[];
extern const char
    kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings[];
extern const char kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial[];
extern const char kHatsSurveyTriggerWallpaperSearch[];
extern const char kHatsSurveyTriggerWhatsNew[];
extern const char kHatsSurveyTriggerWhatsNewAlternate[];
#else
extern const char kHatsSurveyTriggerAndroidStartupSurvey[];
extern const char kHatsSurveyTriggerQuickDelete[];
extern const char kHatsSurveyTriggerSafetyHubAndroid[];
#endif  // #if !BUILDFLAG(IS_ANDROID)

extern const char kHatsSurveyTriggerPermissionsPrompt[];

extern const char kHatsSurveyTriggerTesting[];
// The Trigger ID for a test HaTS Next survey which is available for testing
// and demo purposes when the migration feature flag is enabled.
extern const char kHatsNextSurveyTriggerIDTesting[];

namespace hats {
struct SurveyConfig {
  // Constructs a SurveyConfig by inspecting |feature|. This includes checking
  // if the feature is enabled, as well as inspecting the feature parameters
  // for the survey probability, and if |presupplied_trigger_id| is not
  // provided, the trigger ID. To pass any product specific data for the
  // survey, configure fields here, matches are CHECK enforced.
  // SurveyConfig that enable |log_responses_to_uma| will need to have surveys
  // reviewed by privacy to ensure they are appropriate to log to UMA.
  // This is enforced through the OWNERS mechanism.
  SurveyConfig(
      const base::Feature* feature,
      const std::string& trigger,
      const std::optional<std::string>& presupplied_trigger_id = std::nullopt,
      const std::vector<std::string>& product_specific_bits_data_fields = {},
      const std::vector<std::string>& product_specific_string_data_fields = {},
      bool log_responses_to_uma = false);

  SurveyConfig();
  SurveyConfig(const SurveyConfig&);
  ~SurveyConfig();

  // Whether the survey is currently enabled and can be shown.
  bool enabled = false;

  // Probability [0,1] of how likely a chosen user will see the survey.
  double probability = 0.0f;

  // The trigger for this survey within the browser.
  std::string trigger;

  // Trigger ID for the survey.
  std::string trigger_id;

  // Histogram name for the survey.
  std::optional<std::string> histogram_name;

  // The survey will prompt every time because the user has explicitly decided
  // to take the survey e.g. clicking a link.
  bool user_prompted = false;

  // Product Specific Bit Data fields which are sent with the survey
  // response.
  std::vector<std::string> product_specific_bits_data_fields;

  // Product Specific String Data fields which are sent with the survey
  // response.
  std::vector<std::string> product_specific_string_data_fields;
};

using SurveyConfigs = base::flat_map<std::string, SurveyConfig>;
// Get the list of active ongoing surveys and store into
// |survey_configs_by_triggers_|.
void GetActiveSurveyConfigs(SurveyConfigs& survey_configs_by_triggers_);

}  // namespace hats

#endif  // CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_
