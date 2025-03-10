// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.EXPORT_AND_DELETE_BUTTON_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinates the export flow. Ties the export flow, the dialog fragment and its content view
 * together.
 */
public class PasswordAccessLossExportDialogCoordinator {
    private final FragmentActivity mActivity;
    private final PasswordAccessLossExportDialogFragment mFragment;
    private final PasswordAccessLossExportDialogMediator mMediator;

    public PasswordAccessLossExportDialogCoordinator(FragmentActivity activity, Profile profile) {
        mActivity = activity;
        View dialogView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.password_access_loss_export_dialog_view, null);
        mFragment = new PasswordAccessLossExportDialogFragment();
        mMediator =
                new PasswordAccessLossExportDialogMediator(
                        activity, profile, dialogView, mFragment);
        initialize(dialogView);
    }

    @VisibleForTesting
    PasswordAccessLossExportDialogCoordinator(
            FragmentActivity activity,
            PasswordAccessLossExportDialogFragment fragment,
            PasswordAccessLossExportDialogMediator mediator,
            View dialogView) {
        mActivity = activity;
        mFragment = fragment;
        mMediator = mediator;
        initialize(dialogView);
    }

    private void initialize(View dialogView) {
        mFragment.setView(dialogView);
        mFragment.setDelegate(mMediator);
        bindDialogView(dialogView);
    }

    private void bindDialogView(View dialogView) {
        PropertyModel model =
                new PropertyModel.Builder(PasswordAccessLossExportDialogProperties.ALL_KEYS)
                        .with(
                                EXPORT_AND_DELETE_BUTTON_CALLBACK,
                                mMediator::handlePositiveButtonClicked)
                        .with(CLOSE_BUTTON_CALLBACK, mMediator::onExportFlowCanceled)
                        .build();

        PropertyModelChangeProcessor.create(
                model, dialogView, PasswordAccessLossExportDialogBinder::bind);
    }

    public void showExportDialog() {
        mFragment.show(mActivity.getSupportFragmentManager(), null);
    }
}
