// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "services/device/geolocation/network_location_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace device {

using ::device::mojom::LocationProviderManagerMode::kCustomOnly;
using ::device::mojom::LocationProviderManagerMode::kHybridFallbackNetwork;
using ::device::mojom::LocationProviderManagerMode::kHybridPlatform;
using ::device::mojom::LocationProviderManagerMode::kNetworkOnly;
using ::device::mojom::LocationProviderManagerMode::kPlatformOnly;

LocationProviderManager::LocationProviderManager(
    CustomLocationProviderCallback custom_location_provider_getter,
    GeolocationSystemPermissionManager* geolocation_system_permission_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const std::string& api_key,
    std::unique_ptr<PositionCache> position_cache,
    base::RepeatingClosure internals_updated_closure,
    NetworkLocationProvider::NetworkRequestCallback network_request_callback,
    NetworkLocationProvider::NetworkResponseCallback network_response_callback)
    : custom_location_provider_getter_(
          std::move(custom_location_provider_getter)),
      geolocation_system_permission_manager_(
          geolocation_system_permission_manager),
      url_loader_factory_(url_loader_factory),
      api_key_(api_key),
      position_cache_(std::move(position_cache)),
      internals_updated_closure_(std::move(internals_updated_closure)),
      network_request_callback_(std::move(network_request_callback)),
      network_response_callback_(std::move(network_response_callback)) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, default to using the platform location provider.
  provider_manager_mode_ = kPlatformOnly;
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // On Ash / Lacros / Linux, default to using the network location provider.
  provider_manager_mode_ = kNetworkOnly;
#else
  // On macOS / Windows platforms, use the mode specified by the feature flag.
  provider_manager_mode_ = features::kLocationProviderManagerParam.Get();
#endif
}

LocationProviderManager::~LocationProviderManager() {
  // Release the global wifi polling policy state.
  WifiPollingPolicy::Shutdown();
}

bool LocationProviderManager::HasPermissionBeenGrantedForTest() const {
  return is_permission_granted_;
}

void LocationProviderManager::OnPermissionGranted() {
  is_permission_granted_ = true;

  if (network_location_provider_) {
    network_location_provider_->OnPermissionGranted();
  }

  if (platform_location_provider_) {
    platform_location_provider_->OnPermissionGranted();
  }

  if (custom_location_provider_) {
    custom_location_provider_->OnPermissionGranted();
  }
}

void LocationProviderManager::StartProvider(bool enable_high_accuracy) {
  // Ensure a provider is created successfully before starting.
  if (!InitializeProvider()) {
    // If no location providers are available, report a "Position Unavailable"
    // error.
    location_update_callback_.Run(
        this, mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
                  mojom::GeopositionErrorCode::kPositionUnavailable, "", "")));
  } else {
    enable_high_accuracy_ = enable_high_accuracy;
    is_running_ = true;
    switch (provider_manager_mode_) {
      case kCustomOnly:
        custom_location_provider_->StartProvider(enable_high_accuracy_);
        break;
      case kNetworkOnly:
      case kHybridFallbackNetwork:
        network_location_provider_->StartProvider(enable_high_accuracy_);
        break;
      case kPlatformOnly:
      case kHybridPlatform:
        platform_location_provider_->StartProvider(enable_high_accuracy_);
    }
  }
}

void LocationProviderManager::StopProvider() {
  // Reset the reference location state (provider+result)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  network_location_provider_result_.reset();
  platform_location_provider_result_.reset();
  custom_location_provider_result_.reset();
  network_location_provider_.reset();
  platform_location_provider_.reset();
  custom_location_provider_.reset();
  is_running_ = false;
}

void LocationProviderManager::RegisterProvider(LocationProvider& provider) {
  provider.SetUpdateCallback(base::BindRepeating(
      &LocationProviderManager::OnLocationUpdate, base::Unretained(this)));
  if (is_permission_granted_) {
    provider.OnPermissionGranted();
  }
}

