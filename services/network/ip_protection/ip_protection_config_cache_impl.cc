// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_config_cache_impl.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager_impl.h"
#include "services/network/ip_protection/ip_protection_token_cache_manager.h"
#include "services/network/ip_protection/ip_protection_token_cache_manager_impl.h"

namespace network {

namespace {
// Rewrite the proxy list to use SCHEME_QUIC. In order to fall back to HTTPS
// quickly if QUIC is broken, the first chain is included once with
// SCHEME_QUIC and once with SCHEME_HTTPS. The remaining chains are included
// only with SCHEME_QUIC.
std::vector<net::ProxyChain> MakeQuicProxyList(
    const std::vector<net::ProxyChain>& proxy_list,
    bool include_https_fallback = true) {
  if (proxy_list.empty()) {
    return proxy_list;
  }
  auto to_quic = [](const net::ProxyChain& proxy_chain) {
    std::vector<net::ProxyServer> quic_servers;
    quic_servers.reserve(proxy_chain.length());
    for (auto& proxy_server : proxy_chain.proxy_servers()) {
      CHECK(proxy_server.is_https());
      quic_servers.emplace_back(net::ProxyServer::Scheme::SCHEME_QUIC,
                                proxy_server.host_port_pair());
    }
    return net::ProxyChain::ForIpProtection(
        std::move(quic_servers), proxy_chain.ip_protection_chain_id());
  };

  std::vector<net::ProxyChain> quic_proxy_list;
  quic_proxy_list.reserve(proxy_list.size() + (include_https_fallback ? 1 : 0));
  quic_proxy_list.push_back(to_quic(proxy_list[0]));
  if (include_https_fallback) {
    quic_proxy_list.push_back(proxy_list[0]);
  }

  for (size_t i = 1; i < proxy_list.size(); i++) {
    quic_proxy_list.push_back(to_quic(proxy_list[i]));
  }

  return quic_proxy_list;
}

}  // namespace

IpProtectionConfigCacheImpl::IpProtectionConfigCacheImpl(
    std::unique_ptr<IpProtectionConfigGetter> config_getter)
    : ipp_over_quic_(net::features::kIpPrivacyUseQuicProxies.Get()),
      enable_token_caching_by_geo_(
          net::features::kIpPrivacyCacheTokensByGeo.Get()) {
  // Proxy list is null upon cache creation.
  ipp_proxy_list_manager_ = nullptr;

  // This type may be constructed with the `config_getter` being a `nullptr`,
  // for testing/experimental purposes. In that case, the list manager and cache
  // managers should not be created.
  if (config_getter.get() != nullptr && config_getter->IsAvailable()) {
    config_getter_ = std::move(config_getter);

    ipp_proxy_list_manager_ =
        std::make_unique<IpProtectionProxyListManagerImpl>(this,
                                                           *config_getter_);

    ipp_token_cache_managers_[IpProtectionProxyLayer::kProxyA] =
        std::make_unique<IpProtectionTokenCacheManagerImpl>(
            this, config_getter_.get(), IpProtectionProxyLayer::kProxyA);

    ipp_token_cache_managers_[IpProtectionProxyLayer::kProxyB] =
        std::make_unique<IpProtectionTokenCacheManagerImpl>(
            this, config_getter_.get(), IpProtectionProxyLayer::kProxyB);
  }

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

IpProtectionConfigCacheImpl::~IpProtectionConfigCacheImpl() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

bool IpProtectionConfigCacheImpl::AreAuthTokensAvailable() {
  // If proxy list is not available, tokens cannot be available. Also if there
  // are no token cache managers, there are no tokens.
  if (!IsProxyListAvailable() || ipp_token_cache_managers_.empty()) {
    return false;
  }

  bool all_caches_have_tokens = true;
  for (const auto& manager : ipp_token_cache_managers_) {
    if (!manager.second->IsAuthTokenAvailable(
            ipp_proxy_list_manager_->CurrentGeo())) {
      base::UmaHistogramEnumeration(
          "NetworkService.IpProtection.EmptyTokenCache", manager.first);
      all_caches_have_tokens = false;
    }
  }

  return all_caches_have_tokens;
}

std::optional<BlindSignedAuthToken> IpProtectionConfigCacheImpl::GetAuthToken(
    size_t chain_index) {
  std::optional<BlindSignedAuthToken> result;

  // If the proxy list is empty, there cannot be any matching tokens.
  if (!IsProxyListAvailable() || ipp_token_cache_managers_.empty()) {
    return result;
  }

  auto proxy_layer = chain_index == 0 ? IpProtectionProxyLayer::kProxyA
                                      : IpProtectionProxyLayer::kProxyB;
  if (ipp_token_cache_managers_.count(proxy_layer) > 0) {
    result = ipp_token_cache_managers_[proxy_layer]->GetAuthToken(
        ipp_proxy_list_manager_->CurrentGeo());
  }
  return result;
}

void IpProtectionConfigCacheImpl::InvalidateTryAgainAfterTime() {
  for (const auto& manager : ipp_token_cache_managers_) {
    manager.second->InvalidateTryAgainAfterTime();
  }
}

void IpProtectionConfigCacheImpl::SetIpProtectionTokenCacheManagerForTesting(
    IpProtectionProxyLayer proxy_layer,
    std::unique_ptr<IpProtectionTokenCacheManager> ipp_token_cache_manager) {
  ipp_token_cache_managers_[proxy_layer] = std::move(ipp_token_cache_manager);
}

IpProtectionTokenCacheManager*
IpProtectionConfigCacheImpl::GetIpProtectionTokenCacheManagerForTesting(
    IpProtectionProxyLayer proxy_layer) {
  return ipp_token_cache_managers_[proxy_layer].get();
}

void IpProtectionConfigCacheImpl::SetIpProtectionProxyListManagerForTesting(
    std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager) {
  ipp_proxy_list_manager_ = std::move(ipp_proxy_list_manager);
}

IpProtectionProxyListManager*
IpProtectionConfigCacheImpl::GetIpProtectionProxyListManagerForTesting() {
  return ipp_proxy_list_manager_.get();
}

bool IpProtectionConfigCacheImpl::IsProxyListAvailable() {
  return ipp_proxy_list_manager_ != nullptr
             ? ipp_proxy_list_manager_->IsProxyListAvailable()
             : false;
}

void IpProtectionConfigCacheImpl::QuicProxiesFailed() {
  ipp_over_quic_ = false;
}

std::vector<net::ProxyChain> IpProtectionConfigCacheImpl::GetProxyChainList() {
  if (ipp_proxy_list_manager_ == nullptr) {
    return {};
  }
  std::vector<net::ProxyChain> proxy_list =
      ipp_proxy_list_manager_->ProxyList();

  bool ipp_over_quic_only = net::features::kIpPrivacyUseQuicProxiesOnly.Get();
  if (ipp_over_quic_ || ipp_over_quic_only) {
    proxy_list = MakeQuicProxyList(
        proxy_list, /*include_https_fallback=*/!ipp_over_quic_only);
  }

  return proxy_list;
}

void IpProtectionConfigCacheImpl::RequestRefreshProxyList() {
  if (ipp_proxy_list_manager_ != nullptr) {
    ipp_proxy_list_manager_->RequestRefreshProxyList();
  }
}

void IpProtectionConfigCacheImpl::GeoObserved(const std::string& geo_id) {
  // If token caching by geo is disabled, short-circuit and don't do anything.
  if (!enable_token_caching_by_geo_) {
    return;
  }

  if (ipp_proxy_list_manager_ != nullptr &&
      ipp_proxy_list_manager_->CurrentGeo() != geo_id) {
    ipp_proxy_list_manager_->RefreshProxyListForGeoChange();
  }

  for (auto& [_, token_manager] : ipp_token_cache_managers_) {
    if (token_manager->CurrentGeo() != geo_id) {
      token_manager->SetCurrentGeo(geo_id);
    }
  }
}

void IpProtectionConfigCacheImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // When the network changes, but there is still a network, reset the
  // tracking of whether QUIC proxies work, and try to fetch a new proxy list.
  if (type != net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    ipp_over_quic_ = net::features::kIpPrivacyUseQuicProxies.Get();
    RequestRefreshProxyList();
  }
}

}  // namespace network
