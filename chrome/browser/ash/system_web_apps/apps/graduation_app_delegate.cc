// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/graduation_app_delegate.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/graduation/url_constants.h"
#include "ash/webui/grit/ash_graduation_resources.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash::graduation {

GraduationAppDelegate::GraduationAppDelegate(Profile* profile)
    : SystemWebAppDelegate(SystemWebAppType::GRADUATION,
                           "Graduation",
                           GURL(kChromeUIGraduationAppURL),
                           profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
GraduationAppDelegate::GetWebAppInfo() const {
  GURL start_url = GURL(kChromeUIGraduationAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(kChromeUIGraduationAppURL);
  info->title = l10n_util::GetStringUTF16(IDS_GRADUATION_APP_TITLE);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {
          {
              .icon_name = "graduation_app_icon_128.png",
              .size = 128,
              .resource_id = IDR_ASH_GRADUATION_GRADUATION_APP_ICON_128_PNG,
          },
          {
              .icon_name = "graduation_app_icon_256.png",
              .size = 256,
              .resource_id = IDR_ASH_GRADUATION_GRADUATION_APP_ICON_256_PNG,
          },
      },
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

bool GraduationAppDelegate::ShouldShowInSearchAndShelf() const {
  return true;
}

bool GraduationAppDelegate::IsAppEnabled() const {
  return features::IsGraduationEnabled();
}

}  // namespace ash::graduation
