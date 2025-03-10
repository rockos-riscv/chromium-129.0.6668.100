// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <string>
#include <tuple>

#include "ash/constants/ash_switches.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace {

constexpr char kSerialNumberValue[] = "a_value";
constexpr char kBrandCodeValue[] = "brand_code";
constexpr char kActivateDateValue[] = "activated";
constexpr char kMalformedEmbargoDateValue[] = "adventure_time";

std::string ToUTCString(const base::Time& time) {
  return base::UnlocalizedTimeFormatWithPattern(time, "yyyy-MM-dd",
                                                icu::TimeZone::getGMT());
}

using USDStatus = policy::AutoEnrollmentTypeChecker::USDStatus;

}  // namespace

namespace policy {

class AutoEnrollmentTypeCheckerTest : public testing::Test {
 public:
  AutoEnrollmentTypeCheckerTest() = default;
  ~AutoEnrollmentTypeCheckerTest() override = default;

 protected:
  void SetUpNonchromeDevice() {
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kFirmwareTypeKey,
        ash::system::kFirmwareTypeValueNonchrome);
  }

  void SetUpFlexDeviceWithFREOnFlexEnabled() {
    enrollment_test_helper_.SetUpFlexDevice();
    enrollment_test_helper_.EnableFREOnFlex();
  }

  void SetupFREEnabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnterpriseEnrollmentInitialModulus);
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnterpriseEnrollmentModulusLimit);

    ASSERT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
  }

  void SetupFREDisabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);

    ASSERT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
  }

  void SetupFREEnabledButNotRequired() {
    SetupFREEnabled();

    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.ClearMachineStatistic(
        ash::system::kActivateDateKey);

    ASSERT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  void SetupFREEnabledAndRequired() {
    SetupFREEnabled();

    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                  kActivateDateValue);

    ASSERT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  void SetupInitialEnrollmentEnabled() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableInitialEnrollment,
        AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);

    ASSERT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
  }

  void SetupInitialEnrollmentEnabledButNotRequired() {
    SetupInitialEnrollmentEnabled();

    fake_statistics_provider_.ClearMachineStatistic(
        ash::system::kSerialNumberKey);
  }

  void SetupInitialEnrollmentEnabledAndRequired() {
    SetupInitialEnrollmentEnabled();

    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kSerialNumberValue);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                  kBrandCodeValue);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        kMalformedEmbargoDateValue);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr bool is_google_branded_ = true;
#else
  static constexpr bool is_google_branded_ = false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  test::EnrollmentTestHelper enrollment_test_helper_{
      &command_line_, &fake_statistics_provider_};
};

TEST_F(AutoEnrollmentTypeCheckerTest, FREEnabledWhenSwitchIsAlways) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest, FREEnabledWhenSwitchIsOfficialBuild) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentOfficialBuild);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsFREEnabled(), is_google_branded_);
}

TEST_F(AutoEnrollmentTypeCheckerTest, FREEnabledWhenSwitchIsEmpty) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnterpriseEnableForcedReEnrollment);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsFREEnabled(), is_google_branded_);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREDisabledWhenSwitchIsOfficialBuildOnNonChrome) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentOfficialBuild);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest, FREDisabledWhenSwitchIsNever) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentEnabledWhenSwitchIsAlways) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentEnabledWhenSwitchIsOfficialBuild) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentOfficialBuild);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentEnabledWhenSwitchIsEmpty) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNormal);
  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnterpriseEnableInitialEnrollment);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentDisabledWhenSwitchIsOfficialBuildButItsNot) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareTypeKey, ash::system::kFirmwareTypeValueNonchrome);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentOfficialBuild);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       InitialEnrollmentDisabledWhenSwitchIsNever) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyNotRequiredAccordingToVPDWhenFlagIsZero) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "0");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyNotRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyRequiredAccordingToVPDWhenFlagIsOne) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "1");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FREExplicitlyRequiredAccordingToVPDWhenFlagIsInvalid) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "woops");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRENotRequiredAccordingToVPDWhenVPDCanBeReadButDeviceIsNotOwned) {
  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kRoInvalid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kNotRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kInvalid);
    // Not setting |kActivateDateKey| statistic to indicate a lack of ownership.

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredAccordingToVPDWhenVPDIsBroken) {
  fake_statistics_provider_.SetVpdStatus(
      ash::system::StatisticsProvider::VpdStatus::kRwInvalid);

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredAccordingToVPDWhenDeviceIsOwned) {
  fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                kActivateDateValue);

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kRoInvalid);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }

  {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kInvalid);

    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &fake_statistics_provider_),
              AutoEnrollmentTypeChecker::FRERequirement::kRequired);
  }
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredOnFlexEnabledByCommandLineSwitch) {
  SetUpFlexDeviceWithFREOnFlexEnabled();

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());
  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kDisabled);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredOnFlexOverridenByFREEnabledCommandLineSwitchSetToNever) {
  SetUpFlexDeviceWithFREOnFlexEnabled();
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       FRERequiredOnFlexNotEnabledByCommandLineSwitch) {
  enrollment_test_helper_.SetUpFlexDevice();

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());
  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kDisabled);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       DetermineAutoEnrollmentCheckTypeOnFlexWhenTokenPresent) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
  AutoEnrollmentTypeChecker::CheckType check_type =
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          true, &fake_statistics_provider_, false);

  AutoEnrollmentTypeChecker::CheckType expected_check_type =
      is_google_branded_
          ? AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination
          : AutoEnrollmentTypeChecker::CheckType::kNone;
  EXPECT_EQ(check_type, expected_check_type);
}

