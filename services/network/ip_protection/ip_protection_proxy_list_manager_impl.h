// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/proxy_chain.h"
#include "services/network/ip_protection/ip_protection_config_cache.h"
#include "services/network/ip_protection/ip_protection_config_getter.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/ip_protection/ip_protection_geo_utils.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"

namespace network {

// An implementation of IpProtectionProxyListManager that populates itself using
// a passed in IpProtectionConfigGetter pointer from the cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyListManagerImpl
    : public IpProtectionProxyListManager {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ProxyListResult {
    kFailed = 0,
    kEmptyList = 1,
    kPopulatedList = 2,
    kMaxValue = kPopulatedList,
  };

  explicit IpProtectionProxyListManagerImpl(
      IpProtectionConfigCache* config_cache,
      IpProtectionConfigGetter& config_getter,
      bool disable_proxy_refreshing_for_testing = false);
  ~IpProtectionProxyListManagerImpl() override;

  // IpProtectionProxyListManager implementation.
  bool IsProxyListAvailable() override;
  const std::vector<net::ProxyChain>& ProxyList() override;
  const std::string& CurrentGeo() override;
  void RefreshProxyListForGeoChange() override;
  void RequestRefreshProxyList() override;

  // Set a callback to occur when the proxy list has been refreshed.
  void SetOnProxyListRefreshedForTesting(
      base::OnceClosure on_proxy_list_refreshed) {
    on_proxy_list_refreshed_for_testing_ = std::move(on_proxy_list_refreshed);
  }

  // Trigger a proxy list refresh.
  void EnableAndTriggerProxyListRefreshingForTesting() {
    EnableProxyListRefreshingForTesting();
    RefreshProxyList();
  }

  // Enable proxy refreshing.
  // This does not trigger an immediate proxy list refresh.
  void EnableProxyListRefreshingForTesting() {
    disable_proxy_refreshing_for_testing_ = false;
  }

  void SetProxyListForTesting(
      std::optional<std::vector<net::ProxyChain>> proxy_list,
      std::optional<GeoHint> geo_hint) {
    current_geo_id_ = network::GetGeoIdFromGeoHint(geo_hint);
    proxy_list_ = *proxy_list;
    have_fetched_proxy_list_ = true;
  }

 private:
  void RefreshProxyList();
  void ScheduleRefreshProxyList(base::TimeDelta delay);
  void OnGotProxyList(base::TimeTicks refresh_start_time_for_metrics,
                      std::optional<std::vector<net::ProxyChain>> proxy_list,
                      std::optional<GeoHint> geo_hint);
  bool IsProxyListOlderThanMinAge() const;

  // Latest fetched proxy list.
  std::vector<net::ProxyChain> proxy_list_;

  // Current geo of the proxy list.
  std::string current_geo_id_ = "";

  // True if an invocation of `config_getter_.GetProxyList()` is
  // outstanding.
  bool fetching_proxy_list_ = false;

  // True if the proxy list has been fetched at least once.
  bool have_fetched_proxy_list_ = false;

  // Pointer to the `IpProtectionConfigCache` that holds the proxy list and
  // tokens. Required to observe geo changes from refreshed proxy lists.
  // The lifetime of the `IpProtectionConfigCache` object WILL ALWAYS outlive
  // this class b/c `ip_protection_config_cache_` owns this (at least outside of
  // testing).
  const raw_ptr<IpProtectionConfigCache> ip_protection_config_cache_;

  // Source of proxy list, when needed.
  raw_ref<IpProtectionConfigGetter> config_getter_;

  // The last time this instance began refreshing the proxy list.
  base::Time last_proxy_list_refresh_;

  // The min age of the proxy list before an additional refresh is allowed.
  const base::TimeDelta proxy_list_min_age_;

  // The regular time interval where the proxy list is refreshed.
  const base::TimeDelta proxy_list_refresh_interval_;

  // Feature flag to safely introduce token caching by geo.
  const bool enable_token_caching_by_geo_;

  // A timer to run `RefreshProxyList()` when necessary.
  base::OneShotTimer next_refresh_proxy_list_;

  // A callback triggered when an asynchronous proxy-list refresh is complete,
  // for use in testing.
  base::OnceClosure on_proxy_list_refreshed_for_testing_;

  // If true, do not try to automatically refresh the proxy list.
  bool disable_proxy_refreshing_for_testing_ = false;

  base::WeakPtrFactory<IpProtectionProxyListManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_
