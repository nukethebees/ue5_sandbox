#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelDefinition.h>

#include <CQTest.h>

#include <limits>

namespace {
auto contains_error(ioj::FLevelValidationResult const& result,
                    ioj::ELevelValidationErrorCode const code) -> bool {
    return result.errors.ContainsByPredicate(
        [code](ioj::FLevelValidationError const& error) { return error.code == code; });
}

auto make_camera_level() -> ioj::FLevelDraft {
    ioj::FLevelBuilder builder;
    builder.set_metadata(ioj::FLevelMetadata{.id = ioj::FLevelId{FName{TEXT("camera-level")}},
                                             .title = TEXT("Camera Level")});
    builder.add_entity(ioj::FEntitySpawnDefinition{
        .id = ioj::FLevelEntityId{FName{TEXT("blue-capital")}},
        .archetype = ioj::level_archetypes::capital_ship,
        .team = ::ioj::Team::Blue,
        .position = FVector{-1000.0, 0.0, 0.0},
    });
    builder.add_entity(ioj::FEntitySpawnDefinition{
        .id = ioj::FLevelEntityId{FName{TEXT("red-capital")}},
        .archetype = ioj::level_archetypes::capital_ship,
        .team = ::ioj::Team::Red,
        .position = FVector{1000.0, 0.0, 0.0},
    });
    builder.set_camera(ioj::FLevelCameraDefinition{
        .target_entity_ids = {ioj::FLevelEntityId{FName{TEXT("blue-capital")}},
                              ioj::FLevelEntityId{FName{TEXT("red-capital")}}},
        .offset_direction = FVector{-1.0, -1.0, 0.5},
        .distance = 10000.0,
    });
    return builder.finish();
}
}