// If there is an enrollment token present for whatever reason on a non-Flex
// device, auto_enrollment_type_checker should ignore it and continue initial
// state determination as normal (and the token won't be included in the state
// retrieval request).
TEST_F(AutoEnrollmentTypeCheckerTest,
       DetermineAutoEnrollmentCheckTypeNotOnFlexWhenTokenPresent) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
  AutoEnrollmentTypeChecker::CheckType check_type =
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          true, &fake_statistics_provider_, false);

  AutoEnrollmentTypeChecker::CheckType expected_check_type =
      is_google_branded_
          ? AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination
          : AutoEnrollmentTypeChecker::CheckType::kNone;

  EXPECT_EQ(check_type, expected_check_type);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       DetermineAutoEnrollmentCheckTypeOnFlexWithoutTokenPresent) {
  enrollment_test_helper_.SetUpFlexDevice();
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
  AutoEnrollmentTypeChecker::CheckType check_type =
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          true, &fake_statistics_provider_, false);

  EXPECT_EQ(check_type, AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_F(AutoEnrollmentTypeCheckerTest,
       DetermineAutoEnrollmentCheckTypeOnFlexWithEmptyToken) {
  // TODO(b/331285209): Change the JSON key to "enrollmentToken" along with the
  // key definition in configuration_keys.h.
  constexpr char kEmptyEnrollmentTokenOobeConfig[] = R"({
    "enrollmentToken": ""
  })";
  enrollment_test_helper_.SetUpEnrollmentTokenConfig(
      kEmptyEnrollmentTokenOobeConfig);
  enrollment_test_helper_.SetUpFlexDevice();
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled(),
            is_google_branded_);
  AutoEnrollmentTypeChecker::CheckType check_type =
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          true, &fake_statistics_provider_, false);

  EXPECT_EQ(check_type, AutoEnrollmentTypeChecker::CheckType::kNone);
}

