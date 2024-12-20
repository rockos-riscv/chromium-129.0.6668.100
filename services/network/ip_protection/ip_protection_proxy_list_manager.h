// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace net {

class ProxyChain;

}  // namespace net

namespace network {

// Manages a list of currently cached proxy hostnames.
//
// This class is responsible for checking, fetching, and refreshing the proxy
// list for IpProtectionConfigCache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyListManager {
 public:
  virtual ~IpProtectionProxyListManager() = default;

  // Check whether a proxy list is available.
  virtual bool IsProxyListAvailable() = 0;

  // Return the currently cached proxy list. This list may be empty even
  // if `IsProxyListAvailable()` returned true.
  virtual const std::vector<net::ProxyChain>& ProxyList() = 0;

  // Returns the geo id of the current proxy list.
  //
  // If there is not an available proxy list, an empty string will be returned.
  // If token caching by geo is disabled, this will always return "EARTH".
  virtual const std::string& CurrentGeo() = 0;

  // Requests a proxy list refresh when a geo change has occurred. This will
  // either kick off an immediate refresh or schedule a refresh for the soonest
  // possible time.
  virtual void RefreshProxyListForGeoChange() = 0;

  // Request a refresh of the proxy list. Call this when it's likely that the
  // proxy list is out of date.
  virtual void RequestRefreshProxyList() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_
