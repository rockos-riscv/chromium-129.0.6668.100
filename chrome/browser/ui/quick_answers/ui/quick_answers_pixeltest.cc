// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

constexpr char kScreenshotPrefix[] = "quick_answers";
constexpr char kTestTitle[] = "TestTitle. A selected text.";
constexpr char kTestQuery[] = "TestQuery";
constexpr char kTestPhoneticsUrl[] = "https://example.com/";
constexpr char kTestDefinition[] =
    "TestDefinition. A test definition for TestTitle.";
constexpr char kTextToTranslateLong[] =
    "Text to translate. This text is long enough as this can be elided.";
constexpr char kSourceLocaleJaJp[] = "ja-JP";
constexpr char kTranslatedText[] = "Translated text";
constexpr gfx::Rect kContextMenuRectNarrow = {100, 100, 100, 200};
constexpr gfx::Rect kContextMenuRectWide = {100, 100, 300, 200};

using PixelTestParam = std::tuple<bool, bool, bool, Design, bool>;

bool IsDarkMode(const PixelTestParam& pixel_test_param) {
  return std::get<0>(pixel_test_param);
}

bool IsRtl(const PixelTestParam& pixel_test_param) {
  return std::get<1>(pixel_test_param);
}

bool IsNarrowLayout(const PixelTestParam& pixel_test_param) {
  return std::get<2>(pixel_test_param);
}

Design GetDesign(const PixelTestParam& pixel_test_param) {
  return std::get<3>(pixel_test_param);
}

bool IsInternal(const PixelTestParam& pixel_test_param) {
  return std::get<4>(pixel_test_param);
}

std::string GetDarkModeParamValue(const PixelTestParam& pixel_test_param) {
  return IsDarkMode(pixel_test_param) ? "Dark" : "Light";
}

std::string GetRtlParamValue(const PixelTestParam& pixel_test_param) {
  return IsRtl(pixel_test_param) ? "Rtl" : "Ltr";
}

std::string GetNarrowLayoutParamValue(const PixelTestParam& pixel_test_param) {
  return IsNarrowLayout(pixel_test_param) ? "Narrow" : "Wide";
}

std::optional<std::string> MaybeGetDesignParamValue(
    const PixelTestParam& pixel_test_param) {
  switch (GetDesign(pixel_test_param)) {
    case Design::kCurrent:
      return std::nullopt;
    case Design::kRefresh:
      return "Refresh";
    case Design::kMagicBoost:
      return "MagicBoost";
  }

  CHECK(false) << "Invalid design enum class value specified";
}

std::optional<std::string> MaybeInternalParamValue(
    const PixelTestParam& pixel_test_param) {
  if (IsInternal(pixel_test_param)) {
    return "Internal";
  }

  return std::nullopt;
}

std::string GetParamName(const PixelTestParam& param,
                         std::string_view separator) {
  std::vector<std::string> param_names;
  param_names.push_back(GetDarkModeParamValue(param));
  param_names.push_back(GetRtlParamValue(param));
  param_names.push_back(GetNarrowLayoutParamValue(param));
  std::optional<std::string> design_param_value =
      MaybeGetDesignParamValue(param);
  if (design_param_value) {
    param_names.push_back(*design_param_value);
  }
  std::optional<std::string> internal_param_value =
      MaybeInternalParamValue(param);
  if (internal_param_value) {
    param_names.push_back(*internal_param_value);
  }
  return base::JoinString(param_names, separator);
}

std::string GenerateParamName(
    const testing::TestParamInfo<PixelTestParam>& test_param_info) {
  return GetParamName(test_param_info.param, /*separator=*/"");
}

std::string GetScreenshotName(const std::string& test_name,
                              const PixelTestParam& param) {
  return test_name + "." + GetParamName(param, /*separator=*/".");
}

