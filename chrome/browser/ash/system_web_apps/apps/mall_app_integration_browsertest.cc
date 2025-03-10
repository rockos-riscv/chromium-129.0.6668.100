// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/mall/url_constants.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MallAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  MallAppIntegrationTest() {
    features_.InitWithFeatures(
        {chromeos::features::kCrosMall, chromeos::features::kCrosMallSwa},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that the Mall app installs and launches correctly.
IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, MallApp) {
  const GURL url{ash::kChromeUIMallUrl};
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::MALL, url,
                              /*title=*/"Get Apps and Games"));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, EmbedMallWithContext) {
  WaitForTestSystemAppInstall();

  content::WebContents* contents = LaunchApp(ash::SystemWebAppType::MALL);

  // Poll to wait for an iframe to be embedded on the page, and resolve with the
  // 'src' attribute.
  constexpr char kScript[] = R"js(
    (async () => {
      await new Promise((resolve, reject) => {
        let intervalId = setInterval(() => {
          if (document.querySelector("iframe")) {
            clearInterval(intervalId);
            resolve();
          }
        }, 50);
      });

      return document.querySelector("iframe").src;
    })();
  )js";

  EXPECT_THAT(content::EvalJs(contents, kScript).ExtractString(),
              testing::StartsWith("https://discover.apps.chrome/?context="));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MallAppIntegrationTest);

}  // namespace
