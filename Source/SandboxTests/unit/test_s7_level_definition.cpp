#include <SpaceGameS7/definition_reader.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>

#include <algorithm>

namespace {
constexpr TCHAR valid_level[]{LR"(
(level
  :id 'scripted-example
  :title "Scripted Example"
  :description "Built as ordinary Scheme data."
  :par-time 75.5
  :teams '(blue red)
  :player 'player
  :mission (mission
    :mode 'kill-enemies
    :heroes '(player blue-capital)
    :must-survive '(blue-capital)
    :required-kills '(red-capital))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(100 200 300) :rotation '(0 45 0))
    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(1000 0 0) :rotation '(0 0 0))
    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(-1000 0 0) :rotation '(0 180 0))
    (entity :id 'red-turret :archetype 'static-turret :team 'red
      :position '(0 500 0) :rotation '(0 90 0))))
)"};

constexpr TCHAR valid_camera_level[]{LR"(
(level
  :id 'camera-example
  :title "Camera Example"
  :teams '(blue red)
  :camera (camera
    :look-at '(blue-capital red-capital)
    :distance 10000
    :offset-direction '(-1 -1 0.5))
  :entities (list
    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(-1000 0 0) :rotation '(0 0 0))
    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0))))
)"};

constexpr TCHAR scheduled_level[]{LR"(
(level
  :id 'scheduled-example
  :title "Scheduled Example"
  :teams '(blue red)
  :camera (camera :look-at '(hero) :distance 1000 :offset-direction '(-1 0 0))
  :mission (mission :mode 'kill-enemies :kill-count 1 :heroes '(hero))
  :mission-events (list
    (mission-event :at 1.25
      :add-required-kills '(enemy)
      :increase-kill-count 2))
  :entities (list
    (entity :id 'hero :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))
    (entity :id 'enemy :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0) :spawn-at 1.25)))
)"};

auto contains_error(::ioj::levels::authoring::FLevelDefinitionReadResult const& result,
                    ml::ELevelValidationErrorCode const code) -> bool {
    return !result &&
           std::ranges::any_of(result.error(), [code](ioj::levels::Diagnostic const& error) {
               return error.code == code;
           });
}
}

TEST_CLASS(S7LevelDefinition, "Sandbox.UnitTests")
{
    TEST_METHOD(DecodesScheduledSpawnsAndMissionObjectives)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(scheduled_level)};
        if (!TestRunner->TestTrue(TEXT("Scheduled script produces a definition"),
                                  static_cast<bool>(result))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(result.error())));
            return;
        }

        auto const& definition{result.value()};
        TestRunner->TestEqual(
            TEXT("Spawn time is decoded"), definition.entities.spawn_times_seconds[1], 1.25);
        TestRunner->TestEqual(
            TEXT("One objective event is decoded"), definition.mission_events.Num(), 1);
        auto const& event{definition.mission_events[0]};
        TestRunner->TestEqual(TEXT("Objective event time is decoded"), event.time_seconds, 1.25);
        TestRunner->TestEqual(
            TEXT("Kill target increase is decoded"), event.kill_target_increase, 2);
        TestRunner->TestEqual(
            TEXT("Required kill is decoded"), event.required_kill_entity_ids.Num(), 1);
    }

    TEST_METHOD(DecodesSchemeDataIntoAValidatedNativeSoA)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(valid_level)};

        if (!TestRunner->TestTrue(TEXT("Script produces a definition"),
                                  static_cast<bool>(result))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(result.error())));
            return;
        }

        auto const& definition{result.value()};
        TestRunner->TestEqual(TEXT("Definition has two teams"), definition.teams.Num(), 2);
        TestRunner->TestEqual(TEXT("Definition has four entities"), definition.entities.num(), 4);
        TestRunner->TestTrue(TEXT("Stable level id is decoded"),
                             definition.metadata.id ==
                                 ml::FLevelId{FName{TEXT("scripted-example")}});
        TestRunner->TestEqual(
            TEXT("Title is decoded"), definition.metadata.title, FString{TEXT("Scripted Example")});
        TestRunner->TestEqual(
            TEXT("Par time is decoded"), definition.metadata.par_time_seconds.GetValue(), 75.5f);
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
                                 mission.mode == ::ioj::levels::LevelMissionMode::KillEnemies);
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
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(TEXT("(undefined-level-function)"))};

        TestRunner->TestFalse(TEXT("Invalid script does not produce a definition"),
                              static_cast<bool>(result));
        TestRunner->TestFalse(
            TEXT("Scheme error is reported"),
            ml::to_fstring(::ioj::levels::format_diagnostics(result.error())).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Evaluation error is classified"),
            contains_error(result, ioj::levels::DiagnosticCode::ScriptEvaluationFailed));
    }

    TEST_METHOD(DecodesPlayerlessCameraDefinition)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(valid_camera_level)};

        if (!TestRunner->TestTrue(TEXT("Camera script produces a definition"),
                                  static_cast<bool>(result))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(result.error())));
            return;
        }

        auto const& definition{result.value()};
        TestRunner->TestFalse(TEXT("Camera level has no player"),
                              definition.player_entity_id.is_set());
        TestRunner->TestFalse(TEXT("Omitted par time remains unset"),
                              definition.metadata.par_time_seconds.IsSet());
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
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(TEXT("(level :title 42)"))};

        TestRunner->TestFalse(TEXT("Malformed data is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(
            TEXT("Scheme evaluation succeeded"),
            contains_error(result, ioj::levels::DiagnosticCode::ScriptEvaluationFailed));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.error().empty());
        if (!result.error().empty()) {
            TestRunner->TestTrue(TEXT("Decode error identifies the title"),
                                 result.error()[0].node_path.contains("level"));
        }
    }

    TEST_METHOD(RejectsComplexTransformComponents)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'complex-position
  :title "Complex Position"
  :teams '(blue)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position (list 1+2i 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Complex transform is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.error().empty());
        if (!result.error().empty()) {
            TestRunner->TestTrue(
                TEXT("Decode error requires a real number"),
                contains_error(result, ioj::levels::DiagnosticCode::UnsupportedValue));
        }
    }

    TEST_METHOD(RejectsDuplicateCollectionClauses)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'duplicate-teams
  :title "Duplicate Teams"
  :teams '(blue)
  :teams '(red)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate collection is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.error().empty());
        if (!result.error().empty()) {
            TestRunner->TestTrue(
                TEXT("Duplicate clause is identified"),
                contains_error(result, ioj::levels::DiagnosticCode::DuplicateProperty));
        }
    }

    TEST_METHOD(RejectsDuplicateIdClauses)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'first-id
  :id 'second-id
  :title "Duplicate Id"
  :teams '(blue)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate id is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Decode error is reported"), result.error().empty());
        if (!result.error().empty()) {
            TestRunner->TestTrue(
                TEXT("Duplicate id clause is identified"),
                contains_error(result, ioj::levels::DiagnosticCode::DuplicateProperty));
        }
    }

    TEST_METHOD(ReportsMalformedAndDuplicateParTimes)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const malformed{reader.read_level_source(LR"(
(level
  :id 'malformed-par
  :title "Malformed Par"
  :par-time "fast"
  :teams '(blue)
  :player 'player
  :mission (mission :mode 'kill-enemies :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Malformed par time is rejected"), static_cast<bool>(malformed));
        TestRunner->TestFalse(TEXT("Malformed par time reports a decode error"),
                              malformed.error().empty());

        auto const duplicate{reader.read_level_source(LR"(
(level
  :id 'duplicate-par
  :title "Duplicate Par"
  :par-time 30
  :par-time 45
  :teams '(blue)
  :player 'player
  :mission (mission :mode 'kill-enemies :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Duplicate par time is rejected"), static_cast<bool>(duplicate));
        TestRunner->TestFalse(TEXT("Duplicate par time reports a decode error"),
                              duplicate.error().empty());
        if (!duplicate.error().empty()) {
            TestRunner->TestTrue(
                TEXT("Duplicate par-time clause is identified"),
                contains_error(duplicate, ioj::levels::DiagnosticCode::DuplicateProperty));
        }
    }

    TEST_METHOD(ReportsNativeValidationErrorsAfterDecoding)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'invalid-team
  :title "Invalid Team"
  :teams '(blue)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0) :rotation '(0 0 0))
    (entity :id 'enemy :archetype 'capital-ship :team 'red :position '(100 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Invalid definition is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(
            TEXT("Scheme evaluation succeeded"),
            contains_error(result, ioj::levels::DiagnosticCode::ScriptEvaluationFailed));
        TestRunner->TestTrue(
            TEXT("Unknown team is reported by native validation"),
            contains_error(result, ml::ELevelValidationErrorCode::UnknownTeamReference));
    }

    TEST_METHOD(ReportsMalformedCameraData)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'malformed-camera
  :title "Malformed Camera"
  :teams '(blue)
  :camera (camera
    :look-at '(capital)
    :distance "far"
    :offset-direction '(-1 0 0))
  :entities (list
    (entity :id 'capital :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Malformed camera is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Camera decode error is reported"), result.error().empty());
    }

    TEST_METHOD(RejectsDuplicateCameraClauses)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'duplicate-camera
  :title "Duplicate Camera"
  :teams '(blue)
  :camera (camera :look-at '(capital) :distance 1000 :offset-direction '(-1 0 0))
  :camera (camera :look-at '(capital) :distance 2000 :offset-direction '(1 0 0))
  :entities (list
    (entity :id 'capital :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Duplicate camera is rejected"), static_cast<bool>(result));
        TestRunner->TestFalse(TEXT("Duplicate camera decode error is reported"),
                              result.error().empty());
    }

    TEST_METHOD(ReportsInvalidCameraTargetReferences)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const empty_targets{reader.read_level_source(LR"(
(level
  :id 'empty-targets
  :title "Empty Targets"
  :teams '(blue)
  :camera (camera :look-at '() :distance 1000 :offset-direction '(-1 0 0))
  :entities (list
    (entity :id 'capital :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestTrue(
            TEXT("Empty camera targets are reported by native validation"),
            contains_error(empty_targets, ml::ELevelValidationErrorCode::MissingCameraTarget));

        auto const unknown_target{reader.read_level_source(LR"(
(level
  :id 'unknown-target
  :title "Unknown Target"
  :teams '(blue)
  :camera (camera :look-at '(missing) :distance 1000 :offset-direction '(-1 0 0))
  :entities (list
    (entity :id 'capital :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestTrue(
            TEXT("Unknown camera target is reported by native validation"),
            contains_error(unknown_target, ml::ELevelValidationErrorCode::CameraTargetNotFound));
    }

    TEST_METHOD(DecodesTimedMissionValues)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'timed-mission
  :title "Timed Mission"
  :teams '(blue red)
  :player 'player
  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 45.5
    :kill-count 3
    :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))
    (entity :id 'enemy :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0))))
)")};

        if (!TestRunner->TestTrue(TEXT("Timed mission is valid"), static_cast<bool>(result))) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Timed mission is decoded"), result->mission.IsSet())) {
            return;
        }
        auto const& mission{result->mission.GetValue()};
        TestRunner->TestTrue(TEXT("Timed mode is decoded"),
                             mission.mode ==
                                 ::ioj::levels::LevelMissionMode::KillEnemiesWithinTime);
        TestRunner->TestEqual(
            TEXT("Time limit is decoded"), mission.time_limit_seconds.GetValue(), 45.5f);
        TestRunner->TestEqual(TEXT("Kill count is decoded"), mission.kill_count.GetValue(), 3);
    }

    TEST_METHOD(ReportsInvalidMissionData)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const duplicate_clause{reader.read_level_source(LR"(
(level
  :id 'duplicate-mission-clause
  :title "Duplicate Mission Clause"
  :teams '(blue)
  :player 'player
  :mission (mission :mode 'kill-enemies :mode 'survive-time :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Duplicate mission clause is rejected"),
                              static_cast<bool>(duplicate_clause));
        TestRunner->TestFalse(TEXT("Duplicate mission clause reports a decode error"),
                              duplicate_clause.error().empty());

        auto const unknown_reference{reader.read_level_source(LR"(
(level
  :id 'unknown-mission-entity
  :title "Unknown Mission Entity"
  :teams '(blue)
  :player 'player
  :mission (mission :mode 'kill-enemies :heroes '(missing))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestTrue(TEXT("Unknown mission entity uses native validation"),
                             contains_error(unknown_reference,
                                            ml::ELevelValidationErrorCode::MissionEntityNotFound));

        auto const fractional_count{reader.read_level_source(LR"(
(level
  :id 'fractional-count
  :title "Fractional Count"
  :teams '(blue)
  :player 'player
  :mission (mission :mode 'kill-enemies :kill-count 1.5 :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};
        TestRunner->TestFalse(TEXT("Fractional kill count is rejected"),
                              static_cast<bool>(fractional_count));
        TestRunner->TestFalse(TEXT("Fractional kill count reports a decode error"),
                              fractional_count.error().empty());
    }

    TEST_METHOD(DecodesDeclarativeUnlockCriteria)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'locked-level
  :title "Locked Level"
  :unlock (list
    (level-completed 'first-level)
    (level-completed 'second-level))
  :teams '(blue)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        if (!TestRunner->TestTrue(TEXT("Unlock criteria are valid"), static_cast<bool>(result))) {
            return;
        }
        auto const& criteria{result->unlock_criteria};
        TestRunner->TestEqual(TEXT("Both criteria are decoded"), criteria.Num(), 2);
        TestRunner->TestTrue(TEXT("First prerequisite id is decoded"),
                             criteria[0].Get<ml::FLevelCompletedUnlockCriterion>().level_id ==
                                 ml::FLevelId{FName{TEXT("first-level")}});
    }

    TEST_METHOD(RejectsSelfUnlockDependency)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'self-locked
  :title "Self Locked"
  :unlock (list (level-completed 'self-locked))
  :teams '(blue)
  :player 'player
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))))
)")};

        TestRunner->TestFalse(TEXT("Self dependency is rejected"), static_cast<bool>(result));
        TestRunner->TestTrue(
            TEXT("Self dependency is reported"),
            contains_error(result, ml::ELevelValidationErrorCode::SelfUnlockDependency));
    }
};
