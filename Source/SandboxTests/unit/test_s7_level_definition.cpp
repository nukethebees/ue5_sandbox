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
  :mission (mission :objectives (list
    (objective :id 'primary :team 'blue
      :pass (entity-destroyed 'red-capital)
      :fail (entity-destroyed 'blue-capital))))
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
  :mission (mission :objectives (list
    (objective :id 'primary :team 'blue :pass (entity-destroyed 'enemy))))
  :entities (list
    (entity :id 'hero :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))
    (entity :id 'enemy :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0) :spawn-at 1.25)))
)"};

auto contains_error(::ioj::levels::authoring::FLevelDefinitionReadResult const& result,
                    ioj::levels::DiagnosticCode const code) -> bool {
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
            TEXT("Spawn time is decoded"), definition.entities()[1].spawn_time_seconds, 1.25);
        TestRunner->TestEqual(
            TEXT("Future entity condition is decoded"),
            ioj::to_fstring(std::get<ioj::levels::EntityDestroyedExpression>(
                                definition.mission()->objectives[0].expressions[0].payload)
                                .entity.value),
            FString{TEXT("enemy")});
    }

    TEST_METHOD(DecodesSchemeDataIntoATrustedDefinition)
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
        TestRunner->TestEqual(TEXT("Definition has four entities"),
                              static_cast<int32>(definition.entities().size()),
                              4);
        TestRunner->TestTrue(TEXT("Stable level id is decoded"),
                             definition.metadata().id == ioj::levels::LevelId{"scripted-example"});
        TestRunner->TestEqual(TEXT("Title is decoded"),
                              ioj::to_fstring(definition.metadata().title),
                              FString{TEXT("Scripted Example")});
        TestRunner->TestEqual(
            TEXT("Par time is decoded"), definition.metadata().par_time_seconds.value(), 75.5f);
        TestRunner->TestTrue(TEXT("Player id is decoded"),
                             definition.player_entity_id() == ioj::levels::EntityId{"player"});
        TestRunner->TestTrue(TEXT("Player position is decoded"),
                             definition.entities()[0].position ==
                                 (ioj::Vector3d{100.0, 200.0, 300.0}));
        TestRunner->TestTrue(TEXT("Entity rotation is decoded"),
                             definition.entities()[3].rotation == (ioj::Rotator3d{0.0, 90.0, 0.0}));
        if (TestRunner->TestTrue(TEXT("Mission is decoded"), definition.mission().has_value())) {
            TestRunner->TestEqual(TEXT("Objective team is decoded"),
                                  definition.mission()->objectives[0].team,
                                  ioj::Team::Blue);
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
                              !definition.player_entity_id().empty());
        TestRunner->TestFalse(TEXT("Omitted par time remains unset"),
                              definition.metadata().par_time_seconds.has_value());
        if (!TestRunner->TestTrue(TEXT("Camera is decoded"), definition.camera().has_value())) {
            return;
        }

        auto const& camera{definition.camera().value()};
        TestRunner->TestEqual(
            TEXT("Camera has two targets"), static_cast<int32>(camera.target_entity_ids.size()), 2);
        TestRunner->TestTrue(TEXT("Camera direction is decoded"),
                             camera.offset_direction == (ioj::Vector3d{-1.0, -1.0, 0.5}));
        TestRunner->TestEqual(TEXT("Camera distance is decoded"), camera.distance, 10000.0);
    }

    TEST_METHOD(DecodesTimedMissionValues)
    {
        ioj::levels::authoring::FDefinitionReader reader;
        FString source{valid_level};
        source.ReplaceInline(TEXT("(entity-destroyed 'red-capital)"), TEXT("(time-elapsed 45.5)"));
        auto const result{reader.read_level_source(source)};
        if (TestRunner->TestTrue(TEXT("Timer parses"), result.has_value())) {
            TestRunner->TestEqual(TEXT("Timer duration is decoded"),
                                  std::get<ioj::levels::TimeElapsedExpression>(
                                      result->mission()->objectives[0].expressions[0].payload)
                                      .seconds.get(),
                                  45.5);
        }
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
        auto const& criteria{result->unlock_level_ids()};
        TestRunner->TestEqual(
            TEXT("Both criteria are decoded"), static_cast<int32>(criteria.size()), 2);
        TestRunner->TestTrue(TEXT("First prerequisite id is decoded"),
                             criteria[0] == ioj::levels::LevelId{"first-level"});
    }
};
