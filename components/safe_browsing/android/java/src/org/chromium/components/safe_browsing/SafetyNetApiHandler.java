// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

/**
 * Java interface that a SafetyNetApiHandler must implement when used with {@code
 * SafeBrowsingApiBridge}.
 */
public interface SafetyNetApiHandler {
    /** Observer to be notified when the SafetyNetApiHandler determines the verdict for a url. */
    interface Observer {
        // Note: |checkDelta| is the time the remote call took in microseconds.
        void onUrlCheckDone(
                long callbackId,
                @SafeBrowsingResult int resultStatus,
                String metadata,
                long checkDelta);

        void onVerifyAppsEnabledDone(long callbackId, @VerifyAppsResult int result);
    }

    /**
     * Verifies that SafetyNetApiHandler can operate and initializes if feasible. Should be called
     * on the same sequence as |startUriLookup|.
     *
     * @param observer The object on which to call the callback functions when URL checking is
     *     complete.
     * @return whether Safe Browsing is supported for this installation.
     */
    boolean init(Observer observer);

    /**
     * Start a URI-lookup to determine if it matches one of the specified threats.
     * This is called on every URL resource Chrome loads, on the same sequence as |init|.
     */
    void startUriLookup(long callbackId, String uri, int[] threatsOfInterest);

    /**
     * Start a check to determine if a uri is in an allowlist. If true, password protection service
     * will consider the uri to be safe.
     *
     * @param uri The uri from a password protection event(user focuses on password form * or user
     *     reuses their password)
     * @param threatType determines the type of the allowlist that the uri will be matched to.
     * @return true if the uri is found in the corresponding allowlist. Otherwise, false.
     */
    boolean startAllowlistLookup(String uri, int threatType);

    /**
     * Start a check to see if the user has app verification enabled. The response will be provided
     * to the observer with the onVerifyAppsEnabledDone method.
     *
     * @param callbackId The id of the callback which should be returned * with the result.
     */
    void isVerifyAppsEnabled(long callbackId);

    /**
     * Prompt the user to enable app verification. The response will be provided to the observer
     * with the onVerifyAppsEnabledDone method.
     *
     * @param callbackId The id of the callback which should be returned * with the result.
     */
    void enableVerifyApps(long callbackId);
}
