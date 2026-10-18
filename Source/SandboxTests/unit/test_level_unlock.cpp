#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelUnlock.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

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
        auto const definition{ioj::example_levels::make_example_draft()};

        auto const status{
            evaluator.evaluate(ioj::levels::authoring::to_native(definition).value())};
        TestRunner->TestTrue(TEXT("Level is unlocked"), status.unlocked);
        TestRunner->TestTrue(TEXT("There are no criterion statuses"), status.criteria.IsEmpty());
    }

    TEST_METHOD(CompletionCriterionTracksProgression)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.unlock_criteria.Add(completed_criterion(TEXT("asteroid-field")));

        TSet<ioj::FLevelId> const incomplete;
        auto const before{make_evaluator(incomplete)
                              .evaluate(ioj::levels::authoring::to_native(definition).value())};
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
        auto const after{make_evaluator(completed).evaluate(
            ioj::levels::authoring::to_native(definition).value())};
        TestRunner->TestTrue(TEXT("Completed prerequisite passes"), after.unlocked);
        TestRunner->TestTrue(TEXT("Criterion is satisfied"), after.criteria[0].satisfied);
    }

    TEST_METHOD(MultipleCriteriaMustAllPass)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.unlock_criteria.Add(completed_criterion(TEXT("first")));
        definition.unlock_criteria.Add(completed_criterion(TEXT("second")));

        TSet<ioj::FLevelId> const one_completed{
            ioj::FLevelId{FName{TEXT("first")}},
        };
        TestRunner->TestFalse(TEXT("One of two completed is locked"),
                              make_evaluator(one_completed)
                                  .evaluate(ioj::levels::authoring::to_native(definition).value())
                                  .unlocked);

        TSet<ioj::FLevelId> const both_completed{
            ioj::FLevelId{FName{TEXT("first")}},
            ioj::FLevelId{FName{TEXT("second")}},
        };
        TestRunner->TestTrue(TEXT("Both completed is unlocked"),
                             make_evaluator(both_completed)
                                 .evaluate(ioj::levels::authoring::to_native(definition).value())
                                 .unlocked);
    }

    TEST_METHOD(AccessOverridePreservesRealCriterionStatus)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.unlock_criteria.Add(completed_criterion(TEXT("asteroid-field")));
        TSet<ioj::FLevelId> const incomplete;

        auto const overridden{make_evaluator(incomplete, true)
                                  .evaluate(ioj::levels::authoring::to_native(definition).value())};
        TestRunner->TestTrue(TEXT("Access override unlocks the level"), overridden.unlocked);
        if (TestRunner->TestEqual(
                TEXT("Real criterion is still reported"), overridden.criteria.Num(), 1)) {
            TestRunner->TestFalse(TEXT("Real criterion remains unsatisfied"),
                                  overridden.criteria[0].satisfied);
        }

        auto const restored{make_evaluator(incomplete, false)
                                .evaluate(ioj::levels::authoring::to_native(definition).value())};
        TestRunner->TestFalse(TEXT("Disabling the override restores real access"),
                              restored.unlocked);
    }
};
