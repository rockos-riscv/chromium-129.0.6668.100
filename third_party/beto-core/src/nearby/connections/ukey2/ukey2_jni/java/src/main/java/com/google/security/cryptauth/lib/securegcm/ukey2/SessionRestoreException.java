// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.security.cryptauth.lib.securegcm.ukey2;

/** Represents an unrecoverable error that has occurred during the handshake procedure. */
public class SessionRestoreException extends Exception {
  public SessionRestoreException(String message) {
    super(message);
  }

  public SessionRestoreException(Exception e) {
    super(e);
  }

  public SessionRestoreException(String message, Exception e) {
    super(message, e);
  }
}
