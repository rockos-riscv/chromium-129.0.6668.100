// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_

#include <optional>

#include "ash/public/cpp/peripherals_app_delegate.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/peripherals/proto/peripherals.pb.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace apps {
class AlmanacAppIconLoader;
struct QueryError;
}  // namespace apps

class Profile;

namespace ash {

class PeripheralsAppDelegateImpl : public PeripheralsAppDelegate {
 public:
  using GetCompanionAppInfoCallback =
      base::OnceCallback<void(const std::optional<mojom::CompanionAppInfo>&)>;
  PeripheralsAppDelegateImpl();
  PeripheralsAppDelegateImpl(const PeripheralsAppDelegateImpl&) = delete;
  PeripheralsAppDelegateImpl& operator=(const PeripheralsAppDelegateImpl&) =
      delete;
  ~PeripheralsAppDelegateImpl() override;

  // PeripheralsAppDelegate:
  void GetCompanionAppInfo(const std::string& device_key,
                           GetCompanionAppInfoCallback callback) override;

 private:
  void ConvertPeripheralsResponseProto(
      base::WeakPtr<Profile> active_user_profile_weak_ptr,
      GetCompanionAppInfoCallback callback,
      base::expected<apps::proto::PeripheralsGetResponse, apps::QueryError>
          query_response);

  void OnAppIconLoaded(GetCompanionAppInfoCallback callback,
                       mojom::CompanionAppInfo info,
                       apps::IconValuePtr icon_value);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<apps::AlmanacAppIconLoader> icon_loader_;
  base::WeakPtrFactory<PeripheralsAppDelegateImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_
