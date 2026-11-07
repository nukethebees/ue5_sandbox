#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/sim/levels/level_sim.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SandboxEditor/SandboxEditor.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Modules/ModuleManager.h>
#include <Tests/AutomationEditorCommon.h>

#include <algorithm>
#include <expected>

namespace s7_level_authoring_tests {
struct FTemporarySourceDirectory {
    FTemporarySourceDirectory() { IFileManager::Get().MakeDirectory(*path, true); }
    ~FTemporarySourceDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }

    auto root() const -> FString { return FPaths::Combine(path, TEXT("Authoring.scm")); }

    FString path{FPaths::Combine(FPaths::ProjectSavedDir(),
                                 TEXT("Automation"),
                                 TEXT("S7LevelAuthoring"),
                                 FGuid::NewGuid().ToString())};
};

template <typename Value>
auto assert_success(FAutomationTestBase& test,
                    TCHAR const* description,
                    std::expected<Value, FString> const& result) -> bool {
    if (test.TestTrue(description, result.has_value())) {
        return true;
    }
    test.AddError(result.error());
    return false;
}

auto write_source(FString const& path, FString const& source) -> bool {
    return FFileHelper::SaveStringToFile(
        source, *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

auto binding(AS7LevelAuthoringDocument& document, FString const& id) -> FS7LevelEntityBinding* {
    return document.entities.FindByPredicate([id](auto const& entry) { return entry.id == id; });
}

auto entity(ioj::levels::LevelDefinition const& definition, std::string_view const id)
    -> ioj::levels::EntitySpawnDefinition const* {
    auto const& entities{definition.entities()};
    auto const found{
        std::ranges::find_if(entities, [id](auto const& value) { return value.id.value == id; })};
    return found == entities.end() ? nullptr : &*found;
}
}

TEST_CLASS(S7LevelAuthoring, "Sandbox.UnitTests")
{
    TEST_METHOD(ExportComposesWithHumanSourceAndPreservesOwnership)
    {
        using namespace s7_level_authoring_tests;
        using namespace ioj::levels;
        using ioj::Team;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        if (!TestRunner->TestNotNull(TEXT("Editor world"), world)) {
            return;
        }
        auto& level{*world->GetCurrentLevel()};
        auto created{ioj::editor::create_level_authoring_document(level, directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring document"), created)) {
            return;
        }
        auto& document{**created};
        auto baseline{ioj::editor::read_s7_editor_input(document)};
        if (!assert_success(*TestRunner, TEXT("Read generated baseline"), baseline)) {
            return;
        }
        baseline->id = LevelId{"export-test"};
        baseline->title = "Export test";
        baseline->player = EntityId{"player"};
        baseline->entities = {
            {.id = EntityId{"player"},
             .archetype = EntityArchetype::PlayerFighter,
             .team = Team::Blue},
            {.id = EntityId{"capital"},
             .archetype = EntityArchetype::CapitalShip,
             .team = Team::Red,
             .position = {100000., 0., 0.}},
            {.id = EntityId{"turret"},
             .archetype = EntityArchetype::StaticTurret,
             .team = Team::Blue,
             .position = {-100000., 0., 0.}},
            {.id = EntityId{"spinner"},
             .archetype = EntityArchetype::TubeSpinner,
             .team = Team::White,
             .position = {300000., 0., 0.}},
        };
        auto const generated_path{ioj::editor::s7_editor_config_path(document)};
        auto const human_source{
            FString{TEXT("; Human-owned source must survive editor export.\n"
                         "(let ((base (load-script \"Authoring/editor-config.scm\")))\n"
                         "  (level-with base\n"
                         "    :entities (append (level-ref base :entities)\n"
                         "      (list (entity :id 'human :archetype 'capital-ship :team 'red\n"
                         "        :position '(600000 0 0) :initial-spawn-delay 60)))\n"
                         "    :mission (mission :objectives (list\n"
                         "      (objective :id 'destroy-capital :team 'blue\n"
                         "        :pass (entity-destroyed 'capital))))))\n")}};
        if (!TestRunner->TestTrue(
                TEXT("Write initial generated records"),
                write_source(generated_path,
                             ioj::to_fstring(authoring::emit_level_source(*baseline)))) ||
            !TestRunner->TestTrue(TEXT("Write composed human root"),
                                  write_source(directory.root(), human_source))) {
            return;
        }
        auto definition{ioj::editor::read_s7_authored_level(document)};
        if (!assert_success(*TestRunner, TEXT("Read composed level"), definition) ||
            !assert_success(*TestRunner,
                            TEXT("Materialize composed level"),
                            ioj::editor::import_s7_level(level, document, *definition))) {
            return;
        }
        auto* const player_binding{binding(document, TEXT("player"))};
        auto* const capital_binding{binding(document, TEXT("capital"))};
        auto* const turret_binding{binding(document, TEXT("turret"))};
        auto* const spinner_binding{binding(document, TEXT("spinner"))};
        auto* const human_binding{binding(document, TEXT("human"))};
        if (!TestRunner->TestNotNull(TEXT("Player binding"), player_binding) ||
            !TestRunner->TestNotNull(TEXT("Capital binding"), capital_binding) ||
            !TestRunner->TestNotNull(TEXT("Turret binding"), turret_binding) ||
            !TestRunner->TestNotNull(TEXT("Spinner binding"), spinner_binding) ||
            !TestRunner->TestNotNull(TEXT("Human binding"), human_binding)) {
            return;
        }
        TestRunner->TestTrue(
            TEXT("Generated entities are exported"),
            player_binding->export_to_script && capital_binding->export_to_script &&
                turret_binding->export_to_script && spinner_binding->export_to_script);
        TestRunner->TestFalse(TEXT("Human entity is not exported"),
                              human_binding->export_to_script);
        auto* const capital{Cast<ATestCapitalShipProxy>(capital_binding->actor)};
        auto* const turret{Cast<ATestStaticTurretsProxy>(turret_binding->actor)};
        auto* const spinner{Cast<ATestTubeSpinnerProxy>(spinner_binding->actor)};
        if (!TestRunner->TestNotNull(TEXT("Capital proxy"), capital) ||
            !TestRunner->TestNotNull(TEXT("Turret proxy"), turret) ||
            !TestRunner->TestNotNull(TEXT("Spinner proxy"), spinner) ||
            !TestRunner->TestTrue(TEXT("Spinner has multiple fire points"),
                                  document.level_config->tube_spinners.fire_point_offsets.Num() >
                                      1)) {
            return;
        }
        player_binding->player_health = 123;
        capital_binding->spawn_time_seconds = 0.1;
        capital->SetActorLocation(FVector{100000.123456789, 42.125, 100.5});
        capital->SetActorRotation(FRotator{10., 20., 30.});
        capital->set_health(456);
        capital->set_initial_spawn_delay(7.25f);
        capital->set_spawn_cooldown(9.5f);
        turret->set_health(789);
        turret->set_laser_damage(321);
        spinner->SetActorRotation(FRotator{0., 45., 0.});
        spinner->set_initial_active_fire_point(1);
        human_binding->actor->SetActorLocation(FVector{700000., 0., 0.});

        if (!assert_success(*TestRunner,
                            TEXT("Export editor-owned records"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        FString saved_root;
        TestRunner->TestTrue(TEXT("Read human source"),
                             FFileHelper::LoadFileToString(saved_root, *directory.root()));
        TestRunner->TestEqual(TEXT("Human source is untouched"), saved_root, human_source);
        auto exported{ioj::editor::read_s7_editor_input(document)};
        if (!assert_success(*TestRunner, TEXT("Read exported data"), exported)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Only four entities are exported"), exported->entities.size(), size_t{4});
        definition = ioj::editor::read_s7_authored_level(document);
        if (!assert_success(*TestRunner, TEXT("Recompose exported level"), definition)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Human entity appears once"), definition->entities().size(), size_t{5});
        auto const* saved_capital{entity(*definition, "capital")};
        auto const* saved_turret{entity(*definition, "turret")};
        auto const* saved_spinner{entity(*definition, "spinner")};
        auto const* saved_player{entity(*definition, "player")};
        auto const* saved_human{entity(*definition, "human")};
        if (!TestRunner->TestTrue(TEXT("Stable authored IDs survive export"),
                                  saved_capital && saved_turret && saved_spinner && saved_player &&
                                      saved_human)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Capital transform retains precision"),
                             saved_capital->position.x() == 100000.123456789 &&
                                 saved_capital->position.y() == 42.125 &&
                                 saved_capital->position.z() == 100.5 &&
                                 FMath::IsNearlyEqual(saved_capital->rotation.pitch, 10.) &&
                                 FMath::IsNearlyEqual(saved_capital->rotation.yaw, 20.) &&
                                 FMath::IsNearlyEqual(saved_capital->rotation.roll, 30.));
        TestRunner->TestTrue(TEXT("Capital overrides and spawn time survive"),
                             saved_capital->spawn_time_seconds == 0.1 &&
                                 saved_capital->overrides.health == 456 &&
                                 saved_capital->overrides.initial_spawn_delay == 7.25f &&
                                 saved_capital->overrides.spawn_cooldown == 9.5f);
        TestRunner->TestTrue(TEXT("Turret overrides survive"),
                             saved_turret->overrides.health == 789 &&
                                 saved_turret->overrides.laser_damage == 321);
        TestRunner->TestTrue(TEXT("Spinner overrides survive"),
                             saved_spinner->overrides.initial_fire_point == 1);
        TestRunner->TestTrue(TEXT("Spinner rotation survives"),
                             FMath::IsNearlyEqual(saved_spinner->rotation.yaw, 45.));
        TestRunner->TestTrue(TEXT("Player override survives"),
                             saved_player->overrides.health == 123);
        TestRunner->TestTrue(TEXT("Human source controls its entity"),
                             saved_human->position.x() == 600000.);

        auto const written_at{IFileManager::Get().GetTimeStamp(*generated_path)};
        if (!assert_success(*TestRunner,
                            TEXT("Repeat unchanged export"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        TestRunner->TestEqual(TEXT("Unchanged output is not rewritten"),
                              IFileManager::Get().GetTimeStamp(*generated_path),
                              written_at);
        auto compiled{
            ioj::editor::compile_playable_s7_definition(*definition, *document.level_config)};
        if (!assert_success(
                *TestRunner, TEXT("Compile composed level and objective references"), compiled)) {
            return;
        }
        ioj::sim::LevelSim simulation{std::move(*compiled)};
        simulation.finish_initialisation();
        TestRunner->TestEqual(TEXT("Only human capital spawns at time zero"),
                              simulation.capital_ships().num_instances(),
                              uint32{1});
        TestRunner->TestEqual(TEXT("Spinner spawns through authored events"),
                              simulation.spinners().num_instances(),
                              uint32{1});
        simulation.start();
        simulation.advance(0.2);
        TestRunner->TestEqual(TEXT("Delayed exported capital spawns"),
                              simulation.capital_ships().num_instances(),
                              uint32{2});

        auto* const previous_human{human_binding->actor.Get()};
        human_binding->export_to_script = true;
        if (!assert_success(*TestRunner,
                            TEXT("Refresh composed scene"),
                            ioj::editor::import_s7_level(level, document, *definition))) {
            return;
        }
        auto* const refreshed_human{binding(document, TEXT("human"))};
        TestRunner->TestTrue(TEXT("Explicit ownership choice persists by ID"),
                             refreshed_human && refreshed_human->export_to_script &&
                                 refreshed_human->actor == previous_human);
        if (!assert_success(*TestRunner,
                            TEXT("Import with new-root ownership"),
                            ioj::editor::import_s7_level(level, document, *definition, true))) {
            return;
        }
        auto* const new_root_human{binding(document, TEXT("human"))};
        if (!TestRunner->TestNotNull(TEXT("Human binding after root change"), new_root_human)) {
            return;
        }
        TestRunner->TestFalse(TEXT("Root changes discard the previous source's ownership choices"),
                              new_root_human->export_to_script);
        new_root_human->export_to_script = true;
        if (!assert_success(*TestRunner,
                            TEXT("Export explicitly adopted human entity"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        TestRunner->TestFalse(TEXT("Duplicate declaration is reported by normal validation"),
                              ioj::editor::read_s7_authored_level(document).has_value());
    }

    TEST_METHOD(ImportEditsTheBaselineWithoutCompoundingHumanTransforms)
    {
        using namespace s7_level_authoring_tests;
        using namespace ioj::levels;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto& level{*world->GetCurrentLevel()};
        auto created{ioj::editor::create_level_authoring_document(level, directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring document"), created)) {
            return;
        }
        auto& document{**created};
        auto baseline{ioj::editor::read_s7_editor_input(document)};
        if (!assert_success(*TestRunner, TEXT("Read baseline"), baseline)) {
            return;
        }
        baseline->title = "Base";
        baseline->player = EntityId{"none"};
        baseline->entities = {{.id = EntityId{"none"},
                               .archetype = EntityArchetype::PlayerFighter,
                               .team = ioj::Team::Blue,
                               .position = {100., 0., 0.}}};
        if (!TestRunner->TestTrue(
                TEXT("Write generated baseline"),
                write_source(ioj::editor::s7_editor_config_path(document),
                             ioj::to_fstring(authoring::emit_level_source(*baseline)))) ||
            !TestRunner->TestTrue(
                TEXT("Write human transformations"),
                write_source(
                    directory.root(),
                    TEXT("(let ((base (load-script \"Authoring/editor-config.scm\")))\n"
                         "  (level-with base\n"
                         "    :title (string-append (level-ref base :title) \" suffix\")\n"
                         "    :entities (map (lambda (ship)\n"
                         "      (entity-with ship :position\n"
                         "        (map (lambda (x) (+ x 10)) (entity-ref ship :position))))\n"
                         "      (level-ref base :entities))))\n")))) {
            return;
        }
        for (auto iteration{0}; iteration < 2; ++iteration) {
            auto definition{ioj::editor::read_s7_authored_level(document)};
            if (!assert_success(*TestRunner, TEXT("Read composed level"), definition) ||
                !assert_success(*TestRunner,
                                TEXT("Import baseline for editing"),
                                ioj::editor::import_s7_level(level, document, *definition))) {
                return;
            }
            auto const* pilot{binding(document, TEXT("none"))};
            if (!TestRunner->TestNotNull(TEXT("The authored symbol none remains a valid binding"),
                                         pilot)) {
                return;
            }
            TestRunner->TestTrue(TEXT("Exported proxy retains baseline coordinates"),
                                 pilot->actor->GetActorLocation() == FVector{100., 0., 0.});
            TestRunner->TestTrue(TEXT("Human title transform executes once"),
                                 definition->metadata().title == "Base suffix");
            TestRunner->TestTrue(TEXT("Human position transform executes once"),
                                 definition->entities().front().position ==
                                     ioj::Vector3d{110., 10., 10.});
            if (!assert_success(*TestRunner,
                                TEXT("Export unchanged baseline"),
                                ioj::editor::export_s7_editor_config(level, document))) {
                return;
            }
            auto saved{ioj::editor::read_s7_editor_input(document)};
            if (!assert_success(*TestRunner, TEXT("Read exported baseline"), saved)) {
                return;
            }
            TestRunner->TestTrue(TEXT("Composed values are never copied into the baseline"),
                                 *saved == *baseline);
        }
    }

    TEST_METHOD(NewProxiesExportThroughTheGeneratedEntryPoint)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto& level{*world->GetCurrentLevel()};
        auto created{ioj::editor::create_level_authoring_document(level, directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring files"), created)) {
            return;
        }
        auto& document{**created};
        auto* const player{world->SpawnActor<ATestSpaceShip>(
            document.level_config->classes.player_ship_class.Get(), FTransform::Identity)};
        if (!TestRunner->TestNotNull(TEXT("New player proxy"), player)) {
            return;
        }
        player->SetActorLabel(TEXT("pilot"));
        player->set_team(ETestTeam::Blue);
        if (!assert_success(*TestRunner,
                            TEXT("Export adopts newly created proxy"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        auto const* adopted{binding(document, TEXT("pilot"))};
        TestRunner->TestTrue(TEXT("New proxy defaults to Export"),
                             adopted && adopted->export_to_script && adopted->actor == player);
        auto definition{ioj::editor::read_s7_authored_level(document)};
        if (!assert_success(
                *TestRunner, TEXT("Generated root includes its own configuration"), definition)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Entry point resolves the adopted player"),
                             definition->player_entity_id() == ioj::levels::EntityId{"pilot"});
    }

    TEST_METHOD(CreatingAuthoringFilesPreservesExistingHumanSource)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto const source{FString{TEXT("; Keep my source\n(level :title \"Human\")\n")}};
        if (!TestRunner->TestTrue(TEXT("Write existing human file"),
                                  write_source(directory.root(), source))) {
            return;
        }
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto created{ioj::editor::create_level_authoring_document(*world->GetCurrentLevel(),
                                                                  directory.root())};
        if (!assert_success(*TestRunner, TEXT("Configure existing entry point"), created)) {
            return;
        }
        FString saved;
        TestRunner->TestTrue(TEXT("Read preserved file"),
                             FFileHelper::LoadFileToString(saved, *directory.root()));
        TestRunner->TestEqual(TEXT("Existing human source is never replaced"), saved, source);
        TestRunner->TestTrue(TEXT("Incomplete generated baseline is readable"),
                             ioj::editor::read_s7_editor_input(**created).has_value());
    }

    TEST_METHOD(MissingAndInvalidEntryPointsDenyPIEPermission)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto created{ioj::editor::create_level_authoring_document(*world->GetCurrentLevel(),
                                                                  directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring document"), created)) {
            return;
        }
        auto* const orchestrator{world->SpawnActor<ATestBatchOrchestrator>()};
        if (!TestRunner->TestNotNull(TEXT("Playable orchestrator"), orchestrator)) {
            return;
        }
        orchestrator->set_start_mode(EOrchestratorStartMode::AuthoredLevel);
        auto& module{
            FModuleManager::LoadModuleChecked<FSandboxEditorModule>(TEXT("SandboxEditor"))};
        if (!TestRunner->TestTrue(TEXT("Remove entry point"),
                                  IFileManager::Get().Delete(*directory.root()))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Missing mandatory root rejects PIE"),
                             module.RequestPIEPermissionInternal(false).HasError());
        if (!TestRunner->TestTrue(TEXT("Write malformed root"),
                                  write_source(directory.root(), TEXT("(level")))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Invalid root rejects PIE"),
                             module.RequestPIEPermissionInternal(false).HasError());
        orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
        auto const unrelated{module.RequestPIEPermissionInternal(false)};
        TestRunner->TestTrue(TEXT("Non-playable utility world is not subject to mandatory scripts"),
                             unrelated.HasValue() && unrelated.GetValue());
    }
};
