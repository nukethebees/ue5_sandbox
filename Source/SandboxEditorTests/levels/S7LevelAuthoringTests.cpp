#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringPreview.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelReconciliation.h>
#include <SandboxEditor/levels/S7LevelSourceSession.h>
#include <SandboxEditor/SandboxEditor.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <CQTest.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <Engine/World.h>
#include <Framework/Docking/TabManager.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>
#include <Toolkits/BaseToolkit.h>
#include <Widgets/Docking/SDockTab.h>

#include <expected>

namespace {
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

struct FPreviewFixture {
    UWorld* world{};
    AS7LevelAuthoringDocument* document{};
    USpaceGameLevelConfig* level_config{};
    ATestSpaceShip* player{};
    ATestCapitalShipProxy* enemy{};

    auto is_valid() const -> bool {
        return IsValid(world) && IsValid(document) && IsValid(level_config) && IsValid(player) &&
               IsValid(enemy);
    }
};

auto make_preview_fixture() -> FPreviewFixture {
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
    result.player = spawn<ATestSpaceShip>(
        *result.world, TEXT("player"), result.level_config->classes.player_ship_class.Get());
    result.enemy = spawn<ATestCapitalShipProxy>(
        *result.world, TEXT("enemy"), result.level_config->classes.capital_ship_proxy_class.Get());
    if (!IsValid(result.player) || !IsValid(result.enemy)) {
        return result;
    }

    result.player->set_team(ETestTeam::Blue);
    result.enemy->set_team(ETestTeam::Red);
    result.document->level_config = result.level_config;
    result.document->level_id = TEXT("preview-level");
    result.document->title = TEXT("Preview Level");
    result.document->description = TEXT("Before");
    result.document->entities = {{.id = TEXT("player"), .actor = result.player},
                                 {.id = TEXT("enemy"), .actor = result.enemy}};
    return result;
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

auto make_stale_preview(FPreviewFixture& fixture, ml::editor::FS7LevelSourceSession& source_session)
    -> std::expected<ml::editor::FS7LevelAuthoringPreview, FString> {
    auto const attached{source_session.attach(*fixture.document)};
    if (!attached) {
        return std::unexpected{attached.error()};
    }
    source_session.set_buffer(TEXT("preview source"));

    auto definition{
        ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(), *fixture.document)};
    if (!definition) {
        return std::unexpected{definition.error()};
    }
    definition->metadata.title = TEXT("From Preview");
    auto const plan{ml::editor::make_s7_level_sync_plan(
        *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
    if (!plan) {
        return std::unexpected{plan.error()};
    }
    return ml::editor::make_s7_level_authoring_preview(
        *fixture.world->GetCurrentLevel(), *fixture.document, source_session, *plan);
}

template <typename TTestRunner>
void assert_stale_preview_does_not_apply(TTestRunner& test_runner,
                                         FPreviewFixture const& fixture,
                                         ml::editor::FS7LevelSourceSession const& source_session,
                                         ml::editor::FS7LevelAuthoringPreview const& preview,
                                         FStringView const expected_reason) {
    auto const player_count{count_actors<ATestSpaceShip>(*fixture.world->GetCurrentLevel())};
    auto const capital_count{
        count_actors<ATestCapitalShipProxy>(*fixture.world->GetCurrentLevel())};
    auto const applied{ml::editor::apply_s7_level_authoring_preview(
        *fixture.world->GetCurrentLevel(), *fixture.document, source_session, preview)};
    test_runner.TestFalse(TEXT("Stale preview is rejected"), applied.has_value());
    if (!applied) {
        test_runner.TestTrue(TEXT("Stale reason is useful"),
                             applied.error().Contains(expected_reason));
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
    TEST_METHOD(StalePreviewRejectsChangedSourceAndPath)
    {
        auto fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture"), fixture.is_valid())) {
            return;
        }
        ml::editor::FS7LevelSourceSession source_session;
        auto const preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), preview.has_value())) {
            TestRunner->AddError(preview.error());
            return;
        }
        source_session.set_buffer(TEXT("changed source"));
        source_session.set_buffer(TEXT("preview source"));
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *preview, TEXT("source or source path"));