class AutoEnrollmentTypeCheckerInitializationTest
    : public AutoEnrollmentTypeCheckerTest {
 public:
  void SetUp() override {
    AutoEnrollmentTypeCheckerTest::SetUp();
    AutoEnrollmentTypeChecker::
        ClearUnifiedStateDeterminationKillSwitchForTesting();
    test_shared_loader_factory_ = test_url_loader_factory_.GetSafeWeakWrapper();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, Request) {
  base::test::TestFuture<void> future;

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto request = test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(request.url,
            "https://www.gstatic.com/chromeos-usd-experiment/v1.json");
  EXPECT_EQ(request.method, "GET");
  EXPECT_TRUE(request.load_flags & net::LOAD_DISABLE_CACHE);
  EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, RetriesAllErrors) {
  base::test::TestFuture<void> future;
  network::URLLoaderCompletionStatus status;

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  // Network changed.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  status.error_code = net::ERR_NETWORK_CHANGED;
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL("https://www.gstatic.com/chromeos-usd-experiment/v1.json"), status,
      network::mojom::URLResponseHead::New(), "");

  // DNS failed.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  status.error_code = net::ERR_TIMED_OUT;
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL("https://www.gstatic.com/chromeos-usd-experiment/v1.json"), status,
      network::mojom::URLResponseHead::New(), "");

  // HTTP 5xx error.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json", "",
      net::HTTP_SERVICE_UNAVAILABLE);

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(AutoEnrollmentTypeChecker::Initialized());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, UmaHistograms) {
  base::HistogramTester histograms;
  base::test::TestFuture<void> future;
  network::URLLoaderCompletionStatus status;

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  status.error_code = net::ERR_NETWORK_CHANGED;
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL("https://www.gstatic.com/chromeos-usd-experiment/v1.json"), status,
      network::mojom::URLResponseHead::New(), "");
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json",
      R"({"disable_up_to_version": 1})", net::HTTP_OK);

  ASSERT_TRUE(future.Wait());
  histograms.ExpectTotalCount(
      kUMAStateDeterminationKillSwitchFetchNetworkErrorCode, 2);
  histograms.ExpectBucketCount(
      kUMAStateDeterminationKillSwitchFetchNetworkErrorCode,
      -net::ERR_NETWORK_CHANGED, 1);
  histograms.ExpectBucketCount(
      kUMAStateDeterminationKillSwitchFetchNetworkErrorCode, -net::OK, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationKillSwitchFetchNumTries,
                                2, 1);
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledBeforeInitStarted) {
  EXPECT_FALSE(AutoEnrollmentTypeChecker::Initialized());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledBeforeInitCompleted) {
  base::test::TestFuture<void> future;

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(AutoEnrollmentTypeChecker::Initialized());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledOnTimeout) {
  base::test::TestFuture<void> future;

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());
  network::SimpleURLLoader::SetTimeoutTickClockForTest(nullptr);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::Initialized());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledOnInvalidJSON) {
  base::test::TestFuture<void> future;
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json", "!!!",
      net::HTTP_OK);

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledOnNonDict) {
  base::test::TestFuture<void> future;
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json", "42",
      net::HTTP_OK);

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledOnMissingKey) {
  base::test::TestFuture<void> future;
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json", "{}",
      net::HTTP_OK);

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, KilledVersion) {
  base::test::TestFuture<void> future;
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json",
      R"({"disable_up_to_version": 1})", net::HTTP_OK);

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(AutoEnrollmentTypeChecker::
                  IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

TEST_F(AutoEnrollmentTypeCheckerInitializationTest, ActiveVersion) {
  base::test::TestFuture<void> future;
  test_url_loader_factory_.AddResponse(
      "https://www.gstatic.com/chromeos-usd-experiment/v1.json",
      R"({"disable_up_to_version": 0})", net::HTTP_OK);

  AutoEnrollmentTypeChecker::Initialize(test_shared_loader_factory_,
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(AutoEnrollmentTypeChecker::
                   IsUnifiedStateDeterminationDisabledByKillSwitchForTesting());
}

class AutoEnrollmentTypeCheckerUSDStatusTest
    : public AutoEnrollmentTypeCheckerTest {
 public:
  void SetUp() override {
    AutoEnrollmentTypeCheckerTest::SetUp();
    AutoEnrollmentTypeChecker::SetUnifiedStateDeterminationKillSwitchForTesting(
        false);
  }

 protected:
  base::HistogramTester histograms_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, Default) {
  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledOnOfficialGoogleChrome, 1);
}

TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, FlexDevice) {
  enrollment_test_helper_.SetUpFlexDevice();

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledOnOfficialGoogleFlex, 1);
}

TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, AlwaysSwitch) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::AutoEnrollmentTypeChecker::
          kUnifiedStateDeterminationAlways);

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kEnabledViaAlwaysSwitch, 1);
}

TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, NeverSwitch) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::AutoEnrollmentTypeChecker::
          kUnifiedStateDeterminationNever);

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledViaNeverSwitch, 1);
}

TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, KillSwitch) {
  AutoEnrollmentTypeChecker::SetUnifiedStateDeterminationKillSwitchForTesting(
      true);

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledViaKillSwitch, 1);
}

TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, NonChrome) {
  SetUpNonchromeDevice();

  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledOnNonChromeDevice, 1);
}
#else
TEST_F(AutoEnrollmentTypeCheckerUSDStatusTest, UnbrandedBuild) {
  AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled();

  histograms_.ExpectUniqueSample(kUMAStateDeterminationStatus,
                                 USDStatus::kDisabledOnUnbrandedBuild, 1);
}
#endif

// An enum for the kind of Chromium OS running on the device.
enum class DeviceOs {
  Chrome = 0,
  Nonchrome = 1,
  // TODO(b/331677599): Delete FlexWithoutFRE, and make FlexWithFRE just Flex.
  FlexWithoutFRE = 2,
  FlexWithFRE = 3,
};

