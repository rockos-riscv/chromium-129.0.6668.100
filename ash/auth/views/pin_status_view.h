// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_PIN_STATUS_VIEW_H_
#define ASH_AUTH_VIEWS_PIN_STATUS_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// Contains the pin status text.
class ASH_EXPORT PinStatusView : public views::View {
  METADATA_HEADER(PinStatusView, views::View)

 public:
  class TestApi {
   public:
    explicit TestApi(PinStatusView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    const std::u16string& GetCurrentText() const;

    raw_ptr<PinStatusView> GetView();

   private:
    const raw_ptr<PinStatusView> view_;
  };

  PinStatusView(const std::u16string& text);

  PinStatusView(const PinStatusView&) = delete;
  PinStatusView& operator=(const PinStatusView&) = delete;

  ~PinStatusView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  base::WeakPtr<PinStatusView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetText(const std::u16string& text_str);

 private:
  raw_ptr<views::Label> text_label_ = nullptr;

  base::WeakPtrFactory<PinStatusView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_PIN_STATUS_VIEW_H_
