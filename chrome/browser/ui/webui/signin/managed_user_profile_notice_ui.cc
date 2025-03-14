// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/ui_strings.h"

ManagedUserProfileNoticeUI::ManagedUserProfileNoticeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIManagedUserProfileNoticeHost);

  static constexpr webui::ResourcePath kResources[] = {
      {"icons.html.js", IDR_SIGNIN_ICONS_HTML_JS},
      {"legacy_managed_user_profile_notice_app.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_LEGACY_MANAGED_USER_PROFILE_NOTICE_APP_JS},
      {"legacy_managed_user_profile_notice_app.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_LEGACY_MANAGED_USER_PROFILE_NOTICE_APP_HTML_JS},
      {"managed_user_profile_notice_app.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_APP_JS},
      {"managed_user_profile_notice_app.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_APP_HTML_JS},
      {"managed_user_profile_notice_disclosure.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DISCLOSURE_HTML_JS},
      {"managed_user_profile_notice_disclosure.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DISCLOSURE_JS},
      {"managed_user_profile_notice_state.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_STATE_HTML_JS},
      {"managed_user_profile_notice_state.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_STATE_JS},
      {"managed_user_profile_notice_browser_proxy.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_BROWSER_PROXY_JS},
      {"images/enrollment_success.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_SUCCESS_SVG},
      {"images/enrollment_failure.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_FAILURE_SVG},
      {"images/enrollment_timeout.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_TIMEOUT_SVG},
      {"images/enrollment_success_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_SUCCESS_DARK_SVG},
      {"images/enrollment_failure_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_FAILURE_DARK_SVG},
      {"images/enrollment_timeout_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_TIMEOUT_DARK_SVG},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
      {"tangible_sync_style_shared.css.js",
       IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS},
  };

  webui::SetupWebUIDataSource(
      source, base::make_span(kResources),
      IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_HTML);

  source->AddResourcePath("images/left-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("images/dialog_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/dialog_illustration_dark.svg",
      IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_DARK_SVG);
  source->AddLocalizedString("enterpriseProfileWelcomeTitle",
                             IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  source->AddLocalizedString("cancelLabel", IDS_CANCEL);
  source->AddLocalizedString("continueLabel", IDS_APP_CONTINUE);
  source->AddLocalizedString("confirmLabel", IDS_CONFIRM);
  source->AddLocalizedString("linkDataText",
                             IDS_ENTERPRISE_PROFILE_WELCOME_LINK_DATA_CHECKBOX);

  source->AddLocalizedString("profileInformationTitle",
                             IDS_ENTERPRISE_WELCOME_PROFILE_INFORMATION_TITLE);
  source->AddLocalizedString(
      "profileInformationDetails",
      IDS_ENTERPRISE_WELCOME_PROFILE_INFORMATION_DETAILS);
  source->AddLocalizedString("deviceInformationTitle",
                             IDS_ENTERPRISE_WELCOME_DEVICE_INFORMATION_TITLE);
  source->AddLocalizedString("deviceInformationDetails",
                             IDS_ENTERPRISE_WELCOME_DEVICE_INFORMATION_DETAILS);

  source->AddLocalizedString("processingSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_PROCESSING_SUBTITLE);
  source->AddLocalizedString("successTitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_SUCCESS_TITLE);
  source->AddLocalizedString("successSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_SUCCESS_SUBTITLE);
  source->AddLocalizedString("timeoutTitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_TIMEOUT_TITLE);
  source->AddLocalizedString("timeoutSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_TIMEOUT_SUBTITLE);
  source->AddLocalizedString("errorTitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_ERROR_TITLE);
  source->AddLocalizedString("errorSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_ERROR_SUBTITLE);

  source->AddBoolean("useUpdatedUi",
                     base::FeatureList::IsEnabled(
                         features::kEnterpriseUpdatedProfileCreationScreen));
  source->AddBoolean("showLinkDataCheckbox", false);
  source->AddBoolean("isModalDialog", false);
}

ManagedUserProfileNoticeUI::~ManagedUserProfileNoticeUI() = default;

void ManagedUserProfileNoticeUI::Initialize(
    Browser* browser,
    ManagedUserProfileNoticeUI::ScreenType type,
    const AccountInfo& account_info,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    signin::SigninChoiceCallbackVariant process_user_choice_callback,
    base::OnceClosure done_callback) {
  auto handler = std::make_unique<ManagedUserProfileNoticeHandler>(
      browser, type, profile_creation_required_by_policy, show_link_data_option,
      account_info, std::move(process_user_choice_callback),
      std::move(done_callback));
  handler_ = handler.get();

  base::Value::Dict update_data;
  if (type ==
      ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation) {
    update_data.Set("isModalDialog", true);

    int title_id = profile_creation_required_by_policy
                       ? IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE
                       : IDS_ENTERPRISE_WELCOME_PROFILE_WILL_BE_MANAGED_TITLE;
    update_data.Set("enterpriseProfileWelcomeTitle",
                    l10n_util::GetStringUTF16(title_id));

    update_data.Set("showLinkDataCheckbox", show_link_data_option);
  } else if (type == ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC) {
    update_data.Set("isModalDialog", true);
    update_data.Set(
        "enterpriseProfileWelcomeTitle",
        l10n_util::GetStringUTF16(IDS_ENTERPRISE_WELCOME_PROFILE_SETUP_TITLE));

    update_data.Set("showLinkDataCheckbox", false);
#if !BUILDFLAG(IS_CHROMEOS)
    update_data.Set(
        "useUpdatedUi",
        base::FeatureList::IsEnabled(
            features::kEnterpriseUpdatedProfileCreationScreen) ||
            base::FeatureList::IsEnabled(
                profile_management::features::kOidcAuthProfileManagement));
#endif
  }
  content::WebUIDataSource::Update(
      Profile::FromWebUI(web_ui()),
      chrome::kChromeUIManagedUserProfileNoticeHost, std::move(update_data));

  web_ui()->AddMessageHandler(std::move(handler));
}

ManagedUserProfileNoticeHandler*
ManagedUserProfileNoticeUI::GetHandlerForTesting() {
  return handler_;
}

WEB_UI_CONTROLLER_TYPE_IMPL(ManagedUserProfileNoticeUI)
