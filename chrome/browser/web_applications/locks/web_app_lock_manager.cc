// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {

// The WebAppLockManager uses the following breakdown to define its locks with
// the PartitionedLockManager:
// - NoopLock:
//   - No locks.
// - SharedWebContentsLock:
//   - Exclusive {kStatic, kBackgroundWebContents}
// - AllAppsLock:
//   - Exclusive {kStatic, kAllApps}
// - AppLock:
//   - Shared {kStatic, kAllApps}
//   - Exclusive {kApp, <app_id>}
//   - <repeat for each app id requested>
// - SharedWebContentsWithAppLock
//   - Exclusive {kStatic, kBackgroundWebContents}
//   - Shared {kStatic, kAllApps}
//   - Exclusive {kApp, <app_id>}
//   - <repeat for each app id requested>
//
// We use strict lock ordering to ensure no deadlocks occur, and to allow
// 'upgrading'.  Locks must be in this order to facilitate the upgrade
// functionality provided by the system and not deadlock
// - kBackgroundWebContents (static partition)
// - kAllApps (static partition)
// - kApp (app partition)
//
// For example, the lock used for the SharedWebContentsLock  has to be
// 'before' the locks used for the AppLock, as this allows the lock upgrade
// method `UpgradeAndAcquireLock` below where a SharedWebContentsLock can be
// upgraded to a SharedWebContentsWithAppLock.

// These values are not persisted to disk or logs.
enum class LockPartition {
  kStatic = 0,
  kApp = 1,
  kMaxValue = kApp,
};

// These values are not persisted to disk or logs.
enum KeysOnStaticPartition {
  kBackgroundWebContents = 0,
  kAllApps = 1,
  kMaxValue = kAllApps,
};

static_assert(LockPartition::kStatic < LockPartition::kApp);
static_assert(KeysOnStaticPartition::kBackgroundWebContents <
              KeysOnStaticPartition::kAllApps);
// Since we use `base::NumberToString` for the static partition data below, and
// the PartitionedLockId uses alphabetical sorting for the data field, the value
// must stay below 10 until there is a different sorting / encoding scheme.
static_assert(KeysOnStaticPartition::kMaxValue < 10);

const char* KeysOnStaticPartitionToString(KeysOnStaticPartition key) {
  switch (key) {
    case KeysOnStaticPartition::kAllApps:
      return "AllApps";
    case KeysOnStaticPartition::kBackgroundWebContents:
      return "BackgroundWebContents";
  }
}

PartitionedLockManager::PartitionedLockRequest GetAllAppsLock(
    PartitionedLockManager::LockType type) {
  return PartitionedLockManager::PartitionedLockRequest(
      PartitionedLockId(
          {static_cast<int>(LockPartition::kStatic),
           base::NumberToString(KeysOnStaticPartition::kAllApps)}),
      type);
}

PartitionedLockManager::PartitionedLockRequest GetSharedWebContentsLock() {
  return PartitionedLockManager::PartitionedLockRequest(
      PartitionedLockId({static_cast<int>(LockPartition::kStatic),
                         base::NumberToString(
                             KeysOnStaticPartition::kBackgroundWebContents)}),
      PartitionedLockManager::LockType::kExclusive);
}

std::vector<PartitionedLockManager::PartitionedLockRequest>
GetExclusiveAppIdLocks(const base::flat_set<webapps::AppId>& app_ids) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests;
  for (const webapps::AppId& app_id : app_ids) {
    lock_requests.emplace_back(
        PartitionedLockId({static_cast<int>(LockPartition::kApp), app_id}),
        PartitionedLockManager::LockType::kExclusive);
  }
  return lock_requests;
}

std::vector<PartitionedLockManager::PartitionedLockRequest>
GetLockRequestsForLock(const LockDescription& lock) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> requests;
  switch (lock.type()) {
    case LockDescription::Type::kNoOp:
      return {};
    case LockDescription::Type::kApp:
      requests = GetExclusiveAppIdLocks(lock.app_ids());
      requests.push_back(
          GetAllAppsLock(PartitionedLockManager::LockType::kShared));
      return requests;
    case LockDescription::Type::kAllAppsLock:
      return {GetAllAppsLock(PartitionedLockManager::LockType::kExclusive)};
    case LockDescription::Type::kAppAndWebContents:
      requests = GetExclusiveAppIdLocks(lock.app_ids());
      requests.push_back(
          GetAllAppsLock(PartitionedLockManager::LockType::kShared));
      requests.push_back(GetSharedWebContentsLock());
      return requests;
    case LockDescription::Type::kBackgroundWebContents:
      return {GetSharedWebContentsLock()};
  }
}

