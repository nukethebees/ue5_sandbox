#include <ioj/levels/authoring/level_definition_writer.h>
#include <SandboxEditor/levels/S7EntityGeometryExport.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>
#include <Curves/CurveFloat.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <FileHelpers.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformFileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <SceneManagement.h>
#include <SceneView.h>
#include <ScopedTransaction.h>
#include <Tests/AutomationEditorCommon.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>

#include <algorithm>
#include <expected>

namespace s7_level_playable_tests {
struct FOverlayLines final : FSimpleElementCollector {
    bool IsHitTesting() override { return false; }
    void DrawLine(FVector const& start,
                  FVector const& end,
                  FLinearColor const&,
                  uint8 depth,
                  float,
                  float,
                  bool) override {
        if (depth == SDPG_World) {
            points.Add(start);
            points.Add(end);
        }
    }

    void DrawTranslucentLine(FVector const& start,
                             FVector const& end,
                             FLinearColor const& colour,
                             uint8 depth,
                             float thickness,
                             float bias,
                             bool screen) override {
        DrawLine(start, end, colour, depth, thickness, bias, screen);
    }
    TArray<FVector> points;
};

auto capture_overlay(US7LevelAuthoringMode& mode, UWorld& world) -> TArray<FVector> {
    FSceneViewFamilyContext family{FSceneViewFamily::ConstructionValues{
        nullptr,
        world.Scene,
        FEngineShowFlags{ESFIM_Editor}}.SetTime(FGameTime{})};
    FSceneViewInitOptions options;
    options.ViewFamily = &family;
    options.SetViewRectangle(FIntRect{0, 0, 640, 480});
    FSceneView view{options};
    FOverlayLines lines;
    mode.Render(&view, nullptr, &lines);
    return MoveTemp(lines.points);
}

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
}

