// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/installer/util/initial_preferences.h"

namespace first_run {
namespace internal {

bool IsOrganicFirstRun() {
  // We treat all installs as organic.
  return true;
}

base::FilePath InitialPrefsPath() {
  base::FilePath dir_exe = base::FilePath("/etc/chromium");

  return installer::InitialPreferences::Path(dir_exe);
}

}  // namespace internal
}  // namespace first_run
