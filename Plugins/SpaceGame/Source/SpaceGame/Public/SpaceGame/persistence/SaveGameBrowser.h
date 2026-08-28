#pragma once

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <CoreMinimal.h>
#include <Templates/Function.h>

namespace ml::ioj {
struct SPACEGAME_API FSaveGameSummary {
    FString save_id{};
    FString display_name{};
    FDateTime timestamp{};
    FName scenario_name{};
    float simulation_duration_seconds{};
    int32 score{};
    FString result{};
};

struct SPACEGAME_API FSaveGameBrowser {
    using FSummarySource = TFunction<TArray<FSaveGameSummary>()>;

    FSaveGameBrowser() = default;
    explicit FSaveGameBrowser(FSummarySource summary_source);

    void refresh();

    [[nodiscard]] auto get_summaries() const -> TConstArrayView<FSaveGameSummary>;
  private:
    FSummarySource summary_source_{};
    TArray<FSaveGameSummary> summaries_{};
};
}