TEST_CLASS(S7PlayableLevel, "Sandbox.UnitTests")
{
    AFTER_EACH()
    {
        if (previous_overlays_) {
            auto& modes{GLevelEditorModeTools()};
            modes.ActivateMode(US7LevelAuthoringMode::mode_id);
            auto* mode{CastChecked<US7LevelAuthoringMode>(
                modes.GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
            mode->set_collision_overlays(*previous_overlays_);
            if (!authoring_was_active_) {
                modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
            }
        }

        if (!source_directory_.IsEmpty()) {
            IFileManager::Get().DeleteDirectory(*source_directory_, false, true);
        }
    }

    TEST_METHOD(RepeatedGeometryExportsMatch)
    {
        source_directory_ = FPaths::Combine(
            FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString());
        IFileManager::Get().MakeDirectory(*source_directory_, true);
        FString previous;
        for (auto const filename : {TEXT("first.scm"), TEXT("second.scm")}) {
            auto const path{FPaths::Combine(source_directory_, filename)};
            if (!s7_level_playable_tests::assert_success(
                    *TestRunner,
                    TEXT("Export configured meshes"),
                    ioj::editor::export_entity_geometry(path))) {
                return;
            }

            FString source;
            if (!TestRunner->TestTrue(TEXT("Read exported module"),
                                      FFileHelper::LoadFileToString(source, *path))) {
                return;
            }

            if (!previous.IsEmpty()) {
                TestRunner->TestEqual(
                    TEXT("Identical assets give identical output"), source, previous);
            }
            previous = MoveTemp(source);
        }
    }

    TEST_METHOD(DefendCapitalStartsWithExportedGeometry)
    {
        using namespace ioj;
        auto const map{FPaths::Combine(
            FPaths::ProjectContentDir(),
            TEXT("Levels/FeatureTests/FT_soa_turrets/test_levels/defend_capital.umap"))};
        if (!TestRunner->TestTrue(TEXT("Open Defend Capital map"),
                                  FEditorFileUtils::LoadMap(map, false, false))) {
            return;
        }

        auto* world{GEditor->GetEditorWorldContext().World()};
        auto document{editor::find_level_authoring_document(*world->GetCurrentLevel())};
        if (!TestRunner->TestTrue(TEXT("Map embeds its project-relative mission"),
                                  document && IsValid(*document) &&
                                      (*document)->source_path ==
                                          TEXT("LevelScripts/defend_capital/defend_capital.scm"))) {
            return;
        }

        auto definition{editor::read_s7_authored_level(**document)};
        if (!s7_level_playable_tests::assert_success(
                *TestRunner, TEXT("Load linked mission"), definition)) {
            return;
        }

        auto& modes{GLevelEditorModeTools()};
        authoring_was_active_ = modes.IsModeActive(US7LevelAuthoringMode::mode_id);
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);
        auto& mode{*CastChecked<US7LevelAuthoringMode>(
            modes.GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
        previous_overlays_ = mode.collision_overlays();
        auto const was_dirty{world->GetPackage()->IsDirty()};
        auto capture{[&] { return s7_level_playable_tests::capture_overlay(mode, *world); }};
        mode.set_collision_overlays({});
        TestRunner->TestTrue(TEXT("Hidden overlays draw no lines"), capture().IsEmpty());
        mode.set_collision_overlays({.grid = true});
        auto const grid{capture()};
        FBox grid_box{grid};
        TestRunner->TestTrue(TEXT("Grid uses the mission's 500000 cm height"),
                             grid_box.IsValid && grid_box.Min.Z == -250000. &&
                                 grid_box.Max.Z == 250000.);
        mode.set_collision_overlays({.bounds = true});
        auto const boxes{capture()};
        TestRunner->TestFalse(TEXT("Editor draws placed collision boxes"), boxes.IsEmpty());
        mode.set_collision_overlays({.grid = true, .bounds = true});
        TestRunner->TestEqual(
            TEXT("Both overlays draw independently"), capture().Num(), boxes.Num() + grid.Num());
        TestRunner->TestEqual(TEXT("Overlay controls preserve map dirty state"),
                              world->GetPackage()->IsDirty(),
                              was_dirty);

        mode.set_collision_overlays({.bounds = true});
        TActorIterator<ATestCapitalShipProxy> capital{world};
        if (!TestRunner->TestTrue(TEXT("Placed capital"), static_cast<bool>(capital))) {
            return;
        }
        {
            FScopedTransaction move{
                NSLOCTEXT("S7PlayableLevel", "MoveCapital", "Move test capital")};
            capital->Modify();
            capital->GetRootComponent()->Modify();
            capital->SetActorLocationAndRotation(FVector{1234., 5678., 9012.},
                                                 FRotator{15., 32., 8.});
        }
        TestRunner->TestTrue(TEXT("Live preview follows the capital placement"),
                             capture() != boxes);
        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Undo restores the preview before export"), capture() == boxes);
        mode.set_collision_overlays({.grid = true, .bounds = true});
        s7_level_playable_tests::assert_success(
            *TestRunner,
            TEXT("Compile Defend Capital with its exported geometry"),
            editor::compile_playable_s7_definition(*definition, *(*document)->level_config));
    }

    bool authoring_was_active_{};
    TOptional<ioj::FCollisionOverlayVisibility> previous_overlays_;
    FString source_directory_;
};

TEST_CLASS(StagedMission, "Sandbox.StagedEditorTests")
{
    AFTER_EACH()
    {
        if (process_.IsValid()) {
            FPlatformProcess::CloseProc(process_);
        }

        if (!cooked_asset_.IsEmpty()) {
            IFileManager::Get().Delete(*cooked_asset_);
            IFileManager::Get().Delete(*FPaths::ChangeExtension(cooked_asset_, TEXT("uexp")));
        }

        if (!asset_file_.IsEmpty()) {
            IFileManager::Get().Delete(*asset_file_);
        }

        if (!source_directory_.IsEmpty()) {
            IFileManager::Get().DeleteDirectory(*source_directory_, false, true);
        }
    }

    void play_authored_scene(bool const observer) {
        using s7_level_playable_tests::assert_success;
        using namespace ioj::levels;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        if (!TestRunner->TestNotNull(TEXT("Editor world"), world)) {
            return;
        }

        source_directory_ = FPaths::Combine(FPaths::ProjectSavedDir(),
                                            TEXT("Automation"),
                                            TEXT("S7PlayableLevel"),
                                            FGuid::NewGuid().ToString());
        auto const settings{FPaths::Combine(source_directory_, TEXT("Settings"))};
        if (!TestRunner->TestTrue(
                TEXT("Install shared settings and geometry"),
                FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(
                    *settings,
                    *FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts/Settings")),
                    true))) {
            return;
        }

        auto const root{FPaths::Combine(source_directory_, TEXT("Playable/Playable.scm"))};
        auto& editor_level{*world->GetCurrentLevel()};
        auto created{
            ioj::editor::set_up_s7_mission(editor_level, ioj::editor::MissionSetup::Create, root)};
        if (!assert_success(
                *TestRunner, TEXT("Create generated configuration and human root"), created)) {
            return;
        }

        auto* const document{*created};
        auto baseline{ioj::editor::read_s7_editor_input(*document)};
        if (!assert_success(*TestRunner, TEXT("Read generated baseline"), baseline)) {
            return;
        }
        baseline->entities = {
            {.id = EntityId{"enemy"},
             .archetype = EntityArchetype::CapitalShip,
             .team = ioj::Team::Red,
             .position = {30000., 0., 0.},
             .rotation = {10., 370., 20.},
             .spawn_at = ioj::NonNegativeFinite<double>{5.},
             .initial_spawn_delay = ioj::NonNegativeFinite<float>{60.f}},
        };
        if (observer) {
            baseline->entities.push_back({
                .id = EntityId{"ally"},
                .archetype = EntityArchetype::CapitalShip,
                .team = ioj::Team::Blue,
                .position = {-30000., 0., 0.},
                .initial_spawn_delay = ioj::NonNegativeFinite<float>{60.f},
            });
            baseline->camera = LevelCameraDefinition{{EntityId{"ally"}}, {-1., 0., 0.}, 1000.};
        } else {
            baseline->entities.push_back({
                .id = EntityId{"player"},
                .archetype = EntityArchetype::PlayerFighter,
                .team = ioj::Team::Blue,
            });
            baseline->player = EntityId{"player"};
        }

        auto const source{ioj::to_fstring(authoring::emit_editor_level_source(*baseline))};
        if (!TestRunner->TestTrue(TEXT("Write generated records"),
                                  FFileHelper::SaveStringToFile(
                                      source,
                                      *ioj::editor::s7_editor_config_path(*document),
                                      FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) {
            return;
        }

        if (!TestRunner->TestTrue(
                TEXT("Set the mission star scale"),
                FFileHelper::SaveStringToFile(
                    TEXT("\n(set! level-definition (copy-with level-definition :settings (merge-settings (level-ref "
                         "level-definition :settings) "
                         "(settings :starfield (starfield :star (star :scale 2))))))\n"),
                    *root,
                    FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                    &IFileManager::Get(),
                    FILEWRITE_Append))) {
            return;
        }

        auto definition{ioj::editor::read_s7_authored_level(*document)};
        if (!assert_success(*TestRunner, TEXT("Evaluate human entry point"), definition) ||
            !assert_success(*TestRunner,
                            TEXT("Materialize editor proxies"),
                            ioj::editor::import_s7_level(editor_level, *document))) {
            return;
        }

        auto const* enemy_binding{document->entities.FindByPredicate(
            [](auto const& binding) { return binding.id == TEXT("enemy"); })};
        if (!TestRunner->TestNotNull(TEXT("Editor enemy binding"), enemy_binding)) {
            return;
        }

        auto* const editor_enemy{CastChecked<ioj::ATestCapitalShipProxy>(enemy_binding->actor)};
        TestRunner->TestTrue(TEXT("An imported scene matches its saved export"),
                             ioj::editor::s7_scene_needs_export(*document) == false);
        if (!observer) {
            editor_enemy->SetActorLocation(FVector{90000., 0., 0.});
            GEditor->BroadcastEndObjectMovement(*editor_enemy);
            TestRunner->TestTrue(TEXT("Moving an exported actor requires export"),
                                 ioj::editor::s7_scene_needs_export(*document) == true);
        } else {
            editor_enemy->SetActorLocation(FVector{60000., 0., 0.});
            GEditor->BroadcastEndObjectMovement(*editor_enemy);
        }

        auto const config{ioj::editor::s7_editor_config_path(*document)};
        FString const external{source + TEXT("\n; concurrent edit\n")};
        if (!TestRunner->TestTrue(
                TEXT("Make an external source edit"),
                FFileHelper::SaveStringToFile(
                    external, *config, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) {
            return;
        }

        auto conflict{ioj::editor::prepare_staged_mission(*document)};
        TestRunner->TestFalse(TEXT("Conflicting source blocks Play"), conflict.has_value());
        FString untouched;
        TestRunner->TestTrue(TEXT("Play preserves the external edit"),
                             FFileHelper::LoadFileToString(untouched, *config) &&
                                 untouched == external);
        if (!TestRunner->TestTrue(
                TEXT("Restore imported source"),
                FFileHelper::SaveStringToFile(
                    source, *config, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) {
            return;
        }

        auto mission{ioj::editor::prepare_staged_mission(*document)};
        if (!assert_success(
                *TestRunner, TEXT("Prepare the edited mission for the staged player"), mission)) {
            return;
        }

        auto saved{ioj::editor::read_s7_editor_input(*document)};
        if (!assert_success(*TestRunner, TEXT("Read exported scene"), saved)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Play exports the moved enemy"),
                             std::ranges::any_of(saved->entities, [observer](auto const& entity) {
                                 return entity.id == EntityId{"enemy"} &&
                                        entity.position.x() == (observer ? 60000. : 90000.);
                             }));
        if (observer) {
            auto const name{TEXT("RefreshPlay_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)};
            auto const package_name{TEXT("/Game/UI/") + name};
            auto* package{CreatePackage(*package_name)};
            auto* curve{NewObject<UCurveFloat>(package, *name, RF_Public | RF_Standalone)};
            curve->FloatCurve.AddKey(1.f, 2.f);
            asset_file_ = FPackageName::LongPackageNameToFilename(
                package_name, FPackageName::GetAssetPackageExtension());
            FSavePackageArgs save_args;
            save_args.TopLevelFlags = RF_Public | RF_Standalone;
            if (!TestRunner->TestTrue(
                    TEXT("Save an asset in the project's cooked UI directory"),
                    UPackage::SavePackage(package, curve, *asset_file_, save_args))) {
                return;
            }

            auto artifact_root{
                FPlatformMisc::GetEnvironmentVariable(TEXT("IOJ_GAME_ARTIFACT_ROOT"))};
            if (artifact_root.IsEmpty()) {
                artifact_root = FPaths::ProjectDir() / TEXT("out/game-package");
            }

            cooked_asset_ = artifact_root / TEXT("Win64/cook/Windows/Sandbox/Content/UI") /
                            (name + TEXT(".uasset"));
            process_ = ioj::editor::refresh_staged_player();
            if (!TestRunner->TestTrue(TEXT("Start incremental cook and stage"),
                                      process_.IsValid())) {
                return;
            }
            TestCommandBuilder
                .Until([this] { return !FPlatformProcess::IsProcRunning(process_); },
                       FTimespan::FromMinutes(12))
                .Then([this, path = *mission] {
                    if (!check_exit()) {
                        return;
                    }

                    if (TestRunner->TestTrue(TEXT("Refresh cooked the newly saved asset"),
                                             FPaths::FileExists(cooked_asset_))) {
                        launch(path);
                    }
                });
        } else {
            launch(*mission);
        }
        TestCommandBuilder
            .Until(
                [this] {
                    return !process_.IsValid() || !FPlatformProcess::IsProcRunning(process_);
                },
                FTimespan::FromMinutes(2))
            .Then([this] {
                if (process_.IsValid()) {
                    check_exit();
                }
            });
    }

    void launch(FString const& path) {
        auto player{ioj::editor::launch_staged_mission(path, true)};
        if (s7_level_playable_tests::assert_success(
                *TestRunner, TEXT("Launch staged mission"), player)) {
            process_ = *player;
        }
    }

    bool check_exit() {
        int32 code{-1};
        auto const success{FPlatformProcess::GetProcReturnCode(process_, &code)};
        FPlatformProcess::CloseProc(process_);
        return TestRunner->TestTrue(TEXT("Process exited successfully"), success && code == 0);
    }

    TEST_METHOD(PlayExportsAndLaunchesScriptEdits)
    { play_authored_scene(false); }
    TEST_METHOD(RefreshCooksSavedAssetAndPlays)
    { play_authored_scene(true); }

    FProcHandle process_;
    FString asset_file_;
    FString cooked_asset_;
    FString source_directory_;
};
