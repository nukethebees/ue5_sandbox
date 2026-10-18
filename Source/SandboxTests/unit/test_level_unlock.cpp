#include <SpaceGame/levels/LevelUnlock.h>

#include <CQTest.h>

namespace {
auto completed_criterion(TCHAR const* const level_id) -> ioj::FLevelUnlockCriterion {
    return ioj::FLevelUnlockCriterion{
        TInPlaceType<ioj::FLevelCompletedUnlockCriterion>{},
        ioj::FLevelCompletedUnlockCriterion{.level_id = ioj::FLevelId{FName{level_id}}}};
}

auto make_evaluator(TSet<ioj::FLevelId> const& completed_levels, bool const force_unlocked = false)
    -> ioj::FLevelUnlockEvaluator {
    return ioj::FLevelUnlockEvaluator{
        [&completed_levels](ioj::FLevelId const id) { return completed_levels.Contains(id); },
        [](ioj::FLevelId const id) {
            if (id.value == TEXT("asteroid-field")) {
                return FText::FromString(TEXT("Asteroid Field"));
            }
            return FText::FromName(id.value);
        },
        force_unlocked,
    };
}
}

TEST_CLASS(LevelUnlock, "Sandbox.UnitTests")
{
    TEST_METHOD(LevelWithoutCriteriaIsUnlocked)
    {
        TSet<ioj::FLevelId> const completed_levels;
        auto const evaluator{make_evaluator(completed_levels)};
        ioj::FLevelDefinition const definition;

        auto const status{evaluator.evaluate(definition)};
        TestRunner->TestTrue(TEXT("Level is unlocked"), status.unlocked);
        TestRunner->TestTrue(TEXT("There are no criterion statuses"), status.criteria.IsEmpty());
    }

    TEST_METHOD(CompletionCriterionTracksProgression)
    {
        ioj::FLevelDefinition definition;
        definition.unlock_criteria.Add(completed_criterion(TEXT("asteroid-field")));

        TSet<ioj::FLevelId> const incomplete;
        auto const before{make_evaluator(incomplete).evaluate(definition)};
        TestRunner->TestFalse(TEXT("Prerequisite initially fails"), before.unlocked);
        if (TestRunner->TestEqual(TEXT("One criterion is reported"), before.criteria.Num(), 1)) {
            TestRunner->TestFalse(TEXT("Criterion is unsatisfied"), before.criteria[0].satisfied);
            TestRunner->TestEqual(TEXT("Description uses the referenced title"),
                                  before.criteria[0].description.ToString(),
                                  FString{TEXT("Complete Asteroid Field")});
        }

        TSet<ioj::FLevelId> const completed{
            ioj::FLevelId{FName{TEXT("asteroid-field")}},
        };
        auto const after{make_evaluator(completed).evaluate(definition)};
        TestRunner->TestTrue(TEXT("Completed prerequisite passes"), after.unlocked);
        TestRunner->TestTrue(TEXT("Criterion is satisfied"), after.criteria[0].satisfied);
    }

    TEST_METHOD(MultipleCriteriaMustAllPass)
    {
        ioj::FLevelDefinition definition;
        definition.unlock_criteria.Add(completed_criterion(TEXT("first")));
        definition.unlock_criteria.Add(completed_criterion(TEXT("second")));

        TSet<ioj::FLevelId> const one_completed{
            ioj::FLevelId{FName{TEXT("first")}},
        };
        TestRunner->TestFalse(TEXT("One of two completed is locked"),
                              make_evaluator(one_completed).evaluate(definition).unlocked);

        TSet<ioj::FLevelId> const both_completed{
            ioj::FLevelId{FName{TEXT("first")}},
            ioj::FLevelId{FName{TEXT("second")}},
        };
        TestRunner->TestTrue(TEXT("Both completed is unlocked"),
                             make_evaluator(both_completed).evaluate(definition).unlocked);
    }

    TEST_METHOD(AccessOverridePreservesRealCriterionStatus)
    {
        ioj::FLevelDefinition definition;
        definition.unlock_criteria.Add(completed_criterion(TEXT("asteroid-field")));
        TSet<ioj::FLevelId> const incomplete;

        auto const overridden{make_evaluator(incomplete, true).evaluate(definition)};
        TestRunner->TestTrue(TEXT("Access override unlocks the level"), overridden.unlocked);
        if (TestRunner->TestEqual(
                TEXT("Real criterion is still reported"), overridden.criteria.Num(), 1)) {
            TestRunner->TestFalse(TEXT("Real criterion remains unsatisfied"),
                                  overridden.criteria[0].satisfied);
        }

        auto const restored{make_evaluator(incomplete, false).evaluate(definition)};
        TestRunner->TestFalse(TEXT("Disabling the override restores real access"),
                              restored.unlocked);
    }
};
