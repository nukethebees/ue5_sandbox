#include <SpaceGameS7/definition_reader.h>
#include <SpaceGameS7/level_definition_writer.h>

#include <SandboxCoreEngine/strings.h>

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
        auto const source{::ioj::levels::authoring::emit_editor_level_source(make_player_level())};
        if (!TestRunner->TestTrue(TEXT("Source is emitted"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }

        TestRunner->TestTrue(TEXT("Literal vectors are quoted"),
                             source->Contains(TEXT(":position '(0 200.123 -300.5)")));
        TestRunner->TestTrue(TEXT("Literal identifiers are quoted together"),
                             source->Contains(TEXT(":teams '(blue red)")));
        TestRunner->TestTrue(TEXT("Entity constructors are evaluated"),
                             source->Contains(TEXT(":entities (list")));
    }

    TEST_METHOD(OutputDoesNotDependOnDefinitionInsertionOrder)
    {
        auto const first{
            ::ioj::levels::authoring::emit_editor_level_source(make_player_level(false))};
        auto const second{
            ::ioj::levels::authoring::emit_editor_level_source(make_player_level(true))};
        if (!TestRunner->TestTrue(TEXT("First source emits"), first.has_value()) ||
            !TestRunner->TestTrue(TEXT("Second source emits"), second.has_value())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Output is byte-for-byte stable"), *first, *second);
    }

    TEST_METHOD(EmittedSourceReadsBackWithSupportedSemantics)
    {
        auto const source{::ioj::levels::authoring::emit_editor_level_source(make_player_level())};
        if (!TestRunner->TestTrue(TEXT("Source emits"), source.has_value())) {
            return;
        }

        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const read{reader.read_level_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Emitted source reads"), static_cast<bool>(read))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(read.error())));
            return;
        }

        auto const& definition{read.value()};
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

    TEST_METHOD(EmitsScheduledEntities)
    {
        auto definition{make_player_level()};
        definition.entities.spawn_times_seconds[1] = 5.0;
        auto const source{::ioj::levels::authoring::emit_editor_level_source(definition)};

        if (!TestRunner->TestTrue(TEXT("Scheduled entity is emitted"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }
        TestRunner->TestTrue(TEXT("Spawn clause is emitted"),
                             source->Contains(TEXT(":spawn-at 5")));

        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const read{reader.read_level_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Scheduled source reads"), static_cast<bool>(read))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(read.error())));
            return;
        }
        TestRunner->TestEqual(
            TEXT("Spawn time survives"), read->entities.spawn_times_seconds[1], 5.0);
    }

    TEST_METHOD(ExpandsLargeProceduralLevelIntoExplicitSemanticEquivalent)
    {
        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const input{reader.read_level_file(FPaths::Combine(
            FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("BenchmarkFleet_10.scm")))};
        if (!TestRunner->TestTrue(TEXT("Procedural benchmark reads"), static_cast<bool>(input))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(input.error())));
            return;
        }

        auto const source{::ioj::levels::authoring::emit_editor_level_source(input.value())};
        if (!TestRunner->TestTrue(TEXT("Large benchmark emits"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Writer does not reconstruct helper definitions"),
                              source->Contains(TEXT("(define")));
        TestRunner->TestFalse(TEXT("Writer does not reconstruct procedural application"),
                              source->Contains(TEXT("(apply entities")));

        auto const output{reader.read_level_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Expanded benchmark reads"), static_cast<bool>(output))) {
            TestRunner->AddError(ml::to_fstring(::ioj::levels::format_diagnostics(output.error())));
            return;
        }
        TestRunner->TestEqual(TEXT("All capitals survive expansion"), output->entities.num(), 320);
        TestRunner->TestTrue(TEXT("Observer camera survives expansion"), output->camera.IsSet());
    }
};
