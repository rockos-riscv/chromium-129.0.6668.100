// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_service.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_session.h"
#include "chrome/browser/ash/lobster/image_fetcher.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "components/manta/snapper_provider.h"

LobsterService::LobsterService(
    std::unique_ptr<manta::SnapperProvider> snapper_provider)
    : image_provider_(std::move(snapper_provider)),
      image_fetcher_(image_provider_.get(), &candidate_id_generator_),
      resizer_(&image_fetcher_) {}

LobsterService::~LobsterService() = default;

void LobsterService::SetActiveSession(ash::LobsterSession* session) {
  active_session_ = session;
}

ash::LobsterSession* LobsterService::active_session() {
  return active_session_;
}

LobsterSystemStateProvider* LobsterService::system_state_provider() {
  return &system_state_provider_;
}

void LobsterService::RequestCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  image_fetcher_.RequestCandidates(query, num_candidates, std::move(callback));
}

void LobsterService::InflateCandidate(uint32_t seed,
                                      const std::string& query,
                                      ash::InflateCandidateCallback callback) {
  resizer_.InflateImage(seed, query, std::move(callback));
}
