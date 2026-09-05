#include <SpaceGame/levels/LevelUnlock.h>

#include <CQTest.h>

namespace {
auto completed_criterion(TCHAR const* const level_id) -> ml::FLevelUnlockCriterion {
    return ml::FLevelUnlockCriterion{
        TInPlaceType<ml::FLevelCompletedUnlockCriterion>{},
        ml::FLevelCompletedUnlockCriterion{.level_id = ml::FLevelId{FName{level_id}}}};
}

auto make_evaluator(TSet<ml::FLevelId> const& completed_levels) -> ml::FLevelUnlockEvaluator {
    return ml::FLevelUnlockEvaluator{
        [&completed_levels](ml::FLevelId const id) { return completed_levels.Contains(id); },
        [](ml::FLevelId const id) {
            if (id.value == TEXT("asteroid-field")) {
                return FText::FromString(TEXT("Asteroid Field"));
            }
            return FText::FromName(id.value);
        },
    };
}
}

TEST_CLASS(LevelUnlock, "Sandbox.UnitTests")
{
    TEST_METHOD(LevelWithoutCriteriaIsUnlocked)
    {
        TSet<ml::FLevelId> const completed_levels;
        auto const evaluator{make_evaluator(completed_levels)};
        ml::FLevelDefinition const definition;

        auto const status{evaluator.evaluate(definition)};
        TestRunner->TestTrue(TEXT("Level is unlocked"), status.unlocked);
        TestRunner->TestTrue(TEXT("There are no criterion statuses"), status.criteria.IsEmpty());
    }

    TEST_METHOD(CompletionCriterionTracksProgression)
    {
        ml::FLevelDefinition definition;
        definition.unlock_criteria.Add(completed_criterion(TEXT("asteroid-field")));

        TSet<ml::FLevelId> const incomplete;
        auto const before{make_evaluator(incomplete).evaluate(definition)};
        TestRunner->TestFalse(TEXT("Prerequisite initially fails"), before.unlocked);
        if (TestRunner->TestEqual(TEXT("One criterion is reported"), before.criteria.Num(), 1)) {
            TestRunner->TestFalse(TEXT("Criterion is unsatisfied"), before.criteria[0].satisfied);
            TestRunner->TestEqual(TEXT("Description uses the referenced title"),
                                  before.criteria[0].description.ToString(),
                                  FString{TEXT("Complete Asteroid Field")});
        }

        TSet<ml::FLevelId> const completed{
            ml::FLevelId{FName{TEXT("asteroid-field")}},
        };
        auto const after{make_evaluator(completed).evaluate(definition)};
        TestRunner->TestTrue(TEXT("Completed prerequisite passes"), after.unlocked);
        TestRunner->TestTrue(TEXT("Criterion is satisfied"), after.criteria[0].satisfied);
    }

    TEST_METHOD(MultipleCriteriaMustAllPass)
    {
        ml::FLevelDefinition definition;
        definition.unlock_criteria.Add(completed_criterion(TEXT("first")));
        definition.unlock_criteria.Add(completed_criterion(TEXT("second")));

        TSet<ml::FLevelId> const one_completed{
            ml::FLevelId{FName{TEXT("first")}},
        };
        TestRunner->TestFalse(TEXT("One of two completed is locked"),
                              make_evaluator(one_completed).evaluate(definition).unlocked);

        TSet<ml::FLevelId> const both_completed{
            ml::FLevelId{FName{TEXT("first")}},
            ml::FLevelId{FName{TEXT("second")}},
        };
        TestRunner->TestTrue(TEXT("Both completed is unlocked"),
                             make_evaluator(both_completed).evaluate(definition).unlocked);
    }
};
