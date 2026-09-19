#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <Misc/PackageName.h>
#include <ScopedTransaction.h>

namespace ml::editor {
namespace s7_level_authoring_session_detail {
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

auto validation_error(FLevelDefinition const& definition) -> FString {
    auto const validation{validate_level(definition)};
    TArray<FString> messages;
    for (auto const& error : validation.errors) {
        messages.Add(error.message);
    }
    return FString::Join(messages, TEXT("\n"));
}
}

using namespace s7_level_authoring_session_detail;

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
    document->level_id = canonical_s7_level_entity_id(map_name, TEXTVIEW("authored-level"));
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
        auto const resolved{IsValid(actor) ? resolve_s7_level_actor(*actor) : NullOpt};
        if (!resolved.IsSet() || bound.Contains(actor)) {
            continue;
        }
        auto const fallback{to_level_archetype_id(resolved->archetype).value.ToString()};
        auto id{canonical_s7_level_entity_id(actor->GetActorLabel(), fallback)};
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
        auto const resolved{resolve_s7_level_actor(*binding.actor)};
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
        if (IsValid(actor) && resolve_s7_level_actor(*actor).IsSet() &&
            !ids_by_actor.Contains(actor)) {
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
}