TEST_CLASS(LevelDefinition, "Sandbox.UnitTests")
{
    TEST_METHOD(BuilderConstructsValidLevel)
    {
        auto const definition{ioj::example_levels::make_example_draft()};
        auto const validation{ioj::validate_level(definition)};

        TestRunner->TestTrue(TEXT("Definition is valid"), static_cast<bool>(validation));
        TestRunner->TestEqual(
            TEXT("Definition has four entity rows"), definition.entities.Num(), 4);
        TestRunner->TestEqual(
            TEXT("Title is preserved"), definition.metadata.title, FString{TEXT("Native Example")});
        TestRunner->TestFalse(TEXT("Omitted par time remains unset"),
                              definition.metadata.par_time_seconds.IsSet());
        TestRunner->TestTrue(TEXT("Stable level id is preserved"),
                             definition.metadata.id ==
                                 ioj::FLevelId{FName{TEXT("native-example")}});
        TestRunner->TestTrue(TEXT("Player id is preserved"),
                             definition.player_entity_id ==
                                 ioj::FLevelEntityId{FName{TEXT("player")}});
        TestRunner->TestTrue(TEXT("Player archetype is preserved"),
                             definition.entities[0].archetype ==
                                 ioj::level_archetypes::player_fighter);
        TestRunner->TestTrue(TEXT("Player team is preserved"),
                             definition.entities[0].team == ::ioj::Team::Blue);
        TestRunner->TestTrue(
            TEXT("Player position is preserved"),
            definition.entities[0].position.Equals(FVector{0.0, -25000.0, 1000.0}));
        TestRunner->TestTrue(TEXT("Turret archetype is preserved"),
                             definition.entities[3].archetype ==
                                 ioj::level_archetypes::static_turret);
        if (TestRunner->TestTrue(TEXT("Mission is preserved"), definition.mission.IsSet())) {
            TestRunner->TestEqual(TEXT("Objective is preserved"),
                                  static_cast<int32>(definition.mission->objectives.size()),
                                  1);
            TestRunner->TestEqual(TEXT("Team mappings are preserved"),
                                  static_cast<int32>(definition.mission->team_results.size()),
                                  2);
        }
    }

    TEST_METHOD(UnsupportedTeamFailsValidation)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.entities[1].team = static_cast<::ioj::Team>(255);

        auto const validation{ioj::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Unknown team is reported"),
            contains_error(validation, ioj::ELevelValidationErrorCode::UnsupportedTeam));
    }

    TEST_METHOD(BuilderConstructsValidPlayerlessCameraLevel)
    {
        auto const definition{make_camera_level()};
        auto const validation{ioj::validate_level(definition)};

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

    TEST_METHOD(ParTimeRequiresAPositivePlayerMissionTarget)
    {
        auto valid{ioj::example_levels::make_example_draft()};
        valid.metadata.par_time_seconds = 45.0f;
        TestRunner->TestTrue(TEXT("Positive mission par time is valid"),
                             static_cast<bool>(ioj::validate_level(valid)));

        auto zero{valid};
        zero.metadata.par_time_seconds = 0.0f;
        TestRunner->TestTrue(TEXT("Zero par time is rejected"),
                             contains_error(ioj::validate_level(zero),
                                            ioj::ELevelValidationErrorCode::InvalidParTime));

        auto negative{valid};
        negative.metadata.par_time_seconds = -1.0f;
        TestRunner->TestTrue(TEXT("Negative par time is rejected"),
                             contains_error(ioj::validate_level(negative),
                                            ioj::ELevelValidationErrorCode::InvalidParTime));

        auto non_finite{valid};
        non_finite.metadata.par_time_seconds = std::numeric_limits<float>::infinity();
        TestRunner->TestTrue(TEXT("Non-finite par time is rejected"),
                             contains_error(ioj::validate_level(non_finite),
                                            ioj::ELevelValidationErrorCode::InvalidParTime));

        auto playerless{make_camera_level()};
        playerless.metadata.par_time_seconds = 45.0f;
        TestRunner->TestTrue(TEXT("Playerless level par time is rejected"),
                             contains_error(ioj::validate_level(playerless),
                                            ioj::ELevelValidationErrorCode::UnexpectedParTime));
    }

    TEST_METHOD(CameraCanTargetOneEntity)
    {
        auto definition{make_camera_level()};
        definition.camera->target_entity_ids.SetNum(1);

        auto const validation{ioj::validate_level(definition)};
        TestRunner->TestTrue(TEXT("Single-target camera is valid"), static_cast<bool>(validation));
    }

    TEST_METHOD(BuilderCanBeReused)
    {
        ioj::FLevelBuilder builder;
        builder.set_metadata(
            ioj::FLevelMetadata{.id = ioj::FLevelId{FName{TEXT("first")}}, .title = TEXT("First")});
        auto const first_player{builder.add_entity(ioj::FEntitySpawnDefinition{
            .id = ioj::FLevelEntityId{FName{TEXT("first-player")}},
            .archetype = ioj::level_archetypes::player_fighter,
            .team = ::ioj::Team::Blue,
            .position = FVector{10.0, 20.0, 30.0},
        })};
        builder.set_player_entity(first_player);

        auto const first{builder.finish()};
        TestRunner->TestTrue(TEXT("First definition is valid"),
                             static_cast<bool>(ioj::validate_level(first)));
        TestRunner->TestEqual(TEXT("First definition has one row"), first.entities.Num(), 1);
        TestRunner->TestTrue(TEXT("Player references the authored entity"),
                             first.player_entity_id == first_player);
        TestRunner->TestTrue(TEXT("Player entity position is preserved"),
                             first.entities[0].position.Equals(FVector{10.0, 20.0, 30.0}));

        builder.set_metadata(ioj::FLevelMetadata{.id = ioj::FLevelId{FName{TEXT("second")}},
                                                 .title = TEXT("Second")});
        auto const second_player{builder.add_entity(ioj::FEntitySpawnDefinition{
            .id = ioj::FLevelEntityId{FName{TEXT("second-player")}},
            .archetype = ioj::level_archetypes::player_fighter,
            .team = ::ioj::Team::Red,
        })};
        builder.set_player_entity(second_player);
        auto const second{builder.finish()};

        TestRunner->TestTrue(TEXT("Reused builder produces a valid definition"),
                             static_cast<bool>(ioj::validate_level(second)));
        TestRunner->TestEqual(
            TEXT("Reused builder starts with empty entity storage"), second.entities.Num(), 1);
        TestRunner->TestEqual(TEXT("Reused builder replaces metadata"),
                              second.metadata.title,
                              FString{TEXT("Second")});
    }

    TEST_METHOD(DuplicateAuthoredEntityIdFailsValidation)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.entities[1].id = definition.player_entity_id;

        auto const validation{ioj::validate_level(definition)};
        TestRunner->TestFalse(TEXT("Definition is invalid"), static_cast<bool>(validation));
        TestRunner->TestTrue(
            TEXT("Duplicate entity id is reported"),
            contains_error(validation, ioj::ELevelValidationErrorCode::DuplicateEntityId));
    }

    TEST_METHOD(BuilderReuseClearsCameraState)
    {
        ioj::FLevelBuilder builder;
        builder.set_metadata(ioj::FLevelMetadata{.id = ioj::FLevelId{FName{TEXT("camera")}},
                                                 .title = TEXT("Camera")});
        builder.add_entity(ioj::FEntitySpawnDefinition{
            .id = ioj::FLevelEntityId{FName{TEXT("capital")}},
            .archetype = ioj::level_archetypes::capital_ship,
            .team = ::ioj::Team::Blue,
        });
        builder.set_camera(ioj::FLevelCameraDefinition{
            .target_entity_ids = {ioj::FLevelEntityId{FName{TEXT("capital")}}},
            .offset_direction = FVector{-1.0, 0.0, 0.0},
            .distance = 1000.0,
        });
        builder.set_mission({});
        auto const camera_level{builder.finish()};
        TestRunner->TestTrue(TEXT("First definition has a camera"), camera_level.camera.IsSet());
        TestRunner->TestTrue(TEXT("First definition has a mission"), camera_level.mission.IsSet());

        builder.set_metadata(ioj::FLevelMetadata{.id = ioj::FLevelId{FName{TEXT("player")}},
                                                 .title = TEXT("Player")});
        auto const player_id{builder.add_entity(ioj::FEntitySpawnDefinition{
            .id = ioj::FLevelEntityId{FName{TEXT("player")}},
            .archetype = ioj::level_archetypes::player_fighter,
            .team = ::ioj::Team::Blue,
        })};
        builder.set_player_entity(player_id);
        auto const player_level{builder.finish()};

        TestRunner->TestFalse(TEXT("Reused builder clears the camera"),
                              player_level.camera.IsSet());
        TestRunner->TestFalse(TEXT("Reused builder clears the mission"),
                              player_level.mission.IsSet());
        TestRunner->TestTrue(TEXT("Reused builder definition is valid"),
                             static_cast<bool>(ioj::validate_level(player_level)));
    }

    TEST_METHOD(ObjectiveCyclesAreValidated)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        auto& objective{definition.mission->objectives[0]};
        objective.expressions[0] = {.type = ioj::ExpressionType::All, .children = {0}};
        TestRunner->TestTrue(
            TEXT("Cycle is rejected"),
            contains_error(ioj::validate_level(definition),
                           ioj::ELevelValidationErrorCode::InvalidMissionStructure));
    }

    TEST_METHOD(MissionEntityReferencesAreValidated)
    {
        auto definition{ioj::example_levels::make_example_draft()};
        definition.mission->objectives[0].expressions[0].entity = ioj::levels::EntityId{"missing"};
        TestRunner->TestTrue(
            TEXT("Unknown entity is rejected"),
            contains_error(ioj::validate_level(definition),
                           ioj::ELevelValidationErrorCode::InvalidMissionStructure));
    }

    TEST_METHOD(MalformedDefinitionsReportStructuredErrors)
    {
        auto missing_id{ioj::example_levels::make_example_draft()};
        missing_id.metadata.id = {};
        auto const missing_id_validation{ioj::validate_level(missing_id)};
        TestRunner->TestTrue(
            TEXT("Missing stable id is reported"),
            contains_error(missing_id_validation, ioj::ELevelValidationErrorCode::MissingLevelId));

        auto missing_player{ioj::example_levels::make_example_draft()};
        missing_player.player_entity_id = {};
        auto const missing_player_validation{ioj::validate_level(missing_player)};
        TestRunner->TestTrue(TEXT("Missing viewpoint is reported"),
                             contains_error(missing_player_validation,
                                            ioj::ELevelValidationErrorCode::MissingViewpoint));

        auto unsupported_archetype{ioj::example_levels::make_example_draft()};
        unsupported_archetype.entities[1].archetype =
            ioj::FEntityArchetypeId{FName{TEXT("pirate-fighter")}};
        auto const unsupported_archetype_validation{ioj::validate_level(unsupported_archetype)};
        TestRunner->TestTrue(TEXT("Unsupported archetype is reported"),
                             contains_error(unsupported_archetype_validation,
                                            ioj::ELevelValidationErrorCode::UnsupportedArchetype));

        auto invalid_placement{ioj::example_levels::make_example_draft()};
        invalid_placement.entities[1].position.X = std::numeric_limits<double>::quiet_NaN();
        auto const invalid_placement_validation{ioj::validate_level(invalid_placement)};
        TestRunner->TestTrue(TEXT("Invalid placement is reported"),
                             contains_error(invalid_placement_validation,
                                            ioj::ELevelValidationErrorCode::InvalidPlacement));
    }

    TEST_METHOD(CameraAndPlayerAreMutuallyExclusive)
    {
        auto both{ioj::example_levels::make_example_draft()};
        both.camera = make_camera_level().camera;
        auto const both_validation{ioj::validate_level(both)};
        TestRunner->TestTrue(
            TEXT("Player and camera conflict is reported"),
            contains_error(both_validation, ioj::ELevelValidationErrorCode::ConflictingViewpoints));

        auto neither{make_camera_level()};
        neither.camera.Reset();
        auto const neither_validation{ioj::validate_level(neither)};
        TestRunner->TestTrue(
            TEXT("Missing viewpoint is reported"),
            contains_error(neither_validation, ioj::ELevelValidationErrorCode::MissingViewpoint));
    }

    TEST_METHOD(CameraTargetsMustBeUniqueDeclaredEntities)
    {
        auto duplicate{make_camera_level()};
        duplicate.camera->target_entity_ids[1] = duplicate.camera->target_entity_ids[0];
        auto const duplicate_validation{ioj::validate_level(duplicate)};
        TestRunner->TestTrue(TEXT("Duplicate camera target is reported"),
                             contains_error(duplicate_validation,
                                            ioj::ELevelValidationErrorCode::DuplicateCameraTarget));

        auto unknown{make_camera_level()};
        unknown.camera->target_entity_ids[0] = ioj::FLevelEntityId{FName{TEXT("missing-capital")}};
        auto const unknown_validation{ioj::validate_level(unknown)};
        TestRunner->TestTrue(TEXT("Unknown camera target is reported"),
                             contains_error(unknown_validation,
                                            ioj::ELevelValidationErrorCode::CameraTargetNotFound));

        auto empty{make_camera_level()};
        empty.camera->target_entity_ids.Reset();
        auto const empty_validation{ioj::validate_level(empty)};
        TestRunner->TestTrue(
            TEXT("Missing camera target is reported"),
            contains_error(empty_validation, ioj::ELevelValidationErrorCode::MissingCameraTarget));
    }

    TEST_METHOD(CameraPlacementMustBeFiniteAndNonZero)
    {
        auto invalid_distance{make_camera_level()};
        invalid_distance.camera->distance = 0.0;
        auto const distance_validation{ioj::validate_level(invalid_distance)};
        TestRunner->TestTrue(TEXT("Invalid camera distance is reported"),
                             contains_error(distance_validation,
                                            ioj::ELevelValidationErrorCode::InvalidCameraDistance));

        auto invalid_direction{make_camera_level()};
        invalid_direction.camera->offset_direction = FVector::ZeroVector;
        auto const direction_validation{ioj::validate_level(invalid_direction)};
        TestRunner->TestTrue(
            TEXT("Invalid camera direction is reported"),
            contains_error(direction_validation,
                           ioj::ELevelValidationErrorCode::InvalidCameraOffsetDirection));

        auto non_finite_distance{make_camera_level()};
        non_finite_distance.camera->distance = std::numeric_limits<double>::infinity();
        auto const non_finite_distance_validation{ioj::validate_level(non_finite_distance)};
        TestRunner->TestTrue(TEXT("Non-finite camera distance is reported"),
                             contains_error(non_finite_distance_validation,
                                            ioj::ELevelValidationErrorCode::InvalidCameraDistance));

        auto non_finite_direction{make_camera_level()};
        non_finite_direction.camera->offset_direction.X = std::numeric_limits<double>::quiet_NaN();
        auto const non_finite_direction_validation{ioj::validate_level(non_finite_direction)};
        TestRunner->TestTrue(
            TEXT("Non-finite camera direction is reported"),
            contains_error(non_finite_direction_validation,
                           ioj::ELevelValidationErrorCode::InvalidCameraOffsetDirection));
    }
};
