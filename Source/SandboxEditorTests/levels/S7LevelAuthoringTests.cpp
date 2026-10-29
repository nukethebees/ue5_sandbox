#include "S7LevelAuthoringTestActor.h"
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/sim/capital_ships/sim.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringPreview.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelObserverCamera.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SandboxEditor/levels/S7LevelReconciliation.h>
#include <SandboxEditor/levels/S7LevelSourceSession.h>
#include <SandboxEditor/SandboxEditor.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/definition_reader.h>

#include <SandboxCoreEngine/strings.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Framework/Docking/TabManager.h>
#include <HAL/FileManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>
#include <Toolkits/BaseToolkit.h>
#include <UObject/UnrealType.h>
#include <Widgets/Docking/SDockTab.h>

#include <expected>

namespace {
template <typename Value>
bool assert_success(FAutomationTestBase& test,
                    TCHAR const* description,
                    std::expected<Value, FString> const& result) {
    if (test.TestTrue(description, result.has_value())) {
        return true;
    }
    test.AddError(result.error());
    return false;
}

auto emit_source(ioj::FLevelDraft const& definition) -> std::expected<FString, FString> {
    auto checked{ioj::levels::authoring::to_native(definition)};
    if (!checked) {
        return std::unexpected{ioj::to_fstring(ioj::levels::format_diagnostics(checked.error()))};
    }
    auto source{ioj::levels::authoring::emit_editor_level_source(*checked, std::string_view{})};
    if (!source) {
        return std::unexpected{ioj::to_fstring(ioj::levels::format_diagnostics(source.error()))};
    }
    return ioj::to_fstring(*source);
}

auto collect_level(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<ioj::FLevelDraft, FString> {
    return ioj::editor::collect_s7_editor_level(level, document).transform([](auto const& checked) {
        return ioj::levels::authoring::to_unreal(checked);
    });
}

auto make_plan(ULevel const& level,
               AS7LevelAuthoringDocument const& document,
               ioj::FLevelDraft const& definition)
    -> std::expected<ioj::editor::FS7LevelSyncPlan, FString> {
    auto settings{ioj::editor::read_s7_document_settings(document)};
    if (!settings) {
        return std::unexpected{settings.error()};
    }
    auto scene_definition{definition};
    scene_definition.settings = std::move(*settings);
    auto checked{ioj::levels::authoring::to_native(scene_definition)};
    if (!checked) {
        return std::unexpected{ioj::to_fstring(ioj::levels::format_diagnostics(checked.error()))};
    }
    return ioj::editor::make_s7_level_sync_plan(level, document, MoveTemp(*checked));
}

template <typename T>
auto spawn(UWorld& world, TCHAR const* label, UClass* const actor_class = T::StaticClass()) -> T* {
    auto* const actor{world.SpawnActor<T>(actor_class, FTransform::Identity)};
    if (IsValid(actor)) {
        actor->SetActorLabel(label, true);
    }
    return actor;
}

auto config(UObject& outer) -> USpaceGameLevelConfig* {
    auto* const source{LoadObject<USpaceGameLevelConfig>(
        nullptr, TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"))};
    return IsValid(source) ? DuplicateObject<USpaceGameLevelConfig>(source, &outer) : nullptr;
}

struct FDocumentFixture {
    UWorld* world{FAutomationEditorCommonUtils::CreateNewMap()};
    AS7LevelAuthoringDocument* document{
        spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
    USpaceGameLevelConfig* level_config{config(*world)};
};

struct FPreviewFixture {
    UWorld* world{};
    AS7LevelAuthoringDocument* document{};
    USpaceGameLevelConfig* level_config{};
    ATestSpaceShip* player{};
    ATestCapitalShipProxy* ally{};
    ATestCapitalShipProxy* enemy{};

    auto is_valid() const -> bool {
        return IsValid(world) && IsValid(document) && IsValid(level_config) &&
               (IsValid(player) || IsValid(ally)) && IsValid(enemy);
    }
};

auto make_preview_fixture(bool const observer_camera = false) -> FPreviewFixture {
    FPreviewFixture result;
    result.world = FAutomationEditorCommonUtils::CreateNewMap();
    if (!IsValid(result.world)) {
        return result;
    }
    result.document = spawn<AS7LevelAuthoringDocument>(*result.world, TEXT("S7 Document"));
    result.level_config = config(*result.world);
    if (!IsValid(result.document) || !IsValid(result.level_config) ||
        !IsValid(result.level_config->classes.player_ship_class.Get()) ||
        !IsValid(result.level_config->classes.capital_ship_proxy_class.Get())) {
        return result;
    }
    if (observer_camera) {
        result.ally = spawn<ATestCapitalShipProxy>(
            *result.world,
            TEXT("ally"),
            result.level_config->classes.capital_ship_proxy_class.Get());
    } else {
        result.player = spawn<ATestSpaceShip>(
            *result.world, TEXT("player"), result.level_config->classes.player_ship_class.Get());
    }
    result.enemy = spawn<ATestCapitalShipProxy>(
        *result.world, TEXT("enemy"), result.level_config->classes.capital_ship_proxy_class.Get());
    if ((!IsValid(result.player) && !IsValid(result.ally)) || !IsValid(result.enemy)) {
        return result;
    }

    if (observer_camera) {
        result.ally->set_team(ETestTeam::Blue);
    } else {
        result.player->set_team(ETestTeam::Blue);
    }
    result.enemy->set_team(ETestTeam::Red);
    result.document->level_config = result.level_config;
    result.document->level_id = TEXT("preview-level");
    result.document->title = TEXT("Preview Level");
    result.document->description = TEXT("Before");
    result.document->entities =
        observer_camera
            ? TArray<FS7LevelEntityBinding>{{.id = TEXT("ally"), .actor = result.ally},
                                            {.id = TEXT("enemy"), .actor = result.enemy}}
            : TArray<FS7LevelEntityBinding>{{.id = TEXT("player"), .actor = result.player},
                                            {.id = TEXT("enemy"), .actor = result.enemy}};
    if (observer_camera) {
        result.document->use_observer_camera = true;
        result.document->camera.targets = {result.ally};
        result.document->camera.offset_direction = FVector{-1.0, 0.0, 0.0};
        result.document->camera.distance = 1000.0;
    }
    return result;
}

auto collect_source(FAutomationTestBase& test, FPreviewFixture const& fixture)
    -> std::expected<FString, FString> {
    auto const definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
    if (!test.TestTrue(TEXT("Scene collects"), definition.has_value())) {
        return std::unexpected{definition.error()};
    }
    auto source{emit_source(*definition)};
    test.TestTrue(TEXT("Scene writes"), source.has_value());
    return source;
}

template <typename TTestRunner>
auto assert_valid_definition(TTestRunner& test_runner, ioj::FLevelDraft const& definition) -> bool {
    auto const validation{ioj::validate_level(definition)};
    if (test_runner.TestTrue(TEXT("Fixture definition is valid"), static_cast<bool>(validation))) {
        return true;
    }
    for (auto const& error : validation.errors) {
        test_runner.AddError(error.message);
    }
    return false;
}

auto capital_ship_definition(TCHAR const* level_id,
                             TCHAR const* title,
                             TCHAR const* entity_id = TEXT("ship"),
                             FVector const position = FVector::ZeroVector,
                             FRotator const rotation = FRotator::ZeroRotator,
                             double const spawn_time = 0.0) -> ioj::FLevelDraft {
    ioj::FLevelBuilder builder;
    builder.set_metadata({.id = ioj::FLevelId{level_id}, .title = title});
    builder.set_camera({.target_entity_ids = {ioj::FLevelEntityId{entity_id}},
                        .offset_direction = FVector{-1.0, 0.0, 0.0},
                        .distance = 1000.0});
    builder.add_entity({.id = ioj::FLevelEntityId{entity_id},
                        .archetype = ioj::level_archetypes::capital_ship,
                        .team = ioj::Team::Blue,
                        .position = position,
                        .rotation = rotation,
                        .spawn_time_seconds = spawn_time});
    return builder.finish();
}

auto bound_binding(AS7LevelAuthoringDocument const& document, FName const id)
    -> FS7LevelEntityBinding const* {
    return document.entities.FindByPredicate(
        [id](FS7LevelEntityBinding const& candidate) { return candidate.id == id; });
}

auto bound_actor(AS7LevelAuthoringDocument const& document, FName const id) -> AActor* {
    auto const* const binding{bound_binding(document, id)};
    return binding ? binding->actor.Get() : nullptr;
}

template <typename T>
auto count_actors(ULevel const& level) -> int32 {
    int32 count{};
    for (auto const actor : level.Actors) {
        if (IsValid(Cast<T>(actor.Get()))) {
            ++count;
        }
    }
    return count;
}

struct FTemporarySourceDirectory {
    FString path{FPaths::Combine(FPaths::ProjectSavedDir(),
                                 TEXT("Automation"),
                                 TEXT("S7LevelAuthoring"),
                                 FGuid::NewGuid().ToString())};

    FTemporarySourceDirectory() { IFileManager::Get().MakeDirectory(*path, true); }

    ~FTemporarySourceDirectory() { IFileManager::Get().DeleteDirectory(*path, false, true); }
};

auto write_source(FStringView const source, FString const& path) -> bool {
    return FFileHelper::SaveStringToFile(
        FString{source}, *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

auto make_stale_preview(FPreviewFixture& fixture,
                        ioj::editor::FS7LevelSourceSession& source_session)
    -> std::expected<ioj::editor::FS7LevelAuthoringPreview, FString> {
    auto const attached{source_session.attach(*fixture.document)};
    if (!attached) {
        return std::unexpected{attached.error()};
    }
    source_session.set_buffer(TEXT("preview source"));

    auto definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
    if (!definition) {
        return std::unexpected{definition.error()};
    }
    definition->metadata.title = TEXT("From Preview");
    auto const plan{make_plan(*fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
    if (!plan) {
        return std::unexpected{plan.error()};
    }
    return ioj::editor::make_s7_level_authoring_preview(
        *fixture.world->GetCurrentLevel(), *fixture.document, source_session, *plan);
}

template <typename TTestRunner>
void assert_stale_preview_does_not_apply(TTestRunner& test_runner,
                                         FPreviewFixture const& fixture,
                                         ioj::editor::FS7LevelSourceSession const& source_session,
                                         ioj::editor::FS7LevelAuthoringPreview const& preview,
                                         FStringView const expected_reason) {
    auto const player_count{count_actors<ATestSpaceShip>(*fixture.world->GetCurrentLevel())};
    auto const capital_count{
        count_actors<ATestCapitalShipProxy>(*fixture.world->GetCurrentLevel())};
    auto const applied{ioj::editor::apply_s7_level_authoring_preview(
        *fixture.world->GetCurrentLevel(), *fixture.document, source_session, preview)};
    test_runner.TestFalse(TEXT("Stale preview is rejected"), applied.has_value());
    if (!applied) {
        test_runner.TestTrue(TEXT("Stale reason is useful"),
                             applied.error().code == ioj::editor::ES7LevelPreviewError::Stale &&
                                 applied.error().message.Contains(expected_reason));
    }
    test_runner.TestEqual(TEXT("Stale apply preserves title"),
                          fixture.document->title,
                          FString{TEXT("Preview Level")});
    test_runner.TestEqual(TEXT("Stale apply preserves player count"),
                          count_actors<ATestSpaceShip>(*fixture.world->GetCurrentLevel()),
                          player_count);
    test_runner.TestEqual(TEXT("Stale apply preserves capital count"),
                          count_actors<ATestCapitalShipProxy>(*fixture.world->GetCurrentLevel()),
                          capital_count);
}
}

TEST_CLASS(S7LevelAuthoring, "Sandbox.UnitTests")
{
    TEST_METHOD(MetadataIsAuthoredInTheScript)
    {
        auto const* type{AS7LevelAuthoringDocument::StaticClass()};
        for (auto const name : {TEXT("level_id"), TEXT("title"), TEXT("description")}) {
            auto const* property{type->FindPropertyByName(name)};
            if (!TestRunner->TestNotNull(name, property)) {
                return;
            }
            TestRunner->TestFalse(FString::Printf(TEXT("%s has no duplicate details editor"), name),
                                  property->HasAnyPropertyFlags(CPF_Edit));
        }
    }

    TEST_METHOD(NewDocumentAndGameRuntimeUseTheSameCanonicalBase)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto const created{ioj::editor::create_level_authoring_document(*world->GetCurrentLevel())};
        auto* const canonical{ioj::s7_level_config::load_canonical()};
        if (!TestRunner->TestTrue(TEXT("Authoring document is created"), created.has_value()) ||
            !TestRunner->TestNotNull(TEXT("Canonical S7 config loads"), canonical)) {
            return;
        }
        TestRunner->TestTrue(TEXT("New document uses canonical defaults"),
                             (*created)->level_config == canonical);

        auto* const runtime_map{
            LoadObject<UWorld>(nullptr, TEXT("/SpaceGame/Levels/GameRuntime.GameRuntime"))};
        if (!TestRunner->TestNotNull(TEXT("Generated runtime map loads"), runtime_map)) {
            return;
        }
        ATestBatchOrchestrator const* runtime_orchestrator{};
        for (auto const actor_ptr : runtime_map->PersistentLevel->Actors) {
            if (auto const* const candidate{Cast<ATestBatchOrchestrator>(actor_ptr.Get())}) {
                runtime_orchestrator = candidate;
                break;
            }
        }
        if (TestRunner->TestNotNull(TEXT("Runtime map owns an orchestrator"),
                                    runtime_orchestrator)) {
            TestRunner->TestTrue(TEXT("Editor and runtime use the same base asset"),
                                 runtime_orchestrator->get_level_config() == canonical);
        }
    }

    TEST_METHOD(SettingsResolveStoredBaseAndLevelSources)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto const source{collect_source(*TestRunner, fixture)};
        if (!source) {
            return;
        }
        fixture.document->synchronized_base_settings_source = TEXT(
            "(settings :collision-grid (collision-grid :level-size '(3000000 3000000 100000)))");
        fixture.document->evaluated_base_settings_source =
            fixture.document->synchronized_base_settings_source;
        fixture.document->synchronized_source =
            source->Replace(TEXT("(level"),
                            TEXT("(level :settings (settings :collision-grid (collision-grid "
                                 ":cell-size '(6000 6000 20000)))"));
        fixture.document->evaluated_level_source = fixture.document->synchronized_source;
        auto const settings{ioj::editor::read_s7_document_settings(*fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Source chain resolves"), settings.has_value())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Omitted size inherits from base"),
                             settings->collision_grid.level_size ==
                                 ioj::Vector3f{3000000.f, 3000000.f, 100000.f});
        TestRunner->TestTrue(TEXT("Level replaces supplied cells"),
                             settings->collision_grid.cell_size ==
                                 ioj::Vector3f{6000.f, 6000.f, 20000.f});
        fixture.level_config->collision_grid.grid_size = FVector3f::ZeroVector;
        auto const unchanged{ioj::editor::read_s7_document_settings(*fixture.document)};
        TestRunner->TestTrue(TEXT("Resource grid edits do not alter the applied source chain"),
                             unchanged.has_value() &&
                                 unchanged->collision_grid == settings->collision_grid);
    }

    TEST_METHOD(SettingsApplyStoresReplayInputsAndUndoRestoresThem)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto const source{collect_source(*TestRunner, fixture)};
        if (!source) {
            return;
        }
        auto const incoming{
            source->Replace(TEXT("(level"),
                            TEXT("(level :settings (settings :collision-grid (collision-grid "
                                 ":cell-size '(7000 8000 22000)))"))};
        ioj::editor::FS7LevelSourceSession session;
        if (!TestRunner->TestTrue(TEXT("Session attaches"),
                                  session.attach(*fixture.document).has_value())) {
            return;
        }
        session.set_buffer(incoming);
        auto const base{ioj::editor::read_s7_document_settings(*fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Base settings resolve"), base.has_value())) {
            return;
        }
        auto definition{session.read_replay(*base)};
        if (!TestRunner->TestTrue(TEXT("Incoming source parses"), definition.has_value())) {
            return;
        }
        auto const plan{ioj::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(),
            *fixture.document,
            definition->definition,
            ioj::editor::FS7LevelReplaySources{
                incoming, TEXT("(settings)"), definition->evaluated_source, TEXT("'(:settings)")})};
        if (!TestRunner->TestTrue(TEXT("Settings change plans"), plan.has_value())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Plan identifies settings change"), plan->settings_changed);
        auto const applied{ioj::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Settings change applies"), applied.has_value())) {
            return;
        }
        auto const settings{ioj::editor::read_s7_document_settings(*fixture.document)};
        TestRunner->TestTrue(TEXT("Applied settings match the replay result"),
                             settings.has_value() &&
                                 *settings == definition->definition.settings());
        TestRunner->TestEqual(
            TEXT("Level source is retained"), fixture.document->synchronized_source, incoming);
        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Undo restores the old level source"),
                             fixture.document->synchronized_source.IsEmpty());
        TestRunner->TestTrue(TEXT("Undo restores the old base source"),
                             fixture.document->synchronized_base_settings_source.IsEmpty());
    }

    TEST_METHOD(CanonicalSceneSavePreservesSparseSettings)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto const source{collect_source(*TestRunner, fixture)};
        if (!source) {
            return;
        }
        fixture.document->synchronized_source =
            source->Replace(TEXT("(level"),
                            TEXT("(level :settings (settings :collision-grid (collision-grid "
                                 ":cell-size '(6000 6000 20000)))"));
        fixture.document->evaluated_level_source = fixture.document->synchronized_source;
        auto const definition{ioj::editor::collect_s7_editor_level(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Resolved scene collects"), definition.has_value())) {
            return;
        }
        auto const saved{ioj::editor::emit_s7_scene_source(*definition, *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Canonical source writes"), saved.has_value())) {
            return;
        }
        TestRunner->TestFalse(TEXT("Inherited size stays omitted"),
                              saved->Contains(TEXT(":level-size")));
        TestRunner->TestTrue(TEXT("Authored cell size persists"),
                             saved->Contains(TEXT(":cell-size")));
    }

    TEST_METHOD(ObserverCameraTransformMatchesRuntimeCalculation)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }

        fixture.player->SetActorLocation(FVector{100.0, 200.0, 300.0});
        fixture.enemy->SetActorLocation(FVector{700.0, 1000.0, 1300.0});
        fixture.document->use_observer_camera = true;
        fixture.document->camera.targets = {fixture.player};
        fixture.document->camera.offset_direction = FVector{3.0, 4.0, 0.0};
        fixture.document->camera.distance = 1000.0;

        auto const one_target{ioj::editor::make_s7_observer_camera_transform(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Single-target observer camera is valid"),
                                  one_target.has_value()) ||
            !TestRunner->TestTrue(TEXT("Single-target transform is present"),
                                  one_target && one_target->IsSet())) {
            return;
        }
        auto const expected_one_target_position{FVector{700.0, 1000.0, 300.0}};
        TestRunner->TestTrue(
            TEXT("Single target determines focus"),
            one_target->GetValue().focus.Equals(fixture.player->GetActorLocation(), 0.001));
        TestRunner->TestTrue(TEXT("Offset direction is normalized before distance"),
                             one_target->GetValue().transform.GetLocation().Equals(
                                 expected_one_target_position, 0.001));
        TestRunner->TestTrue(
            TEXT("Observer camera looks back at focus"),
            one_target->GetValue().transform.GetRotation().Rotator().Equals(
                (one_target->GetValue().focus - expected_one_target_position).Rotation(), 0.001));

        fixture.document->camera.targets = {fixture.player, fixture.enemy};
        auto const multiple_targets{ioj::editor::make_s7_observer_camera_transform(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Multiple-target observer camera is valid"),
                                  multiple_targets.has_value()) ||
            !TestRunner->TestTrue(TEXT("Multiple-target transform is present"),
                                  multiple_targets && multiple_targets->IsSet())) {
            return;
        }
        auto const expected_focus{FVector{400.0, 600.0, 800.0}};
        TestRunner->TestTrue(TEXT("Multiple targets average to focus"),
                             multiple_targets->GetValue().focus.Equals(expected_focus, 0.001));
        TestRunner->TestTrue(TEXT("Multiple-target camera position matches runtime formula"),
                             multiple_targets->GetValue().transform.GetLocation().Equals(
                                 FVector{1000.0, 1400.0, 800.0}, 0.001));

        fixture.document->camera.targets.Reset();
        auto const missing_targets{ioj::editor::make_s7_observer_camera_transform(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        TestRunner->TestFalse(TEXT("Missing camera targets are rejected"),
                              missing_targets.has_value());

        fixture.document->camera.targets = {fixture.player};
        fixture.document->camera.offset_direction = FVector::ZeroVector;
        auto const invalid_direction{ioj::editor::make_s7_observer_camera_transform(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        TestRunner->TestFalse(TEXT("Zero camera direction is rejected"),
                              invalid_direction.has_value());

        fixture.document->use_observer_camera = false;
        auto const player_camera{ioj::editor::make_s7_observer_camera_transform(
            *fixture.world->GetCurrentLevel(), *fixture.document)};
        TestRunner->TestTrue(TEXT("Player-authored levels have no observer visualization"),
                             player_camera.has_value() && !player_camera->IsSet());
    }

    TEST_METHOD(StalePreviewRejectsChangedSourceAndPath)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), preview)) {
            return;
        }
        source_session.set_buffer(TEXT("changed source"));
        source_session.set_buffer(TEXT("preview source"));
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *preview, TEXT("source or source path"));

        ioj::editor::FS7LevelSourceSession path_session;
        auto const path_preview{make_stale_preview(fixture, path_session)};
        if (!assert_success(*TestRunner, TEXT("Path preview builds"), path_preview)) {
            return;
        }
        FTemporarySourceDirectory directory;
        auto const path{FPaths::Combine(directory.path, TEXT("other.scm"))};
        if (!TestRunner->TestTrue(
                TEXT("Source path changes"),
                path_session.save_as(path, ioj::editor::ES7SourceOverwritePolicy::ReplaceExisting)
                    .has_value())) {
            return;
        }
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, path_session, *path_preview, TEXT("source or source path"));
    }

    TEST_METHOD(StalePreviewRejectsSceneAndDocumentChanges)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const transform_preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Transform preview builds"), transform_preview)) {
            return;
        }
        fixture.player->SetActorLocation(FVector{100.0, 0.0, 0.0});
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *transform_preview, TEXT("scene"));
        TestRunner->TestEqual(TEXT("Stale apply preserves transform"),
                              fixture.player->GetActorLocation(),
                              FVector{100.0, 0.0, 0.0});