        fixture = make_preview_fixture();
        if (!TestRunner->TestTrue(TEXT("Path fixture"), fixture.is_valid())) {
            return;
        }
        ml::editor::FS7LevelSourceSession path_session;
        auto const path_preview{make_stale_preview(fixture, path_session)};
        if (!TestRunner->TestTrue(TEXT("Path preview builds"), path_preview.has_value())) {
            TestRunner->AddError(path_preview.error());
            return;
        }
        FTemporarySourceDirectory directory;
        auto const path{FPaths::Combine(directory.path, TEXT("other.scm"))};
        if (!TestRunner->TestTrue(
                TEXT("Source path changes"),
                path_session.save_as(path, ml::editor::ES7SourceOverwritePolicy::ReplaceExisting)
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
        ml::editor::FS7LevelSourceSession source_session;
        auto const transform_preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Transform preview builds"),
                                  transform_preview.has_value())) {
            TestRunner->AddError(transform_preview.error());
            return;
        }
        fixture.player->SetActorLocation(FVector{100.0, 0.0, 0.0});
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *transform_preview, TEXT("scene"));
        TestRunner->TestEqual(TEXT("Stale apply preserves transform"),
                              fixture.player->GetActorLocation(),
                              FVector{100.0, 0.0, 0.0});

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession team_session;
        auto const team_preview{make_stale_preview(fixture, team_session)};
        if (!TestRunner->TestTrue(TEXT("Team preview builds"), team_preview.has_value())) {
            TestRunner->AddError(team_preview.error());
            return;
        }
        fixture.enemy->set_team(ETestTeam::Blue);
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, team_session, *team_preview, TEXT("scene"));
        TestRunner->TestEqual(
            TEXT("Stale apply preserves team"), fixture.enemy->get_team(), ETestTeam::Blue);

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession metadata_session;
        auto const metadata_preview{make_stale_preview(fixture, metadata_session)};
        if (!TestRunner->TestTrue(TEXT("Metadata preview builds"), metadata_preview.has_value())) {
            TestRunner->AddError(metadata_preview.error());
            return;
        }
        fixture.document->description = TEXT("Edited after preview");
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, metadata_session, *metadata_preview, TEXT("document"));
        TestRunner->TestEqual(TEXT("Stale apply preserves metadata"),
                              fixture.document->description,
                              FString{TEXT("Edited after preview")});

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession mission_session;
        auto const mission_preview{make_stale_preview(fixture, mission_session)};
        if (!TestRunner->TestTrue(TEXT("Mission preview builds"), mission_preview.has_value())) {
            TestRunner->AddError(mission_preview.error());
            return;
        }
        fixture.document->mission.mode = ETestMissionMode::SurviveTime;
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, mission_session, *mission_preview, TEXT("scene"));
        TestRunner->TestEqual(TEXT("Stale apply preserves mission"),
                              fixture.document->mission.mode,
                              ETestMissionMode::SurviveTime);

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession camera_session;
        auto const camera_preview{make_stale_preview(fixture, camera_session)};
        if (!TestRunner->TestTrue(TEXT("Camera preview builds"), camera_preview.has_value())) {
            TestRunner->AddError(camera_preview.error());
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
        ml::editor::FS7LevelSourceSession source_session;
        auto const id_preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Binding preview builds"), id_preview.has_value())) {
            TestRunner->AddError(id_preview.error());
            return;
        }
        fixture.document->entities[1].id = TEXT("renamed-enemy");
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *id_preview, TEXT("bindings"));

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession replacement_session;
        auto const replacement_preview{make_stale_preview(fixture, replacement_session)};
        if (!TestRunner->TestTrue(TEXT("Replacement preview builds"),
                                  replacement_preview.has_value())) {
            TestRunner->AddError(replacement_preview.error());
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

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession deletion_session;
        auto const deletion_preview{make_stale_preview(fixture, deletion_session)};
        if (!TestRunner->TestTrue(TEXT("Deletion preview builds"), deletion_preview.has_value())) {
            TestRunner->AddError(deletion_preview.error());
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
        ml::editor::FS7LevelSourceSession source_session;
        auto const pointer_preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Pointer preview builds"), pointer_preview.has_value())) {
            TestRunner->AddError(pointer_preview.error());
            return;
        }
        fixture.document->level_config = config(*fixture.world);
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, source_session, *pointer_preview, TEXT("level configuration"));

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession value_session;
        auto const value_preview{make_stale_preview(fixture, value_session)};
        if (!TestRunner->TestTrue(TEXT("Value preview builds"), value_preview.has_value())) {
            TestRunner->AddError(value_preview.error());
            return;
        }
        fixture.level_config->laser_debug_shapes = !fixture.level_config->laser_debug_shapes;
        assert_stale_preview_does_not_apply(
            *TestRunner, fixture, value_session, *value_preview, TEXT("level configuration"));

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession class_session;
        auto const class_preview{make_stale_preview(fixture, class_session)};
        if (!TestRunner->TestTrue(TEXT("Class preview builds"), class_preview.has_value())) {
            TestRunner->AddError(class_preview.error());
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
        ml::editor::FS7LevelSourceSession source_session;
        auto const level_preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Level preview builds"), level_preview.has_value())) {
            TestRunner->AddError(level_preview.error());
            return;
        }
        auto* const other_world{FAutomationEditorCommonUtils::CreateNewMap()};
        if (!TestRunner->TestNotNull(TEXT("Other world"), other_world)) {
            return;
        }
        auto const applied{ml::editor::apply_s7_level_authoring_preview(
            *other_world->GetCurrentLevel(), *fixture.document, source_session, *level_preview)};
        TestRunner->TestFalse(TEXT("Different level is rejected"), applied.has_value());
        TestRunner->TestEqual(TEXT("Different level preserves original document title"),
                              fixture.document->title,
                              FString{TEXT("Preview Level")});

        fixture = make_preview_fixture();
        ml::editor::FS7LevelSourceSession document_session;
        auto const document_preview{make_stale_preview(fixture, document_session)};
        if (!TestRunner->TestTrue(TEXT("Document preview builds"), document_preview.has_value())) {
            TestRunner->AddError(document_preview.error());
            return;
        }
        auto* const other_document{
            spawn<AS7LevelAuthoringDocument>(*fixture.world, TEXT("Other Document"))};
        if (!TestRunner->TestNotNull(TEXT("Other document"), other_document)) {
            return;
        }
        auto const document_applied{
            ml::editor::apply_s7_level_authoring_preview(*fixture.world->GetCurrentLevel(),
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
        ml::editor::FS7LevelSourceSession source_session;
        auto const preview{make_stale_preview(fixture, source_session)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), preview.has_value())) {
            TestRunner->AddError(preview.error());
            return;
        }
        auto const applied{ml::editor::apply_s7_level_authoring_preview(
            *fixture.world->GetCurrentLevel(), *fixture.document, source_session, *preview)};
        TestRunner->TestTrue(TEXT("Unchanged preview applies"), applied.has_value());
        TestRunner->TestEqual(TEXT("Unchanged preview applies planned title"),
                              fixture.document->title,
                              FString{TEXT("From Preview")});

        fixture = make_preview_fixture();
        FTemporarySourceDirectory directory;
        auto const source_path{FPaths::Combine(directory.path, TEXT("level.scm"))};
        if (!TestRunner->TestTrue(TEXT("Initial disk source writes"),
                                  write_source(TEXT("initial source"), source_path))) {
            return;
        }
        fixture.document->source_path = source_path;
        ml::editor::FS7LevelSourceSession disk_session;
        auto const disk_preview{make_stale_preview(fixture, disk_session)};
        if (!TestRunner->TestTrue(TEXT("Disk preview builds"), disk_preview.has_value())) {
            TestRunner->AddError(disk_preview.error());
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
        auto const disk_applied{ml::editor::apply_s7_level_authoring_preview(
            *fixture.world->GetCurrentLevel(), *fixture.document, disk_session, *disk_preview)};
        TestRunner->TestTrue(TEXT("External disk edit leaves preview valid"),
                             disk_applied.has_value());
        TestRunner->TestFalse(TEXT("External disk edit blocks save"),
                              disk_session.save().has_value());
    }

    TEST_METHOD(ObjectivesRoundTripThroughDocumentAndWriter)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const player{spawn<ATestSpaceShip>(*world, TEXT("Player"))};
        auto* const enemy{spawn<ATestCapitalShipProxy>(*world, TEXT("Enemy"))};
        if (!TestRunner->TestNotNull(TEXT("World"), world) ||
            !TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Player"), player) ||
            !TestRunner->TestNotNull(TEXT("Enemy"), enemy)) {
            return;
        }
        player->set_team(ETestTeam::Blue);
        enemy->set_team(ETestTeam::Red);
        document->level_id = TEXT("objective-round-trip");
        document->title = TEXT("Objective Round Trip");
        document->entities = {{.id = TEXT("player"), .actor = player},
                              {.id = TEXT("enemy"), .actor = enemy}};
        document->mission.mode = ETestMissionMode::KillEnemies;
        document->mission.heroes = {player};
        document->mission.must_survive = {player};
        document->mission.required_kills = {enemy};

        auto const definition{
            ml::editor::collect_s7_editor_level(*world->GetCurrentLevel(), *document)};
        if (!TestRunner->TestTrue(TEXT("Document collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        auto const source{ml::s7::emit_editor_level_source(*definition)};
        if (!TestRunner->TestTrue(TEXT("Definition writes"), source.has_value())) {
            return;
        }
        ml::s7::FLevelDefinitionReader reader;
        auto const read{reader.read_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Generated source reads"), static_cast<bool>(read))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Mission survives"), read.definition->mission.IsSet());
        TestRunner->TestEqual(TEXT("Required kill survives"),
                              read.definition->mission->required_kill_entity_ids.Num(),
                              1);
    }

    TEST_METHOD(PreviewReportsMetadataOnlyChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                            *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Baseline collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        definition->metadata.title = TEXT("After");
        definition->metadata.description = TEXT("After description");

        auto const plan{ml::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestTrue(TEXT("Metadata change is reported"), plan->metadata_changed);
        TestRunner->TestFalse(TEXT("Viewpoint is unchanged"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(TEXT("Entities are unchanged"), plan->changes.Num(), 0);

        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
            return;
        }
        auto const collected{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                                 *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Applied scene collects"), collected.has_value())) {
            TestRunner->AddError(collected.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("Title is applied"), collected->metadata.title, FString{TEXT("After")});
        TestRunner->TestEqual(TEXT("Description is applied"),
                              collected->metadata.description,
                              FString{TEXT("After description")});
    }

    TEST_METHOD(PreviewReportsMissionOnlyChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        fixture.document->mission.mode = ETestMissionMode::KillEnemiesWithinTime;
        fixture.document->mission.time_limit_seconds = 30.0f;
        fixture.document->mission.use_explicit_kill_count = true;
        fixture.document->mission.kill_count = 1;
        fixture.document->mission.heroes = {fixture.player};
        fixture.document->mission.required_kills = {fixture.enemy};
        auto definition{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                            *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Baseline collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        definition->mission->time_limit_seconds = 60.0f;
        definition->mission->kill_count = 2;
        definition->mission->required_kill_entity_ids.Reset();

        auto const plan{ml::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestFalse(TEXT("Viewpoint is unchanged"), plan->viewpoint_changed);
        TestRunner->TestTrue(TEXT("Mission change is reported"), plan->mission_changed);
        TestRunner->TestEqual(TEXT("Entities are unchanged"), plan->changes.Num(), 0);

        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
            return;
        }
        auto const collected{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                                 *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Applied scene collects"), collected.has_value()) ||
            !TestRunner->TestTrue(TEXT("Mission is present"), collected->mission.IsSet())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Mission mode is applied"),
                              collected->mission->mode,
                              ::ioj::sim::levels::LevelMissionMode::KillEnemiesWithinTime);
        TestRunner->TestEqual(TEXT("Mission time is applied"),
                              collected->mission->time_limit_seconds.GetValue(),
                              60.0f);
        TestRunner->TestEqual(
            TEXT("Kill count is applied"), collected->mission->kill_count.GetValue(), 2);
        TestRunner->TestEqual(
            TEXT("Hero is applied"), collected->mission->hero_entity_ids.Num(), 1);
        TestRunner->TestEqual(TEXT("Required kills are applied"),
                              collected->mission->required_kill_entity_ids.Num(),
                              0);
    }

    TEST_METHOD(PreviewReportsObserverCameraOnlyChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        fixture.document->use_observer_camera = true;
        fixture.document->camera.targets = {fixture.player};
        fixture.document->camera.offset_direction = FVector{-1.0, 0.0, 0.0};
        fixture.document->camera.distance = 1000.0;
        auto definition{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                            *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Baseline collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        definition->camera->target_entity_ids = {ml::FLevelEntityId{TEXT("enemy")}};

        auto const plan{ml::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestTrue(TEXT("Viewpoint change is reported"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(TEXT("Entities are unchanged"), plan->changes.Num(), 0);

        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
            return;
        }
        auto const collected{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                                 *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Applied scene collects"), collected.has_value()) ||
            !TestRunner->TestTrue(TEXT("Camera is present"), collected->camera.IsSet())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Camera target is applied"),
                             collected->camera->target_entity_ids ==
                                 TArray<ml::FLevelEntityId>{ml::FLevelEntityId{TEXT("enemy")}});
    }

    TEST_METHOD(PreviewReportsPlayerToCameraViewpointChange)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        auto definition{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                            *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Baseline collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        definition->player_entity_id = {};
        definition->camera = ml::FLevelCameraDefinition{
            .target_entity_ids = {ml::FLevelEntityId{TEXT("enemy")}},
            .offset_direction = FVector{-1.0, 0.0, 0.0},
            .distance = 1000.0,
        };

        auto const plan{ml::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestTrue(TEXT("Viewpoint change is reported"), plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission is unchanged"), plan->mission_changed);
        TestRunner->TestEqual(TEXT("Entities are unchanged"), plan->changes.Num(), 0);

        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
            return;
        }
        auto const collected{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                                 *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Applied scene collects"), collected.has_value())) {
            TestRunner->AddError(collected.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Player viewpoint is cleared"),
                              collected->player_entity_id.is_set());
        TestRunner->TestTrue(TEXT("Camera viewpoint is applied"), collected->camera.IsSet());
    }

    TEST_METHOD(EquivalentDefinitionReportsNoChanges)
    {
        auto const fixture{make_preview_fixture()};
        if (!TestRunner->TestTrue(TEXT("Fixture is valid"), fixture.is_valid())) {
            return;
        }
        fixture.document->use_observer_camera = true;
        fixture.document->camera.targets = {fixture.player, fixture.enemy};
        fixture.document->camera.offset_direction = FVector{-1.0, 0.0, 0.0};
        fixture.document->camera.distance = 1000.0;
        fixture.document->mission.mode = ETestMissionMode::KillEnemies;
        fixture.document->mission.use_explicit_kill_count = true;
        fixture.document->mission.kill_count = 1;
        fixture.document->mission.heroes = {fixture.player, fixture.enemy};
        auto definition{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                            *fixture.document)};
        if (!TestRunner->TestTrue(TEXT("Baseline collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        definition->camera->target_entity_ids.Swap(0, 1);
        definition->mission->hero_entity_ids.Swap(0, 1);

        auto const plan{ml::editor::make_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *definition)};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestFalse(TEXT("Metadata is unchanged"), plan->metadata_changed);
        TestRunner->TestFalse(TEXT("Target order is semantically unchanged"),
                              plan->viewpoint_changed);
        TestRunner->TestFalse(TEXT("Mission role order is semantically unchanged"),
                              plan->mission_changed);
        TestRunner->TestEqual(TEXT("Entities are unchanged"), plan->changes.Num(), 0);
        TestRunner->TestFalse(TEXT("Plan reports no changes"), plan->has_changes());

        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *fixture.world->GetCurrentLevel(), *fixture.document, *plan)};
        if (!TestRunner->TestTrue(TEXT("No-op preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
            return;
        }
        auto const collected{ml::editor::collect_s7_editor_level(*fixture.world->GetCurrentLevel(),
                                                                 *fixture.document)};
        TestRunner->TestTrue(TEXT("Applied scene still collects"), collected.has_value());
    }

    TEST_METHOD(ApplyAddsConfiguredActor)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("add")}, .title = TEXT("Add")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue,
                            .position = FVector{100.0, 200.0, 300.0},
                            .rotation = FRotator{10.0, 20.0, 30.0}});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor adds"), plan->count(ml::editor::ES7LevelSyncAction::Add), 1);
        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("update")}, .title = TEXT("Update")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue,
                            .position = FVector{100.0, 200.0, 300.0},
                            .rotation = FRotator{10.0, 20.0, 30.0}});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor updates"), plan->count(ml::editor::ES7LevelSyncAction::Update), 1);

        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("replace")}, .title = TEXT("Replace")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor replaces"), plan->count(ml::editor::ES7LevelSyncAction::Replace), 1);

        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("remove")}, .title = TEXT("Remove")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("kept")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("kept")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor removes"), plan->count(ml::editor::ES7LevelSyncAction::Remove), 1);

        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("invalid-later-class")},
                              .title = TEXT("Invalid Later Class")});
        builder.add_team(ml::level_teams::blue);
        builder.add_team(ml::level_teams::red);
        builder.set_player_entity(ml::FLevelEntityId{TEXT("player")});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("player")},
                            .archetype = ml::level_archetypes::player_fighter,
                            .team = ml::level_teams::blue});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("capital")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::red});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds with valid classes"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }

        level_config->classes.capital_ship_proxy_class = nullptr;
        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
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

    TEST_METHOD(RejectsDeclaredTeamUnusedByEntities)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;
        document->level_id = TEXT("original");

        ml::FLevelBuilder builder;
        builder.set_metadata(
            {.id = ml::FLevelId{TEXT("unused-team")}, .title = TEXT("Unused Team")});
        builder.add_team(ml::level_teams::blue);
        builder.add_team(ml::level_teams::red);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue});
        auto const definition{builder.finish()};

        auto const preview{
            ml::editor::make_s7_level_sync_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Preview rejects unused team"), preview.has_value());
        if (!preview) {
            TestRunner->TestTrue(TEXT("Error explains focused-mode loss"),
                                 preview.error().Contains(TEXT("cannot preserve")) &&
                                     preview.error().Contains(TEXT("red")));
        }

        ml::editor::FS7LevelSyncPlan const unchecked_plan{.definition = definition};
        auto const applied{ml::editor::apply_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, unchecked_plan)};
        TestRunner->TestFalse(TEXT("Apply revalidates unused team"), applied.has_value());
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("bindings")}, .title = TEXT("Bindings")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue});
        auto const definition{builder.finish()};

        document->entities = {{.id = NAME_None, .actor = actor}};
        auto invalid{
            ml::editor::make_s7_level_sync_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Invalid binding is rejected"), invalid.has_value());

        document->entities = {{.id = TEXT("ship"), .actor = actor},
                              {.id = TEXT("ship"), .actor = actor}};
        auto duplicate_id{
            ml::editor::make_s7_level_sync_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Duplicate id is rejected"), duplicate_id.has_value());

        document->entities = {{.id = TEXT("ship"), .actor = actor},
                              {.id = TEXT("other"), .actor = actor}};
        auto duplicate_actor{
            ml::editor::make_s7_level_sync_plan(*world->GetCurrentLevel(), *document, definition)};
        TestRunner->TestFalse(TEXT("Duplicate actor is rejected"), duplicate_actor.has_value());
    }

    TEST_METHOD(OneUndoRestoresMixedApply)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        level_config->classes.player_ship_class = ATestSpaceShip::StaticClass();
        level_config->classes.capital_ship_proxy_class = ATestCapitalShipProxy::StaticClass();
        level_config->classes.static_turret_proxy_class = ATestStaticTurretsProxy::StaticClass();

        auto* const updated{spawn<ATestCapitalShipProxy>(*world, TEXT("old-update-label"))};
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
        document->use_observer_camera = false;
        document->camera.targets = {removed};
        document->camera.offset_direction = FVector{0.0, 1.0, 0.0};
        document->camera.distance = 2500.0;
        document->mission.mode = ETestMissionMode::SurviveTime;
        document->mission.time_limit_seconds = 12.0f;
        document->mission.must_survive = {updated};

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("after")},
                              .title = TEXT("After"),
                              .description = TEXT("After description")});
        builder.add_team(ml::level_teams::blue);
        builder.add_team(ml::level_teams::red);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("update")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.set_mission({.mode = ::ioj::sim::levels::LevelMissionMode::KillEnemies,
                             .kill_count = 1,
                             .hero_entity_ids = {ml::FLevelEntityId{TEXT("update")}},
                             .required_kill_entity_ids = {ml::FLevelEntityId{TEXT("replace")}}});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("update")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue,
                            .position = FVector{100.0, 200.0, 300.0},
                            .rotation = FRotator{10.0, 20.0, 30.0},
                            .spawn_time_seconds = 3.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("replace")},
                            .archetype = ml::level_archetypes::static_turret,
                            .team = ml::level_teams::red,
                            .position = FVector{400.0, 500.0, 600.0}});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("add")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::red,
                            .position = FVector{700.0, 800.0, 900.0}});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Mixed preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor adds"), plan->count(ml::editor::ES7LevelSyncAction::Add), 1);
        TestRunner->TestEqual(
            TEXT("One actor updates"), plan->count(ml::editor::ES7LevelSyncAction::Update), 1);
        TestRunner->TestEqual(
            TEXT("One actor replaces"), plan->count(ml::editor::ES7LevelSyncAction::Replace), 1);
        TestRunner->TestEqual(
            TEXT("One actor removes"), plan->count(ml::editor::ES7LevelSyncAction::Remove), 1);

        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Mixed preview applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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
        TestRunner->TestEqual(
            TEXT("Mission is published"), document->mission.mode, ETestMissionMode::KillEnemies);
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
        TestRunner->TestEqual(TEXT("Updated label is restored"),
                              updated->GetActorLabel(),
                              FString{TEXT("old-update-label")});
        TestRunner->TestEqual(TEXT("Spawn time is restored"),
                              bound_binding(*document, TEXT("update"))->spawn_time_seconds,
                              7.0);
        TestRunner->TestEqual(
            TEXT("Metadata is restored"), document->level_id, FName{TEXT("before")});
        TestRunner->TestEqual(TEXT("Title is restored"), document->title, FString{TEXT("Before")});
        TestRunner->TestEqual(TEXT("Description is restored"),
                              document->description,
                              FString{TEXT("Before description")});
        TestRunner->TestFalse(TEXT("Viewpoint is restored"), document->use_observer_camera);
        if (TestRunner->TestEqual(
                TEXT("Camera target count is restored"), document->camera.targets.Num(), 1)) {
            TestRunner->TestEqual(TEXT("Camera target is restored"),
                                  document->camera.targets[0].Get(),
                                  static_cast<AActor*>(removed));
        }
        TestRunner->TestEqual(TEXT("Mission mode is restored"),
                              document->mission.mode,
                              ETestMissionMode::SurviveTime);
        TestRunner->TestEqual(
            TEXT("Mission time is restored"), document->mission.time_limit_seconds, 12.0f);
        if (TestRunner->TestEqual(TEXT("Mission survivor count is restored"),
                                  document->mission.must_survive.Num(),
                                  1)) {
            TestRunner->TestEqual(TEXT("Mission survivor is restored"),
                                  document->mission.must_survive[0].Get(),
                                  static_cast<AActor*>(updated));
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

        auto const document{ml::editor::find_level_authoring_document(*world->GetCurrentLevel())};
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
        auto const definition{
            ml::editor::collect_s7_editor_level(*world->GetCurrentLevel(), **document)};
        TestRunner->TestTrue(TEXT("Adopted proxies can be exported"), definition.has_value());

        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
    }

    TEST_METHOD(RepairBindingsCleansReferencesAdoptsDuplicatesAndUndoes)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
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

        document->entities = {{.id = TEXT("enemy-group"), .actor = retained},
                              {.id = TEXT("deleted"), .actor = deleted},
                              {.id = TEXT("enemy"), .actor = retained}};
        document->camera.targets = {retained, deleted, duplicate_one};
        document->mission.heroes = {retained, deleted};
        document->mission.must_survive = {deleted};
        document->mission.required_kills = {duplicate_two};

        auto const repaired{
            ml::editor::repair_s7_level_bindings(*world->GetCurrentLevel(), *document)};
        if (!TestRunner->TestTrue(TEXT("Bindings repair succeeds"), repaired.has_value())) {
            TestRunner->AddError(repaired.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("Dead and duplicate bindings are removed"), repaired->removed_bindings, 2);
        TestRunner->TestEqual(TEXT("Invalid and formerly unbound references are removed"),
                              repaired->removed_references,
                              5);
        TestRunner->TestEqual(
            TEXT("Both duplicated actors are adopted"), repaired->adopted_entities, 2);
        TestRunner->TestTrue(TEXT("Stable binding ID is preserved"),
                             bound_actor(*document, TEXT("enemy-group")) == retained);
        TestRunner->TestTrue(TEXT("First canonical collision receives suffix"),
                             bound_actor(*document, TEXT("enemy-group-2")) == duplicate_one);
        TestRunner->TestTrue(TEXT("Second canonical collision receives suffix"),
                             bound_actor(*document, TEXT("enemy-group-3")) == duplicate_two);
        TestRunner->TestEqual(
            TEXT("Only retained camera reference survives"), document->camera.targets.Num(), 1);
        TestRunner->TestTrue(TEXT("Camera reference remains bound"),
                             document->camera.targets[0] == retained);
        TestRunner->TestEqual(
            TEXT("Only retained hero reference survives"), document->mission.heroes.Num(), 1);
        TestRunner->TestTrue(TEXT("No stale must-survive references remain"),
                             document->mission.must_survive.IsEmpty());
        TestRunner->TestTrue(TEXT("No stale required-kill references remain"),
                             document->mission.required_kills.IsEmpty());

        GEditor->UndoTransaction();

        TestRunner->TestEqual(TEXT("Repair undo restores bindings"), document->entities.Num(), 3);
        TestRunner->TestEqual(
            TEXT("Repair undo restores camera references"), document->camera.targets.Num(), 3);
        TestRunner->TestEqual(
            TEXT("Repair undo restores mission references"), document->mission.heroes.Num(), 2);
    }

    TEST_METHOD(SelectedEntityIdRenameValidatesAndUndoes)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const level_config{config(*world)};
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

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("delayed")}, .title = TEXT("Delayed")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue,
                            .spawn_time_seconds = 2.5});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Delayed spawn preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("Delayed actor adds"), plan->count(ml::editor::ES7LevelSyncAction::Add), 1);

        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        if (!TestRunner->TestTrue(TEXT("Delayed spawn applies"), applied.has_value())) {
            TestRunner->AddError(applied.error());
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

        auto const collected{
            ml::editor::collect_s7_editor_level(*world->GetCurrentLevel(), *document)};
        if (!TestRunner->TestTrue(TEXT("Delayed scene collects"), collected.has_value())) {
            TestRunner->AddError(collected.error());
            return;
        }
        auto const source{ml::s7::emit_editor_level_source(*collected)};
        if (!TestRunner->TestTrue(TEXT("Delayed scene writes"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }
        TestRunner->TestTrue(TEXT("Spawn clause survives"),
                             source->Contains(TEXT("(spawn-at 2.5)")));

        auto updated_definition{*collected};
        updated_definition.entities.spawn_times_seconds[0] = 4.0;
        auto const update_plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, updated_definition)};
        if (!TestRunner->TestTrue(TEXT("Delay-only preview builds"), update_plan.has_value())) {
            TestRunner->AddError(update_plan.error());
            return;
        }
        TestRunner->TestEqual(TEXT("Delay-only change updates"),
                              update_plan->count(ml::editor::ES7LevelSyncAction::Update),
                              1);

        auto const updated{ml::editor::apply_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, *update_plan)};
        if (!TestRunner->TestTrue(TEXT("Delay-only update applies"), updated.has_value())) {
            TestRunner->AddError(updated.error());
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

        mode->set_source_buffer(TEXT("(level)"));
        TestRunner->TestEqual(TEXT("Mode owns script-editor buffer edits"),
                              mode->source_session().buffer(),
                              FString{TEXT("(level)")});
        TestRunner->TestTrue(TEXT("Unsaved buffer state is surfaced"),
                             mode->script_editor_state().source_dirty);

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
