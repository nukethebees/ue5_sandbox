#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelDefinition.h>

#include <CQTest.h>
#include <limits>

namespace {
auto contains_error(ml::FLevelValidationResult const& result,
                    ml::ELevelValidationErrorCode const code) -> bool {
    return result.errors.ContainsByPredicate(
        [code](ml::FLevelValidationError const& error) { return error.code == code; });
}
}

TEST_CLASS(LevelDefinition, "Sandbox.UnitTests")
{
    TEST_METHOD(BuilderConstructsValidLevel)
    {
        auto const definition{ml::example_levels::make_native_example()};
        auto const validation{ml::validate_level(definition)};

        TestRunner->TestTrue(TEXT("Definition is valid"), static_cast<bool>(validation));
        TestRunner->TestEqual(TEXT("Definition has two teams"), definition.teams.Num(), 2);
        TestRunner->TestEqual(TEXT("Definition has three entities"), definition.entities.Num(), 3);
        if (!TestRunner->TestTrue(TEXT("Definition has a player"), definition.player.IsSet())) {
            return;
        }

        auto const& player{definition.player.GetValue()};
        TestRunner->TestTrue(TEXT("Player archetype is preserved"),
                             player.archetype == ml::level_archetypes::player_fighter);
        TestRunner->TestTrue(TEXT("Player team is preserved"),
                             player.team == ml::level_teams::blue);
        TestRunner->TestTrue(TEXT("Player position is preserved"),
                             player.position.Equals(FVector{100.0, 200.0, 300.0}));
        TestRunner->TestTrue(TEXT("Turret archetype is preserved"),
                             definition.entities[2].archetype ==
                                 ml::level_archetypes::static_turret);
    }

    TEST_METHOD(UnknownTeamReferenceFailsValidation)
    {
        auto definition{ml::example_levels::make_native_example()};
        definition.entities[0].team = ml::level_teams::green;

        auto const validation{ml::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Unknown team is reported"),
            contains_error(validation, ml::ELevelValidationErrorCode::UnknownTeamReference));
    }

    TEST_METHOD(DuplicateAuthoredEntityIdFailsValidation)
    {
        auto definition{ml::example_levels::make_native_example()};
        definition.entities[0].id = definition.player.GetValue().id;

        auto const validation{ml::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Duplicate entity id is reported"),
            contains_error(validation, ml::ELevelValidationErrorCode::DuplicateEntityId));
    }

    TEST_METHOD(MalformedDefinitionsReportStructuredErrors)
    {
        auto missing_player{ml::example_levels::make_native_example()};
        missing_player.player.Reset();
        auto const missing_player_validation{ml::validate_level(missing_player)};
        TestRunner->TestTrue(TEXT("Missing player is reported"),
                             contains_error(missing_player_validation,
                                            ml::ELevelValidationErrorCode::MissingPlayer));

        auto unsupported_archetype{ml::example_levels::make_native_example()};
        unsupported_archetype.entities[0].archetype =
            ml::FEntityArchetypeId{FName{TEXT("pirate-fighter")}};
        auto const unsupported_archetype_validation{ml::validate_level(unsupported_archetype)};
        TestRunner->TestTrue(TEXT("Unsupported archetype is reported"),
                             contains_error(unsupported_archetype_validation,
                                            ml::ELevelValidationErrorCode::UnsupportedArchetype));

        auto invalid_placement{ml::example_levels::make_native_example()};
        invalid_placement.entities[0].position.X = std::numeric_limits<double>::quiet_NaN();
        auto const invalid_placement_validation{ml::validate_level(invalid_placement)};
        TestRunner->TestTrue(TEXT("Invalid placement is reported"),
                             contains_error(invalid_placement_validation,
                                            ml::ELevelValidationErrorCode::InvalidPlacement));
    }
};
