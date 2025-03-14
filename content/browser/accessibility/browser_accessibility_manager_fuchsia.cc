// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include <lib/inspect/component/cpp/component.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(initial_tree, node_id_delegate,
                                                delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(
      BrowserAccessibilityManagerFuchsia::GetEmptyDocument(), node_id_delegate,
      delegate);
}

BrowserAccessibilityManagerFuchsia::BrowserAccessibilityManagerFuchsia(
    const ui::AXTreeUpdate& initial_tree,
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(node_id_delegate, delegate) {
  Initialize(initial_tree);

  ui::AccessibilityBridgeFuchsia* accessibility_bridge =
      GetAccessibilityBridge();
  if (accessibility_bridge) {
    inspect_node_ = accessibility_bridge->GetInspectNode();
    tree_dump_node_ = inspect_node_.CreateLazyNode("tree-data", [this]() {
      inspect::Inspector inspector;

      auto str = ax_tree()->ToString();
      auto str_capacity = str.capacity();
      inspector.GetRoot().CreateString(GetTreeID().ToString(), std::move(str),
                                       &inspector);

      // Test to check if the string fit in memory.
      if (inspector.GetStats().failed_allocations > 0) {
        ZX_LOG(WARNING, ZX_OK)
            << "Inspector had failed allocations. Some semantic tree data may "
               "be missing. Size of the string we tried to store: "
            << str_capacity << " bytes";
      }

      return fpromise::make_ok_promise(inspector);
    });
  }
}

BrowserAccessibilityManagerFuchsia::~BrowserAccessibilityManagerFuchsia() =
    default;

ui::AccessibilityBridgeFuchsia*
BrowserAccessibilityManagerFuchsia::GetAccessibilityBridge() const {
  if (accessibility_bridge_for_test_)
    return accessibility_bridge_for_test_;

  gfx::NativeWindow top_level_native_window =
      delegate_ ? delegate_->GetTopLevelNativeWindow() : gfx::NativeWindow();

  aura::Window* accessibility_bridge_key =
      top_level_native_window ? top_level_native_window->GetRootWindow()
                              : nullptr;
  if (!accessibility_bridge_key) {
    return nullptr;
  }

  ui::AccessibilityBridgeFuchsiaRegistry* accessibility_bridge_registry =
      ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(accessibility_bridge_registry);

  return accessibility_bridge_registry->GetAccessibilityBridge(
      accessibility_bridge_key);
}

void BrowserAccessibilityManagerFuchsia::FireFocusEvent(ui::AXNode* node) {
  ui::AXTreeManager::FireFocusEvent(node);

  if (!GetAccessibilityBridge())
    return;

  BrowserAccessibilityFuchsia* new_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(GetFromAXNode(node));

  BrowserAccessibilityFuchsia* old_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(GetFromAXNode(GetLastFocusedNode()));

  if (old_focus_fuchsia)
    old_focus_fuchsia->OnDataChanged();

  if (new_focus_fuchsia)
    new_focus_fuchsia->OnDataChanged();
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerFuchsia::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = ui::kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerFuchsia::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node,
    int action_request_id) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);

  // Blink fires a hover event on the result of a hit test, so we should process
  // it here.
  if (event_type == ax::mojom::Event::kHover)
    OnHitTestResult(action_request_id, node);
}

void BrowserAccessibilityManagerFuchsia::OnHitTestResult(
    int action_request_id,
    BrowserAccessibility* node) {
  if (!GetAccessibilityBridge())
    return;

  std::optional<uint32_t> hit_result_id;

  if (node) {
    BrowserAccessibilityFuchsia* hit_result =
        ToBrowserAccessibilityFuchsia(node);
    DCHECK(hit_result);
    hit_result_id = hit_result->GetFuchsiaNodeID();
  }

  GetAccessibilityBridge()->OnAccessibilityHitTestResult(action_request_id,
                                                         hit_result_id);
}

void BrowserAccessibilityManagerFuchsia::UpdateDeviceScaleFactor() {
  ui::AccessibilityBridgeFuchsia* accessibility_bridge =
      GetAccessibilityBridge();
  if (accessibility_bridge)
    device_scale_factor_ = accessibility_bridge->GetDeviceScaleFactor();
  else
    BrowserAccessibilityManager::UpdateDeviceScaleFactor();
}

void BrowserAccessibilityManagerFuchsia::SetAccessibilityBridgeForTest(
    ui::AccessibilityBridgeFuchsia* accessibility_bridge_for_test) {
  accessibility_bridge_for_test_ = accessibility_bridge_for_test;
}

}  // namespace content