#if DCHECK_IS_ON()
void LogLockRequest(
    const LockDescription& description,
    const base::Location& location,
    const std::vector<PartitionedLockManager::PartitionedLockRequest>& requests,
    const PartitionedLockManager& lock_manager) {
  DVLOG(1) << "Requesting or upgrading to lock " << description
           << " for location " << location.ToString();
  std::vector<base::Location> locations =
      lock_manager.GetHeldAndQueuedLockLocations(requests);
  if (!locations.empty()) {
    DVLOG(1) << "Lock currently held (or queued to be held) by:";
    for (const auto& held_location : locations) {
      DVLOG(1) << " " << held_location.ToString();
    }
  }
}
#endif

}  // namespace

WebAppLockManager::WebAppLockManager() = default;
WebAppLockManager::~WebAppLockManager() = default;

void WebAppLockManager::SetProvider(base::PassKey<WebAppCommandManager>,
                                    WebAppProvider& provider) {
  provider_ = &provider;
}

bool WebAppLockManager::IsSharedWebContentsLockFree() {
  return lock_manager_.TestLock(GetSharedWebContentsLock()) ==
         PartitionedLockManager::TestLockResult::kFree;
}

template <>
void WebAppLockManager::AcquireLock(
    const LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<NoopLock>)> on_lock_acquired,
    const base::Location& location) {
  CHECK(lock_description.type() == LockDescription::Type::kNoOp);

  auto lock = base::WrapUnique(new NoopLock(
      std::make_unique<PartitionedLockHolder>(), weak_factory_.GetWeakPtr()));
  base::WeakPtr<PartitionedLockHolder> holder = lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)),
              location);
}

template <>
void WebAppLockManager::AcquireLock(
    const LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsLock>)>
        on_lock_acquired,
    const base::Location& location) {
  CHECK(lock_description.type() ==
        LockDescription::Type::kBackgroundWebContents);

  auto lock = base::WrapUnique(new SharedWebContentsLock(
      weak_factory_.GetWeakPtr(), std::make_unique<PartitionedLockHolder>(),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey())));

  base::WeakPtr<PartitionedLockHolder> holder = lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)),
              location);
}

template <>
void WebAppLockManager::AcquireLock(
    const LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired,
    const base::Location& location) {
  CHECK(lock_description.type() == LockDescription::Type::kApp);

  auto lock = base::WrapUnique(new AppLock(
      weak_factory_.GetWeakPtr(), std::make_unique<PartitionedLockHolder>()));

  base::WeakPtr<PartitionedLockHolder> holder = lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)),
              location);
}

template <>
void WebAppLockManager::AcquireLock(
    const LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
        on_lock_acquired,
    const base::Location& location) {
  CHECK(lock_description.type() == LockDescription::Type::kAppAndWebContents);

  auto lock = base::WrapUnique(new SharedWebContentsWithAppLock(
      weak_factory_.GetWeakPtr(), std::make_unique<PartitionedLockHolder>(),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey())));

  base::WeakPtr<PartitionedLockHolder> holder = lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)),
              location);
}

template <>
void WebAppLockManager::AcquireLock(
    const LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<AllAppsLock>)> on_lock_acquired,
    const base::Location& location) {
  CHECK(lock_description.type() == LockDescription::Type::kAllAppsLock);

  auto lock = base::WrapUnique(new AllAppsLock(
      weak_factory_.GetWeakPtr(), std::make_unique<PartitionedLockHolder>()));
  base::WeakPtr<PartitionedLockHolder> holder = lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)),
              location);
}

