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

auto make_camera_level() -> ml::FLevelDefinition {
    ml::FLevelBuilder builder;
    builder.set_metadata(ml::FLevelMetadata{.title = TEXT("Camera Level")});
    builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::blue});
    builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::red});
    builder.add_entity(ml::FEntitySpawnDefinition{
        .id = ml::FLevelEntityId{FName{TEXT("blue-capital")}},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::blue,
        .position = FVector{-1000.0, 0.0, 0.0},
    });
    builder.add_entity(ml::FEntitySpawnDefinition{
        .id = ml::FLevelEntityId{FName{TEXT("red-capital")}},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::red,
        .position = FVector{1000.0, 0.0, 0.0},
    });
    builder.set_camera(ml::FLevelCameraDefinition{
        .target_entity_ids = {ml::FLevelEntityId{FName{TEXT("blue-capital")}},
                              ml::FLevelEntityId{FName{TEXT("red-capital")}}},
        .offset_direction = FVector{-1.0, -1.0, 0.5},
        .distance = 10000.0,
    });
    return builder.finish();
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

    TEST_METHOD(BuilderConstructsValidPlayerlessCameraLevel)
    {
        auto const definition{make_camera_level()};
        auto const validation{ml::validate_level(definition)};

        TestRunner->TestTrue(TEXT("Camera definition is valid"), static_cast<bool>(validation));
        TestRunner->TestFalse(TEXT("Camera definition has no player"),
                              definition.player_entity_id.is_set());
        if (!TestRunner->TestTrue(TEXT("Camera is preserved"), definition.camera.IsSet())) {
            return;
        }

        auto const& camera{definition.camera.GetValue()};
        TestRunner->TestEqual(TEXT("Camera has two targets"), camera.target_entity_ids.Num(), 2);
        TestRunner->TestTrue(TEXT("Camera offset direction is preserved"),
                             camera.offset_direction.Equals(FVector{-1.0, -1.0, 0.5}));
        TestRunner->TestEqual(TEXT("Camera distance is preserved"), camera.distance, 10000.0);
    }

    TEST_METHOD(CameraCanTargetOneEntity)
    {
        auto definition{make_camera_level()};
        definition.camera->target_entity_ids.SetNum(1);

        auto const validation{ml::validate_level(definition)};
        TestRunner->TestTrue(TEXT("Single-target camera is valid"), static_cast<bool>(validation));
    }

    TEST_METHOD(BuilderCanBeReused)
    {
        ml::FLevelBuilder builder;
        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("First")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::blue});
        auto const first_player{builder.add_entity(ml::FEntitySpawnDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("first-player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::blue,
            .position = FVector{10.0, 20.0, 30.0},
        })};
        builder.set_player_entity(first_player);

        auto const first{builder.finish()};
        TestRunner->TestTrue(TEXT("First definition is valid"),
                             static_cast<bool>(ml::validate_level(first)));
        TestRunner->TestEqual(TEXT("First definition has one row"), first.entities.num(), 1);
        TestRunner->TestTrue(TEXT("Player references the authored entity"),
                             first.player_entity_id == first_player);
        TestRunner->TestTrue(
            TEXT("Player entity position is preserved"),
            first.entities.positions.get_const_view()[0].Equals(FVector{10.0, 20.0, 30.0}));

        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("Second")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::red});
        auto const second_player{builder.add_entity(ml::FEntitySpawnDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("second-player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::red,
        })};
        builder.set_player_entity(second_player);
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

    TEST_METHOD(BuilderReuseClearsCameraState)
    {
        ml::FLevelBuilder builder;
        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("Camera")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::blue});
        builder.add_entity(ml::FEntitySpawnDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("capital")}},
            .archetype = ml::level_archetypes::capital_ship,
            .team = ml::level_teams::blue,
        });
        builder.set_camera(ml::FLevelCameraDefinition{
            .target_entity_ids = {ml::FLevelEntityId{FName{TEXT("capital")}}},
            .offset_direction = FVector{-1.0, 0.0, 0.0},
            .distance = 1000.0,
        });
        auto const camera_level{builder.finish()};
        TestRunner->TestTrue(TEXT("First definition has a camera"), camera_level.camera.IsSet());

        builder.set_metadata(ml::FLevelMetadata{.title = TEXT("Player")});
        builder.add_team(ml::FTeamDefinition{.id = ml::level_teams::blue});
        auto const player_id{builder.add_entity(ml::FEntitySpawnDefinition{
            .id = ml::FLevelEntityId{FName{TEXT("player")}},
            .archetype = ml::level_archetypes::player_fighter,
            .team = ml::level_teams::blue,
        })};
        builder.set_player_entity(player_id);
        auto const player_level{builder.finish()};

        TestRunner->TestFalse(TEXT("Reused builder clears the camera"),
                              player_level.camera.IsSet());
        TestRunner->TestTrue(TEXT("Reused builder definition is valid"),
                             static_cast<bool>(ml::validate_level(player_level)));
    }

    TEST_METHOD(MalformedDefinitionsReportStructuredErrors)
    {
        auto missing_player{ml::example_levels::make_native_example()};
        missing_player.player_entity_id = {};
        auto const missing_player_validation{ml::validate_level(missing_player)};
        TestRunner->TestTrue(TEXT("Missing viewpoint is reported"),
                             contains_error(missing_player_validation,
                                            ml::ELevelValidationErrorCode::MissingViewpoint));

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

    TEST_METHOD(CameraAndPlayerAreMutuallyExclusive)
    {
        auto both{ml::example_levels::make_native_example()};
        both.camera = make_camera_level().camera;
        auto const both_validation{ml::validate_level(both)};
        TestRunner->TestTrue(
            TEXT("Player and camera conflict is reported"),
            contains_error(both_validation, ml::ELevelValidationErrorCode::ConflictingViewpoints));

        auto neither{make_camera_level()};
        neither.camera.Reset();
        auto const neither_validation{ml::validate_level(neither)};
        TestRunner->TestTrue(
            TEXT("Missing viewpoint is reported"),
            contains_error(neither_validation, ml::ELevelValidationErrorCode::MissingViewpoint));
    }

    TEST_METHOD(CameraTargetsMustBeUniqueDeclaredEntities)
    {
        auto duplicate{make_camera_level()};
        duplicate.camera->target_entity_ids[1] = duplicate.camera->target_entity_ids[0];
        auto const duplicate_validation{ml::validate_level(duplicate)};
        TestRunner->TestTrue(TEXT("Duplicate camera target is reported"),
                             contains_error(duplicate_validation,
                                            ml::ELevelValidationErrorCode::DuplicateCameraTarget));

        auto unknown{make_camera_level()};
        unknown.camera->target_entity_ids[0] = ml::FLevelEntityId{FName{TEXT("missing-capital")}};
        auto const unknown_validation{ml::validate_level(unknown)};
        TestRunner->TestTrue(TEXT("Unknown camera target is reported"),
                             contains_error(unknown_validation,
                                            ml::ELevelValidationErrorCode::CameraTargetNotFound));

        auto empty{make_camera_level()};
        empty.camera->target_entity_ids.Reset();
        auto const empty_validation{ml::validate_level(empty)};
        TestRunner->TestTrue(
            TEXT("Missing camera target is reported"),
            contains_error(empty_validation, ml::ELevelValidationErrorCode::MissingCameraTarget));
    }

    TEST_METHOD(CameraPlacementMustBeFiniteAndNonZero)
    {
        auto invalid_distance{make_camera_level()};
        invalid_distance.camera->distance = 0.0;
        auto const distance_validation{ml::validate_level(invalid_distance)};
        TestRunner->TestTrue(TEXT("Invalid camera distance is reported"),
                             contains_error(distance_validation,
                                            ml::ELevelValidationErrorCode::InvalidCameraDistance));

        auto invalid_direction{make_camera_level()};
        invalid_direction.camera->offset_direction = FVector::ZeroVector;
        auto const direction_validation{ml::validate_level(invalid_direction)};
        TestRunner->TestTrue(
            TEXT("Invalid camera direction is reported"),
            contains_error(direction_validation,
                           ml::ELevelValidationErrorCode::InvalidCameraOffsetDirection));

        auto non_finite_distance{make_camera_level()};
        non_finite_distance.camera->distance = std::numeric_limits<double>::infinity();
        auto const non_finite_distance_validation{ml::validate_level(non_finite_distance)};
        TestRunner->TestTrue(TEXT("Non-finite camera distance is reported"),
                             contains_error(non_finite_distance_validation,
                                            ml::ELevelValidationErrorCode::InvalidCameraDistance));

        auto non_finite_direction{make_camera_level()};
        non_finite_direction.camera->offset_direction.X = std::numeric_limits<double>::quiet_NaN();
        auto const non_finite_direction_validation{ml::validate_level(non_finite_direction)};
        TestRunner->TestTrue(
            TEXT("Non-finite camera direction is reported"),
            contains_error(non_finite_direction_validation,
                           ml::ELevelValidationErrorCode::InvalidCameraOffsetDirection));
    }
};