// To run a pixel test locally:
//
// e.g., for only running light mode variants of Loading case:
// browser_tests --gtest_filter=*QuickAnswersPixelTest.Loading/Light*
//   --enable-pixel-output-in-tests
//   --browser-ui-tests-verify-pixels
//   --skia-gold-local-png-write-directory=/tmp/qa_pixel_test
class QuickAnswersPixelTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  void SetUp() override {
    // Make sure kQuickAnswersRichCard is disabled. It might be enabled via
    // fieldtrial_testing_config.
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kQuickAnswersRichCard);

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsRtl(GetParam())) {
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
    }

    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (!command_line->HasSwitch(switches::kVerifyPixels)) {
      GTEST_SKIP() << "A pixel test requires kVerifyPixels flag.";
    }

    pixel_diff_.emplace(kScreenshotPrefix);
  }

  void SetUpOnMainThread() override {
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  void CreateAndShowQuickAnswersViewForLoading(Intent intent) {
    QuickAnswersUiController* quick_answers_ui_controller =
        GetQuickAnswersUiController();
    ASSERT_TRUE(quick_answers_ui_controller);
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
        quick_answers_ui_controller->GetReadWriteCardsUiController();

    quick_answers_ui_controller->CreateQuickAnswersViewForPixelTest(
        browser()->profile(), kTestQuery,
        {
            .title = kTestTitle,
            .design = GetDesign(GetParam()),
            .intent = intent,
            .is_internal = IsInternal(GetParam()),
        });
    read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
    ASSERT_TRUE(read_write_cards_ui_controller.widget_for_test())
        << "A widget must be created to show a UI.";

    QuickAnswersController* quick_answers_controller =
        QuickAnswersController::Get();
    ASSERT_TRUE(quick_answers_controller);
    quick_answers_controller->SetVisibility(
        QuickAnswersVisibility::kQuickAnswersVisible);
  }

  void CreateAndShowUserConsentView(IntentType intent_type,
                                    bool use_refreshed_design) {
    QuickAnswersUiController* quick_answers_ui_controller =
        GetQuickAnswersUiController();
    ASSERT_TRUE(quick_answers_ui_controller);
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
        quick_answers_ui_controller->GetReadWriteCardsUiController();

    QuickAnswersController* quick_answers_controller =
        QuickAnswersController::Get();
    ASSERT_TRUE(quick_answers_controller);
    quick_answers_controller->SetVisibility(QuickAnswersVisibility::kPending);
    quick_answers_ui_controller->CreateUserConsentViewForPixelTest(
        GetContextMenuRect(), intent_type, u"Test", use_refreshed_design);
    read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
    ASSERT_TRUE(read_write_cards_ui_controller.widget_for_test())
        << "A widget must be created to show a UI.";
  }

  gfx::Rect GetContextMenuRect() {
    return IsNarrowLayout(GetParam()) ? kContextMenuRectNarrow
                                      : kContextMenuRectWide;
  }

  QuickAnswersUiController* GetQuickAnswersUiController() {
    QuickAnswersController* controller = QuickAnswersController::Get();
    if (!controller) {
      return nullptr;
    }

    return static_cast<QuickAnswersControllerImpl*>(controller)
        ->quick_answers_ui_controller();
  }

  views::Widget* GetWidget() {
    return GetQuickAnswersUiController()
        ->GetReadWriteCardsUiController()
        .widget_for_test();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<views::ViewSkiaGoldPixelDiff> pixel_diff_;
};

using QuickAnswersPixelTest = QuickAnswersPixelTestBase;
using QuickAnswersPixelTestInternal = QuickAnswersPixelTestBase;
using QuickAnswersPixelTestResultView = QuickAnswersPixelTestBase;
using QuickAnswersPixelTestUserConsentView = QuickAnswersPixelTestBase;

INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(Design::kCurrent,
                                     Design::kRefresh,
                                     Design::kMagicBoost),
                     /*is_internal=*/testing::Values(false)),
    &GenerateParamName);

// Separate parameterized test suite for an internal UI to avoid having large
// number of screenshots.
INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTestInternal,
    testing::Combine(/*is_dark_mode=*/testing::Values(false),
                     /*is_rtl=*/testing::Values(false),
                     /*is_narrow=*/testing::Values(false),
                     testing::Values(Design::kCurrent,
                                     Design::kRefresh,
                                     Design::kMagicBoost),
                     /*is_internal=*/testing::Values(true)),
    &GenerateParamName);

// `QuickAnswersPixelTestResultView` is for testing sub text in the result view.
// Use `Design::kRefresh` as a sub text is not used in `Design::kCurrent`.
INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTestResultView,
    testing::Combine(/*is_dark_mode=*/testing::Values(false),
                     /*is_rtl=*/testing::Values(false),
                     /*is_narrow=*/testing::Bool(),
                     testing::Values(Design::kRefresh),
                     /*is_internal=*/testing::Values(false)),
    &GenerateParamName);