std::unique_ptr<SharedWebContentsWithAppLockDescription>
WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<SharedWebContentsLock> lock,
    const base::flat_set<webapps::AppId>& app_ids,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
        on_lock_acquired,
    const base::Location& location) {
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      result_lock_description =
          std::make_unique<SharedWebContentsWithAppLockDescription>(app_ids);
  auto result_lock = base::WrapUnique(new SharedWebContentsWithAppLock(
      weak_factory_.GetWeakPtr(), std::move(lock->holder_),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey())));
  base::WeakPtr<PartitionedLockHolder> holder =
      result_lock->holder_->AsWeakPtr();
  // Request the extra locks needed for the app lock.
  std::vector<PartitionedLockManager::PartitionedLockRequest> requests =
      GetLockRequestsForLock(AppLockDescription(app_ids));
#if DCHECK_IS_ON()
  LogLockRequest(*result_lock_description, location, requests, lock_manager_);
#endif
  lock_manager_.AcquireLocks(
      std::move(requests), holder,
      base::BindOnce(std::move(on_lock_acquired), std::move(result_lock)),
      location);

  return result_lock_description;
}

std::unique_ptr<AppLockDescription> WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<NoopLock> lock,
    const base::flat_set<webapps::AppId>& app_ids,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired,
    const base::Location& location) {
  std::unique_ptr<AppLockDescription> result_lock_description =
      std::make_unique<AppLockDescription>(app_ids);

  auto result_lock = base::WrapUnique(
      new AppLock(weak_factory_.GetWeakPtr(), std::move(lock->holder_)));
  base::WeakPtr<PartitionedLockHolder> holder =
      result_lock->holder_->AsWeakPtr();

  std::vector<PartitionedLockManager::PartitionedLockRequest> requests =
      GetLockRequestsForLock(*result_lock_description);
#if DCHECK_IS_ON()
  LogLockRequest(*result_lock_description, location, requests, lock_manager_);
#endif
  lock_manager_.AcquireLocks(
      std::move(requests), holder,
      base::BindOnce(std::move(on_lock_acquired), std::move(result_lock)),
      location);
  return result_lock_description;
}

base::Value WebAppLockManager::ToDebugValue() const {
  return lock_manager_.ToDebugValue([](const PartitionedLockId& lock)
                                        -> std::string {
    // Out of bounds things should NOT happen here, but given this is being
    // called from debug UI, it's better to return something reasonable
    // instead of doing undefined behavior.
    DCHECK_GE(lock.partition, 0);
    DCHECK_LE(lock.partition, static_cast<int>(LockPartition::kMaxValue));
    if (lock.partition < 0 ||
        lock.partition > static_cast<int>(LockPartition::kMaxValue)) {
      return base::StringPrintf("Invalid lock partition: %i", lock.partition);
    }
    LockPartition partition = static_cast<LockPartition>(lock.partition);
    switch (partition) {
      case LockPartition::kApp:
        return base::StrCat({"App, ", lock.key});
      case LockPartition::kStatic: {
        int lock_key = -1;
        if (!base::StringToInt(lock.key, &lock_key)) {
          return base::StringPrintf("Static, invalid number '%s'",
                                    lock.key.c_str());
        }
        DCHECK_GE(lock_key, 0);
        DCHECK_LE(lock_key, static_cast<int>(KeysOnStaticPartition::kMaxValue));
        if (lock_key < 0 ||
            lock_key > static_cast<int>(KeysOnStaticPartition::kMaxValue)) {
          return base::StringPrintf("Static, invalid key number: %i", lock_key);
        }
        KeysOnStaticPartition key =
            static_cast<KeysOnStaticPartition>(lock_key);
        return base::StringPrintf("Static, '%s'",
                                  KeysOnStaticPartitionToString(key));
      }
    }
  });
}

void WebAppLockManager::AcquireLock(base::WeakPtr<PartitionedLockHolder> holder,
                                    const LockDescription& lock_description,
                                    base::OnceClosure on_lock_acquired,
                                    const base::Location& location) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> requests =
      GetLockRequestsForLock(lock_description);
#if DCHECK_IS_ON()
  LogLockRequest(lock_description, location, requests, lock_manager_);
#endif
  lock_manager_.AcquireLocks(std::move(requests), holder,
                             std::move(on_lock_acquired), location);
}

}  // namespace web_app
