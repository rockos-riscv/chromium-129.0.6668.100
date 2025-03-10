// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "net/base/proxy_chain.h"
#include "services/network/ip_protection/ip_protection_data_types.h"

namespace network {
// Interface used by the network service to get the IP Protection configuration
// (authentication tokens + proxy list).
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigGetter {
 public:
  virtual ~IpProtectionConfigGetter() = default;

  using TryGetAuthTokensCallback =
      base::OnceCallback<void(std::optional<std::vector<BlindSignedAuthToken>>,
                              std::optional<::base::Time>)>;

  // Try to get a batch of IP Protection tokens.
  //
  // On success, the callback is triggered with the tokens. On error, the
  // callback is triggered with nullopt for tokens and will contain the
  // time after which the getter suggests trying again, as a form of backoff.
  //
  // External network requests may be made by this method.
  //
  // It is forbidden for two calls to this method to be outstanding at the same
  // time.
  //
  // `callback` is invoked on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of construction of
  // this object).
  virtual void TryGetAuthTokens(uint32_t batch_size,
                                IpProtectionProxyLayer proxy_layer,
                                TryGetAuthTokensCallback callback) = 0;

  using GetProxyListCallback =
      base::OnceCallback<void(std::optional<std::vector<::net::ProxyChain>>,
                              std::optional<GeoHint> geo_hint)>;

  // Get the list of IP Protection proxy chains.
  //
  // The list contains lists of proxy chain hostnames, in order of preference.
  // Callers should prefer the first proxy, falling back to later proxies in the
  // list. All proxies are implicitly HTTPS.
  //
  // This callback is triggered with an up-to-date list.
  //
  // External network requests may be made by this method.
  //
  // `callback` is invoked on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of construction of
  // this object).
  virtual void GetProxyList(GetProxyListCallback callback) = 0;

  // Checks if the getter is available for use after construction.
  virtual bool IsAvailable() = 0;
};
}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_H_
