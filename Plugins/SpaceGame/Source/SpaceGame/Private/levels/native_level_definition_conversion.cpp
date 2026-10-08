#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

#include <algorithm>
#include <format>

namespace ioj::levels::authoring {
namespace {
auto columns_have_equal_size(ml::FLevelEntityTable const& entities) -> bool {
    auto const count{entities.ids.Num()};
    return entities.archetypes.Num() == count && entities.teams.Num() == count &&
           entities.positions.num() == count && entities.rotations.num() == count &&
           entities.spawn_times_seconds.Num() == count;
}

auto to_utf8(FString const& value) -> std::string {
    auto const converted{FTCHARToUTF8{*value, value.Len()}};
    return {converted.Get(), static_cast<std::size_t>(converted.Length())};
}

auto to_utf8(FName const value) -> std::string {
    if (value.IsNone()) {
        return {};
    }
    return to_utf8(value.ToString().ToLower());
}

auto to_fname(std::string_view const value) -> FName {
    return FName{ml::to_fstring(std::string{value})};
}

template <typename NativeId, typename Id>
auto to_ids(TConstArrayView<Id> const source) -> std::vector<NativeId> {
    std::vector<NativeId> result;
    result.reserve(source.Num());
    for (auto const id : source) {
        result.emplace_back(to_utf8(id.value));
    }
    return result;
}

template <typename Id, typename NativeId>
auto to_unreal_ids(std::vector<NativeId> const& source) -> TArray<Id> {
    TArray<Id> result;
    result.Reserve(static_cast<int32>(source.size()));
    for (auto const& id : source) {
        result.Add(Id{to_fname(id.value)});
    }
    return result;
}
} // namespace

auto to_native(ml::FLevelDefinition const& definition)
    -> std::expected<LevelDefinition, Diagnostics> {
    if (!columns_have_equal_size(definition.entities)) {
        return std::unexpected{Diagnostics{{DiagnosticCode::MismatchedEntityColumns,
                                            "level.entities",
                                            "Level entity columns have inconsistent lengths"}}};
    }

    LevelDefinition result;
    Diagnostics errors;
    result.metadata.id = LevelId{to_utf8(definition.metadata.id.value)};
    result.metadata.title = to_utf8(definition.metadata.title);
    result.metadata.description = to_utf8(definition.metadata.description);
    if (definition.metadata.par_time_seconds.IsSet()) {
        result.metadata.par_time_seconds = definition.metadata.par_time_seconds.GetValue();
    }

    result.unlock_level_ids.reserve(definition.unlock_criteria.Num());
    for (auto const& criterion : definition.unlock_criteria) {
        result.unlock_level_ids.emplace_back(
            to_utf8(criterion.Get<ml::FLevelCompletedUnlockCriterion>().level_id.value));
    }
    result.player_entity_id = EntityId{to_utf8(definition.player_entity_id.value)};

    if (definition.collision_grid.IsSet()) {
        auto const& grid{definition.collision_grid.GetValue()};
        LevelCollisionGridDefinition native_grid;
        if (grid.level_size.IsSet()) {
            auto const size{grid.level_size.GetValue()};
            native_grid.level_size = ::ml::Vector3d{size.X, size.Y, size.Z};
        }
        if (grid.cell_size.IsSet()) {
            auto const size{grid.cell_size.GetValue()};
            native_grid.cell_size = ::ml::Vector3d{size.X, size.Y, size.Z};
        }
        result.collision_grid = MoveTemp(native_grid);
    }

    if (definition.camera.IsSet()) {
        auto const& camera{definition.camera.GetValue()};
        result.camera = {
            .target_entity_ids = to_ids<EntityId, ml::FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = {camera.offset_direction.X,
                                 camera.offset_direction.Y,
                                 camera.offset_direction.Z},
            .distance = camera.distance,
        };
    }
    if (definition.mission.IsSet()) {
        auto const& mission{definition.mission.GetValue()};
        LevelMissionDefinition native_mission{
            .mode = mission.mode,
            .hero_entity_ids = to_ids<EntityId, ml::FLevelEntityId>(mission.hero_entity_ids),
            .must_survive_entity_ids =
                to_ids<EntityId, ml::FLevelEntityId>(mission.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_ids<EntityId, ml::FLevelEntityId>(mission.required_kill_entity_ids),
        };
        if (mission.time_limit_seconds.IsSet()) {
            native_mission.time_limit_seconds = mission.time_limit_seconds.GetValue();
        }
        if (mission.kill_count.IsSet()) {
            native_mission.kill_count = mission.kill_count.GetValue();
        }
        result.mission = MoveTemp(native_mission);
    }

    result.mission_events.reserve(definition.mission_events.Num());
    for (auto const& event : definition.mission_events) {
        result.mission_events.push_back({
            .time_seconds = event.time_seconds,
            .must_survive_entity_ids =
                to_ids<EntityId, ml::FLevelEntityId>(event.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_ids<EntityId, ml::FLevelEntityId>(event.required_kill_entity_ids),
            .kill_target_increase = event.kill_target_increase,
        });
    }
    auto const team_count{definition.teams.Num()};
    for (int32 index{}; index < team_count; ++index) {
        auto const name{to_utf8(definition.teams[index].value)};
        auto const team{try_parse_serialized_team_id(name)};
        if (team) {
            result.teams.push_back(*team);
        } else {
            errors.emplace_back(name.empty() ? DiagnosticCode::EmptyTeamId
                                             : DiagnosticCode::UnsupportedTeamId,
                                std::format("level.teams[{}]", index),
                                std::format("Unknown team '{}'", name));
        }
    }

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    result.entities.reserve(entity_count);
    for (int32 index{}; index < entity_count; ++index) {
        auto const archetype_name{to_utf8(entities.archetypes[index].value)};
        auto const archetype{try_parse_serialized_entity_archetype(archetype_name)};
        auto const team_name{to_utf8(entities.teams[index].value)};
        auto const team{try_parse_serialized_team_id(team_name)};
        if (!archetype) {
            errors.emplace_back(archetype_name.empty() ? DiagnosticCode::EmptyArchetypeId
                                                       : DiagnosticCode::UnsupportedArchetype,
                                std::format("level.entities[{}].archetype", index),
                                std::format("Unknown archetype '{}'", archetype_name));
        }
        if (!team) {
            errors.emplace_back(DiagnosticCode::UnknownTeamReference,
                                std::format("level.entities[{}].team", index),
                                std::format("Unknown team '{}'", team_name));
        }
        result.entities.push_back({
            .id = EntityId{to_utf8(entities.ids[index].value)},
            .archetype = archetype.value_or(EntityArchetype::PlayerFighter),
            .team = team.value_or(TeamId::White),
            .position = {entities.positions.xs[index],
                         entities.positions.ys[index],
                         entities.positions.zs[index]},
            .rotation = {entities.rotations.pitches[index],
                         entities.rotations.yaws[index],
                         entities.rotations.rolls[index]},
            .spawn_time_seconds = entities.spawn_times_seconds[index],
        });
    }
    auto validation{validate_level(result)};
    if (!validation) {
        for (auto& diagnostic : validation.error()) {
            if (std::ranges::none_of(errors, [&](Diagnostic const& error) {
                    return error.node_path == diagnostic.node_path;
                })) {
                errors.push_back(std::move(diagnostic));
            }
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return result;
}

auto to_unreal(LevelDefinition definition) -> ml::FLevelDefinition {
    ml::FLevelBuilder builder;
    ml::FLevelMetadata metadata{
        .id = ml::FLevelId{to_fname(definition.metadata.id.value)},
        .title = ml::to_fstring(definition.metadata.title),
        .description = ml::to_fstring(definition.metadata.description),
    };
    if (definition.metadata.par_time_seconds) {
        metadata.par_time_seconds = *definition.metadata.par_time_seconds;
    }
    builder.set_metadata(metadata);

    if (definition.collision_grid) {
        auto const& grid{*definition.collision_grid};
        ml::FLevelCollisionGridDefinition unreal_grid;
        if (grid.level_size) {
            unreal_grid.level_size = FVector3f{static_cast<float>(grid.level_size->x),
                                               static_cast<float>(grid.level_size->y),
                                               static_cast<float>(grid.level_size->z)};
        }
        if (grid.cell_size) {
            unreal_grid.cell_size = FVector3f{static_cast<float>(grid.cell_size->x),
                                              static_cast<float>(grid.cell_size->y),
                                              static_cast<float>(grid.cell_size->z)};
        }
        builder.set_collision_grid(unreal_grid);
    }

    for (auto const& level_id : definition.unlock_level_ids) {
        builder.add_unlock_criterion(
            ml::FLevelUnlockCriterion{TInPlaceType<ml::FLevelCompletedUnlockCriterion>{},
                                      ml::FLevelCompletedUnlockCriterion{
                                          .level_id = ml::FLevelId{to_fname(level_id.value)}}});
    }
    for (auto const& team : definition.teams) {
        builder.add_team(ml::FLevelTeamId{to_fname(to_serialized_string(team))});
    }
    if (!definition.player_entity_id.empty()) {
        builder.set_player_entity(ml::FLevelEntityId{to_fname(definition.player_entity_id.value)});
    }

    if (definition.camera) {
        auto const& camera{*definition.camera};
        builder.set_camera(ml::FLevelCameraDefinition{
            .target_entity_ids = to_unreal_ids<ml::FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = FVector{camera.offset_direction.x,
                                        camera.offset_direction.y,
                                        camera.offset_direction.z},
            .distance = camera.distance,
        });
    }
    if (definition.mission) {
        auto const& source{*definition.mission};
        ml::FLevelMissionDefinition mission{
            .mode = source.mode,
            .hero_entity_ids = to_unreal_ids<ml::FLevelEntityId>(source.hero_entity_ids),
            .must_survive_entity_ids =
                to_unreal_ids<ml::FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_unreal_ids<ml::FLevelEntityId>(source.required_kill_entity_ids),
        };
        if (source.time_limit_seconds) {
            mission.time_limit_seconds = *source.time_limit_seconds;
        }
        if (source.kill_count) {
            mission.kill_count = *source.kill_count;
        }
        builder.set_mission(mission);
    }

    for (auto const& source : definition.mission_events) {
        builder.add_mission_event(ml::FLevelMissionObjectiveEvent{
            .time_seconds = source.time_seconds,
            .must_survive_entity_ids =
                to_unreal_ids<ml::FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_unreal_ids<ml::FLevelEntityId>(source.required_kill_entity_ids),
            .kill_target_increase = source.kill_target_increase,
        });
    }
    for (auto const& source : definition.entities) {
        builder.add_entity(ml::FEntitySpawnDefinition{
            .id = ml::FLevelEntityId{to_fname(source.id.value)},
            .archetype = ml::FEntityArchetypeId{to_fname(to_serialized_string(source.archetype))},
            .team = ml::FLevelTeamId{to_fname(to_serialized_string(source.team))},
            .position = FVector{source.position.x, source.position.y, source.position.z},
            .rotation = FRotator{source.rotation.pitch, source.rotation.yaw, source.rotation.roll},
            .spawn_time_seconds = source.spawn_time_seconds,
        });
    }
    return builder.finish();
}
} // namespace ioj::levels::authoring
