// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class Browser;
class GURL;
enum class WindowOpenDisposition;
struct NavigateParams;

namespace apps {
struct AppLaunchParams;
}

namespace content {
class WebContents;
}

namespace web_app {

class AppBrowserController;
class WithAppResources;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LaunchedAppType {
  kDiy = 0,
  kCrafted = 1,
  kMaxValue = kCrafted,
};

std::optional<webapps::AppId> GetWebAppForActiveTab(const Browser* browser);

// Clears navigation history prior to user entering app scope.
void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents);

// Invokes ReparentWebContentsIntoAppBrowser() for the active tab for the
// web app that has the tab's URL in its scope. Does nothing if there is no web
// app in scope.
Browser* ReparentWebAppForActiveTab(Browser* browser);

// Reparents |contents| into a standalone web app window for |app_id|.
// - If the web app has a launch_handler set to reuse existing windows and there
// are existing web app windows around this will launch the web app into the
// existing window and close |contents|.
// - If the web app is in experimental tabbed mode and has and existing web app
// window, |contents| will be reparented into the existing window.
// - Otherwise a new browser window is created for |contents| to be reparented
// into.
Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const webapps::AppId& app_id);

// Tags `contents` with the given app id and marks it as an app. This
// differentiates it from a `WebContents` which happens to be hosting a page
// that is part of an app.
void SetWebContentsActingAsApp(content::WebContents* contents,
                               const webapps::AppId& app_id);

// Marks the web contents as being the pinned home tab of a tabbed web app.
void SetWebContentsIsPinnedHomeTab(content::WebContents* contents);

// Set preferences that are unique to app windows.
void SetAppPrefsForWebContents(content::WebContents* web_contents);

// Clear preferences that are unique to app windows.
void ClearAppPrefsForWebContents(content::WebContents* web_contents);

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser);

void MaybeAddPinnedHomeTab(Browser* browser, const std::string& app_id);

// This creates appropriate CreateParams for creating a PWA window or PWA popup
// window.
Browser::CreateParams CreateParamsForApp(const webapps::AppId& app_id,
                                         bool is_popup,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture);

Browser* CreateWebAppWindowMaybeWithHomeTab(
    const webapps::AppId& app_id,
    const Browser::CreateParams& params);

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition);

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params);

// RecordLaunchMetrics methods report UMA metrics. It shouldn't have other
// side-effects (e.g. updating app launch time).
void RecordLaunchMetrics(const webapps::AppId& app_id,
                         apps::LaunchContainer container,
                         apps::LaunchSource launch_source,
                         const GURL& launch_url,
                         content::WebContents* web_contents);

// Updates statistics about web app launch. For example, app's last launch time
// (populates recently launched app list) and site engagement stats.
void UpdateLaunchStats(content::WebContents* web_contents,
                       const webapps::AppId& app_id,
                       const GURL& launch_url);

// Locks that lock apps all have the WithAppResources mixin, allowing any
// app-locking lock to call this method.
void LaunchWebApp(apps::AppLaunchParams params,
                  LaunchWebAppWindowSetting launch_setting,
                  Profile& profile,
                  WithAppResources& app_resources,
                  LaunchWebAppDebugValueCallback callback);

// Encapsulates web_app capturing and launching during navigation requests.
// Returns a valid Browser and tab index if web app handling is appropriate,
// otherwise nullopt. May create a browser, app window or tab as needed.
//
// A value of std::nullopt means that the web app system cannot handle the
// navigation, and as such, would allow the "normal" workflow to identify a
// browser to perform navigation in to proceed. See
// `GetBrowserAndTabForDisposition()` for more information.
//
// TODO(crbug.com/351775835): Integrate with web_applications system to
// determine which browser to complete navigation in based on launch handlers.
std::optional<std::pair<Browser*, int>> MaybeHandleAppNavigation(
    Profile* profile,
    const NavigateParams& navigate_params);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
