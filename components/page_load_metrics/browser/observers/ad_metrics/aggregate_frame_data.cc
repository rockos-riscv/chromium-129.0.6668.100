// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/aggregate_frame_data.h"

#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

AggregateFrameData::AggregateFrameData() = default;
AggregateFrameData::~AggregateFrameData() = default;

void AggregateFrameData::UpdateCpuUsage(base::TimeTicks update_time,
                                        base::TimeDelta update,
                                        bool is_ad) {
  // Update the overall usage for all of the relevant buckets.
  cpu_usage_ += update;

  // Update the peak usage.
  total_peak_cpu_.UpdatePeakWindowedPercent(update, update_time);
  if (!is_ad)
    non_ad_peak_cpu_.UpdatePeakWindowedPercent(update, update_time);
}

void AggregateFrameData::UpdateFirstAdFCPSinceNavStart(
    base::TimeDelta time_since_nav_start) {
  if (!first_ad_fcp_after_main_nav_start_ ||
      time_since_nav_start < first_ad_fcp_after_main_nav_start_.value()) {
    first_ad_fcp_after_main_nav_start_ = time_since_nav_start;
  }
}

void AggregateFrameData::OnAdAuctionComplete() {
  if (!first_ad_fcp_after_main_nav_start_) {
    completed_fledge_auction_before_fcp_ = true;
  }
}

void AggregateFrameData::ProcessResourceLoadInFrame(
    const mojom::ResourceDataUpdatePtr& resource,
    bool is_outermost_main_frame) {
  resource_data_.ProcessResourceLoad(resource);
  if (is_outermost_main_frame) {
    outermost_main_frame_resource_data_.ProcessResourceLoad(resource);
  }
}

void AggregateFrameData::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                                       ResourceMimeType mime_type,
                                       bool is_outermost_main_frame) {
  // TODO(crbug.com/40216775): Test coverage isn't enough for this
  // method. Add more tests.
  resource_data_.AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  if (is_outermost_main_frame) {
    outermost_main_frame_resource_data_.AdjustAdBytes(unaccounted_ad_bytes,
                                                      mime_type);
  }
}

}  // namespace page_load_metrics
