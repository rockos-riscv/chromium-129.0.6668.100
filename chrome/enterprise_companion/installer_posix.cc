// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer_posix.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/installer_paths.h"

namespace enterprise_companion {

namespace {

constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                 base::FILE_PERMISSION_READ_BY_GROUP |
                                 base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                 base::FILE_PERMISSION_READ_BY_OTHERS |
                                 base::FILE_PERMISSION_EXECUTE_BY_OTHERS;

}  // namespace

bool InstallToDir(const base::FilePath& install_directory) {
  base::FilePath source_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &source_exe_path)) {
    LOG(ERROR) << "Failed to retrieve the current executable's path.";
    return false;
  }

  base::FilePath dest_exe_path = install_directory.AppendASCII(kExecutableName);
  if (!base::CopyFile(source_exe_path, dest_exe_path)) {
    LOG(ERROR) << "Failed to copy the new executable to the install directory.";
    return false;
  }

  if (!base::SetPosixFilePermissions(install_directory, kPermissionsMask)) {
    LOG(ERROR) << "Failed to set permissions to drwxr-xr-x at"
               << install_directory;
    return false;
  }

  if (!base::SetPosixFilePermissions(dest_exe_path, kPermissionsMask)) {
    LOG(ERROR) << "Failed to set permissions to rwxr-xr-x at" << dest_exe_path;
    return false;
  }

  return true;
}

}  // namespace enterprise_companion
