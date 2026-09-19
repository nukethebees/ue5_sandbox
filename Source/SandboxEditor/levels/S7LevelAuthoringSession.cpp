#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <LevelUtils.h>
#include <Misc/PackageName.h>
#include <ScopedTransaction.h>

namespace ml::editor {
namespace s7_level_authoring_detail {
struct FResolvedActor {
    EResolvedLevelArchetype archetype{};
    FLevelTeamId team{};
};

struct FPreparedSyncEntity {
    FLevelEntityId id{};
    EResolvedLevelArchetype archetype{};
    ETestTeam team{};
    UClass* actor_class{};
    FTransform transform{};
    AActor* existing_actor{};
    bool requires_spawn{};
};

struct FPreparedSyncPlan {
    TArray<FPreparedSyncEntity> entities{};
    TArray<AActor*> actors_to_destroy{};
    TArray<FS7LevelSyncChange> changes{};
    bool metadata_changed{};
    bool viewpoint_changed{};
    bool mission_changed{};
};

auto resolve_actor(AActor const& actor) -> TOptional<FResolvedActor> {
    EResolvedLevelArchetype archetype{};
    ETestTeam team{};
    if (auto const* const player{Cast<ATestSpaceShip>(&actor)}) {
        archetype = EResolvedLevelArchetype::PlayerFighter;
        team = player->get_team();
    } else if (auto const* const capital{Cast<ATestCapitalShipProxy>(&actor)}) {
        archetype = EResolvedLevelArchetype::CapitalShip;
        team = capital->get_team();
    } else if (auto const* const turret{Cast<ATestStaticTurretsProxy>(&actor)}) {
        archetype = EResolvedLevelArchetype::StaticTurret;
        team = turret->get_team();
    } else {
        return NullOpt;
    }

    auto const level_team{to_level_team_id(team)};
    return level_team.IsSet()
             ? TOptional<FResolvedActor>{FResolvedActor{archetype, level_team.GetValue()}}
             : NullOpt;
}

auto actor_class(EResolvedLevelArchetype const archetype, USpaceGameLevelConfig const& config)
    -> UClass* {
    switch (archetype) {
        case EResolvedLevelArchetype::PlayerFighter:
            return config.classes.player_ship_class.Get();
        case EResolvedLevelArchetype::CapitalShip:
            return config.classes.capital_ship_proxy_class.Get();
        case EResolvedLevelArchetype::StaticTurret:
            return config.classes.static_turret_proxy_class.Get();
    }
    return nullptr;
}

void configure_actor(AActor& actor,
                     EResolvedLevelArchetype const archetype,
                     ETestTeam const team,
                     USpaceGameLevelConfig& config,
                     FTransform const& transform,
                     FName const label) {
    actor.Modify();
    actor.SetActorTransform(transform);
    if (archetype == EResolvedLevelArchetype::PlayerFighter) {
        auto& player{*CastChecked<ATestSpaceShip>(&actor)};
        player.set_actor_config(&config.player_ship);
        player.set_team(team);
    } else if (archetype == EResolvedLevelArchetype::CapitalShip) {
        auto& capital{*CastChecked<ATestCapitalShipProxy>(&actor)};
        capital.set_level_config_asset(&config);
        capital.set_team(team);
    } else {
        auto& turret{*CastChecked<ATestStaticTurretsProxy>(&actor)};
        turret.set_actor_config(&config.turrets);
        turret.set_team(team);
    }
    actor.RerunConstructionScripts();
    actor.PostEditMove(true);
    actor.SetActorLabel(label.ToString(), true);
}

auto canonical_id(FStringView const text, FStringView const fallback) -> FName {
    FString result;
    for (auto const character : text) {
        auto const alpha{FChar::IsAlpha(character)};
        auto const digit{FChar::IsDigit(character)};
        if (!alpha && !digit) {
            if (!result.IsEmpty() && !result.EndsWith(TEXT("-"))) {
                result.AppendChar(TEXT('-'));
            }
            continue;
        }
        result.AppendChar(FChar::ToLower(character));
    }
    while (result.EndsWith(TEXT("-"))) {
        result.LeftChopInline(1, EAllowShrinking::No);
    }
    if (result.IsEmpty()) {
        result = fallback;
    }
    if (!FChar::IsAlpha(result[0])) {
        result = FString{fallback} + TEXT("-") + result;
    }
    return FName{result};
}

auto strict_subset_error(FLevelDefinition const& definition) -> FString {
    TArray<FString> unsupported;
    if (definition.metadata.par_time_seconds.IsSet()) {
        unsupported.Add(TEXT("par time"));
    }
    if (!definition.unlock_criteria.IsEmpty()) {
        unsupported.Add(TEXT("unlock criteria"));
    }
    if (!definition.mission_events.IsEmpty()) {
        unsupported.Add(TEXT("mission events"));
    }
    auto const entities{definition.entities.get_const_view()};
    TSet<FLevelTeamId> used_teams;
    auto const entity_count{entities.num()};
    for (int32 index{}; index < entity_count; ++index) {
        used_teams.Add(entities.teams[index]);
    }

    TArray<FString> unused_teams;
    for (auto const team : definition.teams) {
        if (!used_teams.Contains(team)) {
            unused_teams.Add(team.value.ToString());
        }
    }

    TArray<FString> errors;
    if (!unsupported.IsEmpty()) {
        errors.Add(FString::Printf(TEXT("The focused authoring mode does not support: %s."),
                                   *FString::Join(unsupported, TEXT(", "))));
    }
    if (!unused_teams.IsEmpty()) {
        errors.Add(FString::Printf(
            TEXT("The focused authoring mode cannot preserve declared teams unused by entities: "
                 "%s."),
            *FString::Join(unused_teams, TEXT(", "))));
    }
    return FString::Join(errors, TEXT("\n"));
}

auto mission_mode(ETestMissionMode const mode) -> ::ioj::sim::levels::LevelMissionMode {
    using Result = ::ioj::sim::levels::LevelMissionMode;
    switch (mode) {
        case ETestMissionMode::SurviveTime:
            return Result::SurviveTime;
        case ETestMissionMode::KillEnemies:
            return Result::KillEnemies;
        case ETestMissionMode::KillEnemiesWithinTime:
            return Result::KillEnemiesWithinTime;
        case ETestMissionMode::None:
            return Result::Unspecified;
    }
    return Result::Unspecified;
}

auto authoring_mode(::ioj::sim::levels::LevelMissionMode const mode) -> ETestMissionMode {
    using Source = ::ioj::sim::levels::LevelMissionMode;
    switch (mode) {
        case Source::SurviveTime:
            return ETestMissionMode::SurviveTime;
        case Source::KillEnemies:
            return ETestMissionMode::KillEnemies;
        case Source::KillEnemiesWithinTime:
            return ETestMissionMode::KillEnemiesWithinTime;
        case Source::Unspecified:
            return ETestMissionMode::None;
    }
    return ETestMissionMode::None;
}

auto sorted_ids(TArray<FLevelEntityId> ids) -> TArray<FLevelEntityId> {
    ids.Sort([](FLevelEntityId const lhs, FLevelEntityId const rhs) {
        return lhs.value.LexicalLess(rhs.value);
    });
    return ids;
}

auto actor_ids(TArray<TObjectPtr<AActor>> const& actors,
               TMap<AActor const*, FLevelEntityId> const& ids_by_actor)
    -> TOptional<TArray<FLevelEntityId>> {
    TArray<FLevelEntityId> result;
    result.Reserve(actors.Num());
    for (auto const actor : actors) {
        auto const* const id{ids_by_actor.Find(actor.Get())};
        if (!id) {
            return NullOpt;
        }
        result.Add(*id);
    }
    return sorted_ids(MoveTemp(result));
}

auto metadata_changed(AS7LevelAuthoringDocument const& document, FLevelMetadata const& metadata)
    -> bool {
    return document.level_id != metadata.id.value || document.title != metadata.title ||
           document.description != metadata.description;
}

auto viewpoint_changed(AS7LevelAuthoringDocument const& document,
                       FLevelDefinition const& definition,
                       TMap<AActor const*, FLevelEntityId> const& ids_by_actor) -> bool {
    if (document.use_observer_camera != definition.camera.IsSet()) {
        return true;
    }
    if (document.use_observer_camera) {
        auto const current_targets{actor_ids(document.camera.targets, ids_by_actor)};
        if (!current_targets.IsSet()) {
            return true;
        }
        auto const& camera{definition.camera.GetValue()};
        return current_targets.GetValue() != sorted_ids(camera.target_entity_ids) ||
               document.camera.offset_direction != camera.offset_direction ||
               document.camera.distance != camera.distance;
    }

    TOptional<FLevelEntityId> current_player;
    for (auto const& binding : document.entities) {
        if (!IsValid(Cast<ATestSpaceShip>(binding.actor))) {
            continue;
        }
        if (current_player.IsSet()) {
            return true;
        }
        current_player = FLevelEntityId{binding.id};
    }
    return !current_player.IsSet() || current_player.GetValue() != definition.player_entity_id;
}

auto optionals_equal(TOptional<float> const lhs, TOptional<float> const rhs) -> bool {
    return lhs.IsSet() == rhs.IsSet() && (!lhs.IsSet() || lhs.GetValue() == rhs.GetValue());
}

auto optionals_equal(TOptional<int32> const lhs, TOptional<int32> const rhs) -> bool {
    return lhs.IsSet() == rhs.IsSet() && (!lhs.IsSet() || lhs.GetValue() == rhs.GetValue());
}

auto mission_changed(AS7LevelAuthoringDocument const& document,
                     FLevelDefinition const& definition,
                     TMap<AActor const*, FLevelEntityId> const& ids_by_actor) -> bool {
    auto const has_current{document.mission.mode != ETestMissionMode::None};
    if (has_current != definition.mission.IsSet()) {
        return true;
    }
    if (!has_current) {
        return false;
    }

    auto const& incoming{definition.mission.GetValue()};
    if (mission_mode(document.mission.mode) != incoming.mode) {
        return true;
    }

    TOptional<float> current_time;
    if (document.mission.mode == ETestMissionMode::SurviveTime ||
        document.mission.mode == ETestMissionMode::KillEnemiesWithinTime) {
        current_time = document.mission.time_limit_seconds;
    }
    TOptional<int32> current_kill_count;
    if (document.mission.use_explicit_kill_count &&
        document.mission.mode != ETestMissionMode::SurviveTime) {
        current_kill_count = document.mission.kill_count;
    }
    if (!optionals_equal(current_time, incoming.time_limit_seconds) ||
        !optionals_equal(current_kill_count, incoming.kill_count)) {
        return true;
    }

    auto const heroes{actor_ids(document.mission.heroes, ids_by_actor)};
    auto const survivors{actor_ids(document.mission.must_survive, ids_by_actor)};
    auto const required_kills{actor_ids(document.mission.required_kills, ids_by_actor)};
    return !heroes.IsSet() || !survivors.IsSet() || !required_kills.IsSet() ||
           heroes.GetValue() != sorted_ids(incoming.hero_entity_ids) ||
           survivors.GetValue() != sorted_ids(incoming.must_survive_entity_ids) ||
           required_kills.GetValue() != sorted_ids(incoming.required_kill_entity_ids);
}

auto validation_error(FLevelDefinition const& definition) -> FString {
    auto const validation{validate_level(definition)};
    TArray<FString> messages;
    for (auto const& error : validation.errors) {
        messages.Add(error.message);
    }
    return FString::Join(messages, TEXT("\n"));
}

auto prepare_sync_plan(ULevel const& level,
                       AS7LevelAuthoringDocument const& document,
                       FLevelDefinition const& definition)
    -> std::expected<FPreparedSyncPlan, FString> {
    if (!GEditor) {
        return std::unexpected{TEXT("GEditor is unavailable.")};
    }

    auto* const editor_world{GEditor->GetEditorWorldContext().World()};
    if (!IsValid(editor_world) || editor_world->IsGameWorld()) {
        return std::unexpected{TEXT("The editor world is unavailable.")};
    }
    if (level.OwningWorld != editor_world || editor_world->GetCurrentLevel() != &level) {
        return std::unexpected{TEXT("The target is not the current editor level.")};
    }
    if (FLevelUtils::IsLevelLocked(const_cast<ULevel*>(&level))) {
        return std::unexpected{TEXT("The current editor level is locked.")};
    }
    if (!IsValid(&document) || document.GetLevel() != &level) {
        return std::unexpected{
            TEXT("The authoring document does not belong to the current level.")};
    }
    if (!IsValid(document.level_config)) {
        return std::unexpected{TEXT("The authoring document has no level configuration.")};
    }
    if (auto const error{validation_error(definition)}; !error.IsEmpty()) {
        return std::unexpected{error};
    }
    if (auto const error{strict_subset_error(definition)}; !error.IsEmpty()) {
        return std::unexpected{error};
    }

    TMap<FName, FS7LevelEntityBinding const*> bindings_by_id;
    TMap<AActor const*, FLevelEntityId> ids_by_actor;
    TSet<AActor*> bound_actors;
    for (auto const& binding : document.entities) {
        if (binding.id.IsNone() || !IsValid(binding.actor)) {
            return std::unexpected{TEXT("Every entity binding must have a valid actor and id.")};
        }
        if (binding.actor->GetLevel() != &level) {
            return std::unexpected{FString::Printf(TEXT("Entity '%s' belongs to another level."),
                                                   *binding.id.ToString())};
        }
        if (bindings_by_id.Contains(binding.id)) {
            return std::unexpected{
                FString::Printf(TEXT("The authoring document contains duplicate entity id '%s'."),
                                *binding.id.ToString())};
        }
        if (bound_actors.Contains(binding.actor.Get())) {
            return std::unexpected{
                FString::Printf(TEXT("Actor '%s' is bound to more than one entity id."),
                                *binding.actor->GetActorLabel())};
        }

        bindings_by_id.Add(binding.id, &binding);
        ids_by_actor.Add(binding.actor.Get(), FLevelEntityId{binding.id});
        bound_actors.Add(binding.actor.Get());
    }

    FPreparedSyncPlan prepared;
    prepared.metadata_changed = metadata_changed(document, definition.metadata);
    prepared.viewpoint_changed = viewpoint_changed(document, definition, ids_by_actor);
    prepared.mission_changed = mission_changed(document, definition, ids_by_actor);
    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    prepared.entities.Reserve(entity_count);

    TSet<FName> incoming_ids;
    for (int32 index{}; index < entity_count; ++index) {
        auto const id{entities.ids[index]};
        auto const archetype{resolve_level_archetype(entities.archetypes[index])};
        auto const team{resolve_level_team(entities.teams[index])};
        auto* const type{archetype.IsSet() ? actor_class(*archetype, *document.level_config)
                                           : nullptr};
        if (!archetype.IsSet() || !team.IsSet() || !IsValid(type) ||
            type->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient)) {
            return std::unexpected{
                FString::Printf(TEXT("Entity '%s' cannot be materialised."), *id.value.ToString())};
        }
        if (incoming_ids.Contains(id.value)) {
            return std::unexpected{
                FString::Printf(TEXT("The level definition contains duplicate entity id '%s'."),
                                *id.value.ToString())};
        }
        incoming_ids.Add(id.value);

        auto const* const existing_binding{bindings_by_id.FindRef(id.value)};
        auto* const existing_actor{existing_binding ? existing_binding->actor.Get() : nullptr};
        auto const transform{FTransform{FRotator{entities.rotations.pitches[index],
                                                 entities.rotations.yaws[index],
                                                 entities.rotations.rolls[index]},
                                        FVector{entities.positions.xs[index],
                                                entities.positions.ys[index],
                                                entities.positions.zs[index]}}};
        auto action{TOptional<ES7LevelSyncAction>{}};
        auto requires_spawn{false};
        if (!IsValid(existing_actor)) {
            action = ES7LevelSyncAction::Add;
            requires_spawn = true;
        } else if (existing_actor->GetClass() != type) {
            action = ES7LevelSyncAction::Replace;
            requires_spawn = true;
            prepared.actors_to_destroy.Add(existing_actor);
        } else {
            auto const resolved{resolve_actor(*existing_actor)};
            if (!resolved.IsSet() || resolved->team != entities.teams[index] ||
                !existing_actor->GetActorTransform().Equals(transform, 0.001) ||
                existing_actor->GetActorLabel() != id.value.ToString() ||
                !FMath::IsNearlyEqual(existing_binding->spawn_time_seconds,
                                      entities.spawn_times_seconds[index],
                                      0.001)) {
                action = ES7LevelSyncAction::Update;
            }
        }
        if (action.IsSet()) {
            prepared.changes.Add({id, action.GetValue()});
        }
        prepared.entities.Add({.id = id,
                               .archetype = archetype.GetValue(),
                               .team = team.GetValue(),
                               .actor_class = type,
                               .transform = transform,
                               .existing_actor = existing_actor,
                               .requires_spawn = requires_spawn});
    }

