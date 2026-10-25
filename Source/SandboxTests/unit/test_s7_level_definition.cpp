#include <ioj/s7/interpreter.h>
#include <ioj/s7/reader.h>
#include <ioj/settings/settings.h>
#include <SpaceGameS7/definition_reader.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/VectorConversion.h>

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
    TEST_METHOD(AssignsUnrealVectorSettingsAtomically)
    {
        ioj::s7::Interpreter interpreter;
        auto const assign{[&]<typename T>(std::string_view source, T& target) {
            auto ast{interpreter.evaluate_ast(source)};
            if (!ast) {
                return false;
            }
            auto value{ioj::s7::read_override(ioj::s7::Reader{*ast}, ast->root, "vector", target)};
            if (!value) {
                return false;
            }
            target = *value;
            return true;
        }};
        ioj::Vector2d size{1., 2.};
        TestRunner->TestTrue(TEXT("Two-component setting converts"), assign("'(3 4)", size));
        TestRunner->TestEqual(
            TEXT("Both components are assigned"), ioj::to_unreal(size), FVector2D{3., 4.});
        TestRunner->TestFalse(TEXT("Wrong dimensions fail"), assign("'(5 6 7)", size));
        TestRunner->TestEqual(TEXT("Dimension failure preserves destination"),
                              ioj::to_unreal(size),
                              FVector2D{3., 4.});

        ioj::Vector3f offset{1.f, 2.f, 3.f};
        TestRunner->TestFalse(TEXT("Out-of-range component fails"), assign("'(4 5 1e100)", offset));
        TestRunner->TestEqual(TEXT("Conversion failure preserves every component"),
                              ioj::to_unreal(offset),
                              FVector3f{1.f, 2.f, 3.f});
        TestRunner->TestTrue(TEXT("Three-component setting converts"), assign("'(4 5 6)", offset));
        TestRunner->TestEqual(
            TEXT("Every component is assigned"), ioj::to_unreal(offset), FVector3f{4.f, 5.f, 6.f});
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
};
