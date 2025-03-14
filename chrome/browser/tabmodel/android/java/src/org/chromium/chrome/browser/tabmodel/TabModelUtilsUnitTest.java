// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link TabModelUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelUtilsUnitTest {
    private static final int TAB_ID = 5;
    private static final int INCOGNITO_TAB_ID = 7;
    private static final int UNUSED_TAB_ID = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Tab mTab;
    @Mock private Tab mIncognitoTab;
    @Mock private Callback<TabModelSelector> mTabModelSelectorCallback;
    @Mock private WindowAndroid mWindowAndroid;

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private MockTabModelSelector mTabModelSelector;
    private MockTabModel mTabModel;
    private MockTabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mIncognitoTab.getId()).thenReturn(INCOGNITO_TAB_ID);
        when(mIncognitoTab.isIncognito()).thenReturn(true);
        mTabModelSelector = spy(new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null));
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mTabModel = (MockTabModel) mTabModelSelector.getModel(false);
        mTabModel.addTab(
                mTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
        mIncognitoTabModel = (MockTabModel) mTabModelSelector.getModel(true);
        mIncognitoTabModel.addTab(
                mIncognitoTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
    }

    @Test
    @SmallTest
    public void testSelectTabById() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(mTabModelSelector, TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TAB_ID, mTabModel.getTabAt(mTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdIncognito() {
        assertEquals(TabList.INVALID_TAB_INDEX, mIncognitoTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, INCOGNITO_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(
                INCOGNITO_TAB_ID, mIncognitoTabModel.getTabAt(mIncognitoTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpInvalidTabId() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, Tab.INVALID_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpNotFound() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(mTabModelSelector, UNUSED_TAB_ID, TabSelectionType.FROM_USER);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedCallback() {
        mTabModelSelector.markTabStateInitialized();
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelectorCallback).onResult(mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedObserver() {
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabModelSelectorCallback, never()).onResult(mTabModelSelector);
        mTabModelSelector.markTabStateInitialized();
        verify(mTabModelSelectorCallback).onResult(mTabModelSelector);
        verify(mTabModelSelector).removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testRunOnTabStateInitializedRemoveObserverWhenDestroyed() {
        TabModelUtils.runOnTabStateInitialized(mTabModelSelector, mTabModelSelectorCallback);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabModelSelector, never())
                .removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
        mTabModelSelector.destroy();
        verify(mTabModelSelector).removeObserver(eq(mTabModelSelectorObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testGetTabModelFilterByTab() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelFilter filter = TabModelUtils.getTabModelFilterByTab(mTab);
        assertEquals(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), filter);
    }
}