bool LocationProviderManager::InitializeProvider() {
  // Provider is already initialized, return true here to skip rest of creation
  // process.
  if (network_location_provider_ || platform_location_provider_ ||
      custom_location_provider_) {
    return true;
  }

  // If a custom location provider is injected and available, override the mode
  // to use the custom provider exclusively.
  if (custom_location_provider_getter_) {
    custom_location_provider_ = custom_location_provider_getter_.Run();
    if (custom_location_provider_) {
      provider_manager_mode_ = kCustomOnly;
    }
  }

  switch (provider_manager_mode_) {
    case kCustomOnly:
      RegisterProvider(*custom_location_provider_.get());
      break;
    case kNetworkOnly:
    case kHybridFallbackNetwork:
      if (!url_loader_factory_) {
        return false;
      }
      network_location_provider_ =
          NewNetworkLocationProvider(url_loader_factory_, api_key_);
      RegisterProvider(*network_location_provider_.get());
      break;
    case kPlatformOnly:
    case kHybridPlatform:
      platform_location_provider_ = NewSystemLocationProvider();
      if (!platform_location_provider_) {
        return false;
      }
      RegisterProvider(*platform_location_provider_.get());
  }

  return true;
}

void LocationProviderManager::OnLocationUpdate(
    const LocationProvider* provider,
    mojom::GeopositionResultPtr new_result) {
  DCHECK(new_result);
  DCHECK(new_result->is_error() ||
         new_result->is_position() &&
             ValidateGeoposition(*new_result->get_position()));

  switch (provider_manager_mode_) {
    case kHybridPlatform:
    // TODO(crbug.com/346842084): kHybridPlatform mode currently behaves the
    // same as kPlatformOnly. fallback mechanism will not be added until
    // platform provider is fully evaluated.
    case kPlatformOnly:
      platform_location_provider_result_ = new_result.Clone();
      break;
    case kNetworkOnly:
    case kHybridFallbackNetwork:
      network_location_provider_result_ = new_result.Clone();
      break;
    case kCustomOnly:
      custom_location_provider_result_ = new_result.Clone();
      break;
  }

  location_update_callback_.Run(this, std::move(new_result));
}

const mojom::GeopositionResult* LocationProviderManager::GetPosition() {
  switch (provider_manager_mode_) {
    case kHybridPlatform:
    case kPlatformOnly:
      return platform_location_provider_result_.get();
    case kNetworkOnly:
    case kHybridFallbackNetwork:
      return network_location_provider_result_.get();
    case kCustomOnly:
      return custom_location_provider_result_.get();
  }
}

void LocationProviderManager::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_running_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
    return;
  }
  switch (provider_manager_mode_) {
    case kHybridPlatform:
    case kPlatformOnly:
      return platform_location_provider_->FillDiagnostics(diagnostics);
    case kNetworkOnly:
    case kHybridFallbackNetwork:
      return network_location_provider_->FillDiagnostics(diagnostics);
    case kCustomOnly:
      return custom_location_provider_->FillDiagnostics(diagnostics);
  }
  if (position_cache_) {
    diagnostics.position_cache_diagnostics =
        mojom::PositionCacheDiagnostics::New();
    position_cache_->FillDiagnostics(*diagnostics.position_cache_diagnostics);
  }
  if (WifiPollingPolicy::IsInitialized()) {
    diagnostics.wifi_polling_policy_diagnostics =
        mojom::WifiPollingPolicyDiagnostics::New();
    WifiPollingPolicy::Get()->FillDiagnostics(
        *diagnostics.wifi_polling_policy_diagnostics);
  }
}

void LocationProviderManager::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(!callback.is_null());
  location_update_callback_ = callback;
}

std::unique_ptr<LocationProvider>
LocationProviderManager::NewNetworkLocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key) {
  DCHECK(url_loader_factory);
#if BUILDFLAG(IS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return nullptr;
#else
  return std::make_unique<NetworkLocationProvider>(
      std::move(url_loader_factory), api_key, position_cache_.get(),
      internals_updated_closure_, network_request_callback_,
      network_response_callback_);
#endif
}

std::unique_ptr<LocationProvider>
LocationProviderManager::NewSystemLocationProvider() {
#if BUILDFLAG(IS_APPLE)
  CHECK(geolocation_system_permission_manager_);
  return device::NewSystemLocationProvider(
      geolocation_system_permission_manager_->GetSystemGeolocationSource());
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return device::NewSystemLocationProvider();
#else
  return nullptr;
#endif
}


}  // namespace device
