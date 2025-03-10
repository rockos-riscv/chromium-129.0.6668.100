// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

class ASH_PUBLIC_EXPORT LobsterSession {
 public:
  using StatusCallback = base::OnceCallback<void(bool)>;

  virtual ~LobsterSession() = default;

  virtual void DownloadCandidate(int candidate_id, StatusCallback) = 0;
  virtual void CommitAsInsert(int candidate_id, StatusCallback callback) = 0;
  virtual void CommitAsDownload(int candidate_id, StatusCallback callback) = 0;
  virtual void RequestCandidates(const std::string& query,
                                 int num_candidates,
                                 RequestCandidatesCallback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SESSION_H_