// This is parameterized by device OS and USD kill switch enablement.
class AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP
    : public AutoEnrollmentTypeCheckerTest,
      public testing::WithParamInterface<std::tuple<DeviceOs, bool>> {
 protected:
  void SetUp() override {
    AutoEnrollmentTypeCheckerTest::SetUp();
    if (device_os_ == DeviceOs::Nonchrome) {
      SetUpNonchromeDevice();
    } else if (device_os_ == DeviceOs::FlexWithoutFRE) {
      enrollment_test_helper_.SetUpFlexDevice();
    } else if (device_os_ == DeviceOs::FlexWithFRE) {
      SetUpFlexDeviceWithFREOnFlexEnabled();
    }
    AutoEnrollmentTypeChecker::SetUnifiedStateDeterminationKillSwitchForTesting(
        kill_switch_enabled_);
  }

  bool IsFRESupportedByDevice() {
    return (google_branded_ && device_os_ == DeviceOs::Chrome) ||
           device_os_ == DeviceOs::FlexWithFRE;
  }

  bool IsOfficialGoogleOS() {
    return google_branded_ && (device_os_ == DeviceOs::Chrome ||
                               device_os_ == DeviceOs::FlexWithoutFRE ||
                               device_os_ == DeviceOs::FlexWithFRE);
  }

  const DeviceOs device_os_ = std::get<0>(GetParam());
  const bool kill_switch_enabled_ = std::get<1>(GetParam());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool google_branded_ = true;
#else
  const bool google_branded_ = false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Default) {
  EXPECT_EQ(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled(),
            !kill_switch_enabled_ && IsOfficialGoogleOS());
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, OfficialBuild) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationOfficialBuild);

  EXPECT_EQ(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled(),
            !kill_switch_enabled_ && IsOfficialGoogleOS());
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Never) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled());

  // Check that FRE switches are respected.
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);
  EXPECT_FALSE(AutoEnrollmentTypeChecker::IsFREEnabled());

  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsFREEnabled());

  ash::system::FakeStatisticsProvider statistics_provider;
  if (device_os_ == DeviceOs::Chrome) {
    // Check that the FRE requirement is read from VPD on Chrome.
    statistics_provider.SetMachineStatistic(ash::system::kCheckEnrollmentKey,
                                            "0");
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
            &statistics_provider),
        AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyNotRequired);
    statistics_provider.SetMachineStatistic(ash::system::kCheckEnrollmentKey,
                                            "1");
    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &statistics_provider),
              AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired);
  } else if (device_os_ == DeviceOs::FlexWithoutFRE ||
             device_os_ == DeviceOs::FlexWithFRE) {
    // Check that the FRE requirement is as expected on Flex, where we don't
    // support legacy FRE.
    EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                  &statistics_provider),
              AutoEnrollmentTypeChecker::FRERequirement::kDisabled);
  }
}

TEST_P(AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP, Always) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);

  EXPECT_TRUE(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled());

  // FRE is independent of USD.
  EXPECT_EQ(IsFRESupportedByDevice(),
            AutoEnrollmentTypeChecker::IsFREEnabled());
  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                /*statistics_provider=*/nullptr),
            IsFRESupportedByDevice()
                ? AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyRequired
                : AutoEnrollmentTypeChecker::FRERequirement::kDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestSuite,
    AutoEnrollmentTypeCheckerUnifiedStateDeterminationTestP,
    testing::Combine(testing::Values(DeviceOs::Chrome,
                                     DeviceOs::Nonchrome,
                                     DeviceOs::FlexWithoutFRE,
                                     DeviceOs::FlexWithFRE),
                     testing::Bool()));

