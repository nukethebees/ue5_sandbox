#include <SpaceGameS7/LevelDefinitionReader.h>

#include <CQTest.h>

namespace {
constexpr TCHAR valid_level[]{LR"(
(level
  (id 'scripted-example)
  (title "Scripted Example")
  (description "Built as ordinary Scheme data.")
  (teams (team 'blue) (team 'red))
  (player 'player)
  (mission
    (mode 'kill-enemies)
    (heroes 'player 'blue-capital)
    (must-survive 'blue-capital)
    (required-kills 'red-capital))
  (entities
    (entity 'player 'player-fighter 'blue
      (position 100 200 300) (rotation 0 45 0))
    (entity 'blue-capital 'capital-ship 'blue
      (position 1000 0 0) (rotation 0 0 0))
    (entity 'red-capital 'capital-ship 'red
      (position -1000 0 0) (rotation 0 180 0))
    (entity 'red-turret 'static-turret 'red
      (position 0 500 0) (rotation 0 90 0))))
)"};

constexpr TCHAR valid_camera_level[]{LR"(
(level
  (id 'camera-example)
  (title "Camera Example")
  (teams (team 'blue) (team 'red))
  (camera
    (look-at 'blue-capital 'red-capital)
    (distance 10000)
    (offset-direction -1 -1 0.5))
  (entities
    (entity 'blue-capital 'capital-ship 'blue
      (position -1000 0 0) (rotation 0 0 0))
    (entity 'red-capital 'capital-ship 'red
      (position 1000 0 0) (rotation 0 180 0))))
)"};

auto contains_error(ml::s7::FLevelDefinitionReadResult const& result,
                    ml::ELevelValidationErrorCode const code) -> bool {
    return result.validation_errors.ContainsByPredicate(
        [code](ml::FLevelValidationError const& error) { return error.code == code; });
}
}

TEST_CLASS(S7LevelDefinition, "Sandbox.UnitTests")
{
    TEST_METHOD(DecodesSchemeDataIntoAValidatedNativeSoA)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(valid_level)};

        if (!TestRunner->TestTrue(TEXT("Script produces a definition"),
                                  static_cast<bool>(result))) {
            TestRunner->AddError(result.script_error);
            return;
        }

        auto const& definition{result.definition.GetValue()};
        TestRunner->TestEqual(TEXT("Definition has two teams"), definition.teams.Num(), 2);
        TestRunner->TestEqual(TEXT("Definition has four entities"), definition.entities.num(), 4);
        TestRunner->TestTrue(TEXT("Stable level id is decoded"),
                             definition.metadata.id ==
                                 ml::FLevelId{FName{TEXT("scripted-example")}});
        TestRunner->TestEqual(
            TEXT("Title is decoded"), definition.metadata.title, FString{TEXT("Scripted Example")});
        TestRunner->TestTrue(TEXT("Player id is decoded"),
                             definition.player_entity_id ==
                                 ml::FLevelEntityId{FName{TEXT("player")}});
        TestRunner->TestTrue(
            TEXT("Player position is decoded"),
            definition.entities.positions.get_const_view()[0].Equals(FVector{100.0, 200.0, 300.0}));
        TestRunner->TestTrue(TEXT("Entity rotation is decoded"),
                             FRotator{definition.entities.rotations.pitches[3],
                                      definition.entities.rotations.yaws[3],
                                      definition.entities.rotations.rolls[3]}
                                 .Equals(FRotator{0.0, 90.0, 0.0}));
        if (TestRunner->TestTrue(TEXT("Mission is decoded"), definition.mission.IsSet())) {
            auto const& mission{definition.mission.GetValue()};
            TestRunner->TestTrue(TEXT("Mission mode is decoded"),
                                 mission.mode == ml::ELevelMissionMode::KillEnemies);
            TestRunner->TestFalse(TEXT("Omitted kill count selects automatic targeting"),
                                  mission.kill_count.IsSet());
            TestRunner->TestEqual(
                TEXT("Mission heroes are decoded"), mission.hero_entity_ids.Num(), 2);
            TestRunner->TestEqual(TEXT("Mission survivor is decoded"),
                                  mission.must_survive_entity_ids[0].value,
                                  FName{TEXT("blue-capital")});
            TestRunner->TestEqual(TEXT("Mission required kill is decoded"),
                                  mission.required_kill_entity_ids[0].value,
                                  FName{TEXT("red-capital")});
        }
    }

    TEST_METHOD(ReportsSchemeErrorsWithoutDecoding)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(TEXT("(undefined-level-function)"))};

        TestRunner->TestFalse(TEXT("Invalid script does not produce a definition"),
                              static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Scheme error is reported"), result.script_error.IsEmpty());
        TestRunner->TestTrue(TEXT("Decoder did not run"), result.decode_errors.IsEmpty());
    }

    TEST_METHOD(DecodesPlayerlessCameraDefinition)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(valid_camera_level)};

        if (!TestRunner->TestTrue(TEXT("Camera script produces a definition"),
                                  static_cast<bool>(result))) {
            TestRunner->AddError(result.script_error);
            return;
        }

        auto const& definition{result.definition.GetValue()};
        TestRunner->TestFalse(TEXT("Camera level has no player"),
                              definition.player_entity_id.is_set());
        if (!TestRunner->TestTrue(TEXT("Camera is decoded"), definition.camera.IsSet())) {
            return;
        }

        auto const& camera{definition.camera.GetValue()};
        TestRunner->TestEqual(TEXT("Camera has two targets"), camera.target_entity_ids.Num(), 2);
        TestRunner->TestTrue(TEXT("Camera direction is decoded"),
                             camera.offset_direction.Equals(FVector{-1.0, -1.0, 0.5}));
        TestRunner->TestEqual(TEXT("Camera distance is decoded"), camera.distance, 10000.0);
    }

    TEST_METHOD(ReportsStructuralDecodeErrorsWithPaths)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(TEXT("(level (title 42))"))};

        TestRunner->TestFalse(TEXT("Malformed data is rejected"), static_cast<bool>(result));
        TestRunner->TestTrue(TEXT("Scheme evaluation succeeded"), result.script_error.IsEmpty());
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.decode_errors.IsEmpty());
        if (!result.decode_errors.IsEmpty()) {
            TestRunner->TestTrue(TEXT("Decode error identifies the title"),
                                 result.decode_errors[0].path.Contains(TEXT("level")));
        }
    }

    TEST_METHOD(RejectsComplexTransformComponents)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'complex-position)
  (title "Complex Position")
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 1+2i 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Complex transform is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.decode_errors.IsEmpty());
        if (!result.decode_errors.IsEmpty()) {
            TestRunner->TestTrue(TEXT("Decode error requires a real number"),
                                 result.decode_errors[0].message.Contains(TEXT("real number")));
        }
    }

    TEST_METHOD(RejectsDuplicateCollectionClauses)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'duplicate-teams)
  (title "Duplicate Teams")
  (teams (team 'blue))
  (teams (team 'red))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate collection is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.decode_errors.IsEmpty());
        if (!result.decode_errors.IsEmpty()) {
            TestRunner->TestTrue(TEXT("Duplicate clause is identified"),
                                 result.decode_errors[0].message.Contains(TEXT("Duplicate teams")));
        }
    }

    TEST_METHOD(RejectsDuplicateIdClauses)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'first-id)
  (id 'second-id)
  (title "Duplicate Id")
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate id is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.decode_errors.IsEmpty());
        if (!result.decode_errors.IsEmpty()) {
            TestRunner->TestTrue(TEXT("Duplicate id clause is identified"),
                                 result.decode_errors[0].message.Contains(TEXT("Duplicate id")));
        }
    }

    TEST_METHOD(ReportsNativeValidationErrorsAfterDecoding)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'invalid-team)
  (title "Invalid Team")
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue (position 0 0 0) (rotation 0 0 0))
    (entity 'enemy 'capital-ship 'red (position 100 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Invalid definition is rejected"), static_cast<bool>(result));
        TestRunner->TestTrue(TEXT("Scheme evaluation succeeded"), result.script_error.IsEmpty());
        TestRunner->TestTrue(
            TEXT("Unknown team is reported by native validation"),
            contains_error(result, ml::ELevelValidationErrorCode::UnknownTeamReference));
    }

    TEST_METHOD(ReportsMalformedCameraData)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'malformed-camera)
  (title "Malformed Camera")
  (teams (team 'blue))
  (camera
    (look-at 'capital)
    (distance "far")
    (offset-direction -1 0 0))
  (entities
    (entity 'capital 'capital-ship 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Malformed camera is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Camera decode error is reported"),
                              result.decode_errors.IsEmpty());
    }

    TEST_METHOD(RejectsDuplicateCameraClauses)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'duplicate-camera)
  (title "Duplicate Camera")
  (teams (team 'blue))
  (camera (look-at 'capital) (distance 1000) (offset-direction -1 0 0))
  (camera (look-at 'capital) (distance 2000) (offset-direction 1 0 0))
  (entities
    (entity 'capital 'capital-ship 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate camera is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Duplicate camera decode error is reported"),
                              result.decode_errors.IsEmpty());
    }

    TEST_METHOD(ReportsInvalidCameraTargetReferences)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const empty_targets{reader.read_source(LR"(
(level
  (id 'empty-targets)
  (title "Empty Targets")
  (teams (team 'blue))
  (camera (look-at) (distance 1000) (offset-direction -1 0 0))
  (entities
    (entity 'capital 'capital-ship 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};
        TestRunner->TestTrue(
            TEXT("Empty camera targets are reported by native validation"),
            contains_error(empty_targets, ml::ELevelValidationErrorCode::MissingCameraTarget));

        auto const unknown_target{reader.read_source(LR"(
(level
  (id 'unknown-target)
  (title "Unknown Target")
  (teams (team 'blue))
  (camera (look-at 'missing) (distance 1000) (offset-direction -1 0 0))
  (entities
    (entity 'capital 'capital-ship 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};
        TestRunner->TestTrue(
            TEXT("Unknown camera target is reported by native validation"),
            contains_error(unknown_target, ml::ELevelValidationErrorCode::CameraTargetNotFound));
    }

    TEST_METHOD(DecodesTimedMissionValues)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'timed-mission)
  (title "Timed Mission")
  (teams (team 'blue) (team 'red))
  (player 'player)
  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 45.5)
    (kill-count 3)
    (heroes 'player))
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))
    (entity 'enemy 'capital-ship 'red
      (position 1000 0 0) (rotation 0 180 0))))
)")};

        if (!TestRunner->TestTrue(TEXT("Timed mission is valid"), static_cast<bool>(result))) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Timed mission is decoded"),
                                  result.definition->mission.IsSet())) {
            return;
        }
        auto const& mission{result.definition->mission.GetValue()};
        TestRunner->TestTrue(TEXT("Timed mode is decoded"),
                             mission.mode == ml::ELevelMissionMode::KillEnemiesWithinTime);
        TestRunner->TestEqual(
            TEXT("Time limit is decoded"), mission.time_limit_seconds.GetValue(), 45.5f);
        TestRunner->TestEqual(TEXT("Kill count is decoded"), mission.kill_count.GetValue(), 3);
    }

    TEST_METHOD(ReportsInvalidMissionData)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const duplicate_clause{reader.read_source(LR"(
(level
  (id 'duplicate-mission-clause)
  (title "Duplicate Mission Clause")
  (teams (team 'blue))
  (player 'player)
  (mission (mode 'kill-enemies) (mode 'survive-time) (heroes 'player))
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Duplicate mission clause is rejected"),
                              static_cast<bool>(duplicate_clause));
        TestRunner->TestFalse(TEXT("Duplicate mission clause reports a decode error"),
                              duplicate_clause.decode_errors.IsEmpty());

        auto const unknown_reference{reader.read_source(LR"(
(level
  (id 'unknown-mission-entity)
  (title "Unknown Mission Entity")
  (teams (team 'blue))
  (player 'player)
  (mission (mode 'kill-enemies) (heroes 'missing))
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};
        TestRunner->TestTrue(TEXT("Unknown mission entity uses native validation"),
                             contains_error(unknown_reference,
                                            ml::ELevelValidationErrorCode::MissionEntityNotFound));

        auto const fractional_count{reader.read_source(LR"(
(level
  (id 'fractional-count)
  (title "Fractional Count")
  (teams (team 'blue))
  (player 'player)
  (mission (mode 'kill-enemies) (kill-count 1.5) (heroes 'player))
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Fractional kill count is rejected"),
                              static_cast<bool>(fractional_count));
        TestRunner->TestFalse(TEXT("Fractional kill count reports a decode error"),
                              fractional_count.decode_errors.IsEmpty());
    }

    TEST_METHOD(DecodesDeclarativeUnlockCriteria)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'locked-level)
  (title "Locked Level")
  (unlock
    (level-completed 'first-level)
    (level-completed 'second-level))
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        if (!TestRunner->TestTrue(TEXT("Unlock criteria are valid"), static_cast<bool>(result))) {
            return;
        }
        auto const& criteria{result.definition->unlock_criteria};
        TestRunner->TestEqual(TEXT("Both criteria are decoded"), criteria.Num(), 2);
        TestRunner->TestTrue(TEXT("First prerequisite id is decoded"),
                             criteria[0].Get<ml::FLevelCompletedUnlockCriterion>().level_id ==
                                 ml::FLevelId{FName{TEXT("first-level")}});
    }

    TEST_METHOD(RejectsSelfUnlockDependency)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const result{reader.read_source(LR"(
(level
  (id 'self-locked)
  (title "Self Locked")
  (unlock (level-completed 'self-locked))
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0) (rotation 0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Self dependency is rejected"), static_cast<bool>(result));
        TestRunner->TestTrue(
            TEXT("Self dependency is reported"),
            contains_error(result, ml::ELevelValidationErrorCode::SelfUnlockDependency));
    }
};
