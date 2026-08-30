#pragma once

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <Misc/Optional.h>
#include <Templates/Function.h>

namespace ml::ioj {
struct SPACEGAME_API FLevelOutcomeStatistic {
    FString label{};
    FString value{};
};

struct SPACEGAME_API FLevelOutcomeSummary {
    FString outcome_id{};
    FString display_name{};
    FDateTime completed_at{};
    float simulation_duration_seconds{};
    int32 kills{};
    FString result{};
    TArray<FLevelOutcomeStatistic> statistics{};
};

struct SPACEGAME_API FSaveProfileSummary {
    FString profile_id{};
    FString display_name{};
    FDateTime created_at{};
    FDateTime last_played_at{};
    float total_simulation_duration_seconds{};
    int32 total_kills{};
    int32 outcome_count{};
    bool active{};
};

struct SPACEGAME_API FSaveProfileReport {
    FString profile_id{};
    TArray<FLevelOutcomeSummary> outcomes{};
};

struct SPACEGAME_API FSaveGameBrowser {
    using FSummarySource = TFunction<TArray<FSaveProfileSummary>()>;
    using FReportSource = TFunction<TOptional<FSaveProfileReport>(FString const&)>;

    FSaveGameBrowser() = default;
    FSaveGameBrowser(FSummarySource summary_source, FReportSource report_source);

    void refresh();
    bool load_profile_report(FString const& profile_id);

    [[nodiscard]] auto get_summaries() const -> TConstArrayView<FSaveProfileSummary>;
    [[nodiscard]] auto get_loaded_profile_report() const -> FSaveProfileReport const*;
  private:
    FSummarySource summary_source_{};
    FReportSource report_source_{};
    TArray<FSaveProfileSummary> summaries_{};
    TOptional<FSaveProfileReport> loaded_report_{};
};
}
