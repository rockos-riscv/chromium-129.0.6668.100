/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.nearby.presence.rust.credential;

import static com.google.android.nearby.presence.rust.credential.Utils.copyBytes;

/** A V1 discovery credential in a format that is ready to be passed to native code. */
public final class V1DiscoveryCredential {
  private final byte[] keySeed;
  private final byte[] expectedMicShortSaltIdentityTokenHmac;
  private final byte[] expectedMicExtendedSaltIdentityTokenHmac;
  private final byte[] expectedSignatureIdentityTokenHmac;
  private final byte[] pubKey;

  /** Create the credential. Each array is exactly 32 bytes. */
  public V1DiscoveryCredential(
      byte[] keySeed,
      byte[] expectedMicShortSaltIdentityTokenHmac,
      byte[] expectedMicExtendedSaltIdentityTokenHmac,
      byte[] expectedSignatureIdentityTokenHmac,
      byte[] pubKey) {
    this.keySeed = copyBytes(keySeed, 32);
    this.expectedMicShortSaltIdentityTokenHmac =
        copyBytes(expectedMicShortSaltIdentityTokenHmac, 32);
    this.expectedMicExtendedSaltIdentityTokenHmac =
        copyBytes(expectedMicExtendedSaltIdentityTokenHmac, 32);
    this.expectedSignatureIdentityTokenHmac = copyBytes(expectedSignatureIdentityTokenHmac, 32);
    this.pubKey = copyBytes(pubKey, 32);
  }
}
