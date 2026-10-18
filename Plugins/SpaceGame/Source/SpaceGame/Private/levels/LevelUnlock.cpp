#include "SpaceGame/levels/LevelUnlock.h"

#include <SpaceGame/levels/native_level_definition_conversion.h>

namespace ioj {
FLevelUnlockEvaluator::FLevelUnlockEvaluator(FCompletionQuery completion_query,
                                             FTitleQuery title_query,
                                             bool const force_unlocked)
    : completion_query_{MoveTemp(completion_query)}
    , title_query_{MoveTemp(title_query)}
    , force_unlocked_{force_unlocked} {}

auto FLevelUnlockEvaluator::evaluate(levels::LevelDefinition const& definition) const
    -> FLevelUnlockStatus {
    FLevelUnlockStatus status;
    status.criteria.Reserve(static_cast<int32>(definition.unlock_level_ids().size()));
    for (auto const& id : definition.unlock_level_ids()) {
        FLevelUnlockCriterion criterion{
            TInPlaceType<FLevelCompletedUnlockCriterion>{},
            FLevelCompletedUnlockCriterion{.level_id = levels::authoring::to_unreal(id)}};
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