        fixture.player->SetActorLocation(FVector::ZeroVector);
        ioj::editor::FS7LevelSourceSession team_session;
        auto const team_preview{make_stale_preview(fixture, team_session)};
        if (!assert_success(*TestRunner, TEXT("Team preview builds"), team_preview)) {
            return;
        }
        fixture.enemy->set_team(ETestTeam::Blue);
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, team_session, *team_preview, TEXT("scene"));
        TestRunner->TestEqual(
            TEXT("Stale apply preserves team"), fixture.enemy->get_team(), ETestTeam::Blue);

        fixture.enemy->set_team(ETestTeam::Red);
        ioj::editor::FS7LevelSourceSession metadata_session;
        auto const metadata_preview{make_stale_preview(fixture, metadata_session)};
        if (!assert_success(*TestRunner, TEXT("Metadata preview builds"), metadata_preview)) {
            return;
        }
        fixture.document->description = TEXT("Edited after preview");
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, metadata_session, *metadata_preview, TEXT("document"));
        TestRunner->TestEqual(TEXT("Stale apply preserves metadata"),
                              fixture.document->description,
                              FString{TEXT("Edited after preview")});

        fixture.document->description = TEXT("Before");
        ioj::editor::FS7LevelSourceSession camera_session;
        auto const camera_preview{make_stale_preview(fixture, camera_session)};
        if (!assert_success(*TestRunner, TEXT("Camera preview builds"), camera_preview)) {
            return;
        }
        fixture.document->use_observer_camera = true;
        fixture.document->camera.targets = {fixture.player};
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, camera_session, *camera_preview, TEXT("scene"));
        TestRunner->TestTrue(TEXT("Stale apply preserves camera"),
                             fixture.document->use_observer_camera);
    }

    TEST_METHOD(StalePreviewRejectsBindingsAndActorReplacement)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const id_preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Binding preview builds"), id_preview)) {
            return;
        }
        fixture.document->entities[1].id = TEXT("renamed-enemy");
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *id_preview, TEXT("bindings"));

        fixture.document->entities[1].id = TEXT("enemy");
        ioj::editor::FS7LevelSourceSession replacement_session;
        auto const replacement_preview{make_stale_preview(fixture, replacement_session)};
        if (!assert_success(*TestRunner, TEXT("Replacement preview builds"), replacement_preview)) {
            return;
        }
        auto* const replacement{spawn<ATestCapitalShipProxy>(
            *fixture.world,
            TEXT("replacement"),
            fixture.level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("Replacement actor"), replacement)) {
            return;
        }
        replacement->set_team(ETestTeam::Red);
        fixture.document->entities[1].actor = replacement;
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, replacement_session, *replacement_preview, TEXT("bindings"));
        TestRunner->TestEqual(TEXT("Stale apply preserves replacement binding"),
                              fixture.document->entities[1].actor.Get(),
                              static_cast<AActor*>(replacement));

        fixture.document->entities[1].actor = fixture.enemy;
        replacement->Destroy();
        ioj::editor::FS7LevelSourceSession deletion_session;
        auto const deletion_preview{make_stale_preview(fixture, deletion_session)};
        if (!assert_success(*TestRunner, TEXT("Deletion preview builds"), deletion_preview)) {
            return;
        }
        fixture.enemy->Destroy();
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, deletion_session, *deletion_preview, TEXT("bindings"));
    }

    TEST_METHOD(StalePreviewRejectsConfigurationChanges)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const pointer_preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Pointer preview builds"), pointer_preview)) {
            return;
        }
        fixture.document->level_config = config(*fixture.world);
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *pointer_preview, TEXT("level configuration"));

        fixture.document->level_config = fixture.level_config;
        ioj::editor::FS7LevelSourceSession value_session;
        auto const value_preview{make_stale_preview(fixture, value_session)};
        if (!assert_success(*TestRunner, TEXT("Value preview builds"), value_preview)) {
            return;
        }
        fixture.level_config->laser_debug_shapes = !fixture.level_config->laser_debug_shapes;
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, value_session, *value_preview, TEXT("level configuration"));

        fixture.level_config->laser_debug_shapes = !fixture.level_config->laser_debug_shapes;
        ioj::editor::FS7LevelSourceSession class_session;
        auto const class_preview{make_stale_preview(fixture, class_session)};
        if (!assert_success(*TestRunner, TEXT("Class preview builds"), class_preview)) {
            return;
        }
        fixture.level_config->classes.player_ship_class = nullptr;
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, class_session, *class_preview, TEXT("level configuration"));
    }

    TEST_METHOD(StalePreviewRejectsLevelAndDocumentChanges)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const level_preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Level preview builds"), level_preview)) {
            return;
        }
        auto* const other_level{NewObject<ULevel>(fixture.world)};
        if (!TestRunner->TestNotNull(TEXT("Other level"), other_level)) {
            return;
        }
        auto const applied{ioj::editor::apply_s7_level_authoring_preview(
            *other_level, *fixture.document, source_session, *level_preview)};
        TestRunner->TestFalse(TEXT("Different level is rejected"), applied.has_value());
        TestRunner->TestEqual(TEXT("Different level preserves original document title"),
                              fixture.document->title,
                              FString{TEXT("Preview Level")});

        ioj::editor::FS7LevelSourceSession document_session;
        auto const document_preview{make_stale_preview(fixture, document_session)};
        if (!assert_success(*TestRunner, TEXT("Document preview builds"), document_preview)) {
            return;
        }
        auto* const other_document{
            spawn<AS7LevelAuthoringDocument>(*fixture.world, TEXT("Other Document"))};
        if (!TestRunner->TestNotNull(TEXT("Other document"), other_document)) {
            return;
        }
        auto const document_applied{
            ioj::editor::apply_s7_level_authoring_preview(*fixture.world->GetCurrentLevel(),
                                                          *other_document,
                                                          document_session,
                                                          *document_preview)};
        TestRunner->TestFalse(TEXT("Different document is rejected"), document_applied.has_value());
        TestRunner->TestEqual(TEXT("Different document preserves original title"),
                              fixture.document->title,
                              FString{TEXT("Preview Level")});
    }

    TEST_METHOD(UnchangedPreviewAppliesAndExternalDiskEditsDoNotInvalidateIt)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ioj::editor::FS7LevelSourceSession source_session;
        auto const preview{make_stale_preview(fixture, source_session)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), preview)) {
            return;
        }
        auto const applied{ioj::editor::apply_s7_level_authoring_preview(
            *fixture.world->GetCurrentLevel(), *fixture.document, source_session, *preview)};
        TestRunner->TestTrue(TEXT("Unchanged preview applies"), applied.has_value());
        TestRunner->TestEqual(TEXT("Unchanged preview applies planned title"),
                              fixture.document->title,
                              FString{TEXT("From Preview")});

        fixture.document->title = TEXT("Preview Level");
        FTemporarySourceDirectory directory;
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        if (!TestRunner->TestTrue(TEXT("Initial disk source writes"),
                                  write_source(TEXT("initial source"), source_path))) {
            return;
        }
        fixture.document->source_path = source_path;
        ioj::editor::FS7LevelSourceSession disk_session;
        auto const disk_preview{make_stale_preview(fixture, disk_session)};
        if (!assert_success(*TestRunner, TEXT("Disk preview builds"), disk_preview)) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("External source changes"),
                                  write_source(TEXT("external source"), source_path)) ||
            !TestRunner->TestTrue(TEXT("Conflict refreshes"),
                                  disk_session.refresh_external_conflict().has_value())) {
            return;
        }
        TestRunner->TestTrue(TEXT("External source marks a conflict"),
                             disk_session.has_external_conflict());
        auto const disk_applied{ioj::editor::apply_s7_level_authoring_preview(
            *fixture.world->GetCurrentLevel(), *fixture.document, disk_session, *disk_preview)};
        TestRunner->TestTrue(TEXT("External disk edit leaves preview valid"),
                             disk_applied.has_value());
        TestRunner->TestFalse(TEXT("External disk edit blocks save"),
                              disk_session.save().has_value());
    }

    TEST_METHOD(PreviewReportsMetadataOnlyChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Baseline collects"), definition)) {
            return;
        }
        definition->metadata.title = TEXT("After");
        definition->metadata.description = TEXT("After description");

        auto const plan{
            make_plan(*fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Metadata change is reported"), plan->metadata_changed);
        TestRunner->TestFalse(TEXT("Viewpoint is unchanged"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(
            TEXT("Entities are unchanged"), static_cast<int32>(plan->changes.size()), 0);

        ioj::editor::FS7LevelSourceSession session;
        if (!TestRunner->TestTrue(TEXT("Source attaches"),
                                  session.attach(*fixture.document).has_value())) {
            return;
        }
        auto const position{fixture.player->GetActorLocation()};
        fixture.player->SetActorLocation(position + FVector{100.0, 0.0, 0.0});
        auto const preview{ioj::editor::make_s7_level_authoring_preview(
            *fixture.world->GetCurrentLevel(), *fixture.document, session, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview retains the plan baseline"), preview.has_value())) {
            return;
        }
        assert_stale_preview_does_not_apply(*TestRunner, fixture, session, *preview, TEXT("scene"));
        fixture.player->SetActorLocation(position);

        auto const applied{ioj::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        auto const collected{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Applied scene collects"), collected)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Title is applied"), collected->metadata.title, FString{TEXT("After")});
        TestRunner->TestEqual(TEXT("Description is applied"),
                              collected->metadata.description,
                              FString{TEXT("After description")});
    }

    TEST_METHOD(ObjectiveMissionEditingIsExplicitlyRejected)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Scene collects"), definition.has_value())) {
            return;
        }
        definition->mission = ioj::levels::MissionDefinition{
            .objectives = {
                {.id = ioj::levels::ObjectiveName{"primary"},
                 .team = ioj::Team::Blue,
                 .expressions = {{ioj::levels::TimeElapsedExpr{
                     .seconds = ioj::NonNegativeFinite<double>::make(1.0)}}},
                 .roots = ioj::levels::ObjectiveRoots{std::array<ioj::levels::ExprIndex, 3>{
                     0, ioj::levels::absent_expression, ioj::levels::absent_expression}}}}};
        auto const plan{
            make_plan(*fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        TestRunner->TestFalse(TEXT("Unsupported graphs cannot be applied lossily"),
                              plan.has_value());
        TestRunner->TestFalse(TEXT("Unsupported graphs cannot be written lossily"),
                              emit_source(*definition).has_value());
    }

    TEST_METHOD(PreviewReportsObserverCameraOnlyChange)
    {
        auto const fixture{make_preview_fixture(true)};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Baseline collects"), definition)) {
            return;
        }
        definition->camera->target_entity_ids = {ioj::FLevelEntityId{TEXT("enemy")}};
        if (!assert_valid_definition(*TestRunner, *definition)) {
            return;
        }

        auto const plan{
            make_plan(*fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestTrue(TEXT("Viewpoint change is reported"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(
            TEXT("Entities are unchanged"), static_cast<int32>(plan->changes.size()), 0);

        auto const applied{ioj::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        auto const collected{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Applied scene collects"), collected.has_value()) ||
            !TestRunner->TestTrue(TEXT("Camera is present"), collected->camera.IsSet())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Camera target is applied"),
                             collected->camera->target_entity_ids ==
                                 TArray<ioj::FLevelEntityId>{ioj::FLevelEntityId{TEXT("enemy")}});
    }

    TEST_METHOD(PreviewReportsPlayerToCameraViewpointChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Baseline collects"), definition)) {
            return;
        }
        definition->player_entity_id = {};
        definition->camera = ioj::FLevelCameraDefinition{
            .target_entity_ids = {ioj::FLevelEntityId{TEXT("enemy")}},
            .offset_direction = FVector{-1.0, 0.0, 0.0},
            .distance = 1000.0,
        };
        definition->entities.RemoveAtSwap(0, 1, EAllowShrinking::No);
        if (!assert_valid_definition(*TestRunner, *definition)) {
            return;
        }

        auto const plan{
            make_plan(*fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestTrue(TEXT("Viewpoint change is reported"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(
            TEXT("Player actor is removed"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Remove)),
            1);

        auto const applied{ioj::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        auto const collected{collect_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Applied scene collects"), collected)) {
            return;
        }
        TestRunner->TestFalse(TEXT("Player viewpoint is cleared"),
                              collected->player_entity_id.is_set());
        TestRunner->TestTrue(TEXT("Camera viewpoint is applied"), collected->camera.IsSet());
        TestRunner->TestNull(TEXT("Player actor is removed"),
                             bound_actor(*fixture.document, TEXT("player")));
    }

    TEST_METHOD(UnchangedActorIsNeverModifiedOrReconstructed)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        level_config->classes.player_ship_class = AS7LevelAuthoringTestActor::StaticClass();
        document->level_config = level_config;
        document->level_id = TEXT("unchanged");
        document->title = TEXT("Unchanged");
        auto* const actor{spawn<AS7LevelAuthoringTestActor>(*world, TEXT("player"))};
        actor->set_team(ETestTeam::Blue);
        document->entities = {{.id = TEXT("player"), .actor = actor}};
        auto definition{collect_level(*world->GetCurrentLevel(), *document)};
        if (!TestRunner->TestTrue(TEXT("Definition collects"), definition.has_value())) {
            return;
        }

        auto const modifications{actor->modifications};
        auto const constructions{actor->constructions};
        auto* const root{actor->GetRootComponent()};
        for (auto const metadata_change : {false, true}) {
            if (metadata_change) {
                definition->metadata.title = TEXT("Updated title");
            }
            auto const plan{make_plan(*world->GetCurrentLevel(), *document, *definition)};
            if (!TestRunner->TestTrue(TEXT("Plan builds"), plan.has_value())) {
                return;
            }
            TestRunner->TestTrue(
                TEXT("Plan applies"),
                ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)
                    .has_value());
            TestRunner->TestEqual(
                TEXT("Actor is not modified"), actor->modifications, modifications);
            TestRunner->TestEqual(
                TEXT("Construction is not rerun"), actor->constructions, constructions);
            TestRunner->TestTrue(TEXT("Component identity survives"),
                                 actor->GetRootComponent() == root);
        }
    }

    TEST_METHOD(ApplyAddsConfiguredActor)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;

        auto const definition{capital_ship_definition(TEXT("add"),
                                                      TEXT("Add"),
                                                      TEXT("ship"),
                                                      FVector{100.0, 200.0, 300.0},
                                                      FRotator{10.0, 20.0, 30.0})};
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(TEXT("One actor adds"),
                              static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Add)),
                              1);
        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }

        auto* const actor{Cast<ATestCapitalShipProxy>(bound_actor(*document, TEXT("ship")))};
        if (!TestRunner->TestNotNull(TEXT("Added actor is bound"), actor)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Configured class is exact"),
                             actor->GetClass() ==
                                 level_config->classes.capital_ship_proxy_class.Get());
        TestRunner->TestTrue(TEXT("Transform is applied"),
                             actor->GetActorTransform().Equals(FTransform{
                                 FRotator{10.0, 20.0, 30.0}, FVector{100.0, 200.0, 300.0}}));
        TestRunner->TestEqual(TEXT("Team is applied"), actor->get_team(), ETestTeam::Blue);
        TestRunner->TestEqual(
            TEXT("Label is applied"), actor->GetActorLabel(), FString{TEXT("ship")});
    }

    TEST_METHOD(ApplyUpdatesActorInPlace)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config) ||
            !TestRunner->TestNotNull(TEXT("Configured capital class"),
                                     level_config->classes.capital_ship_proxy_class.Get())) {
            return;
        }
        auto* const actor{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Wrong Label"), level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("Actor"), actor)) {
            return;
        }
        actor->set_team(ETestTeam::Red);
        actor->SetActorTransform(FTransform{FVector{-100.0, -200.0, -300.0}});
        document->level_config = level_config;
        document->entities = {{.id = TEXT("ship"), .actor = actor}};

        auto const definition{capital_ship_definition(TEXT("update"),
                                                      TEXT("Update"),
                                                      TEXT("ship"),
                                                      FVector{100.0, 200.0, 300.0},
                                                      FRotator{10.0, 20.0, 30.0})};
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor updates"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Update)),
            1);

        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Actor identity is preserved"),
                             bound_actor(*document, TEXT("ship")) == actor);
        TestRunner->TestTrue(TEXT("Transform is updated"),
                             actor->GetActorTransform().Equals(FTransform{
                                 FRotator{10.0, 20.0, 30.0}, FVector{100.0, 200.0, 300.0}}));
        TestRunner->TestEqual(TEXT("Team is updated"), actor->get_team(), ETestTeam::Blue);
        TestRunner->TestEqual(
            TEXT("Label is updated"), actor->GetActorLabel(), FString{TEXT("ship")});
    }

    TEST_METHOD(ExactConfiguredClassMismatchReplacesActor)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config) ||
            !TestRunner->TestNotNull(TEXT("Configured capital class"),
                                     level_config->classes.capital_ship_proxy_class.Get())) {
            return;
        }
        auto* const actor{spawn<ATestCapitalShipProxy>(
            *world, TEXT("ship"), level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("Actor"), actor) ||
            !TestRunner->TestTrue(TEXT("Fixture uses a configured subclass"),
                                  actor->GetClass() != ATestCapitalShipProxy::StaticClass())) {
            return;
        }
        actor->set_team(ETestTeam::Blue);
        level_config->classes.capital_ship_proxy_class = ATestCapitalShipProxy::StaticClass();
        document->level_config = level_config;
        document->entities = {{.id = TEXT("ship"), .actor = actor}};

        auto const definition{capital_ship_definition(TEXT("replace"), TEXT("Replace"))};
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor replaces"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Replace)),
            1);

        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        auto* const replacement{bound_actor(*document, TEXT("ship"))};
        TestRunner->TestTrue(TEXT("Original actor is destroyed"), !IsValid(actor));
        TestRunner->TestTrue(TEXT("Replacement has a new identity"), replacement != actor);
        TestRunner->TestTrue(TEXT("Replacement uses the exact configured class"),
                             IsValid(replacement) &&
                                 replacement->GetClass() == ATestCapitalShipProxy::StaticClass());
    }

    TEST_METHOD(ApplyRemovesOnlyMissingActor)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config) ||
            !TestRunner->TestNotNull(TEXT("Configured capital class"),
                                     level_config->classes.capital_ship_proxy_class.Get())) {
            return;
        }
        auto* const kept{spawn<ATestCapitalShipProxy>(
            *world, TEXT("kept"), level_config->classes.capital_ship_proxy_class.Get())};
        auto* const removed{spawn<ATestCapitalShipProxy>(
            *world, TEXT("removed"), level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("Kept actor"), kept) ||
            !TestRunner->TestNotNull(TEXT("Removed actor"), removed)) {
            return;
        }
        kept->set_team(ETestTeam::Blue);
        removed->set_team(ETestTeam::Blue);
        document->level_config = level_config;
        document->entities = {{.id = TEXT("kept"), .actor = kept},
                              {.id = TEXT("removed"), .actor = removed}};

        auto const definition{
            capital_ship_definition(TEXT("remove"), TEXT("Remove"), TEXT("kept"))};
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor removes"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Remove)),
            1);

        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Preview applies"), applied)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Kept actor identity is preserved"),
                             bound_actor(*document, TEXT("kept")) == kept);
        TestRunner->TestTrue(TEXT("Removed actor is destroyed"), !IsValid(removed));
    }

    TEST_METHOD(ApplyRevalidatesAllClassesBeforeAddingActors)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const existing{spawn<ATestStaticTurretsProxy>(*world, TEXT("existing"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Existing actor"), existing) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        auto const existing_transform{FTransform{FVector{-100.0, -200.0, -300.0}}};
        existing->SetActorTransform(existing_transform);
        existing->set_team(ETestTeam::Yellow);
        document->level_config = level_config;
        document->level_id = TEXT("original");
        document->title = TEXT("Original");
        document->description = TEXT("Unchanged");
        document->entities = {{.id = TEXT("existing"), .actor = existing}};

        ioj::FLevelBuilder builder;
        builder.set_metadata({.id = ioj::FLevelId{TEXT("invalid-later-class")},
                              .title = TEXT("Invalid Later Class")});
        builder.set_player_entity(ioj::FLevelEntityId{TEXT("player")});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("player")},
                            .archetype = ioj::level_archetypes::player_fighter,
                            .team = ::ioj::Team::Blue});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("capital")},
                            .archetype = ioj::level_archetypes::capital_ship,
                            .team = ::ioj::Team::Red});
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, builder.finish())};
        if (!assert_success(*TestRunner, TEXT("Preview builds with valid classes"), plan)) {
            return;
        }

        level_config->classes.capital_ship_proxy_class = nullptr;
        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        TestRunner->TestFalse(TEXT("Invalid later class is rejected"), applied.has_value());
        TestRunner->TestEqual(TEXT("No player was staged"),
                              count_actors<ATestSpaceShip>(*world->GetCurrentLevel()),
                              0);
        TestRunner->TestEqual(TEXT("No capital was staged"),
                              count_actors<ATestCapitalShipProxy>(*world->GetCurrentLevel()),
                              0);
        TestRunner->TestTrue(TEXT("Existing actor identity is unchanged"),
                             bound_actor(*document, TEXT("existing")) == existing);
        TestRunner->TestTrue(TEXT("Existing actor remains valid"), IsValid(existing));
        TestRunner->TestTrue(TEXT("Existing actor transform is unchanged"),
                             existing->GetActorTransform().Equals(existing_transform));
        TestRunner->TestEqual(
            TEXT("Existing actor team is unchanged"), existing->get_team(), ETestTeam::Yellow);
        TestRunner->TestEqual(TEXT("Existing actor label is unchanged"),
                              existing->GetActorLabel(),
                              FString{TEXT("existing")});
        TestRunner->TestEqual(
            TEXT("Level id is unchanged"), document->level_id, FName{TEXT("original")});
        TestRunner->TestEqual(
            TEXT("Title is unchanged"), document->title, FString{TEXT("Original")});
        TestRunner->TestEqual(
            TEXT("Description is unchanged"), document->description, FString{TEXT("Unchanged")});
        TestRunner->TestEqual(TEXT("Bindings are unchanged"), document->entities.Num(), 1);
    }

    TEST_METHOD(RejectsInvalidEntityTeam)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;
        document->level_id = TEXT("original");

        ioj::FLevelBuilder builder;
        builder.set_metadata(
            {.id = ioj::FLevelId{TEXT("invalid-team")}, .title = TEXT("Invalid Team")});
        builder.set_camera({.target_entity_ids = {ioj::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("ship")},
                            .archetype = ioj::level_archetypes::capital_ship,
                            .team = static_cast<::ioj::Team>(255)});
        auto const definition{builder.finish()};

        auto const preview{make_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Preview rejects invalid team"), preview.has_value());
        if (!preview) {
            TestRunner->TestTrue(TEXT("Error explains invalid team"),
                                 preview.error().Contains(TEXT("invalid team")));
        }

        TestRunner->TestEqual(TEXT("No actor is added"),
                              count_actors<ATestCapitalShipProxy>(*world->GetCurrentLevel()),
                              0);
        TestRunner->TestEqual(
            TEXT("Document is unchanged"), document->level_id, FName{TEXT("original")});
    }

    TEST_METHOD(RejectsInvalidAndDuplicateBindings)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const actor{spawn<ATestCapitalShipProxy>(*world, TEXT("ship"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Actor"), actor) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;

        auto const definition{capital_ship_definition(TEXT("bindings"), TEXT("Bindings"))};

        document->entities = {{.id = NAME_None, .actor = actor}};
        auto invalid{make_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Invalid binding is rejected"), invalid.has_value());

        document->entities = {{.id = TEXT("ship"), .actor = actor},
                              {.id = TEXT("ship"), .actor = actor}};
        auto duplicate_id{make_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Duplicate id is rejected"), duplicate_id.has_value());

        document->entities = {{.id = TEXT("ship"), .actor = actor},
                              {.id = TEXT("other"), .actor = actor}};
        auto duplicate_actor{make_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Duplicate actor is rejected"), duplicate_actor.has_value());
    }

    TEST_METHOD(OneUndoRestoresMixedApply)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        level_config->classes.player_ship_class = ATestSpaceShip::StaticClass();
        level_config->classes.capital_ship_proxy_class = ATestCapitalShipProxy::StaticClass();
        level_config->classes.static_turret_proxy_class = ATestStaticTurretsProxy::StaticClass();

        auto* const updated{spawn<ATestCapitalShipProxy>(*world, TEXT("CapitalShip_Red"))};
        auto* const replaced{spawn<ATestCapitalShipProxy>(*world, TEXT("replace"))};
        auto* const removed{spawn<ATestStaticTurretsProxy>(*world, TEXT("remove"))};
        if (!TestRunner->TestNotNull(TEXT("Updated actor"), updated) ||
            !TestRunner->TestNotNull(TEXT("Replaced actor"), replaced) ||
            !TestRunner->TestNotNull(TEXT("Removed actor"), removed)) {
            return;
        }
        auto const original_transform{
            FTransform{FRotator{1.0, 2.0, 3.0}, FVector{-100.0, -200.0, -300.0}}};
        updated->SetActorTransform(original_transform);
        updated->set_team(ETestTeam::Red);
        replaced->set_team(ETestTeam::Red);
        removed->set_team(ETestTeam::Red);

        document->level_config = level_config;
        document->level_id = TEXT("before");
        document->title = TEXT("Before");
        document->description = TEXT("Before description");
        document->entities = {{.id = TEXT("update"), .actor = updated, .spawn_time_seconds = 7.0},
                              {.id = TEXT("replace"), .actor = replaced},
                              {.id = TEXT("remove"), .actor = removed}};
        document->use_observer_camera = true;
        document->camera.targets = {removed};
        document->camera.offset_direction = FVector{0.0, 1.0, 0.0};
        document->camera.distance = 2500.0;

        auto const baseline{collect_level(*world->GetCurrentLevel(), *document)};
        if (!assert_success(*TestRunner, TEXT("Undo baseline is valid"), baseline)) {
            return;
        }
        auto const original_label{updated->GetActorLabel()};

        ioj::FLevelBuilder builder;
        builder.set_metadata({.id = ioj::FLevelId{TEXT("after")},
                              .title = TEXT("After"),
                              .description = TEXT("After description")});
        builder.set_camera({.target_entity_ids = {ioj::FLevelEntityId{TEXT("add")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("update")},
                            .archetype = ioj::level_archetypes::capital_ship,
                            .team = ::ioj::Team::Blue,
                            .position = FVector{100.0, 200.0, 300.0},
                            .rotation = FRotator{10.0, 20.0, 30.0},
                            .spawn_time_seconds = 3.0});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("replace")},
                            .archetype = ioj::level_archetypes::static_turret,
                            .team = ::ioj::Team::Red,
                            .position = FVector{400.0, 500.0, 600.0}});
        builder.add_entity({.id = ioj::FLevelEntityId{TEXT("add")},
                            .archetype = ioj::level_archetypes::capital_ship,
                            .team = ::ioj::Team::Blue,
                            .position = FVector{700.0, 800.0, 900.0}});
        auto const definition{builder.finish()};
        if (!assert_valid_definition(*TestRunner, definition)) {
            return;
        }
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Mixed preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(TEXT("One actor adds"),
                              static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Add)),
                              1);
        TestRunner->TestEqual(
            TEXT("One actor updates"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Update)),
            1);
        TestRunner->TestEqual(
            TEXT("One actor replaces"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Replace)),
            1);
        TestRunner->TestEqual(
            TEXT("One actor removes"),
            static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Remove)),
            1);

        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Mixed preview applies"), applied)) {
            return;
        }
        auto* const added{bound_actor(*document, TEXT("add"))};
        auto* const replacement{bound_actor(*document, TEXT("replace"))};
        if (!TestRunner->TestNotNull(TEXT("Added actor is bound"), added) ||
            !TestRunner->TestNotNull(TEXT("Replacement actor is bound"), replacement)) {
            GEditor->UndoTransaction();
            return;
        }
        TestRunner->TestTrue(TEXT("Update keeps identity"),
                             bound_actor(*document, TEXT("update")) == updated);
        TestRunner->TestTrue(TEXT("Replacement changes identity"), replacement != replaced);
        TestRunner->TestTrue(TEXT("Removed actor is destroyed"), !IsValid(removed));
        TestRunner->TestEqual(
            TEXT("Metadata is published"), document->level_id, FName{TEXT("after")});
        TestRunner->TestTrue(TEXT("Viewpoint is published"), document->use_observer_camera);
        TestRunner->TestTrue(TEXT("Camera target is published"),
                             document->camera.targets.Num() == 1 &&
                                 document->camera.targets[0] == added);
        TestRunner->TestEqual(TEXT("Spawn time is published"),
                              bound_binding(*document, TEXT("update"))->spawn_time_seconds,
                              3.0);

        GEditor->UndoTransaction();

        TestRunner->TestTrue(TEXT("Added actor is removed by undo"), !IsValid(added));
        TestRunner->TestTrue(TEXT("Replacement actor is removed by undo"), !IsValid(replacement));
        TestRunner->TestTrue(TEXT("Replaced actor is restored"), IsValid(replaced));
        TestRunner->TestTrue(TEXT("Removed actor is restored"), IsValid(removed));
        TestRunner->TestTrue(TEXT("Updated actor identity is restored"),
                             bound_actor(*document, TEXT("update")) == updated);
        TestRunner->TestTrue(TEXT("Replaced binding is restored"),
                             bound_actor(*document, TEXT("replace")) == replaced);
        TestRunner->TestTrue(TEXT("Removed binding is restored"),
                             bound_actor(*document, TEXT("remove")) == removed);
        TestRunner->TestTrue(TEXT("Updated transform is restored"),
                             updated->GetActorTransform().Equals(original_transform));
        TestRunner->TestEqual(
            TEXT("Updated team is restored"), updated->get_team(), ETestTeam::Red);
        TestRunner->TestEqual(
            TEXT("Updated label is restored"), updated->GetActorLabel(), original_label);
        TestRunner->TestEqual(TEXT("Spawn time is restored"),
                              bound_binding(*document, TEXT("update"))->spawn_time_seconds,
                              7.0);
        TestRunner->TestEqual(
            TEXT("Metadata is restored"), document->level_id, FName{TEXT("before")});
        TestRunner->TestEqual(TEXT("Title is restored"), document->title, FString{TEXT("Before")});
        TestRunner->TestEqual(TEXT("Description is restored"),
                              document->description,
                              FString{TEXT("Before description")});
        TestRunner->TestTrue(TEXT("Viewpoint is restored"), document->use_observer_camera);
        if (TestRunner->TestEqual(
                TEXT("Camera target count is restored"), document->camera.targets.Num(), 1)) {
            TestRunner->TestEqual(TEXT("Camera target is restored"),
                                  document->camera.targets[0].Get(),
                                  static_cast<AActor*>(removed));
        }
    }

    TEST_METHOD(EnteringModeCreatesDocumentAndAdoptsExistingProxies)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const player{spawn<ATestSpaceShip>(*world, TEXT("Player"))};
        auto* const capital{spawn<ATestCapitalShipProxy>(*world, TEXT("Capital"))};
        if (!TestRunner->TestNotNull(TEXT("World"), world) ||
            !TestRunner->TestNotNull(TEXT("Player"), player) ||
            !TestRunner->TestNotNull(TEXT("Capital"), capital)) {
            return;
        }
        player->set_team(ETestTeam::Blue);
        capital->set_team(ETestTeam::Red);

        auto& modes{GLevelEditorModeTools()};
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);

        auto const document{ioj::editor::find_level_authoring_document(*world->GetCurrentLevel())};
        TestRunner->TestTrue(TEXT("Document is created on mode entry"), document.has_value());
        if (!document || !*document) {
            modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
            return;
        }
        TestRunner->TestEqual(TEXT("Existing proxies are adopted"), (*document)->entities.Num(), 2);
        TestRunner->TestTrue(TEXT("Capital proxy is adopted"),
                             (*document)->entities.ContainsByPredicate(
                                 [capital](FS7LevelEntityBinding const& binding) {
                                     return binding.actor == capital;
                                 }));
        auto const definition{collect_level(*world->GetCurrentLevel(), **document)};
        TestRunner->TestTrue(TEXT("Adopted proxies can be exported"), definition.has_value());

        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
    }

    TEST_METHOD(RepairBindingsCleansReferencesAdoptsDuplicatesAndUndoes)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config) ||
            !TestRunner->TestNotNull(TEXT("Capital class"),
                                     level_config->classes.capital_ship_proxy_class.Get())) {
            return;
        }

        auto* const retained{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Retained"), level_config->classes.capital_ship_proxy_class.Get())};
        auto* const deleted{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Deleted"), level_config->classes.capital_ship_proxy_class.Get())};
        auto* const duplicate_one{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Enemy Group"), level_config->classes.capital_ship_proxy_class.Get())};
        auto* const duplicate_two{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Enemy-Group"), level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("Retained actor"), retained) ||
            !TestRunner->TestNotNull(TEXT("Deleted actor"), deleted) ||
            !TestRunner->TestNotNull(TEXT("First duplicate"), duplicate_one) ||
            !TestRunner->TestNotNull(TEXT("Second duplicate"), duplicate_two)) {
            return;
        }
        retained->set_team(ETestTeam::Blue);
        deleted->set_team(ETestTeam::Red);
        duplicate_one->set_team(ETestTeam::Red);
        duplicate_two->set_team(ETestTeam::Red);
        deleted->Destroy();

        document->entities = {
            {.id = TEXT("enemy-group"), .actor = retained, .spawn_time_seconds = 7.0},
            {.id = TEXT("deleted"), .actor = deleted},
            {.id = TEXT("enemy"), .actor = retained},
            {.id = TEXT("bad id"), .actor = duplicate_one, .spawn_time_seconds = 9.0}};
        document->camera.targets = {retained, deleted, duplicate_one};

        auto const repaired{
            ioj::editor::repair_s7_level_bindings(*world->GetCurrentLevel(), *document)};
        TestRunner->TestEqual(TEXT("Invalid bindings are removed"), repaired.removed_bindings, 3);
        TestRunner->TestEqual(
            TEXT("Only dead references are removed"), repaired.removed_references, 1);
        TestRunner->TestEqual(
            TEXT("Both duplicated actors are adopted"), repaired.adopted_entities, 2);
        TestRunner->TestTrue(TEXT("Stable binding ID is preserved"),
                             bound_actor(*document, TEXT("enemy-group")) == retained);
        TestRunner->TestTrue(TEXT("First canonical collision receives suffix"),
                             bound_actor(*document, TEXT("enemy-group-2")) == duplicate_one);
        TestRunner->TestTrue(TEXT("Second canonical collision receives suffix"),
                             bound_actor(*document, TEXT("enemy-group-3")) == duplicate_two);
        TestRunner->TestEqual(TEXT("Retained and adopted camera references survive"),
                              document->camera.targets.Num(),
                              2);
        TestRunner->TestTrue(TEXT("Camera reference remains bound"),
                             document->camera.targets[0] == retained);
        TestRunner->TestEqual(
            TEXT("Retained spawn time survives"), document->entities[0].spawn_time_seconds, 7.0);
        TestRunner->TestEqual(
            TEXT("Readopted spawn time survives"), document->entities[1].spawn_time_seconds, 9.0);
        TestRunner->TestFalse(
            TEXT("Repeated repair changes nothing"),
            ioj::editor::repair_s7_level_bindings(*world->GetCurrentLevel(), *document)
                .has_changes());
        TestRunner->TestEqual(
            TEXT("Repeated adoption changes nothing"),
            ioj::editor::adopt_unbound_level_entities(*world->GetCurrentLevel(), *document),
            0);

        GEditor->UndoTransaction();

        TestRunner->TestEqual(TEXT("Repair undo restores bindings"), document->entities.Num(), 4);
        TestRunner->TestEqual(
            TEXT("Repair undo restores camera references"), document->camera.targets.Num(), 3);
    }

    TEST_METHOD(SelectedEntityIdRenameValidatesAndUndoes)
    {
        auto const [world, document, level_config]{FDocumentFixture{}};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config) ||
            !TestRunner->TestNotNull(TEXT("Capital class"),
                                     level_config->classes.capital_ship_proxy_class.Get())) {
            return;
        }

        auto* const first{spawn<ATestCapitalShipProxy>(
            *world, TEXT("First"), level_config->classes.capital_ship_proxy_class.Get())};
        auto* const second{spawn<ATestCapitalShipProxy>(
            *world, TEXT("Second"), level_config->classes.capital_ship_proxy_class.Get())};
        if (!TestRunner->TestNotNull(TEXT("First actor"), first) ||
            !TestRunner->TestNotNull(TEXT("Second actor"), second)) {
            return;
        }
        first->set_team(ETestTeam::Blue);
        second->set_team(ETestTeam::Red);
        document->entities = {{.id = TEXT("first"), .actor = first},
                              {.id = TEXT("second"), .actor = second}};

        auto& modes{GLevelEditorModeTools()};
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);
        auto* const mode{Cast<US7LevelAuthoringMode>(
            modes.GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
        if (!TestRunner->TestNotNull(TEXT("Authoring mode"), mode)) {
            return;
        }

        GEditor->SelectNone(false, true);
        GEditor->SelectActor(first, true, true);
        mode->rename_selected_entity(TEXT("renamed-first"));
        TestRunner->TestTrue(TEXT("Selected binding is renamed"),
                             bound_actor(*document, TEXT("renamed-first")) == first);

        GEditor->UndoTransaction();
        TestRunner->TestTrue(TEXT("Rename undo restores ID"),
                             bound_actor(*document, TEXT("first")) == first);

        mode->rename_selected_entity(TEXT("Invalid ID"));
        TestRunner->TestTrue(TEXT("Invalid symbol does not mutate binding"),
                             bound_actor(*document, TEXT("first")) == first);
        mode->rename_selected_entity(TEXT("Uppercase"));
        TestRunner->TestTrue(TEXT("Uppercase symbol does not mutate binding"),
                             bound_actor(*document, TEXT("first")) == first);
        mode->rename_selected_entity(TEXT("second"));
        TestRunner->TestTrue(TEXT("Duplicate ID does not mutate binding"),
                             bound_actor(*document, TEXT("first")) == first);

        GEditor->SelectNone(false, true);
        mode->rename_selected_entity(TEXT("unselected"));
        TestRunner->TestTrue(TEXT("No-selection error leaves binding unchanged"),
                             bound_actor(*document, TEXT("first")) == first);

        GEditor->SelectActor(first, true, true);
        GEditor->SelectActor(second, true, true);
        mode->rename_selected_entity(TEXT("multiple"));
        TestRunner->TestTrue(TEXT("Multiple-selection error leaves binding unchanged"),
                             bound_actor(*document, TEXT("first")) == first);

        GEditor->SelectNone(false, true);
        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
    }

    TEST_METHOD(DelayedSpawnsApplyUpdateAndRoundTrip)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        document->level_config = config(*world);
        if (!TestRunner->TestNotNull(TEXT("Config"), document->level_config.Get())) {
            return;
        }

        auto const definition{capital_ship_definition(TEXT("delayed"),
                                                      TEXT("Delayed"),
                                                      TEXT("ship"),
                                                      FVector::ZeroVector,
                                                      FRotator::ZeroRotator,
                                                      2.5)};
        auto const plan{make_plan(*world->GetCurrentLevel(), *document, definition)};
        if (!assert_success(*TestRunner, TEXT("Delayed spawn preview builds"), plan)) {
            return;
        }
        TestRunner->TestEqual(TEXT("Delayed actor adds"),
                              static_cast<int32>(plan->count(ioj::editor::ES7LevelSyncAction::Add)),
                              1);

        auto const applied{
            ioj::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!assert_success(*TestRunner, TEXT("Delayed spawn applies"), applied)) {
            return;
        }
        auto* const actor{bound_actor(*document, TEXT("ship"))};
        auto const* const binding{bound_binding(*document, TEXT("ship"))};
        if (!TestRunner->TestNotNull(TEXT("Delayed actor is bound"), actor) ||
            !TestRunner->TestNotNull(TEXT("Delayed binding exists"), binding)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Delay is stored in the world document"), binding->spawn_time_seconds, 2.5);

        auto const collected{collect_level(*world->GetCurrentLevel(), *document)};
        if (!assert_success(*TestRunner, TEXT("Delayed scene collects"), collected)) {
            return;
        }
        auto const source{emit_source(*collected)};
        if (!assert_success(*TestRunner, TEXT("Delayed scene writes"), source)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Spawn clause survives"),
                             source->Contains(TEXT(":spawn-at 2.5")));

        auto updated_definition{*collected};
        updated_definition.entities[0].spawn_time_seconds = 4.0;
        auto const update_plan{make_plan(*world->GetCurrentLevel(), *document, updated_definition)};
        if (!assert_success(*TestRunner, TEXT("Delay-only preview builds"), update_plan)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Delay-only change updates"),
            static_cast<int32>(update_plan->count(ioj::editor::ES7LevelSyncAction::Update)),
            1);

        auto const updated{ioj::editor::apply_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, *update_plan)};
        if (!assert_success(*TestRunner, TEXT("Delay-only update applies"), updated)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Delay-only update preserves actor identity"),
                             bound_actor(*document, TEXT("ship")) == actor);
        TestRunner->TestEqual(TEXT("Updated delay is stored"),
                              bound_binding(*document, TEXT("ship"))->spawn_time_seconds,
                              4.0);
    }

    TEST_METHOD(EditorModeActivatesAndDeactivates)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto const source{ioj::editor::collect_s7_scene_source(*fixture.world->GetCurrentLevel(),
                                                               *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Scene source collects"), source.has_value())) {
            return;
        }
        FTemporarySourceDirectory directory;
        auto const path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        if (!TestRunner->TestTrue(TEXT("Source is saved"), write_source(*source, path))) {
            return;
        }
        fixture.document->source_path = path;
        fixture.document->synchronized_source = *source;
        fixture.document->synchronized_scene_source = *source;

        auto& modes{GLevelEditorModeTools()};
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);
        TestRunner->TestTrue(TEXT("Mode activates"),
                             modes.IsModeActive(US7LevelAuthoringMode::mode_id));

        auto* const mode{Cast<US7LevelAuthoringMode>(
            modes.GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
        if (!TestRunner->TestNotNull(TEXT("Scriptable authoring mode is active"), mode)) {
            modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
            return;
        }
        auto const toolkit{mode->GetToolkit().Pin()};
        if (!TestRunner->TestTrue(TEXT("Authoring toolkit is initialized"), toolkit.IsValid())) {
            modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
            return;
        }
        TestRunner->TestTrue(TEXT("Authoring toolkit has inline controls"),
                             toolkit->GetInlineContent().IsValid());

        mode->preview_apply();
        TestRunner->TestTrue(TEXT("Preview is available"),
                             mode->script_editor_state().preview_valid);
        mode->set_source_buffer(TEXT("(level)"));
        TestRunner->TestEqual(TEXT("Mode owns script-editor buffer edits"),
                              mode->source_session().buffer(),
                              FString{TEXT("(level)")});
        TestRunner->TestTrue(TEXT("Unsaved buffer state is surfaced"),
                             mode->script_editor_state().source_dirty);
        TestRunner->TestFalse(TEXT("Editing invalidates the preview immediately"),
                              mode->script_editor_state().preview_valid);
        TestRunner->TestTrue(TEXT("Editing marks the preview stale"),
                             mode->script_editor_state().preview_stale);
        mode->set_source_buffer(*source);
        mode->reload_s7();
        TestRunner->TestFalse(TEXT("Reload clears the stale preview"),
                              mode->script_editor_state().preview_stale);
        TestRunner->TestFalse(TEXT("Reload does not restore the preview"),
                              mode->script_editor_state().preview_valid);

        auto const tab{FSandboxEditorModule::open_s7_level_script_editor()};
        TestRunner->TestTrue(TEXT("Script editor tab opens"), tab.IsValid());
        if (tab.IsValid()) {
            tab->RequestCloseTab();
        }

        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
        TestRunner->TestFalse(TEXT("Mode deactivates"),
                              modes.IsModeActive(US7LevelAuthoringMode::mode_id));
    }
};

