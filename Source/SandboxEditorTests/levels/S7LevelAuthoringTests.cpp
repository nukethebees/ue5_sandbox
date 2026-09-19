#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>

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
#include <Tests/AutomationEditorCommon.h>

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
}

TEST_CLASS(S7LevelAuthoring, "Sandbox.UnitTests")
{
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
        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
        TestRunner->TestFalse(TEXT("Mode deactivates"),
                              modes.IsModeActive(US7LevelAuthoringMode::mode_id));
    }
};
