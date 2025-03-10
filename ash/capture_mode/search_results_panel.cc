// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/search_results_panel.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// TODO(sophiewen): Remove hardcoded values when we get UX specs.
constexpr int kSearchResultsPanelWidth = 600;
const std::u16string kSearchBoxPlaceholderText = u"Add to your search";

}  // namespace

// `SunfishSearchBoxView` contains an image thumbnail and a textfield.
class SunfishSearchBoxView : public views::View {
  METADATA_HEADER(SunfishSearchBoxView, views::View)

 public:
  SunfishSearchBoxView() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetCollapseMargins(true);
    // TODO(b/356878705): Replace with the captured region screenshot when the
    // backend is hooked up. Currently using the search icon as a placeholder.
    AddChildView(views::Builder<views::ImageView>()
                     .CopyAddressTo(&image_view_)
                     .SetImage(ui::ImageModel::FromVectorIcon(
                         vector_icons::kGoogleColorIcon))
                     .Build());
    AddChildView(views::Builder<views::Textfield>()
                     .CopyAddressTo(&textfield_)
                     .SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT)
                     .SetPlaceholderText(kSearchBoxPlaceholderText)
                     .SetProperty(views::kFlexBehaviorKey,
                                  views::FlexSpecification(
                                      views::LayoutOrientation::kHorizontal,
                                      views::MinimumFlexSizeRule::kPreferred,
                                      views::MaximumFlexSizeRule::kUnbounded))
                     .SetBackgroundEnabled(false)
                     .SetBorder(nullptr)
                     .Build());
    SetBackground(views::CreateThemedSolidBackground(
        cros_tokens::kCrosSysSystemBaseElevated));
  }
  SunfishSearchBoxView(const SunfishSearchBoxView&) = delete;
  SunfishSearchBoxView& operator=(const SunfishSearchBoxView&) = delete;
  ~SunfishSearchBoxView() override = default;

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_view_;
  raw_ptr<views::Textfield> textfield_;
};

BEGIN_METADATA(SunfishSearchBoxView)
END_METADATA

SearchResultsPanel::SearchResultsPanel() {
  DCHECK(features::IsSunfishFeatureEnabled());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true);
  AddChildView(std::make_unique<SunfishSearchBoxView>());
  search_results_view_ =
      AddChildView(CaptureModeController::Get()->CreateSearchResultsView());
  search_results_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  // TODO(b/356878705): Replace this when the backend is hooked up. Currently
  // used for UI debugging.
  search_results_view_->Navigate(GURL("https://www.google.com/search?q=cat"));

  SetBackground(views::CreateThemedSolidBackground(
      cros_tokens::kCrosSysSystemBaseElevated));
}

SearchResultsPanel::~SearchResultsPanel() = default;

// static
std::unique_ptr<views::Widget> SearchResultsPanel::CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // TODO(b/356878705): Use the captured region display and bounds.
  const gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
  gfx::Rect bounds(work_area.right() - kSearchResultsPanelWidth, work_area.y(),
                   kSearchResultsPanelWidth, work_area.height());
  params.bounds = bounds;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SearchResultsPanel>());
  return widget;
}

BEGIN_METADATA(SearchResultsPanel)
END_METADATA

}  // namespace ash