    for (auto const& binding : document.entities) {
        if (!incoming_ids.Contains(binding.id)) {
            prepared.changes.Add({FLevelEntityId{binding.id}, ES7LevelSyncAction::Remove});
            prepared.actors_to_destroy.Add(binding.actor.Get());
        }
    }
    return prepared;
}
}

using namespace s7_level_authoring_detail;

auto FS7LevelSyncPlan::count(ES7LevelSyncAction const action) const -> int32 {
    int32 result{};
    for (auto const& change : changes) {
        if (change.action == action) {
            ++result;
        }
    }
    return result;
}

auto FS7LevelSyncPlan::has_changes() const -> bool {
    return !changes.IsEmpty() || metadata_changed || viewpoint_changed || mission_changed;
}

auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    AS7LevelAuthoringDocument* result{};
    for (auto const actor_ptr : level.Actors) {
        auto* const document{Cast<AS7LevelAuthoringDocument>(actor_ptr.Get())};
        if (!IsValid(document)) {
            continue;
        }
        if (result) {
            return std::unexpected{
                TEXT("The current level contains multiple S7 authoring documents.")};
        }
        result = document;
    }
    return result;
}

auto create_level_authoring_document(ULevel& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    auto const existing{find_level_authoring_document(level)};
    if (!existing) {
        return std::unexpected{existing.error()};
    }
    if (*existing) {
        return *existing;
    }
    if (!GEditor) {
        return std::unexpected{TEXT("GEditor is unavailable.")};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "CreateDocument", "Create S7 Level Document")};
    auto* const document{
        Cast<AS7LevelAuthoringDocument>(GEditor->AddActor(&level,
                                                          AS7LevelAuthoringDocument::StaticClass(),
                                                          FTransform::Identity,
                                                          true,
                                                          RF_Transactional,
                                                          false))};
    if (!IsValid(document)) {
        transaction.Cancel();
        return std::unexpected{TEXT("Could not create the S7 authoring document.")};
    }
    auto const map_name{FPackageName::GetShortName(level.GetOutermost()->GetName())};
    document->level_id = canonical_id(map_name, TEXTVIEW("authored-level"));
    document->title = map_name;
    document->level_config = LoadObject<USpaceGameLevelConfig>(
        nullptr, TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"));
    return document;
}

