// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/child_iterator_base.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using password_manager::features::PasswordGenerationVariation;

const ui::AXPlatformNodeDelegate* FindNode(
    const ui::AXPlatformNodeDelegate* root,
    const std::string& class_name) {
  if (!root) {
    return nullptr;
  }

  if (root->GetStringAttribute(ax::mojom::StringAttribute::kClassName) ==
      class_name) {
    return root;
  }

  for (auto it = root->ChildrenBegin(); *it != *root->ChildrenEnd(); ++(*it)) {
    const ui::AXPlatformNodeDelegate* child_found =
        FindNode(it->get(), class_name);
    if (child_found) {
      return child_found;
    }
  }

  return nullptr;
}

}  // namespace

class PasswordGenerationPopupViewTest : public PasswordManagerBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    NavigateToFile("/password/signup_form_new_password.html");
  }

  const views::ViewAccessibility& GetPasswordViewAccessibility(
      PasswordGenerationPopupViewViews* popup_view) {
    return popup_view->GetPasswordViewViewAccessibilityForTest();
  }

  void SetPasswordSelected(
      base::WeakPtr<PasswordGenerationPopupControllerImpl> controller) {
    controller->Show(PasswordGenerationPopupController::kOfferGeneration);
    static_cast<PasswordGenerationPopupController*>(controller.get())
        ->SetSelected();
  }

  void ClearSelection(
      base::WeakPtr<PasswordGenerationPopupControllerImpl> controller) {
    controller->Show(PasswordGenerationPopupController::kOfferGeneration);
    static_cast<PasswordGenerationPopupController*>(controller.get())
        ->SelectionCleared();
  }
};

// Regression test for crbug.com/400543. Verifying that moving the mouse in the
// editing dialog doesn't crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       MouseMovementInEditingPopup) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();
  ASSERT_TRUE(controller);
  ASSERT_TRUE(controller->IsVisible());

  PasswordGenerationPopupViewTester::For(controller->view())
      ->SimulateMouseMovementAt(gfx::Point(1, 1));
}

// Verify that destroying web contents with visible popup does not crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       CloseWebContentsWithVisiblePopup) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();
  ASSERT_TRUE(controller);
  ASSERT_TRUE(controller->IsVisible());

  content::WebContents* web_contents = WebContents();
  ClearWebContentsPtr();
  web_contents->Close();
}

// Verify that controller is not crashed in case of insufficient vertical space
// for showing popup.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       DoNotCrashInCaseOfInsuffucientVerticalSpace) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, -20, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  EXPECT_FALSE(client->generation_popup_controller());
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       GeneratedPasswordBoxAccessibleProperties) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();
  PasswordGenerationPopupViewViews* popup_view =
      static_cast<PasswordGenerationPopupViewViews*>(controller->view());
  ui::AXNodeData node_data;

  GetPasswordViewAccessibility(popup_view).GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       ExpandedCollapsedAccessibleState) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();

  PasswordGenerationPopupViewViews* popup_view =
      static_cast<PasswordGenerationPopupViewViews*>(controller->view());
  ui::AXNodeData node_data;
  popup_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       PasswordViewSelectionAccessibleState) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();
  PasswordGenerationPopupViewViews* popup_view =
      static_cast<PasswordGenerationPopupViewViews*>(controller->view());

  SetPasswordSelected(controller);
  ui::AXNodeData node_data;
  GetPasswordViewAccessibility(popup_view).GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  ClearSelection(controller);
  node_data = ui::AXNodeData();
  GetPasswordViewAccessibility(popup_view).GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       HeaderAccessibilityProperties) {
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->SetCurrentTargetFrameForTesting(WebContents()->GetPrimaryMainFrame());
  client->ShowPasswordEditingPopup(gfx::RectF(0, 0, 10, 10), FormData(),
                                   FieldRendererId(100), u"password123");
  // Avoid dangling pointers on shutdown.
  client->SetCurrentTargetFrameForTesting(nullptr);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      client->generation_popup_controller();

  PasswordGenerationPopupViewViews* popup_view =
      static_cast<PasswordGenerationPopupViewViews*>(controller->view());

  const std::u16string expected_cached_description = l10n_util::GetStringFUTF16(
      popup_view->GetHelpTextMessageIdForTesting(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT),
      u"");

  if (password_manager::features::kPasswordGenerationExperimentVariationParam
          .Get() == PasswordGenerationVariation::kCrossDevice) {
    const std::u16string description =
        PasswordGenerationPopupViewViews::JoinMultiplePasswordGenerationStrings(
            expected_cached_description);

    EXPECT_EQ(GetPasswordViewAccessibility(popup_view).GetCachedDescription(),
              description);
  } else {
    EXPECT_EQ(GetPasswordViewAccessibility(popup_view).GetCachedDescription(),
              expected_cached_description);
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest, PopupInAxTree) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "PasswordGenerationPopupViewViews");
  // TODO: handle return value.
  std::ignore = content::EvalJs(
      WebContents(), "document.getElementById('password_field').focus()");
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->GeneratePassword(
      autofill::password_generation::PasswordGenerationType::kManual);
  gfx::NativeWindow window;