// This is parametrized with dev_disable_boot.
class AutoEnrollmentTypeCheckerTestP
    : public AutoEnrollmentTypeCheckerTest,
      public testing::WithParamInterface<bool> {
 public:
  // Helper function for all situations in which `dev_disable_boot == true` will
  // be interpreted as `kForcedReEnrollmentExplicitlyRequired`.
  AutoEnrollmentTypeChecker::CheckType fre_or(
      AutoEnrollmentTypeChecker::CheckType other) {
    return dev_disable_boot_ ? AutoEnrollmentTypeChecker::CheckType::
                                   kForcedReEnrollmentExplicitlyRequired
                             : other;
  }
  const bool dev_disable_boot_ = GetParam();
};

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenDisabled) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenGaiaServicesDisabled) {
  SetupFREEnabledAndRequired();
  SetupInitialEnrollmentEnabledAndRequired();

  command_line_.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableGaiaServices);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenFREExplicitlyNotRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "0");

  EXPECT_EQ(AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
                &fake_statistics_provider_),
            AutoEnrollmentTypeChecker::FRERequirement::kExplicitlyNotRequired);
  EXPECT_EQ(
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
          dev_disable_boot_),
      fre_or(AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  EXPECT_EQ(
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
          dev_disable_boot_),
      fre_or(AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenNoEnrollmentModulusSwitchPresent) {
  SetupFREEnabled();
  SetupInitialEnrollmentEnabledButNotRequired();
  fake_statistics_provider_.SetVpdStatus(
      ash::system::StatisticsProvider::VpdStatus::kValid);

  command_line_.GetProcessCommandLine()->RemoveSwitch(
      ash::switches::kEnterpriseEnrollmentInitialModulus);
  command_line_.GetProcessCommandLine()->RemoveSwitch(
      ash::switches::kEnterpriseEnrollmentModulusLimit);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckRequiredWhenFREExplicitlyRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kCheckEnrollmentKey, "1");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckRequiredWhenFREImplicitlyRequired) {
  SetupFREEnabled();
  // Set initial enrollment required. It checks that FRE has priority over
  // initial enrollment.
  SetupInitialEnrollmentEnabledAndRequired();

  fake_statistics_provider_.SetVpdStatus(
      ash::system::StatisticsProvider::VpdStatus::kValid);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                kActivateDateValue);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::
                       kForcedReEnrollmentImplicitlyRequired));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::
                       kForcedReEnrollmentImplicitlyRequired));
}

TEST_P(AutoEnrollmentTypeCheckerTestP,
       AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentDisabled) {
  SetupFREEnabledButNotRequired();

  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment,
      AutoEnrollmentTypeChecker::kInitialEnrollmentNever);

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentNotRequiredWhenVPDIsBroken) {
  // FRE turns required when it does not find serial number. Disable it
  // altogether.
  SetupFREDisabled();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                "");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            AutoEnrollmentTypeChecker::CheckType::kNone);
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckNotRequiredWhenInitialEnrollmentNotRequiredWhenBrandCodeIsMissing) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                "");

  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/false,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                /*is_system_clock_synchronized=*/true,
                &fake_statistics_provider_, dev_disable_boot_),
            fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckUnknownWhenSystemClockNotSynchedAndNotRequiredWhenSynched) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  {
    // Put embargo embargo day way in the future.
    const auto past_embargo_threshold =
        ToUTCString(base::Time::Now() +
                    2 * ash::system::kEmbargoEndDateGarbageDateThreshold);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        past_embargo_threshold);

    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/false,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::
                         kUnknownDueToMissingSystemClockSync));
    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/true,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  }

  {
    // Put embargo embargo day a little bit in the future.
    const auto before_embargo_threshold =
        ToUTCString(base::Time::Now() +
                    ash::system::kEmbargoEndDateGarbageDateThreshold / 2);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        before_embargo_threshold);

    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/false,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::
                         kUnknownDueToMissingSystemClockSync));
    EXPECT_EQ(AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
                  /*is_system_clock_synchronized=*/true,
                  &fake_statistics_provider_, dev_disable_boot_),
              fre_or(AutoEnrollmentTypeChecker::CheckType::kNone));
  }
}

TEST_P(
    AutoEnrollmentTypeCheckerTestP,
    AutoEnrollmentCheckRequiredWhenInitialEnrollmentRequiredWhenEmbargoDateMissingOrPassed) {
  SetupFREEnabledButNotRequired();
  SetupInitialEnrollmentEnabled();

  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);

  {
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        kMalformedEmbargoDateValue);
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  }

  {
    const auto yeasterday_embargo =
        ToUTCString(base::Time::Now() - base::Days(1));
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kEnterpriseManagementEmbargoEndDateKey,
        yeasterday_embargo);
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/false, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
    EXPECT_EQ(
        AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
            /*is_system_clock_synchronized=*/true, &fake_statistics_provider_,
            dev_disable_boot_),
        fre_or(
            AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination));
  }
}

INSTANTIATE_TEST_SUITE_P(AutoEnrollmentTypeCheckerTestSuite,
                         AutoEnrollmentTypeCheckerTestP,
                         testing::Bool());

}  // namespace policy
