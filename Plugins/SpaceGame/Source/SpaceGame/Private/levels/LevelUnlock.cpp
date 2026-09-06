#include "SpaceGame/levels/LevelUnlock.h"

namespace ml {
FLevelUnlockEvaluator::FLevelUnlockEvaluator(FCompletionQuery completion_query,
                                             FTitleQuery title_query,
                                             bool const force_unlocked)
    : completion_query_{MoveTemp(completion_query)}
    , title_query_{MoveTemp(title_query)}
    , force_unlocked_{force_unlocked} {}

auto FLevelUnlockEvaluator::evaluate(FLevelDefinition const& definition) const
    -> FLevelUnlockStatus {
    FLevelUnlockStatus status;
    status.criteria.Reserve(definition.unlock_criteria.Num());
    for (auto const& criterion : definition.unlock_criteria) {
        auto criterion_status{evaluate(criterion)};
        status.unlocked = status.unlocked && criterion_status.satisfied;
        status.criteria.Add(MoveTemp(criterion_status));
    }
    status.unlocked = force_unlocked_ || status.unlocked;
    return status;
}

auto FLevelUnlockEvaluator::evaluate(FLevelUnlockCriterion const& criterion) const
    -> FLevelUnlockCriterionStatus {
    auto const& completed{criterion.Get<FLevelCompletedUnlockCriterion>()};
    auto title{title_query_(completed.level_id)};
    if (title.IsEmpty()) {
        title = FText::FromName(completed.level_id.value);
    }
    return {
        .satisfied = completion_query_(completed.level_id),
        .description =
            FText::Format(NSLOCTEXT("LevelUnlock", "CompleteLevel", "Complete {0}"), title),
    };
}
}
