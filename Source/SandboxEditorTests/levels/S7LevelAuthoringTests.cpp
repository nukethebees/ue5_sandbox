#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_writer.h>
#include <ioj/sim/levels/level_sim.h>
#include <SandboxEditor/levels/S7EntityGeometryExport.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighter/TestFighterProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>

#include <SandboxCoreEngine/strings.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <CQTest.h>
#include <Editor.h>
#include <Editor/UnrealEdEngine.h>
#include <EditorModeManager.h>
#include <Engine/Level.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <FileHelpers.h>
#include <HAL/FileManager.h>
#include <IDetailTreeNode.h>
#include <IPropertyRowGenerator.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>
#include <PropertyEditorModule.h>
#include <PropertyHandle.h>
#include <ScopedTransaction.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/MemoryWriter.h>
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include <Tests/AutomationEditorCommon.h>
#include <UnrealEdGlobals.h>
#include <UObject/ObjectSaveContext.h>

#include <algorithm>
#include <array>
#include <expected>
#include <memory_resource>
#include <optional>

namespace s7_level_authoring_tests {
template <typename Proxy>
void set_custom_name(Proxy& proxy, FName name) {
    proxy.custom_name = name;
    FPropertyChangedEvent change{FindFProperty<FProperty>(proxy.GetClass(), TEXT("custom_name"))};
    proxy.PostEditChangeProperty(change);
}

struct FTemporarySourceDirectory {
    FTemporarySourceDirectory() {
        IFileManager::Get().MakeDirectory(*FPaths::Combine(path, TEXT("Authoring")), true);
        auto const settings{FPaths::Combine(path, TEXT("Settings"))};
        IFileManager::Get().MakeDirectory(*settings, true);
        IFileManager::Get().Copy(*FPaths::Combine(settings, TEXT("entity-geometry.scm")),
                                 *ioj::editor::entity_geometry_path());
        FFileHelper::SaveStringToFile(TEXT(
                                          R"scm((export game-settings)
(define game-settings
  (settings
    :entity-geometry (module-ref (import-module "Settings/entity-geometry.scm") 'entity-geometry)
    :tick (tick :tick-rate 75))))scm"),
                                      *FPaths::Combine(settings, TEXT("level-defaults.scm")));
    }
    ~FTemporarySourceDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }

