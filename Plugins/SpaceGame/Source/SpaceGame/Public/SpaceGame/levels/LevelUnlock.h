#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FLevelUnlockCriterionStatus {
    bool satisfied{};
    FText description{};
};

struct SPACEGAME_API FLevelUnlockStatus {
    bool unlocked{true};
    TArray<FLevelUnlockCriterionStatus> criteria{};
};

class SPACEGAME_API FLevelUnlockEvaluator final {
  public:
    using FCompletionQuery = TFunction<bool(FLevelId)>;
    using FTitleQuery = TFunction<FText(FLevelId)>;

    FLevelUnlockEvaluator(FCompletionQuery completion_query, FTitleQuery title_query);

    [[nodiscard]] auto evaluate(FLevelDefinition const& definition) const -> FLevelUnlockStatus;
    [[nodiscard]] auto evaluate(FLevelUnlockCriterion const& criterion) const
        -> FLevelUnlockCriterionStatus;
  private:
    FCompletionQuery completion_query_{};
    FTitleQuery title_query_{};
};
}
