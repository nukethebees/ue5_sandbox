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
    TEST_METHOD(BuilderConstructsValidSoALevel)
    {
        auto const definition{ml::example_levels::make_native_example()};
        auto const validation{ml::validate_level(definition)};

        TestRunner->TestTrue(TEXT("Definition is valid"), static_cast<bool>(validation));
        TestRunner->TestEqual(TEXT("Definition has two teams"), definition.teams.Num(), 2);
        TestRunner->TestEqual(
            TEXT("Definition has four entity rows"), definition.entities.num(), 4);
        TestRunner->TestEqual(
            TEXT("Title is preserved"), definition.metadata.title, FString{TEXT("Native Example")});
        TestRunner->TestTrue(TEXT("Player id is preserved"),
                             definition.player_entity_id ==
                                 ml::FLevelEntityId{FName{TEXT("player")}});
        TestRunner->TestTrue(TEXT("Player archetype is preserved"),
                             definition.entities.archetypes[0] ==
                                 ml::level_archetypes::player_fighter);
        TestRunner->TestTrue(TEXT("Player team is preserved"),
                             definition.entities.teams[0] == ml::level_teams::blue);
        TestRunner->TestTrue(TEXT("Player position is preserved"),
                             definition.entities.positions.get_const_view()[0].Equals(
                                 FVector{0.0, -25000.0, 1000.0}));
        TestRunner->TestTrue(TEXT("Turret archetype is preserved"),
                             definition.entities.archetypes[3] ==
                                 ml::level_archetypes::static_turret);
    }

    TEST_METHOD(UnknownTeamReferenceFailsValidation)
    {
        auto definition{ml::example_levels::make_native_example()};
        definition.entities.teams[1] = ml::level_teams::green;

        auto const validation{ml::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Unknown team is reported"),
            contains_error(validation, ml::ELevelValidationErrorCode::UnknownTeamReference));
    }

    TEST_METHOD(BuilderReplacesPlayerAndCanBeReused)
    {
        ml::FLevelBuilder builder;
        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("First")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::blue});
        builder.set_player(ml::FPlayerDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("first-player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::blue,
        });
        builder.set_player(ml::FPlayerDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("replacement-player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::blue,
            .position = FVector{10.0, 20.0, 30.0},
        });

        auto const first{builder.finish()};
        TestRunner->TestTrue(TEXT("Replacement definition is valid"),
                             static_cast<bool>(ml::validate_level(first)));
        TestRunner->TestEqual(TEXT("Player replacement keeps one row"), first.entities.num(), 1);
        TestRunner->TestTrue(TEXT("Player replacement updates the authored id"),
                             first.player_entity_id ==
                                 ml::FLevelEntityId{FName{TEXT("replacement-player")}});
        TestRunner->TestTrue(
            TEXT("Player replacement updates the position"),
            first.entities.positions.get_const_view()[0].Equals(FVector{10.0, 20.0, 30.0}));

        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("Second")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::red});
        builder.set_player(ml::FPlayerDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("second-player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::red,
        });
        auto const second{builder.finish()};

        TestRunner->TestTrue(TEXT("Reused builder produces a valid definition"),
                             static_cast<bool>(ml::validate_level(second)));
        TestRunner->TestEqual(
            TEXT("Reused builder starts with empty entity storage"), second.entities.num(), 1);
        TestRunner->TestEqual(TEXT("Reused builder replaces metadata"),
                              second.metadata.title,
                              FString{TEXT("Second")});
    }

    TEST_METHOD(DuplicateAuthoredEntityIdFailsValidation)
    {
        auto definition{ml::example_levels::make_native_example()};
        definition.entities.ids[1] = definition.player_entity_id;

        auto const validation{ml::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Duplicate entity id is reported"),
            contains_error(validation, ml::ELevelValidationErrorCode::DuplicateEntityId));
    }

    TEST_METHOD(MalformedDefinitionsReportStructuredErrors)
    {
        auto missing_player{ml::example_levels::make_native_example()};
        missing_player.player_entity_id = {};
        auto const missing_player_validation{ml::validate_level(missing_player)};
        TestRunner->TestTrue(TEXT("Missing player is reported"),
                             contains_error(missing_player_validation,
                                            ml::ELevelValidationErrorCode::MissingPlayer));

        auto unsupported_archetype{ml::example_levels::make_native_example()};
        unsupported_archetype.entities.archetypes[1] =
            ml::FEntityArchetypeId{FName{TEXT("pirate-fighter")}};
        auto const unsupported_archetype_validation{ml::validate_level(unsupported_archetype)};
        TestRunner->TestTrue(TEXT("Unsupported archetype is reported"),
                             contains_error(unsupported_archetype_validation,
                                            ml::ELevelValidationErrorCode::UnsupportedArchetype));

        auto invalid_placement{ml::example_levels::make_native_example()};
        invalid_placement.entities.positions.xs[1] = std::numeric_limits<double>::quiet_NaN();
        auto const invalid_placement_validation{ml::validate_level(invalid_placement)};
        TestRunner->TestTrue(TEXT("Invalid placement is reported"),
                             contains_error(invalid_placement_validation,
                                            ml::ELevelValidationErrorCode::InvalidPlacement));

        auto mismatched_columns{ml::example_levels::make_native_example()};
        mismatched_columns.entities.teams.Pop();
        auto const mismatched_columns_validation{ml::validate_level(mismatched_columns)};
        TestRunner->TestTrue(
            TEXT("Mismatched columns are reported"),
            contains_error(mismatched_columns_validation,
                           ml::ELevelValidationErrorCode::MismatchedEntityColumns));
    }
};