    auto root() const -> FString { return FPaths::Combine(path, TEXT("Authoring/Authoring.scm")); }

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

bool edit_team_in_details(TArray<UObject*> const& selection, FString const& team) {
    auto& module{FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor")};
    auto rows{module.CreatePropertyRowGenerator({})};
    rows->SetObjects(selection);
    auto nodes{rows->GetRootTreeNodes()};
    while (!nodes.IsEmpty()) {
        auto node{nodes.Pop()};
        if (node->GetNodeName() == TEXT("team")) {
            auto handle{node->CreatePropertyHandle()};
            return handle && handle->SetValueFromFormattedString(team) == FPropertyAccess::Success;
        }
        TArray<TSharedRef<IDetailTreeNode>> children;
        node->GetChildren(children);
        nodes.Append(children);
    }

    return false;
}

auto binding(AS7LevelAuthoringDocument& document, FString const& id) -> FS7LevelEntityBinding* {
    return document.entities.FindByPredicate([id](auto const& entry) { return entry.id == id; });
}

auto entity(ioj::levels::LevelDefinition const& definition, std::string_view const id)
    -> ioj::levels::EntitySpawnDefinition const* {
    auto const entities{definition.entities()};
    auto const found{
        std::ranges::find_if(entities, [id](auto const& value) { return value.id.value == id; })};
    return found == entities.end() ? nullptr : &*found;
}
struct FComposedLevelFixture {
    UWorld* world;
    AS7LevelAuthoringDocument* document;
    FString generated_path;
    FString human_source;
    ioj::levels::LevelDefinition definition;
};

auto make_composed_fixture(FAutomationTestBase& test, FTemporarySourceDirectory const& directory)
    -> std::optional<FComposedLevelFixture> {
    using namespace ioj::levels;
    using ioj::Team;
    auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
    if (!test.TestNotNull(TEXT("Editor world"), world)) {
        return std::nullopt;
    }

    auto& level{*world->GetCurrentLevel()};
    auto created{
        ioj::editor::set_up_s7_level(level, ioj::editor::LevelSetup::Create, directory.root())};
    if (!assert_success(test, TEXT("Create authoring document"), created)) {
        return std::nullopt;
    }

    auto& document{**created};
    auto baseline{ioj::editor::read_s7_editor_input(document)};
    if (!assert_success(test, TEXT("Read generated baseline"), baseline)) {
        return std::nullopt;
    }
    baseline->id = LevelId{"export-test"};
    baseline->title = "Export test";
    baseline->player = EntityId{"player"};
    baseline->entities = {
        {.id = EntityId{"player"}, .archetype = EntityArchetype::PlayerFighter, .team = Team::Blue},
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
    auto const human_source{FString{TEXT(R"scm(; Human-owned source must survive editor export.
(export level-definition)
(define base (module-ref (import-module "Authoring/editor-config.scm") 'scene))
(define level-definition
  (copy-with base
    :settings (merge-settings
      (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
      (level-ref base :settings))
    :entities (append (level-ref base :entities)
      (list (entity :id 'human :archetype 'capital-ship :team 'red
        :position '(600000 0 0) :initial-spawn-delay 60)))
    :logic (logic
      (on :when (entity-destroyed 'capital)
          :do (win 'blue)))))
)scm")}};
    if (!test.TestTrue(
            TEXT("Write initial generated records"),
            write_source(generated_path,
                         ioj::to_fstring(authoring::emit_editor_level_source(*baseline)))) ||
        !test.TestTrue(TEXT("Write composed human root"),
                       write_source(directory.root(), human_source))) {
        return std::nullopt;
    }

    auto definition{ioj::editor::read_s7_authored_level(document)};
    if (!assert_success(test, TEXT("Read composed level"), definition) ||
        !assert_success(test,
                        TEXT("Materialize composed level"),
                        ioj::editor::import_s7_level(level, document))) {
        return std::nullopt;
    }

    auto* const player_binding{binding(document, TEXT("player"))};
    auto* const capital_binding{binding(document, TEXT("capital"))};
    auto* const turret_binding{binding(document, TEXT("turret"))};
    auto* const spinner_binding{binding(document, TEXT("spinner"))};
    auto* const human_binding{binding(document, TEXT("human"))};
    if (!test.TestNotNull(TEXT("Player binding"), player_binding) ||
        !test.TestNotNull(TEXT("Capital binding"), capital_binding) ||
        !test.TestNotNull(TEXT("Turret binding"), turret_binding) ||
        !test.TestNotNull(TEXT("Spinner binding"), spinner_binding) ||
        !test.TestNotNull(TEXT("Human binding"), human_binding)) {
        return std::nullopt;
    }

    auto* const player{CastChecked<ioj::ATestSpaceShip>(player_binding->actor)};
    auto* const capital{Cast<ioj::ATestCapitalShipProxy>(capital_binding->actor)};
    auto* const turret{Cast<ioj::ATestStaticTurretsProxy>(turret_binding->actor)};
    auto* const spinner{Cast<ioj::ATestTubeSpinnerProxy>(spinner_binding->actor)};
    if (!test.TestNotNull(TEXT("Capital proxy"), capital) ||
        !test.TestNotNull(TEXT("Turret proxy"), turret) ||
        !test.TestNotNull(TEXT("Spinner proxy"), spinner) ||
        !test.TestTrue(TEXT("Spinner has multiple fire points"),
                       document.level_config->tube_spinners.fire_point_offsets.Num() > 1)) {
        return std::nullopt;
    }

    test.TestTrue(TEXT("Generated entities are exported"),
                  player->export_to_script && capital->export_to_script &&
                      turret->export_to_script && spinner->export_to_script);
    test.TestFalse(TEXT("Human entity is not exported"),
                   CastChecked<ioj::ATestCapitalShipProxy>(human_binding->actor)->export_to_script);
    player->health_override = 123;
    capital->spawn_time_seconds = 0.1;
    capital->SetActorLocation(FVector{100000.123456789, 42.125, 100.5});
    capital->SetActorRotation(FRotator{10., 20., 30.});
    capital->health = 456;
    capital->initial_spawn_delay = 7.25f;
    capital->spawn_cooldown = 9.5f;
    turret->health = 789;
    turret->laser_damage = 321;
    spinner->SetActorRotation(FRotator{0., 45., 0.});
    spinner->initial_fire_point = 1;
    human_binding->actor->SetActorLocation(FVector{700000., 0., 0.});

    if (!assert_success(test,
                        TEXT("Export editor-owned records"),
                        ioj::editor::export_s7_editor_config(level, document))) {
        return std::nullopt;
    }

    auto exported_definition{ioj::editor::read_s7_authored_level(document)};
    if (!assert_success(test, TEXT("Recompose exported level"), exported_definition)) {
        return std::nullopt;
    }

    return FComposedLevelFixture{
        world, &document, generated_path, human_source, std::move(*exported_definition)};
}

}

TEST_CLASS(S7LevelAuthoring, "Sandbox.UnitTests")
{
    TEST_METHOD(ProxiesPersistAuthoredTeamsAndOverrides)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto created{ioj::editor::set_up_s7_level(
            *world->GetCurrentLevel(), ioj::editor::LevelSetup::Create, directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring document"), created)) {
            return;
        }

        auto* const document{*created};
        auto* const capital_actor{world->SpawnActor<ioj::ATestCapitalShipProxy>()};
        auto* const turret_actor{world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        auto* const spinner_actor{world->SpawnActor<ioj::ATestTubeSpinnerProxy>()};
        auto* const player_actor{world->SpawnActor<ioj::ATestSpaceShip>()};
        capital_actor->set_team(ioj::ETestTeam::Red);
        capital_actor->spawn_time_seconds = 7.5;
        capital_actor->health = 456;
        capital_actor->initial_spawn_delay = 2.25f;
        capital_actor->spawn_cooldown = 3.5f;
        turret_actor->set_team(ioj::ETestTeam::Blue);
        turret_actor->export_to_script = false;
        turret_actor->spawn_time_seconds = 1.5;
        turret_actor->health = 789;
        turret_actor->laser_damage = 321;
        spinner_actor->spawn_time_seconds = 2.5;
        spinner_actor->initial_fire_point = 1;
        player_actor->set_team(ioj::ETestTeam::Green);
        player_actor->health_override = 123;
        capital_actor->SetActorLabel(TEXT("capital"));
        spinner_actor->SetActorLabel(TEXT("spinner"));
        player_actor->SetActorLabel(TEXT("player"));
        document->entities = {
            {.id = TEXT("old-capital"), .actor = capital_actor},
            {.id = TEXT("turret"), .actor = turret_actor},
            {.id = TEXT("spinner"), .actor = spinner_actor},
            {.id = TEXT("player"), .actor = player_actor},
        };
        std::array<AActor*, 5> const actors{
            document, capital_actor, turret_actor, spinner_actor, player_actor};
        TArray<uint8> data;
        {
            FMemoryWriter memory{data, true};
            FObjectAndNameAsStringProxyArchive archive{memory, false};
            for (auto* actor : actors) {
                actor->Serialize(archive);
            }
        }
        document->entities.Reset();
        capital_actor->set_team(ioj::ETestTeam::White);
        capital_actor->spawn_time_seconds = 0.;
        capital_actor->health.Reset();
        capital_actor->initial_spawn_delay.Reset();
        capital_actor->spawn_cooldown.Reset();
        turret_actor->set_team(ioj::ETestTeam::White);
        turret_actor->export_to_script = true;
        turret_actor->spawn_time_seconds = 0.;
        turret_actor->health.Reset();
        turret_actor->laser_damage.Reset();
        spinner_actor->spawn_time_seconds = 0.;
        spinner_actor->initial_fire_point.Reset();
        player_actor->set_team(ioj::ETestTeam::White);
        player_actor->health_override.Reset();
        {
            FMemoryReader memory{data, true};
            FObjectAndNameAsStringProxyArchive archive{memory, false};
            for (auto* actor : actors) {
                actor->Serialize(archive);
            }
        }

        if (!TestRunner->TestEqual(
                TEXT("Persistent bindings restored"), document->entities.Num(), 4)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Capital authoring data persists"),
                             capital_actor->get_team() == ioj::ETestTeam::Red &&
                                 capital_actor->spawn_time_seconds == 7.5 &&
                                 capital_actor->health == 456 &&
                                 capital_actor->initial_spawn_delay == 2.25f &&
                                 capital_actor->spawn_cooldown == 3.5f);
        TestRunner->TestTrue(TEXT("Turret authoring and ownership persist"),
                             !turret_actor->export_to_script &&
                                 turret_actor->get_team() == ioj::ETestTeam::Blue &&
                                 turret_actor->spawn_time_seconds == 1.5 &&
                                 turret_actor->health == 789 && turret_actor->laser_damage == 321);
        TestRunner->TestTrue(TEXT("Spinner authoring data persists"),
                             spinner_actor->spawn_time_seconds == 2.5 &&
                                 spinner_actor->initial_fire_point == 1);
        TestRunner->TestTrue(TEXT("Player authoring data persists"),
                             player_actor->get_team() == ioj::ETestTeam::Green &&
                                 player_actor->health_override == 123);
        if (!assert_success(
                *TestRunner,
                TEXT("Export after persistent reload"),
                ioj::editor::export_s7_editor_config(*world->GetCurrentLevel(), *document))) {
            return;
        }

        auto exported{ioj::editor::read_s7_editor_input(*document)};
        if (!assert_success(*TestRunner, TEXT("Read reloaded export"), exported) ||
            !TestRunner->TestEqual(
                TEXT("Reload preserves output ownership"), exported->entities.size(), size_t{3})) {
            return;
        }

        auto const& exported_capital{exported->entities[0]};
        auto const& exported_player{exported->entities[1]};
        auto const& exported_spinner{exported->entities[2]};
        TestRunner->TestTrue(
            TEXT("Capital export uses persistent authoring values"),
            exported_capital.id == ioj::levels::EntityId{"capital"} &&
                exported_capital.team == ioj::Team::Red && exported_capital.spawn_at.get() == 7.5 &&
                exported_capital.health && exported_capital.health->get().value() == 456 &&
                exported_capital.initial_spawn_delay &&
                exported_capital.initial_spawn_delay->get() == 2.25f &&
                exported_capital.spawn_cooldown && exported_capital.spawn_cooldown->get() == 3.5f);
        TestRunner->TestTrue(TEXT("Spinner export uses persistent authoring value"),
                             exported_spinner.id == ioj::levels::EntityId{"spinner"} &&
                                 exported_spinner.spawn_at.get() == 2.5 &&
                                 exported_spinner.initial_fire_point &&
                                 exported_spinner.initial_fire_point->get() == 1);
        TestRunner->TestTrue(TEXT("Player export uses persistent authoring values"),
                             exported_player.id == ioj::levels::EntityId{"player"} &&
                                 exported_player.team == ioj::Team::Green &&
                                 exported_player.health &&
                                 exported_player.health->get().value() == 123);
    }

    TEST_METHOD(ExportComposesWithHumanSourceAndPreservesPrecision)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto& document{*fixture->document};
        auto& level{*fixture->world->GetCurrentLevel()};
        auto const& definition{fixture->definition};
        auto const& generated_path{fixture->generated_path};
        auto const& human_source{fixture->human_source};
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
        TestRunner->TestEqual(
            TEXT("Human entity appears once"), definition.entities().size(), size_t{5});
        auto const* saved_capital{entity(definition, "capital")};
        auto const* saved_turret{entity(definition, "turret")};
        auto const* saved_spinner{entity(definition, "spinner")};
        auto const* saved_player{entity(definition, "player")};
        auto const* saved_human{entity(definition, "human")};
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
                             saved_capital->spawn_time_seconds.get() == 0.1 &&
                                 saved_capital->overrides.health &&
                                 saved_capital->overrides.health->get().value() == 456 &&
                                 saved_capital->overrides.initial_spawn_delay &&
                                 saved_capital->overrides.initial_spawn_delay->get() == 7.25f &&
                                 saved_capital->overrides.spawn_cooldown &&
                                 saved_capital->overrides.spawn_cooldown->get() == 9.5f);
        TestRunner->TestTrue(TEXT("Turret overrides survive"),
                             saved_turret->overrides.health &&
                                 saved_turret->overrides.health->get().value() == 789 &&
                                 saved_turret->overrides.laser_damage &&
                                 saved_turret->overrides.laser_damage->get() == 321);
        TestRunner->TestTrue(TEXT("Spinner overrides survive"),
                             saved_spinner->overrides.initial_fire_point &&
                                 saved_spinner->overrides.initial_fire_point->get() == 1);
        TestRunner->TestTrue(TEXT("Spinner rotation survives"),
                             FMath::IsNearlyEqual(saved_spinner->rotation.yaw, 45.));
        TestRunner->TestTrue(TEXT("Player override survives"),
                             saved_player->overrides.health &&
                                 saved_player->overrides.health->get().value() == 123);
        TestRunner->TestTrue(TEXT("Human source controls its entity"),
                             saved_human->position.x() == 600000.);

        binding(document, TEXT("human"))->actor->SetActorLocation(FVector{800000., 0., 0.});
        GEditor->BroadcastEndObjectMovement(*binding(document, TEXT("human"))->actor);
        TestRunner->TestTrue(TEXT("Changes to excluded actors leave the export current"),
                             ioj::editor::s7_scene_needs_export(document) == false);
        auto const written_at{IFileManager::Get().GetTimeStamp(*generated_path)};
        if (!assert_success(*TestRunner,
                            TEXT("Repeat unchanged export"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        TestRunner->TestEqual(TEXT("Unchanged output is not rewritten"),
                              IFileManager::Get().GetTimeStamp(*generated_path),
                              written_at);
        binding(document, TEXT("capital"))->actor->Destroy();
        TestRunner->TestTrue(TEXT("Deleting an exported actor requires export"),
                             ioj::editor::s7_scene_needs_export(document) == true);
    }

    TEST_METHOD(ImportRestoresExportedProperties)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto& document{*fixture->document};
        auto& level{*fixture->world->GetCurrentLevel()};
        auto* const player_binding{binding(document, TEXT("player"))};
        auto* capital{
            CastChecked<ioj::ATestCapitalShipProxy>(binding(document, TEXT("capital"))->actor)};
        auto* turret{
            CastChecked<ioj::ATestStaticTurretsProxy>(binding(document, TEXT("turret"))->actor)};
        auto* spinner{
            CastChecked<ioj::ATestTubeSpinnerProxy>(binding(document, TEXT("spinner"))->actor)};
        capital->health.Reset();
        capital->initial_spawn_delay.Reset();
        capital->spawn_cooldown.Reset();
        capital->spawn_time_seconds = 0.;
        turret->health.Reset();
        turret->laser_damage.Reset();
        spinner->initial_fire_point.Reset();
        CastChecked<ioj::ATestSpaceShip>(player_binding->actor)->health_override.Reset();
        if (!assert_success(*TestRunner,
                            TEXT("Refresh composed scene"),
                            ioj::editor::import_s7_level(
                                level, document, ioj::editor::RebuildPolicy::DiscardEdits))) {
            return;
        }
        TestRunner->TestFalse(TEXT("Rebuild removed the old proxy"), IsValid(capital));
        capital =
            CastChecked<ioj::ATestCapitalShipProxy>(binding(document, TEXT("capital"))->actor);
        turret =
            CastChecked<ioj::ATestStaticTurretsProxy>(binding(document, TEXT("turret"))->actor);
        spinner =
            CastChecked<ioj::ATestTubeSpinnerProxy>(binding(document, TEXT("spinner"))->actor);
        TestRunner->TestEqual(TEXT("Rebuilt capital uses the root Red folder"),
                              capital->GetFolderPath(),
                              FName{TEXT("Red")});
        TestRunner->TestEqual(TEXT("Rebuilt turret uses the root Blue folder"),
                              turret->GetFolderPath(),
                              FName{TEXT("Blue")});
        TestRunner->TestTrue(TEXT("Import restores capital properties on the proxy"),
                             capital->health == 456 && capital->initial_spawn_delay == 7.25f &&
                                 capital->spawn_cooldown == 9.5f &&
                                 capital->spawn_time_seconds == 0.1);
        TestRunner->TestTrue(TEXT("Import restores turret and spinner properties on the proxies"),
                             turret->health == 789 && turret->laser_damage == 321 &&
                                 spinner->initial_fire_point == 1);
        TestRunner->TestTrue(
            TEXT("Import restores the player authoring override"),
            CastChecked<ioj::ATestSpaceShip>(binding(document, TEXT("player"))->actor)
                    ->health_override == 123);

        capital->SetActorLabel(TEXT("CapitalShip_Red"));
        TArray<AActor*> copies;
        GUnrealEd->DuplicateActors({capital}, copies, &level, FVector::ZeroVector);
        TArray<AActor*> second_copy;
        GUnrealEd->DuplicateActors({capital}, second_copy, &level, FVector::ZeroVector);
        copies.Append(second_copy);
        if (!TestRunner->TestEqual(TEXT("Copy two capitals"), copies.Num(), 2)) {
            return;
        }
        for (auto* copy : copies) {
            set_custom_name(*CastChecked<ioj::ATestCapitalShipProxy>(copy), NAME_None);
        }

        auto* fighter{fixture->world->SpawnActor<ioj::ATestFighterProxy>()};
        fighter->set_team(ioj::ETestTeam::Green);
        set_custom_name(*fighter, TEXT("fighter10"));
        TestRunner->TestEqual(TEXT("An explicit generated-looking name is preserved"),
                              fighter->GetActorLabel(),
                              FString{TEXT("fighter10")});
        set_custom_name(*fighter, NAME_None);
        TestRunner->TestEqual(TEXT("Clearing the override restores automatic naming"),
                              fighter->GetActorLabel(),
                              FString{TEXT("fighter-green-0")});
        fighter->SetActorLabel(TEXT("fighter10"));
        if (!assert_success(*TestRunner,
                            TEXT("Bind copies before their team changes"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Change both copied capitals to orange through Details"),
                                  edit_team_in_details({copies[0], copies[1]}, TEXT("Orange")))) {
            return;
        }

        TArray<FString> ids;
        for (auto* copy : copies) {
            TestRunner->TestTrue(TEXT("Each selected capital immediately displays orange"),
                                 copy->GetActorLabel().StartsWith(TEXT("capitalship-orange-")));
            ids.Add(copy->GetActorLabel());
        }

        set_custom_name(*capital, TEXT("escort"));
        if (!TestRunner->TestTrue(TEXT("Changing the named capital's team preserves its label"),
                                  edit_team_in_details({capital}, TEXT("Blue")) &&
                                      capital->GetActorLabel() == TEXT("escort"))) {
            return;
        }

        auto const pending{ioj::editor::check_s7_scene(level, document)};
        if (!TestRunner->TestTrue(TEXT("Logic must reference the pending entity name"),
                                  !pending &&
                                      pending.error().Contains(TEXT("Unknown entity: capital"))) ||
            !TestRunner->TestTrue(TEXT("Update the flagship reference"),
                                  write_source(directory.root(),
                                               fixture->human_source.Replace(
                                                   TEXT("entity-destroyed 'capital"),
                                                   *(TEXT("entity-destroyed '") + ids[0]))))) {
            return;
        }
        if (!assert_success(*TestRunner,
                            TEXT("Export changed teams"),
                            ioj::editor::export_s7_editor_config(level, document)) ||
            !assert_success(*TestRunner,
                            TEXT("Rebuild copied capitals and legacy fighter"),
                            ioj::editor::import_s7_level(level, document))) {
            return;
        }

        for (auto const& id : ids) {
            auto* entry{binding(document, id)};
            TestRunner->TestTrue(
                TEXT("Export and rebuild use the Outliner label when Custom Name is None"),
                entry && entry->actor->GetActorLabel() == id);
        }

        auto const* named{binding(document, TEXT("escort"))};
        TestRunner->TestTrue(TEXT("The custom name is the exported ID and survives rebuild"),
                             named && named->actor->GetActorLabel() == TEXT("escort"));
        auto* entry{binding(document, TEXT("fighter10"))};
        TestRunner->TestTrue(TEXT("Rebuilding restores the fighter's exported label"),
                             entry && entry->actor->GetActorLabel() == TEXT("fighter10"));
    }

    TEST_METHOD(RebuildRestoresOwnershipAndExportRejectsDuplicateDeclarations)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto& document{*fixture->document};
        auto& level{*fixture->world->GetCurrentLevel()};
        auto* const human_binding{binding(document, TEXT("human"))};
        auto* const previous_human{CastChecked<ioj::ATestCapitalShipProxy>(human_binding->actor)};
        auto* const unbound{fixture->world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        unbound->export_to_script = false;
        previous_human->export_to_script = true;
        auto* const pending{fixture->world->SpawnActor<ioj::ATestFighterProxy>()};
        document.set_needs_export(false);
        auto const blocked{ioj::editor::import_s7_level(level, document)};
        TestRunner->TestTrue(
            TEXT("An unbound exportable fighter blocks rebuild even with a clear pending flag"),
            !blocked && blocked.error().Contains(TEXT("unexported scene changes")) &&
                IsValid(pending) && IsValid(previous_human));
        FString baseline;
        FFileHelper::LoadFileToString(baseline, *fixture->generated_path);
        if (!TestRunner->TestTrue(TEXT("Write malformed script"),
                                  write_source(directory.root(), TEXT("(")))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Failed force rebuild preserves the scene"),
                             !ioj::editor::import_s7_level(
                                 level, document, ioj::editor::RebuildPolicy::DiscardEdits) &&
                                 IsValid(pending) && IsValid(previous_human));
        if (!TestRunner->TestTrue(TEXT("Restore saved level"),
                                  write_source(directory.root(), fixture->human_source))) {
            return;
        }

        if (!assert_success(*TestRunner,
                            TEXT("Refresh adopted entity"),
                            ioj::editor::import_s7_level(
                                level, document, ioj::editor::RebuildPolicy::DiscardEdits))) {
            return;
        }

        auto* const replacement{
            CastChecked<ioj::ATestCapitalShipProxy>(binding(document, TEXT("human"))->actor)};
        TestRunner->TestTrue(TEXT("Rebuild keeps unbound actors excluded from export"),
                             IsValid(unbound));
        TestRunner->TestFalse(TEXT("Force rebuild discards pending placements"), IsValid(pending));
        TestRunner->TestFalse(TEXT("Rebuild replaces the bound proxy"), IsValid(previous_human));
        TestRunner->TestFalse(TEXT("Script-owned replacement defaults to excluded"),
                              replacement->export_to_script);
        FString after_rebuild;
        FFileHelper::LoadFileToString(after_rebuild, *fixture->generated_path);
        TestRunner->TestEqual(
            TEXT("Rebuilding preserves the exported file"), after_rebuild, baseline);
        replacement->export_to_script = true;
        TestRunner->TestFalse(TEXT("Check Scene rejects the duplicate ID"),
                              ioj::editor::check_s7_scene(level, document).has_value());
        TestRunner->TestFalse(TEXT("Export rejects the duplicate ID"),
                              ioj::editor::export_s7_editor_config(level, document).has_value());
        TestRunner->TestTrue(TEXT("Saved level remains playable"),
                             ioj::editor::read_s7_authored_level(document).has_value());
        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Undoing a rebuild requests export"), document.needs_export);
        TestRunner->TestTrue(TEXT("Undo restores the discarded placement"), IsValid(pending));
        auto const* restored{binding(document, TEXT("human"))};
        TestRunner->TestTrue(TEXT("Undo restores original proxy and binding"),
                             restored && IsValid(previous_human) &&
                                 restored->actor == previous_human &&
                                 previous_human->export_to_script);
        GEditor->RedoTransaction();
        TestRunner->TestTrue(TEXT("Redo requests export"), document.needs_export);
    }

    TEST_METHOD(ExportedDefinitionSpawnsRuntimeEntities)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto& document{*fixture->document};
        auto compiled{ioj::editor::compile_playable_s7_definition(fixture->definition,
                                                                  *document.level_config)};
        if (!assert_success(
                *TestRunner, TEXT("Compile composed level and expression references"), compiled)) {
            return;
        }

        ioj::sim::LevelSim simulation{std::move(*compiled), *std::pmr::new_delete_resource()};

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
    }

    TEST_METHOD(ImportEditsTheBaselineWithoutCompoundingHumanTransforms)
    {
        using namespace s7_level_authoring_tests;
        using namespace ioj::levels;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto& level{*world->GetCurrentLevel()};
        auto created{
            ioj::editor::set_up_s7_level(level, ioj::editor::LevelSetup::Create, directory.root())};
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
                             ioj::to_fstring(authoring::emit_editor_level_source(*baseline)))) ||
            !TestRunner->TestTrue(
                TEXT("Write human transformations"),
                write_source(directory.root(), TEXT(R"scm((export level-definition)
(define base (module-ref (import-module "Authoring/editor-config.scm") 'scene))
(define level-definition
  (copy-with base
    :title (string-append (level-ref base :title) " suffix")
    :entities (map (lambda (ship)
      (copy-with ship :position
        (map (lambda (x) (+ x 10)) (entity-ref ship :position))))
      (level-ref base :entities))))
)scm")))) {
            return;
        }

        for (auto iteration{0}; iteration < 2; ++iteration) {
            auto definition{ioj::editor::read_s7_authored_level(document)};
            if (!assert_success(*TestRunner, TEXT("Read composed level"), definition) ||
                !assert_success(*TestRunner,
                                TEXT("Import baseline for editing"),
                                ioj::editor::import_s7_level(level, document))) {
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
                                 saved->title == baseline->title &&
                                     ioj::s7::equivalent(saved->entities, baseline->entities));
        }
    }

    TEST_METHOD(MapSaveExportsEditsAndKeepsFailedEditsForRetry)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto* world{fixture->world};
        auto& document{*fixture->document};
        auto* capital{
            CastChecked<ioj::ATestCapitalShipProxy>(binding(document, TEXT("capital"))->actor)};
        auto* root{capital->GetRootComponent()};
        root->SetRelativeLocation(root->GetRelativeLocation() + FVector{20., 0., 0.});
        FPropertyChangedEvent movement{FindFProperty<FProperty>(
            root->GetClass(), USceneComponent::GetRelativeLocationPropertyName())};
        root->PostEditChangeProperty(movement);
        TestRunner->TestTrue(TEXT("Root transform edits request export"), document.needs_export);
        if (!assert_success(
                *TestRunner,
                TEXT("Export moved capital"),
                ioj::editor::export_s7_editor_config(*world->PersistentLevel, document))) {
            return;
        }

        auto* turret{
            CastChecked<ioj::ATestStaticTurretsProxy>(binding(document, TEXT("turret"))->actor)};
        turret->export_to_script = false;
        FPropertyChangedEvent toggle{FindFProperty<FProperty>(
            turret->GetClass(),
            GET_MEMBER_NAME_CHECKED(ioj::ATestStaticTurretsProxy, export_to_script))};
        turret->PostEditChangeProperty(toggle);
        TestRunner->TestTrue(TEXT("Disabling Export requests export"), document.needs_export);
        auto* extra{world->SpawnActor<ioj::ATestCapitalShipProxy>(
            document.level_config->classes.capital_ship_proxy_class)};
        auto const check_colour{[this, config = document.level_config](
                                    ioj::ATestCapitalShipProxy& actor, ioj::ETestTeam expected) {
            auto const* mesh{actor.FindComponentByClass<UStaticMeshComponent>()};
            auto const& values{mesh->GetCustomPrimitiveData().Data};
            auto const colour{config->capital_ships.team_visual_data->get_colour(expected)};
            TestRunner->TestTrue(
                TEXT("Capital preview uses the shared mesh and team colour"),
                mesh->GetStaticMesh() == config->capital_ships.mesh && values.Num() >= 3 &&
                    FVector3f{values[0], values[1], values[2]}.Equals(FVector3f{colour}));
        }};
        for (auto team : {ioj::ETestTeam::Green, ioj::ETestTeam::Red}) {
            extra->set_team(team);
            FPropertyChangedEvent team_changed{
                FindFProperty<FProperty>(extra->GetClass(), TEXT("team"))};
            extra->PostEditChangeProperty(team_changed);
            check_colour(*extra, team);
        }

        auto* fighter{world->SpawnActor<ioj::ATestFighterProxy>()};
        fighter->set_team(ioj::ETestTeam::Green);
        FPropertyChangedEvent fighter_team{
            FindFProperty<FProperty>(fighter->GetClass(), TEXT("team"))};
        fighter->PostEditChangeProperty(fighter_team);
        set_custom_name(*fighter, TEXT("escort"));
        set_custom_name(*extra, TEXT("outpost"));
        FString previous;
        FFileHelper::LoadFileToString(previous, *fixture->generated_path);
        FObjectSaveContextData autosave;
        autosave.SaveFlags = SAVE_FromAutosave;
        FEditorDelegates::PreSaveWorldWithContext.Broadcast(world, FObjectPreSaveContext{autosave});
        FString saved;
        FFileHelper::LoadFileToString(saved, *fixture->generated_path);
        TestRunner->TestEqual(TEXT("Autosave preserves the exported scene"), saved, previous);
        TestRunner->TestTrue(TEXT("Autosave leaves scene pending"), document.needs_export);

        auto const filename{
            FPaths::Combine(FPaths::ProjectContentDir(),
                            TEXT("S7SaveTest") + FGuid::NewGuid().ToString() + TEXT(".umap"))};
        ON_SCOPE_EXIT {
            FAutomationEditorCommonUtils::CreateNewMap();
            IFileManager::Get().Delete(*filename);
        };
        if (!TestRunner->TestTrue(TEXT("Save editable map"),
                                  FEditorFileUtils::SaveMap(world, filename))) {
            return;
        }
        TestRunner->TestFalse(TEXT("Map save exports edits"), document.needs_export);
        FFileHelper::LoadFileToString(saved, *fixture->generated_path);
        TestRunner->TestTrue(TEXT("Map save publishes the new actor"),
                             saved.Contains(TEXT("outpost")));
        FAutomationEditorCommonUtils::CreateNewMap();
        if (!TestRunner->TestTrue(TEXT("Reopen map"), FEditorFileUtils::LoadMap(filename))) {
            return;
        }
        world = GEditor->GetEditorWorldContext().World();
        auto reopened{ioj::editor::find_level_authoring_document(*world->PersistentLevel)};
        if (!reopened || !TestRunner->TestNotNull(TEXT("Saved document"), *reopened)) {
            return;
        }

        auto* bound{binding(**reopened, TEXT("outpost"))};
        if (!TestRunner->TestTrue(TEXT("Binding survives reopen"),
                                  bound && IsValid(bound->actor))) {
            return;
        }

        check_colour(*CastChecked<ioj::ATestCapitalShipProxy>(bound->actor), ioj::ETestTeam::Red);
        auto const* escort{binding(**reopened, TEXT("escort"))};
        if (!TestRunner->TestTrue(TEXT("Saved fighter binding survives reopen"),
                                  escort && IsValid(escort->actor))) {
            return;
        }

        auto const* preview{
            CastChecked<UInstancedStaticMeshComponent>(escort->actor->GetRootComponent())};
        auto const& rgb{preview->PerInstanceSMCustomData};
        auto const green{(*reopened)->level_config->fighters.team_visual_data->get_colour(
            ioj::ETestTeam::Green)};
        TestRunner->TestTrue(
            TEXT("Reopening restores one fighter instance with its saved colour and label"),
            preview->GetInstanceCount() == 1 && rgb.Num() == 3 &&
                FVector3f{rgb[0], rgb[1], rgb[2]}.Equals(FVector3f{green}) &&
                escort->actor->GetActorLabel() == TEXT("escort"));
        TestRunner->TestFalse(TEXT("Reopening keeps the export current"),
                              (*reopened)->needs_export);
        auto* duplicate{world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        set_custom_name(*duplicate, TEXT("outpost"));
        TestRunner->AddExpectedMessage(TEXT("Scene export failed: Invalid or duplicate entity ID"),
                                       ELogVerbosity::Warning,
                                       EAutomationExpectedMessageFlags::Contains,
                                       1);
        TestRunner->TestTrue(TEXT("Failed export still saves map"),
                             FEditorFileUtils::SaveMap(world, filename));
        TestRunner->TestTrue(TEXT("Failed export remains pending"), (*reopened)->needs_export);
        FString after_failure;
        FFileHelper::LoadFileToString(after_failure, *fixture->generated_path);
        TestRunner->TestEqual(TEXT("Failed export keeps previous file"), after_failure, saved);
        FAutomationEditorCommonUtils::CreateNewMap();
        if (TestRunner->TestTrue(TEXT("Reopen failed export"),
                                 FEditorFileUtils::LoadMap(filename))) {
            auto loaded{ioj::editor::find_level_authoring_document(
                *GEditor->GetEditorWorldContext().World()->PersistentLevel)};
            TestRunner->TestTrue(TEXT("Reopen retains pending edits"),
                                 loaded && *loaded && (*loaded)->needs_export);
        }
    }

    TEST_METHOD(NewProxiesExportThroughTheGeneratedEntryPoint)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto& level{*world->GetCurrentLevel()};
        auto created{
            ioj::editor::set_up_s7_level(level, ioj::editor::LevelSetup::Create, directory.root())};
        if (!assert_success(*TestRunner, TEXT("Create authoring files"), created)) {
            return;
        }

        auto& document{**created};
        auto* const player{world->SpawnActor<ioj::ATestSpaceShip>(
            document.level_config->classes.player_ship_class.Get(), FTransform::Identity)};
        if (!TestRunner->TestNotNull(TEXT("New player proxy"), player)) {
            return;
        }
        TestRunner->TestTrue(TEXT("An unbound player requires export"),
                             ioj::editor::s7_scene_needs_export(document) == true);
        player->export_to_script = false;
        TestRunner->TestTrue(TEXT("Changes remain pending until export"),
                             ioj::editor::s7_scene_needs_export(document) == true);
        player->export_to_script = true;
        auto* const ignored{world->SpawnActor<ioj::ATestStaticTurretsProxy>()};
        ignored->export_to_script = false;
        bool const was_editor_only{ignored->bIsEditorOnlyActor != 0};
        document.camera.targets.Add(ignored);
        FVector const wing_position{5000, 1500, 1000};
        FRotator const wing_rotation{25, 70, 30};
        auto* wing{world->SpawnActor<ioj::ATestFighterProxy>(wing_position, wing_rotation)};
        TestRunner->TestTrue(TEXT("Placed fighter has its team label"),
                             wing->GetActorLabel().StartsWith(TEXT("fighter-white-")));
        {
            FScopedTransaction transaction{FText::FromString(TEXT("Edit fighter team"))};
            wing->Modify();
            wing->set_team(ioj::ETestTeam::Red);
            FPropertyChangedEvent team_change{
                FindFProperty<FProperty>(wing->GetClass(), TEXT("team"))};
            wing->PostEditChangeProperty(team_change);
        }
        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Undo restores the fighter team and label"),
                             wing->get_team() == ioj::ETestTeam::White &&
                                 wing->GetActorLabel().StartsWith(TEXT("fighter-white-")));
        GEditor->RedoTransaction();
        TestRunner->TestTrue(TEXT("Changing the team refreshes the fighter label"),
                             wing->GetActorLabel().StartsWith(TEXT("fighter-red-")));
        TArray<AActor*> copies;
        GUnrealEd->DuplicateActors({wing}, copies, &level, FVector::ZeroVector);
        if (!TestRunner->TestEqual(TEXT("Duplicate the red fighter"), copies.Num(), 1)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Duplicate fighter gets a unique team label"),
                             copies[0]->GetActorLabel().StartsWith(TEXT("fighter-red-")) &&
                                 copies[0]->GetActorLabel() != wing->GetActorLabel());
        world->EditorDestroyActor(copies[0], true);
        set_custom_name(*wing, TEXT("wing"));
        wing->PostEditMove(true);
        TestRunner->TestEqual(TEXT("Moving a fighter keeps its authored label"),
                              wing->GetActorLabel(),
                              FString{TEXT("wing")});
        auto* mesh{CastChecked<UInstancedStaticMeshComponent>(wing->GetRootComponent())};
        auto const colour{
            document.level_config->fighters.team_visual_data->get_colour(ioj::ETestTeam::Red)};
        auto const& rgb{mesh->PerInstanceSMCustomData};
        TestRunner->TestTrue(TEXT("Fighter preview supplies the runtime material its team RGB"),
                             mesh->GetInstanceCount() == 1 && rgb.Num() == 3 &&
                                 FVector3f{rgb[0], rgb[1], rgb[2]}.Equals(FVector3f{colour}));
        TestRunner->TestTrue(TEXT("Placed fighter previews the runtime mesh"),
                             mesh->GetStaticMesh() &&
                                 mesh->GetStaticMesh() ==
                                     document.level_config->fighters.mesh.Get());
        player->SetActorLabel(TEXT("pilot"));
        player->set_team(ioj::ETestTeam::Blue);
        if (!assert_success(*TestRunner,
                            TEXT("Check unbound actor"),
                            ioj::editor::check_s7_scene(level, document))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Checking leaves edits pending"), document.needs_export);
        TestRunner->TestTrue(TEXT("Checking leaves actors unbound"), document.entities.IsEmpty());
        auto untouched{ioj::editor::read_s7_editor_input(document)};
        TestRunner->TestTrue(TEXT("Checking preserves baseline"),
                             untouched && untouched->entities.empty());
        if (!assert_success(*TestRunner,
                            TEXT("Export adopts newly created proxy"),
                            ioj::editor::export_s7_editor_config(level, document))) {
            return;
        }

        auto& modes{GLevelEditorModeTools()};
        auto const had_mode{modes.IsModeActive(US7LevelAuthoringMode::mode_id)};
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);
        ON_SCOPE_EXIT {
            if (!had_mode) {
                modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
            }
        };
        auto& mode{*CastChecked<US7LevelAuthoringMode>(
            modes.GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
        GEditor->SelectNone(false, true);
        GEditor->SelectActor(player, true, true);
        TestRunner->TestEqual(TEXT("Selected player shows its bound ID"),
                              mode.selected_script_id(),
                              FString{TEXT("pilot")});
        mode.rename_selected_entity(TEXT("wing"));
        TestRunner->TestEqual(TEXT("Duplicate ID keeps the binding"),
                              mode.selected_script_id(),
                              FString{TEXT("pilot")});
        {
            TGuardValue<bool> unattended{GIsRunningUnattendedScript, true};
            mode.force_rebuild_scene();
        }
        TestRunner->TestTrue(TEXT("Declining force rebuild keeps the selected actor and bindings"),
                             IsValid(player) && mode.selected_script_id() == TEXT("pilot"));
        mode.rename_selected_entity(TEXT("lead"));
        TestRunner->TestEqual(
            TEXT("Renaming updates the binding"), mode.selected_script_id(), FString{TEXT("lead")});
        GEditor->UndoTransaction();
        TestRunner->TestEqual(
            TEXT("Undo restores the script ID"), mode.selected_script_id(), FString{TEXT("pilot")});
        GEditor->SelectActor(wing, true, true);
        TestRunner->TestTrue(TEXT("Multiple selection hides the ID"),
                             mode.selected_script_id().IsEmpty());
        GEditor->SelectNone(false, true);
        GEditor->SelectActor(ignored, true, true);
        TestRunner->TestTrue(TEXT("Unbound selection hides the ID"),
                             mode.selected_script_id().IsEmpty());

        auto const* adopted{binding(document, TEXT("pilot"))};
        TestRunner->TestTrue(TEXT("New proxy defaults to Export"),
                             adopted && adopted->actor == player && player->export_to_script);
        TestRunner->TestEqual(
            TEXT("Only the pilot and fighter are bound"), document.entities.Num(), 2);
        TestRunner->TestEqual(TEXT("Export preserves the target actor flags"),
                              ignored->bIsEditorOnlyActor != 0,
                              was_editor_only);
        auto definition{ioj::editor::read_s7_authored_level(document)};
        if (!assert_success(
                *TestRunner, TEXT("Generated root includes its own configuration"), definition)) {
            return;
        }
        TestRunner->TestEqual(TEXT("New roots inherit the shared base"),
                              definition->settings().tick.tick_rate.hz(),
                              75.);
        TestRunner->TestTrue(TEXT("Entry point resolves the adopted player"),
                             definition->player_entity_id() == ioj::levels::EntityId{"pilot"});

        auto compiled{
            ioj::editor::compile_playable_s7_definition(*definition, *document.level_config)};
        if (!assert_success(*TestRunner, TEXT("Compile carrier-free level"), compiled)) {
            return;
        }

        ioj::sim::LevelSim sim{std::move(*compiled), *std::pmr::new_delete_resource()};
        auto const fighters{sim.fighters().entities()};
        if (!TestRunner->TestEqual(
                TEXT("Fighter starts without a carrier"), fighters.num(), uint32{1})) {
            return;
        }
        TestRunner->TestTrue(TEXT("Spawned fighter has the authored team"),
                             fighters.teams()[0] == ioj::Team::Red);
        TestRunner->TestTrue(TEXT("Export drops fighter roll"),
                             definition->entity(ioj::levels::EntityId{"wing"}).rotation.roll ==
                                 0.0);

        FString standalone;
        if (!TestRunner->TestTrue(TEXT("Read exported scene"),
                                  FFileHelper::LoadFileToString(
                                      standalone, *ioj::editor::s7_editor_config_path(document))) ||
            !TestRunner->TestTrue(
                TEXT("Make the level standalone"),
                write_source(
                    directory.root(),
                    standalone +
                        TEXT("\n(export level-definition)\n(define level-definition scene)\n")))) {
            return;
        }
        player->SetActorLocation(FVector{100, 20, 30});
        GEditor->BroadcastEndObjectMovement(*player);
        TestRunner->TestTrue(TEXT("Standalone levels do not import the editor config"),
                             ioj::editor::s7_scene_needs_export(document) == false);
        TestRunner->TestFalse(TEXT("Play stops when the level dropped the edited scene"),
                              ioj::editor::prepare_staged_level(document).has_value());
        TestRunner->TestFalse(TEXT("A standalone level cannot export its orphaned config"),
                              ioj::editor::check_s7_scene(level, document).has_value());
        if (!assert_success(*TestRunner,
                            TEXT("Rebuild standalone level"),
                            ioj::editor::import_s7_level(
                                level, document, ioj::editor::RebuildPolicy::DiscardEdits))) {
            return;
        }

        auto const* wing_binding{binding(document, TEXT("wing"))};
        if (!TestRunner->TestNotNull(TEXT("Authored fighter ID survives rebuild"), wing_binding)) {
            return;
        }

        auto* restored{CastChecked<ioj::ATestFighterProxy>(wing_binding->actor)};
        TestRunner->TestEqual(TEXT("Imported fighter uses the script ID as its label"),
                              restored->GetActorLabel(),
                              FString{TEXT("wing")});
        TestRunner->TestTrue(TEXT("Rebuilt fighter retains its placement and team"),
                             restored->GetActorLocation().Equals(wing_position) &&
                                 restored->GetActorForwardVector().Equals(wing_rotation.Vector()) &&
                                 restored->get_team() == ioj::ETestTeam::Red);
        auto const* standalone_pilot{binding(document, TEXT("pilot"))};
        TestRunner->TestTrue(
            TEXT("Standalone level owns its pilot"),
            standalone_pilot &&
                !CastChecked<ioj::ATestSpaceShip>(standalone_pilot->actor)->export_to_script);
    }

    TEST_METHOD(CreatingAuthoringFilesPreservesExistingHumanSource)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto const source{FString{TEXT(R"scm(; Keep my source
(level :title "Human")
)scm")}};
        if (!TestRunner->TestTrue(TEXT("Write existing human file"),
                                  write_source(directory.root(), source))) {
            return;
        }

        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto created{ioj::editor::set_up_s7_level(
            *world->GetCurrentLevel(), ioj::editor::LevelSetup::Link, directory.root())};
        TestRunner->TestFalse(TEXT("Reject invalid level"), created.has_value());
        FString saved;
        TestRunner->TestTrue(TEXT("Read preserved file"),
                             FFileHelper::LoadFileToString(saved, *directory.root()));
        TestRunner->TestEqual(TEXT("Existing human source is never replaced"), saved, source);
        TestRunner->TestFalse(TEXT("Invalid links create no baseline"),
                              FPaths::FileExists(FPaths::Combine(
                                  directory.path, TEXT("Authoring/editor-config.scm"))));
    }

    TEST_METHOD(CheckSceneReportsPlacementScriptAndAssetIssues)
    {
        using namespace s7_level_authoring_tests;
        FTemporarySourceDirectory directory;
        auto fixture{make_composed_fixture(*TestRunner, directory)};
        if (!fixture) {
            return;
        }

        auto& document{*fixture->document};
        auto& level{*fixture->world->GetCurrentLevel()};
        auto* const shared{document.level_config.Get()};
        auto* const assets{DuplicateObject(shared, &document)};
        document.level_config = assets;
        assets->capital_ships.main_death_explosion = nullptr;
        auto const warnings{ioj::editor::check_s7_scene(level, document)};
        TestRunner->TestTrue(TEXT("Missing optional effect gives an actionable warning"),
                             warnings && FString::Join(warnings->warnings, TEXT("\n"))
                                             .Contains(TEXT("capital_ships.main_death_explosion")));
        assets->capital_ships.mesh = nullptr;
        auto const errors{ioj::editor::check_s7_scene(level, document)};
        TestRunner->TestTrue(TEXT("Missing required mesh fails validation"),
                             !errors && errors.error().Contains(TEXT("capital_ships.mesh")));
        document.level_config = nullptr;
        auto const missing{ioj::editor::check_s7_scene(level, document)};
        TestRunner->TestTrue(TEXT("Missing shared resource asset is diagnosed"),
                             !missing && missing.error().Contains(TEXT("Shared level asset")));

        document.level_config = shared;
        auto* const capital{binding(document, TEXT("capital"))->actor.Get()};
        auto const location{capital->GetActorLocation()};
        capital->SetActorLocation(FVector{1.e8, 0., 0.});
        auto const placement{ioj::editor::check_s7_scene(level, document)};
        TestRunner->TestTrue(TEXT("Check sees an unexported out-of-grid placement"),
                             !placement &&
                                 placement.error().Contains(TEXT("outside the collision grid")));
        capital->SetActorLocation(location);

        if (!TestRunner->TestTrue(TEXT("Remove entry point"),
                                  IFileManager::Get().Delete(*directory.root()))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Validate reports the missing root"),
                             !ioj::editor::check_s7_scene(level, document).has_value());
        if (!TestRunner->TestTrue(TEXT("Write malformed root"),
                                  write_source(directory.root(), TEXT("(level")))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Validate reports the malformed root"),
                             !ioj::editor::check_s7_scene(level, document).has_value());
    }
};