// `QuickAnswersPixelTestUserConsentView` is for testing a ui text variant of
// `UserConsentView`. More specifically, this is for testing
// `IntentType::kUnknown`, which is used for Linux-ChromeOS.
INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTestUserConsentView,
    testing::Combine(/*is_dark_mode=*/testing::Values(false),
                     /*is_rtl=*/testing::Values(false),
                     /*is_narrow=*/testing::Values(false),
                     testing::Values(Design::kCurrent, Design::kRefresh),
                     /*is_internal=*/testing::Values(false)),
    &GenerateParamName);

}  // namespace

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Loading) {
  // Spread intent types between tests to have better UI test coverage.
  CreateAndShowQuickAnswersViewForLoading(Intent::kTranslation);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Loading", GetParam()),
      GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Result) {
  // Spread intent types between tests to have better UI test coverage.
  CreateAndShowQuickAnswersViewForLoading(Intent::kDefinition);

  StructuredResult structured_result;
  structured_result.definition_result = std::make_unique<DefinitionResult>();
  structured_result.definition_result->word = kTestTitle;
  structured_result.definition_result->sense.definition = kTestDefinition;
  structured_result.definition_result->phonetics_info.query_text = kTestQuery;
  structured_result.definition_result->phonetics_info.phonetics_audio =
      GURL(kTestPhoneticsUrl);
  structured_result.definition_result->phonetics_info.tts_audio_enabled = true;
  GetQuickAnswersUiController()->RenderQuickAnswersViewWithResult(
      structured_result);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Result", GetParam()), GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Retry) {
  // Spread intent types between tests to have better UI test coverage.
  CreateAndShowQuickAnswersViewForLoading(Intent::kUnitConversion);

  GetQuickAnswersUiController()->ShowRetry();

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Retry", GetParam()), GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, UserConsent) {
  Design design = GetDesign(GetParam());
  if (design == Design::kMagicBoost) {
    GTEST_SKIP()
        << "User consent is handled by MagicBoost UI if MagicBoost is on";
  }

  CreateAndShowUserConsentView(
      IntentType::kDictionary,
      /*use_refreshed_design=*/design == Design::kRefresh);

  // For Narrow layout, we intentionally let it overflow in x-axis. See comments
  // in user_consent_view.cc.
  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("UserConsent", GetParam()),
      GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTestUserConsentView, Unknown) {
  Design design = GetDesign(GetParam());
  CreateAndShowUserConsentView(
      IntentType::kUnknown,
      /*use_refreshed_design=*/design == Design::kRefresh);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("UserConsentIntentUnknown", GetParam()),
      GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTestInternal, InternalUi) {
  CreateAndShowQuickAnswersViewForLoading(Intent::kDefinition);

  StructuredResult structured_result;
  structured_result.definition_result = std::make_unique<DefinitionResult>();
  structured_result.definition_result->word = kTestTitle;
  structured_result.definition_result->sense.definition = kTestDefinition;
  structured_result.definition_result->phonetics_info.query_text = kTestQuery;
  structured_result.definition_result->phonetics_info.phonetics_audio =
      GURL(kTestPhoneticsUrl);
  structured_result.definition_result->phonetics_info.tts_audio_enabled = true;
  GetQuickAnswersUiController()->RenderQuickAnswersViewWithResult(
      structured_result);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("InternalUi", GetParam()),
      GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTestResultView, ElidePrimaryText) {
  CreateAndShowQuickAnswersViewForLoading(Intent::kTranslation);

  // Translation result uses sub text.
  StructuredResult structured_result;
  structured_result.translation_result = std::make_unique<TranslationResult>();
  structured_result.translation_result->text_to_translate =
      kTextToTranslateLong;
  structured_result.translation_result->source_locale = kSourceLocaleJaJp;
  structured_result.translation_result->translated_text = kTranslatedText;
  GetQuickAnswersUiController()->RenderQuickAnswersViewWithResult(
      structured_result);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("ElidePrimaryText", GetParam()),
      GetWidget()->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTestResultView, NoSubText) {
  CreateAndShowQuickAnswersViewForLoading(Intent::kDefinition);

  // No-result case has no sub text.
  StructuredResult structured_result;
  GetQuickAnswersUiController()->RenderQuickAnswersViewWithResult(
      structured_result);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("NoSubText", GetParam()),
      GetWidget()->GetContentsView()));
}

}  // namespace quick_answers