auto adopt_unbound_level_entities(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<int32, FString> {
    TSet<AActor const*> bound;
    TSet<FName> ids;
    for (auto const& binding : document.entities) {
        if (IsValid(binding.actor)) {
            bound.Add(binding.actor);
        }
        if (!binding.id.IsNone()) {
            ids.Add(binding.id);
        }
    }

    FScopedTransaction transaction{NSLOCTEXT("S7LevelAuthoring", "Adopt", "Adopt Level Entities")};
    document.Modify();
    int32 adopted{};
    for (auto const actor_ptr : level.Actors) {
        auto* const actor{actor_ptr.Get()};
        auto const resolved{IsValid(actor) ? resolve_actor(*actor) : NullOpt};
        if (!resolved.IsSet() || bound.Contains(actor)) {
            continue;
        }
        auto const fallback{to_level_archetype_id(resolved->archetype).value.ToString()};
        auto id{canonical_id(actor->GetActorLabel(), fallback)};
        auto const base{id.ToString()};
        int32 suffix{2};
        while (ids.Contains(id)) {
            id = FName{FString::Printf(TEXT("%s-%d"), *base, suffix++)};
        }
        document.entities.Add({.id = id, .actor = actor});
        ids.Add(id);
        ++adopted;
    }
    if (adopted == 0) {
        transaction.Cancel();
    }
    return adopted;
}

auto collect_s7_editor_level(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<FLevelDefinition, FString> {
    TMap<AActor const*, FLevelEntityId> ids_by_actor;
    TSet<FName> ids;
    FLevelBuilder builder;
    builder.set_metadata({
        .id = FLevelId{document.level_id},
        .title = document.title,
        .description = document.description,
    });

    TSet<FLevelTeamId> teams;
    TArray<FEntitySpawnDefinition> entities;
    for (auto const& binding : document.entities) {
        if (!IsValid(binding.actor) || binding.id.IsNone()) {
            return std::unexpected{TEXT("Every entity binding must have a valid actor and id.")};
        }
        if (binding.actor->GetLevel() != &level) {
            return std::unexpected{FString::Printf(TEXT("Entity '%s' belongs to another level."),
                                                   *binding.id.ToString())};
        }
        if (ids.Contains(binding.id) || ids_by_actor.Contains(binding.actor)) {
            return std::unexpected{TEXT("Entity bindings contain a duplicate id or actor.")};
        }
        auto const resolved{resolve_actor(*binding.actor)};
        if (!resolved.IsSet()) {
            return std::unexpected{FString::Printf(
                TEXT("Entity '%s' has an unsupported class or team."), *binding.id.ToString())};
        }
        ids.Add(binding.id);
        ids_by_actor.Add(binding.actor, FLevelEntityId{binding.id});
        teams.Add(resolved->team);
        entities.Add({
            .id = FLevelEntityId{binding.id},
            .archetype = to_level_archetype_id(resolved->archetype),
            .team = resolved->team,
            .position = binding.actor->GetActorLocation(),
            .rotation = binding.actor->GetActorRotation(),
            .spawn_time_seconds = binding.spawn_time_seconds,
        });
    }
    for (auto const actor_ptr : level.Actors) {
        auto const* const actor{actor_ptr.Get()};
        if (IsValid(actor) && resolve_actor(*actor).IsSet() && !ids_by_actor.Contains(actor)) {
            return std::unexpected{FString::Printf(
                TEXT("Supported actor '%s' is not adopted by the authoring document."),
                *actor->GetActorLabel())};
        }
    }
    auto sorted_teams{teams.Array()};
    sorted_teams.Sort([](FLevelTeamId const lhs, FLevelTeamId const rhs) {
        return lhs.value.LexicalLess(rhs.value);
    });
    for (auto const team : sorted_teams) {
        builder.add_team(team);
    }
    for (auto const& entity : entities) {
        builder.add_entity(entity);
    }

    if (document.use_observer_camera) {
        FLevelCameraDefinition camera{
            .offset_direction = document.camera.offset_direction,
            .distance = document.camera.distance,
        };
        for (auto const actor : document.camera.targets) {
            auto const* const id{ids_by_actor.Find(actor)};
            if (!id) {
                return std::unexpected{TEXT("Every camera target must be an adopted entity.")};
            }
            camera.target_entity_ids.Add(*id);
        }
        builder.set_camera(camera);
    } else {
        TArray<FLevelEntityId> players;
        for (auto const& binding : document.entities) {
            if (IsValid(Cast<ATestSpaceShip>(binding.actor))) {
                players.Add(FLevelEntityId{binding.id});
            }
        }
        if (players.Num() != 1) {
            return std::unexpected{
                TEXT("Player-authored levels require exactly one player actor.")};
        }
        builder.set_player_entity(players[0]);
    }

    if (document.mission.mode != ETestMissionMode::None) {
        FLevelMissionDefinition mission{.mode = mission_mode(document.mission.mode)};
        if (document.mission.mode == ETestMissionMode::SurviveTime ||
            document.mission.mode == ETestMissionMode::KillEnemiesWithinTime) {
            mission.time_limit_seconds = document.mission.time_limit_seconds;
        }
        if (document.mission.use_explicit_kill_count &&
            document.mission.mode != ETestMissionMode::SurviveTime) {
            mission.kill_count = document.mission.kill_count;
        }
        auto append_ids = [&ids_by_actor](TArray<FLevelEntityId>& output,
                                          TArray<TObjectPtr<AActor>> const& actors,
                                          FStringView const role) -> FString {
            for (auto const actor : actors) {
                auto const* const id{ids_by_actor.Find(actor)};
                if (!id) {
                    return FString::Printf(TEXT("Every %.*s objective must be an adopted entity."),
                                           role.Len(),
                                           role.GetData());
                }
                output.Add(*id);
            }
            return {};
        };
        if (auto const error{
                append_ids(mission.hero_entity_ids, document.mission.heroes, TEXTVIEW("hero"))};
            !error.IsEmpty()) {
            return std::unexpected{error};
        }
        if (auto const error{append_ids(mission.must_survive_entity_ids,
                                        document.mission.must_survive,
                                        TEXTVIEW("must-survive"))};
            !error.IsEmpty()) {
            return std::unexpected{error};
        }
        if (auto const error{append_ids(mission.required_kill_entity_ids,
                                        document.mission.required_kills,
                                        TEXTVIEW("required-kill"))};
            !error.IsEmpty()) {
            return std::unexpected{error};
        }
        builder.set_mission(mission);
    }

    auto definition{builder.finish()};
    if (auto const error{validation_error(definition)}; !error.IsEmpty()) {
        return std::unexpected{error};
    }
    return definition;
}

auto make_s7_level_sync_plan(ULevel const& level,
                             AS7LevelAuthoringDocument const& document,
                             FLevelDefinition const& definition)
    -> std::expected<FS7LevelSyncPlan, FString> {
    auto prepared{prepare_sync_plan(level, document, definition)};
    if (!prepared) {
        return std::unexpected{prepared.error()};
    }
    return FS7LevelSyncPlan{.definition = definition,
                            .changes = MoveTemp(prepared->changes),
                            .metadata_changed = prepared->metadata_changed,
                            .viewpoint_changed = prepared->viewpoint_changed,
                            .mission_changed = prepared->mission_changed};
}

auto apply_s7_level_sync_plan(ULevel& level,
                              AS7LevelAuthoringDocument& document,
                              FS7LevelSyncPlan const& plan) -> std::expected<void, FString> {
    auto prepared{prepare_sync_plan(level, document, plan.definition)};
    if (!prepared) {
        return std::unexpected{prepared.error()};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "Apply", "Apply S7 Level to Scene")};
    TArray<AActor*> staged_actors;
    staged_actors.Reserve(prepared->entities.Num());
    TMap<FName, AActor*> resolved;
    for (auto const& entity : prepared->entities) {
        auto* actor{entity.existing_actor};
        if (entity.requires_spawn) {
            actor = GEditor->AddActor(
                &level, entity.actor_class, entity.transform, true, RF_Transactional, false);
            if (!IsValid(actor)) {
                for (auto* const staged_actor : staged_actors) {
                    if (IsValid(staged_actor)) {
                        staged_actor->Destroy();
                    }
                }
                transaction.Cancel();
                return std::unexpected{FString::Printf(TEXT("Could not spawn entity '%s'."),
                                                       *entity.id.value.ToString())};
            }
            staged_actors.Add(actor);
        }
        resolved.Add(entity.id.value, actor);
    }

    for (auto const& entity : prepared->entities) {
        auto& actor{*resolved.FindChecked(entity.id.value)};
        configure_actor(actor,
                        entity.archetype,
                        entity.team,
                        *document.level_config,
                        entity.transform,
                        entity.id.value);
    }
    for (auto* const actor : prepared->actors_to_destroy) {
        if (IsValid(actor)) {
            actor->Modify();
            actor->Destroy();
        }
    }

    document.Modify();
    auto const entities{plan.definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    document.level_id = plan.definition.metadata.id.value;
    document.title = plan.definition.metadata.title;
    document.description = plan.definition.metadata.description;
    document.entities.Reset(entity_count);
    for (int32 index{}; index < entity_count; ++index) {
        auto const id{entities.ids[index].value};
        document.entities.Add({.id = id,
                               .actor = resolved.FindChecked(id),
                               .spawn_time_seconds = entities.spawn_times_seconds[index]});
    }
    document.use_observer_camera = plan.definition.camera.IsSet();
    document.camera.targets.Reset();
    if (plan.definition.camera.IsSet()) {
        auto const& camera{plan.definition.camera.GetValue()};
        document.camera.offset_direction = camera.offset_direction;
        document.camera.distance = camera.distance;
        for (auto const id : camera.target_entity_ids) {
            document.camera.targets.Add(resolved.FindChecked(id.value));
        }
    }
    document.mission = {};
    if (plan.definition.mission.IsSet()) {
        auto const& mission{plan.definition.mission.GetValue()};
        document.mission.mode = authoring_mode(mission.mode);
        if (mission.time_limit_seconds.IsSet()) {
            document.mission.time_limit_seconds = mission.time_limit_seconds.GetValue();
        }
        document.mission.use_explicit_kill_count = mission.kill_count.IsSet();
        if (mission.kill_count.IsSet()) {
            document.mission.kill_count = mission.kill_count.GetValue();
        }
        auto append_actors = [&resolved](TArray<TObjectPtr<AActor>>& output,
                                         TArray<FLevelEntityId> const& ids) {
            for (auto const id : ids) {
                output.Add(resolved.FindChecked(id.value));
            }
        };
        append_actors(document.mission.heroes, mission.hero_entity_ids);
        append_actors(document.mission.must_survive, mission.must_survive_entity_ids);
        append_actors(document.mission.required_kills, mission.required_kill_entity_ids);
    }
    return {};
}
}
