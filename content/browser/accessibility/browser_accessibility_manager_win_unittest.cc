// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

namespace {

class TestFragmentRootDelegate : public ui::AXFragmentRootDelegateWin {
 public:
  TestFragmentRootDelegate(
      content::BrowserAccessibilityManager* browser_accessibility_manager)
      : browser_accessibility_manager_(browser_accessibility_manager) {}
  ~TestFragmentRootDelegate() = default;

  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override {
    return browser_accessibility_manager_->GetBrowserAccessibilityRoot()
        ->GetNativeViewAccessible();
  }

  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override {
    return nullptr;
  }

  bool IsAXFragmentRootAControlElement() override { return true; }

  raw_ptr<content::BrowserAccessibilityManager> browser_accessibility_manager_;
};
}  // namespace

namespace content {

class BrowserAccessibilityManagerWinTest : public testing::Test {
 public:
  BrowserAccessibilityManagerWinTest() = default;

  BrowserAccessibilityManagerWinTest(
      const BrowserAccessibilityManagerWinTest&) = delete;
  BrowserAccessibilityManagerWinTest& operator=(
      const BrowserAccessibilityManagerWinTest&) = delete;

  ~BrowserAccessibilityManagerWinTest() override = default;

 protected:
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  ui::TestAXNodeIdDelegate node_id_delegate_;

 private:
  void SetUp() override;

  // This is needed to prevent a DCHECK failure when OnAccessibilityApiUsage
  // is called in BrowserAccessibility::GetRole.
  content::BrowserTaskEnvironment task_environment_;
};

void BrowserAccessibilityManagerWinTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<ui::TestAXPlatformTreeManagerDelegate>();
}

TEST_F(BrowserAccessibilityManagerWinTest, DynamicallyAddedIFrame) {
  base::test::ScopedFeatureList scoped_feature_list(::features::kUiaProvider);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> root_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(root_manager.get());

  ui::AXPlatformNode* root_document_root_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          root_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible());

  std::unique_ptr<ui::AXPlatformNodeDelegate> fragment_root =
      std::make_unique<ui::AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                              &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Simulate the case where an iframe is created but the update to add the
  // element to the root frame's document has not yet come through.
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate> iframe_delegate =
      std::make_unique<ui::TestAXPlatformTreeManagerDelegate>();
  iframe_delegate->is_root_frame_ = false;
  iframe_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> iframe_manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdateForTesting(root),
                                          node_id_delegate_,
                                          iframe_delegate.get()));

  // The new frame is not a root frame, so the fragment root's lone child should
  // still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityManagerWinTest, ChildTree) {
  base::test::ScopedFeatureList scoped_feature_list(::features::kUiaProvider);

  ui::AXNodeData child_tree_root;
  child_tree_root.id = 1;
  child_tree_root.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate child_tree_update =
      MakeAXTreeUpdateForTesting(child_tree_root);

  ui::AXNodeData parent_tree_root;
  parent_tree_root.id = 1;
  parent_tree_root.role = ax::mojom::Role::kRootWebArea;
  parent_tree_root.AddChildTreeId(child_tree_update.tree_data.tree_id);
  ui::AXTreeUpdate parent_tree_update =
      MakeAXTreeUpdateForTesting(parent_tree_root);

  child_tree_update.tree_data.parent_tree_id =
      parent_tree_update.tree_data.tree_id;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          parent_tree_update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(parent_manager.get());

  ui::AXPlatformNode* root_document_root_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          parent_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible());

  std::unique_ptr<ui::AXPlatformNodeDelegate> fragment_root =
      std::make_unique<ui::AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                              &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Add the child tree.
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate> child_tree_delegate =
      std::make_unique<ui::TestAXPlatformTreeManagerDelegate>();
  child_tree_delegate->is_root_frame_ = false;
  child_tree_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(child_tree_update, node_id_delegate_,
                                          child_tree_delegate.get()));

  // The fragment root's lone child should still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

}  // namespace content
