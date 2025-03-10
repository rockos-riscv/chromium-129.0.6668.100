// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "url/gurl.h"

namespace {

// Used to compute signals on whether related URL visit activity has periodicity
// patterns based on the day of the week.
enum DayGroup {
  kWeekday = 0,
  kWeekend = 1,
};

DayGroup GetDayGroupForExplodedTime(const base::Time::Exploded& exploded_time) {
  return (exploded_time.day_of_week == 0 || exploded_time.day_of_week == 6)
             ? DayGroup::kWeekend
             : DayGroup::kWeekday;
}

// Used to compute signals on whether related URL visit activity has periodicity
// patterns based on the time of the day. For simplicity, divides a day into 4
// groups of 6 hours. Time group enum names are of no consequence.
enum TimeGroup {
  kGroup0 = 0,
  kGroup1 = 1,
  kGroup2 = 2,
  kGroup3 = 3,
};

TimeGroup GetTimeGroupForExplodedTime(
    const base::Time::Exploded& exploded_time) {
  static constexpr int kHoursPerGroup = base::Time::kHoursPerDay / 4;

  // Note that since the time groups are meant as approximations, relying only
  // on the `hour` field should be acceptable for the generation of the
  // corresponding signal.
  return static_cast<TimeGroup>(exploded_time.hour / kHoursPerGroup);
}

}  // namespace

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

HistoryURLVisitDataFetcher::HistoryURLVisitDataFetcher(
    history::HistoryService* history_service)
    : history_service_(history_service) {}

HistoryURLVisitDataFetcher::~HistoryURLVisitDataFetcher() = default;

void HistoryURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    const FetcherConfig& config,
    FetchResultCallback callback) {
  history::QueryOptions query_options;
  query_options.begin_time = options.begin_time;
  query_options.duplicate_policy =
      history::QueryOptions::DuplicateHandling::KEEP_ALL_DUPLICATES;

  if (history_service_) {
    history_service_->GetAnnotatedVisits(
        query_options,
        /*compute_redirect_chain_start_properties=*/true,
        /*get_unclustered_visits_only=*/false,
        base::BindOnce(&HistoryURLVisitDataFetcher::OnGotAnnotatedVisits,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       options.fetcher_sources.at(Fetcher::kHistory), config),
        &task_tracker_);
    return;
  }

  std::move(callback).Run({FetchResult::Status::kError, {}});
}

void HistoryURLVisitDataFetcher::OnGotAnnotatedVisits(
    FetchResultCallback callback,
    FetchOptions::FetchSources requested_fetch_sources,
    const FetcherConfig& config,
    std::vector<history::AnnotatedVisit> annotated_visits) {
  std::map<std::string, URLVisitAggregate::HistoryData> url_annotations;

  base::Time::Exploded time_exploded;
  config.clock->Now().LocalExplode(&time_exploded);
  DayGroup current_day_group = GetDayGroupForExplodedTime(time_exploded);
  TimeGroup current_time_group = GetTimeGroupForExplodedTime(time_exploded);
  for (auto& annotated_visit : annotated_visits) {
    // The `originator_cache_guid` field is only set for foreign session visits.
    Source current_visit_source =
        annotated_visit.visit_row.originator_cache_guid.empty()
            ? Source::kLocal
            : Source::kForeign;
    if (!base::Contains(requested_fetch_sources, current_visit_source)) {
      continue;
    }

    auto url_key = ComputeURLMergeKey(annotated_visit.url_row.url(),
                                      annotated_visit.url_row.title(),
                                      config.deduplication_helper);
    if (url_annotations.find(url_key) == url_annotations.end()) {
      // `GetAnnotatedVisits` returns a reverse-chronological sorted list of
      // annotated visits, thus, the first visit in the vector is the last
      // active (i.e. most recent) visit for a given URL.
      url_annotations.emplace(url_key, std::move(annotated_visit));
    } else {
      auto& history = url_annotations.at(url_key);
      history.visit_count += 1;
      if (annotated_visit.context_annotations.total_foreground_duration
              .InMilliseconds() > 0) {
        history.total_foreground_duration +=
            annotated_visit.context_annotations.total_foreground_duration;
      }

      if (!history.last_app_id.has_value() &&
          annotated_visit.visit_row.app_id.has_value()) {
        history.last_app_id = annotated_visit.visit_row.app_id;
      }

      if (history.last_visited.content_annotations.model_annotations
                  .visibility_score ==
              history::VisitContentModelAnnotations::kDefaultVisibilityScore &&
          annotated_visit.content_annotations.model_annotations
                  .visibility_score !=
              history::VisitContentModelAnnotations::kDefaultVisibilityScore) {
        history.last_visited.content_annotations.model_annotations
            .visibility_score = annotated_visit.content_annotations
                                    .model_annotations.visibility_score;
      }

      // TODO(crbug.com/340885723): Wire `in_cluster` signal.
      // TODO(crbug.com/340887237): Wire `interaction_state` signal.
    }

    auto& history = url_annotations.at(url_key);
    base::Time::Exploded visit_time_exploded;
    history.last_visited.visit_row.visit_time.LocalExplode(
        &visit_time_exploded);
    if (GetDayGroupForExplodedTime(visit_time_exploded) == current_day_group) {
      history.same_day_group_visit_count++;
    }
    if (GetTimeGroupForExplodedTime(visit_time_exploded) ==
        current_time_group) {
      history.same_time_group_visit_count++;
    }
  }

  std::map<URLMergeKey, URLVisitVariant> url_visit_variant_map;
  std::transform(
      std::make_move_iterator(url_annotations.begin()),
      std::make_move_iterator(url_annotations.end()),
      std::inserter(url_visit_variant_map, url_visit_variant_map.end()),
      [](auto kv) { return std::make_pair(kv.first, std::move(kv.second)); });

  std::move(callback).Run(
      {FetchResult::Status::kSuccess, std::move(url_visit_variant_map)});
}

}  // namespace visited_url_ranking
