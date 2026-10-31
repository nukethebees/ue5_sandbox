#pragma once

#include <ioj/profiles/progression.h>
#include <SpaceGameSimulation/levels/LevelTypes.h>

#include <CoreMinimal.h>

namespace ioj {
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

    FLevelUnlockEvaluator(FCompletionQuery completion_query,
                          FTitleQuery title_query,
                          bool force_unlocked = false);

    [[nodiscard]] auto evaluate(levels::LevelDefinition const& definition) const
        -> FLevelUnlockStatus;
  private:
    FCompletionQuery completion_query_{};
    FTitleQuery title_query_{};
    bool force_unlocked_{};
};
}
