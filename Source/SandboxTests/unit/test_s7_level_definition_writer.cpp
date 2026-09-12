#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <CQTest.h>
#include <Misc/Paths.h>

namespace {
auto make_player_level(bool const reverse_order = false) -> ml::FLevelDefinition {
    ml::FLevelBuilder builder;
    builder.set_metadata({
        .id = ml::FLevelId{TEXT("writer-example")},
        .title = TEXT("Writer \"Example\""),
        .description = TEXT("Line one\nLine two."),
    });
    if (reverse_order) {
        builder.add_team(ml::level_teams::red);
        builder.add_team(ml::level_teams::blue);
    } else {
        builder.add_team(ml::level_teams::blue);
        builder.add_team(ml::level_teams::red);
    }
    builder.set_player_entity(ml::FLevelEntityId{TEXT("player")});

    ml::FEntitySpawnDefinition const player{
        .id = ml::FLevelEntityId{TEXT("player")},
        .archetype = ml::level_archetypes::player_fighter,
        .team = ml::level_teams::blue,
        .position = FVector{0.0001, 200.12349, -300.5},
        .rotation = FRotator{10.0, 20.25, -0.0001},
    };
    ml::FEntitySpawnDefinition const capital{
        .id = ml::FLevelEntityId{TEXT("blue-capital")},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::blue,
        .position = FVector{1000.0, 2000.0, 3000.0},
        .rotation = FRotator{0.0, 90.0, 0.0},
    };
    ml::FEntitySpawnDefinition const turret{
        .id = ml::FLevelEntityId{TEXT("red-turret")},
        .archetype = ml::level_archetypes::static_turret,
        .team = ml::level_teams::red,
        .position = FVector{-1000.0, -2000.0, -3000.0},
        .rotation = FRotator{0.0, -90.0, 0.0},
    };
    if (reverse_order) {
        builder.add_entity(turret);
        builder.add_entity(capital);
        builder.add_entity(player);
    } else {
        builder.add_entity(player);
        builder.add_entity(capital);
        builder.add_entity(turret);
    }
    return builder.finish();
}
}

TEST_CLASS(S7LevelDefinitionWriter, "Sandbox.UnitTests")
{
    TEST_METHOD(EmitsCanonicalReadableInitialState)
    {
        auto const source{ml::s7::emit_initial_level_source(make_player_level())};
        if (!TestRunner->TestTrue(TEXT("Source is emitted"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }

        constexpr TCHAR expected[]{LR"(;; Initial-state seed exported from Unreal Editor.

(level
  (id 'writer-example)
  (title "Writer \"Example\"")
  (description "Line one\nLine two.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (entities
    ;; Team: blue | Archetype: player-fighter | Count: 1
    (entity 'player 'player-fighter 'blue
      (position 0 200.123 -300.5)
      (rotation 10 20.25 0))

    ;; Team: blue | Archetype: capital-ship | Count: 1
    (entity 'blue-capital 'capital-ship 'blue
      (position 1000 2000 3000)
      (rotation 0 90 0))

    ;; Team: red | Archetype: static-turret | Count: 1
    (entity 'red-turret 'static-turret 'red
      (position -1000 -2000 -3000)
      (rotation 0 -90 0))))
)"};
        TestRunner->TestEqual(TEXT("Canonical source matches"), *source, FString{expected});
    }

    TEST_METHOD(OutputDoesNotDependOnDefinitionInsertionOrder)
    {
        auto const first{ml::s7::emit_initial_level_source(make_player_level(false))};
        auto const second{ml::s7::emit_initial_level_source(make_player_level(true))};
        if (!TestRunner->TestTrue(TEXT("First source emits"), first.has_value()) ||
            !TestRunner->TestTrue(TEXT("Second source emits"), second.has_value())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Output is byte-for-byte stable"), *first, *second);
    }

    TEST_METHOD(EmittedSourceReadsBackWithSupportedSemantics)
    {
        auto const source{ml::s7::emit_initial_level_source(make_player_level())};
        if (!TestRunner->TestTrue(TEXT("Source emits"), source.has_value())) {
            return;
        }

        ml::s7::FLevelDefinitionReader reader;
        auto const read{reader.read_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Emitted source reads"), static_cast<bool>(read))) {
            TestRunner->AddError(read.script_error);
            return;
        }

        auto const& definition{read.definition.GetValue()};
        TestRunner->TestEqual(TEXT("All entities survive"), definition.entities.num(), 3);
        TestRunner->TestTrue(TEXT("Player identity survives"),
                             definition.player_entity_id ==
                                 ml::FLevelEntityId{FName{TEXT("player")}});
        TestRunner->TestTrue(TEXT("Rounded position survives"),
                             definition.entities.positions.get_const_view()[0].Equals(
                                 FVector{0.0, 200.123, -300.5}, 0.001));
        TestRunner->TestTrue(TEXT("Rotation survives"),
                             FRotator{definition.entities.rotations.pitches[0],
                                      definition.entities.rotations.yaws[0],
                                      definition.entities.rotations.rolls[0]}
                                 .Equals(FRotator{10.0, 20.25, 0.0}, 0.001));
    }

    TEST_METHOD(RejectsRuntimeOnlyDefinitionFields)
    {
        auto definition{make_player_level()};
        definition.entities.spawn_times_seconds[1] = 5.0;
        auto const source{ml::s7::emit_initial_level_source(definition)};

        TestRunner->TestFalse(TEXT("Scheduled entity is rejected"), source.has_value());
        if (!source) {
            TestRunner->TestTrue(TEXT("Failure identifies t=0 semantics"),
                                 source.error().Contains(TEXT("initial t=0")));
        }
    }

    TEST_METHOD(ExpandsLargeProceduralLevelIntoExplicitSemanticEquivalent)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const input{reader.read_file(FPaths::Combine(
            FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("BenchmarkFleet_10.scm")))};
        if (!TestRunner->TestTrue(TEXT("Procedural benchmark reads"), static_cast<bool>(input))) {
            TestRunner->AddError(input.script_error);
            return;
        }

        auto const source{ml::s7::emit_initial_level_source(input.definition.GetValue())};
        if (!TestRunner->TestTrue(TEXT("Large benchmark emits"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Writer does not reconstruct helper definitions"),
                              source->Contains(TEXT("(define")));
        TestRunner->TestFalse(TEXT("Writer does not reconstruct procedural application"),
                              source->Contains(TEXT("(apply entities")));

        auto const output{reader.read_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Expanded benchmark reads"), static_cast<bool>(output))) {
            TestRunner->AddError(output.script_error);
            return;
        }
        TestRunner->TestEqual(
            TEXT("All capitals survive expansion"), output.definition->entities.num(), 320);
        TestRunner->TestTrue(TEXT("Observer camera survives expansion"),
                             output.definition->camera.IsSet());
    }
};
