// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace promos_utils {

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
class IOSPromoOnDesktopTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Register the prefs when not on a branded build (they're automatically
    // registered when on a branded build).
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    RegisterProfilePrefs(prefs()->registry());
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  }

  // Getter for the test syncable prefs service.
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  // Getter for the browser task environment.
  content::BrowserTaskEnvironment* environment() { return &task_environment_; }

  // Getter for the histograms tester.
  base::HistogramTester* histograms() { return &histogram_; }

  // Getter for the features list.
  base::test::ScopedFeatureList* features() { return &scoped_feature_list_; }

  // Getter for the testing profile.
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::HistogramTester histogram_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
};

// Tests RecordIOSDesktopPromoUserInteractionHistogram for all promo types.
// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 1, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the password
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 2, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the password
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and action dismissed for the password promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionDismissedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 3, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and no thanks clicked action for the
// password promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionNoThanksClickedForPasswordPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kPassword, 3, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.PasswordPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 1, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for first impression and no thanks clicked action for the address
// promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestFirstImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 1, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.FirstImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 2, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for second impression and no thanks clicked action for the
// address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestSecondImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 2, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.SecondImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and action dismissed for the address promo
// type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionDismissedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 3, DesktopIOSPromoAction::kDismissed);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kDismissed, 1);
}

// Tests that RecordIOSDesktopPromoUserInteractionHistogram records the proper
// histogram for third impression and no thanks clicked action for the
// address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSDesktopPromoUserInteractionHistogramTestThirdImpressionNoThanksClickedForAddressPromo) {
  RecordIOSDesktopPromoUserInteractionHistogram(
      IOSPromoType::kAddress, 3, DesktopIOSPromoAction::kNoThanksClicked);

  histograms()->ExpectUniqueSample(
      "IOS.Desktop.AddressPromo.ThirdImpression.Action",
      DesktopIOSPromoAction::kNoThanksClicked, 1);
}

// Tests ShouldShowIOSDesktopPromo with all promo types.
// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestTrueForPasswordPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressionsForPasswordPromo) {
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 3);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given password promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecentForPasswordPromo) {
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given password promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseUserOptedOutForPasswordPromo) {
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kPassword));
}

// Tests that ShouldShowIOSDesktopPromo returns true when no promo has yet been
// shown for the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestTrueForAddressPromo) {
  EXPECT_TRUE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has already
// seen 3 promos for the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseTooManyImpressionsForAddressPromo) {
  prefs()->SetInteger(promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter,
                      3);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the last seen
// impression is too recent for the given address promo type.
TEST_F(
    IOSPromoOnDesktopTest,
    ShouldShowIOSDesktopPromoTestFalseLastImpressionTooRecentForAddressPromo) {
  prefs()->SetTime(
      promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp,
      base::Time::Now());
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kAddress));
}

// Tests that ShouldShowIOSDesktopPromo returns false when the user has
// opted-out from the given address promo type.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSDesktopPromoTestFalseUserOptedOutForAddressPromo) {
  prefs()->SetBoolean(promos_prefs::kDesktopToiOSAddressPromoOptOut, true);
  EXPECT_FALSE(ShouldShowIOSDesktopPromo(profile(), IOSPromoType::kAddress));
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestFirstImpressionForPasswordPromo) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      1);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectUniqueSample("IOS.Desktop.PasswordPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestSecondImpressionForPasswordPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      2);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the third impression for the given password promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestThirdImpressionForPasswordPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Second impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);

  // Third impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kPassword);
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      3);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount("IOS.Desktop.PasswordPromo.Shown",
                                  DesktopIOSPromoImpression::kThirdImpression,
                                  1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the first impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestFirstImpressionForAddressPromo) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            1);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectUniqueSample("IOS.Desktop.AddressPromo.Shown",
                                   DesktopIOSPromoImpression::kFirstImpression,
                                   1);
}

// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the second impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestSecondImpressionForAddressPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Second impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            2);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.AddressPromo.Shown",
                                  DesktopIOSPromoImpression::kSecondImpression,
                                  1);
}
// Tests that IOSDesktopPromoShown sets the correct prefs and records the
// correct histogram for the third impression for the given address promo
// type.
TEST_F(IOSPromoOnDesktopTest,
       IOSDesktopPromoShownTestThirdImpressionForAddressPromo) {
  // First impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Second impression
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);

  // Third impression
  base::Time before = base::Time::Now();
  IOSDesktopPromoShown(profile(), IOSPromoType::kAddress);
  base::Time after = base::Time::Now();

  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            3);
  ASSERT_GE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            before);
  ASSERT_LE(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            after);

  histograms()->ExpectBucketCount("IOS.Desktop.AddressPromo.Shown",
                                  DesktopIOSPromoImpression::kThirdImpression,
                                  1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for first impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestFirstImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      1, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.FirstImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action dismissed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionDismissed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kDismissed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kDismissed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that RecordIOSPasswordPromoUserInteractionHistogram records the proper
// histogram for second impression and action explicitly closed.
TEST_F(
    IOSPromoOnDesktopTest,
    RecordIOSPasswordPromoUserInteractionHistogramTestSecondImpressionClosed) {
  promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
      2, DesktopIOSPasswordPromoAction::kExplicitlyClosed);
  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.SecondImpression.Action",
      DesktopIOSPasswordPromoAction::kExplicitlyClosed, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns true when no promo has yet been
// shown.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSPasswordPromoTestTrue) {
  EXPECT_TRUE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the user has already
// seen 2 promos.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseTooManyImpressions) {
  prefs()->SetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter, 2);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the last seen
// impression is too recent.
TEST_F(IOSPromoOnDesktopTest,
       ShouldShowIOSPasswordPromoTestFalseLastImpressionTooRecent) {
  prefs()->SetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
                   base::Time::Now());
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that ShouldShowIOSPasswordPromo returns false when the user has
// opted-out from the promo.
TEST_F(IOSPromoOnDesktopTest, ShouldShowIOSPasswordPromoTestFalseUserOptedOut) {
  prefs()->SetBoolean(promos_prefs::kiOSPasswordPromoOptOut, true);
  EXPECT_FALSE(promos_utils::ShouldShowIOSPasswordPromo(profile()));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns true when the
// result is successful and the mobile labels are not present in the
// classification labels.
TEST_F(IOSPromoOnDesktopTest, UserNotClassifiedAsMobileDeviceSwitcherTestTrue) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back("test_label");
  EXPECT_TRUE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when the
// result has an error.
TEST_F(IOSPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseError) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kFailed);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that UserNotClassifiedAsMobileDeviceSwitcher returns false when a
// mobile label is present in the classification results.
TEST_F(IOSPromoOnDesktopTest,
       UserNotClassifiedAsMobileDeviceSwitcherTestFalseMobileLabelPresent) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
  EXPECT_FALSE(promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result));
}

// Tests that RegisterProfilePrefs registers the prefs to their default values
// correctly.
TEST_F(IOSPromoOnDesktopTest, RegisterProfilePrefsTest) {
  ASSERT_FALSE(prefs()->GetBoolean(promos_prefs::kiOSPasswordPromoOptOut));
  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      0);
  ASSERT_EQ(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      base::Time());
  ASSERT_FALSE(
      prefs()->GetBoolean(promos_prefs::kDesktopToiOSAddressPromoOptOut));
  ASSERT_EQ(prefs()->GetInteger(
                promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter),
            0);
  ASSERT_EQ(prefs()->GetTime(
                promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
            base::Time());
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the first impression.
TEST_F(IOSPromoOnDesktopTest, iOSPasswordPromoShownTestFirstImpression) {
  // Record before and after times to ensure the timestamp is within that range.
  base::Time before = base::Time::Now();
  promos_utils::iOSPasswordPromoShown(profile());
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      1);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectUniqueSample(
      "IOS.DesktopPasswordPromo.Shown",
      promos_utils::DesktopIOSPasswordPromoImpression::kFirstImpression, 1);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Tests that iOSPasswordPromoShown sets the correct prefs and records the
// correct histogram for the second impression.
TEST_F(IOSPromoOnDesktopTest, iOSPasswordPromoShownTestSecondImpression) {
  // First impression
  promos_utils::iOSPasswordPromoShown(profile());

  // Second impression
  base::Time before = base::Time::Now();
  promos_utils::iOSPasswordPromoShown(profile());
  base::Time after = base::Time::Now();

  ASSERT_EQ(
      prefs()->GetInteger(promos_prefs::kiOSPasswordPromoImpressionsCounter),
      2);
  ASSERT_GE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      before);
  ASSERT_LE(
      prefs()->GetTime(promos_prefs::kiOSPasswordPromoLastImpressionTimestamp),
      after);

  histograms()->ExpectBucketCount(
      "IOS.DesktopPasswordPromo.Shown",
      promos_utils::DesktopIOSPasswordPromoImpression::kSecondImpression, 1);
}
}  // namespace promos_utils