#if BUILDFLAG(IS_MAC)
  // On Mac the whole ax tree grows from the main root windows and the popup
  // node can be found there. This gives more confidence that it is in
  // the right place than on Linux or Windows (see below) where the popup
  // subtree lives separately.
  waiter.WaitIfNeededAndGet();
  window = chrome::FindLastActive()->window()->GetNativeWindow();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  views::Widget* dialog_widget = waiter.WaitIfNeededAndGet();
  window = dialog_widget->GetNativeWindow();
#endif
  ASSERT_TRUE(client->generation_popup_controller());

  ui::AXPlatformNode* root_node = ui::AXPlatformNode::FromNativeWindow(window);
  ui::AXPlatformNodeDelegate* root_node_delegate = root_node->GetDelegate();
  const ui::AXPlatformNodeDelegate* node_delegate =
      FindNode(root_node_delegate,
               "PasswordGenerationPopupViewViews::GeneratedPasswordBox");

  ASSERT_THAT(node_delegate, ::testing::NotNull());
  EXPECT_FALSE(
      node_delegate->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Set the screen reader focus by calling a method on the controller directly,
  // it normally is triggered by UI events when the screen reader is on,
  // screen reader presence is hard/expensive to emulate.
  static_cast<PasswordGenerationPopupController*>(
      client->generation_popup_controller().get())
      ->SetSelected();

  EXPECT_TRUE(
      node_delegate->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

// The test parameter controls the value of kPasswordGenerationExperiment
// feature param.
class PasswordGenerationPopupViewWithContentExperimentTest
    : public PasswordGenerationPopupViewTest,
      public testing::WithParamInterface<std::string> {
 public:
  PasswordGenerationPopupViewWithContentExperimentTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{password_manager::features::
                                   kPasswordGenerationExperiment,
                               {{"password_generation_variation",
                                 GetParam()}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewWithContentExperimentTest,
                       DoesNotCrashShowingGenerationOfferWithModifiedContent) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "PasswordGenerationPopupViewViews");
  // TODO: handle return value.
  std::ignore = content::EvalJs(
      WebContents(), "document.getElementById('password_field').focus()");
  auto* client = ChromePasswordManagerClient::FromWebContents(WebContents());
  client->GeneratePassword(
      autofill::password_generation::PasswordGenerationType::kManual);

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(client->generation_popup_controller());
  EXPECT_TRUE(client->generation_popup_controller()->IsVisible());
}

INSTANTIATE_TEST_SUITE_P(ContentExperiment,
                         PasswordGenerationPopupViewWithContentExperimentTest,
                         testing::Values("trusted_advice",
                                         "safety_first",
                                         "try_something_new",
                                         "convenience",
                                         "cross_device"));

}  // namespace autofill
