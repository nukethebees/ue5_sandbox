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
    auto const eligibility{profiles::evaluate_unlock(definition, [this](levels::LevelId const& id) {
        return completion_query_(levels::authoring::to_unreal(id));
    })};
    FLevelUnlockStatus status;
    status.unlocked = eligibility.unlocked(force_unlocked_);
    status.criteria.Reserve(static_cast<int32>(eligibility.criteria.size()));
    for (auto const& criterion : eligibility.criteria) {
        auto const id{levels::authoring::to_unreal(criterion.level_id)};
        auto title{title_query_(id)};
        if (title.IsEmpty()) {
            title = FText::FromName(id.value);
        }
        status.criteria.Add(
            {.satisfied = criterion.satisfied,
             .description =
                 FText::Format(NSLOCTEXT("LevelUnlock", "CompleteLevel", "Complete {0}"), title)});
    }
    return status;
}
}
