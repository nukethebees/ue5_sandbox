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
                    ioj::ELevelValidationErrorCode const code) -> bool {
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
            TestRunner->AddError(
                ioj::to_fstring(::ioj::levels::format_diagnostics(result.error())));
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
            TestRunner->AddError(
                ioj::to_fstring(::ioj::levels::format_diagnostics(result.error())));
            return;
        }

        auto const& definition{result.value()};
        TestRunner->TestEqual(TEXT("Definition has four entities"), definition.entities.num(), 4);
        TestRunner->TestTrue(TEXT("Stable level id is decoded"),
                             definition.metadata.id ==
                                 ioj::FLevelId{FName{TEXT("scripted-example")}});
        TestRunner->TestEqual(
            TEXT("Title is decoded"), definition.metadata.title, FString{TEXT("Scripted Example")});
        TestRunner->TestEqual(
            TEXT("Par time is decoded"), definition.metadata.par_time_seconds.GetValue(), 75.5f);
        TestRunner->TestTrue(TEXT("Player id is decoded"),
                             definition.player_entity_id ==
                                 ioj::FLevelEntityId{FName{TEXT("player")}});
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
            ioj::to_fstring(::ioj::levels::format_diagnostics(result.error())).IsEmpty());
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
            TestRunner->AddError(
                ioj::to_fstring(::ioj::levels::format_diagnostics(result.error())));
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

    TEST_METHOD(DecodesTimedMissionValues)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const result{reader.read_level_source(LR"(
(level
  :id 'timed-mission
  :title "Timed Mission"
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
                             criteria[0].Get<ioj::FLevelCompletedUnlockCriterion>().level_id ==
                                 ioj::FLevelId{FName{TEXT("first-level")}});
    }
};