TEST_CLASS(S7PlayableLevel, "Sandbox.UnitTests")
{
    AFTER_EACH()
    {
        if (GEditor && GEditor->PlayWorld) {
            GEditor->EndPlayMap();
        }
    }

    void play_authored_scene(bool const observer) {
        auto fixture{make_preview_fixture(observer)};
        if (!TestRunner->TestTrue(TEXT("PIE fixture"), fixture.is_valid())) {
            return;
        }
        fixture.document->level_config = ioj::s7_level_config::load_canonical();
        fixture.enemy->SetActorLocation(FVector{30000.0, 0.0, 0.0});
        fixture.document->entities[1].spawn_time_seconds = 5.0;
        if (observer) {
            fixture.ally->SetActorLocation(FVector{-30000.0, 0.0, 0.0});
        }
        auto const setup{ioj::editor::set_up_playable_s7_level(*fixture.world->GetCurrentLevel(),
                                                               *fixture.document)};
        if (!assert_success(*TestRunner, TEXT("Playable setup"), setup)) {
            return;
        }

        if (observer) {
            for (TActorIterator<ATestBatchOrchestrator> it{fixture.world}; it; ++it) {
                it->set_start_mode(EOrchestratorStartMode::Automatic);
            }
        }

        TestCommandBuilder.Do([] { FStartPIECommand{false}.Update(); })
            .Until([] { return GEditor->PlayWorld && GEditor->PlayWorld->HasBegunPlay(); },
                   FTimespan::FromSeconds(30))
            .Then([this, observer] {
                auto* const world{GEditor->PlayWorld.Get()};
                if (!TestRunner->TestNotNull(TEXT("PIE world"), world)) {
                    return;
                }
                TActorIterator<ATestBatchOrchestrator> it{world};
                if (!TestRunner->TestTrue(TEXT("PIE orchestrator"), static_cast<bool>(it))) {
                    return;
                }
                auto& orchestrator{**it};
                if (!TestRunner->TestTrue(TEXT("Authored simulation starts"),
                                          orchestrator.get_state() ==
                                              ::ioj::sim::OrchestratorState::Running)) {
                    return;
                }
                orchestrator.SetActorTickEnabled(false);
                auto* const controller{Cast<ASpaceGamePlayerController>(
                    UGameplayStatics::GetPlayerController(world, 0))};
                if (!TestRunner->TestNotNull(TEXT("Space-game controller"), controller)) {
                    return;
                }
                TestRunner->TestTrue(TEXT("Runtime control context matches viewpoint"),
                                     controller->get_active_control_context() ==
                                         (observer ? EPlayerControlContext::Observer
                                                   : EPlayerControlContext::Player));
                TestRunner->TestTrue(TEXT("Runtime uses the authored player or observer camera"),
                                     observer
                                         ? IsValid(Cast<ACameraActor>(controller->GetViewTarget()))
                                         : controller->GetPawn() == orchestrator.get_player_ship());
                auto const initial_capitals{observer ? 1 : 0};
                TestRunner->TestEqual(TEXT("Delayed entity is absent at startup"),
                                      orchestrator.get_capital_ships()->get_num_instances(),
                                      initial_capitals);
                TestRunner->TestTrue(TEXT("Startup precedes scheduled spawn"),
                                     orchestrator.get_simulation_time() < 4.0);
                orchestrator.tick(FMath::Max(0.0, 4.0 - orchestrator.get_simulation_time()));
                TestRunner->TestEqual(TEXT("Delayed entity remains absent before its time"),
                                      orchestrator.get_capital_ships()->get_num_instances(),
                                      initial_capitals);
                orchestrator.tick(1.1);
                TestRunner->TestEqual(TEXT("Existing authored scheduler spawns the entity"),
                                      orchestrator.get_capital_ships()->get_num_instances(),
                                      initial_capitals + 1);
            })
            .Then([this, fixture] {
                GEditor->EndPlayMap();
                TestRunner->TestTrue(TEXT("PIE preserves editor actor identity and bindings"),
                                     IsValid(fixture.enemy) &&
                                         bound_actor(*fixture.document, TEXT("enemy")) ==
                                             fixture.enemy);
            });
    }

    TEST_METHOD(DelayedObserverScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(true); }
    TEST_METHOD(DelayedPlayerScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(false); }
};
