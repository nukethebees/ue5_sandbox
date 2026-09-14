#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

#include <Containers/StringConv.h>

namespace ml::level_authoring {
namespace {
auto to_utf8(FString const& value) -> std::string {
    return TCHAR_TO_UTF8(*value);
}

auto to_utf8(FName const value) -> std::string {
    if (value.IsNone()) {
        return {};
    }
    return to_utf8(value.ToString().ToLower());
}

auto to_fstring(std::string_view const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}

auto to_fname(std::string const& value) -> FName {
    return FName{to_fstring(value)};
}

template <typename Id>
auto to_ids(TConstArrayView<Id> const source) -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(source.Num());
    for (auto const id : source) {
        result.push_back(to_utf8(id.value));
    }
    return result;
}

template <typename Id>
auto to_unreal_ids(std::vector<std::string> const& source) -> TArray<Id> {
    TArray<Id> result;
    result.Reserve(static_cast<int32>(source.size()));
    for (auto const& id : source) {
        result.Add(Id{to_fname(id)});
    }
    return result;
}
} // namespace

auto to_native(FLevelDefinition const& definition) -> ::ioj::sim::levels::LevelDefinition {
    ::ioj::sim::levels::LevelDefinition result;
    result.metadata.id = to_utf8(definition.metadata.id.value);
    result.metadata.title = to_utf8(definition.metadata.title);
    result.metadata.description = to_utf8(definition.metadata.description);
    if (definition.metadata.par_time_seconds.IsSet()) {
        result.metadata.par_time_seconds = definition.metadata.par_time_seconds.GetValue();
    }

    result.unlock_level_ids.reserve(definition.unlock_criteria.Num());
    for (auto const& criterion : definition.unlock_criteria) {
        result.unlock_level_ids.push_back(
            to_utf8(criterion.Get<FLevelCompletedUnlockCriterion>().level_id.value));
    }
    result.player_entity_id = to_utf8(definition.player_entity_id.value);

    if (definition.camera.IsSet()) {
        auto const& camera{definition.camera.GetValue()};
        result.camera = {
            .target_entity_ids = to_ids<FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = {camera.offset_direction.X,
                                 camera.offset_direction.Y,
                                 camera.offset_direction.Z},
            .distance = camera.distance,
        };
    }
    if (definition.mission.IsSet()) {
        auto const& mission{definition.mission.GetValue()};
        ::ioj::sim::levels::LevelMissionDefinition native_mission{
            .mode = mission.mode,
            .hero_entity_ids = to_ids<FLevelEntityId>(mission.hero_entity_ids),
            .must_survive_entity_ids = to_ids<FLevelEntityId>(mission.must_survive_entity_ids),
            .required_kill_entity_ids = to_ids<FLevelEntityId>(mission.required_kill_entity_ids),
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
            .must_survive_entity_ids = to_ids<FLevelEntityId>(event.must_survive_entity_ids),
            .required_kill_entity_ids = to_ids<FLevelEntityId>(event.required_kill_entity_ids),
            .kill_target_increase = event.kill_target_increase,
        });
    }
    result.teams = to_ids<FLevelTeamId>(definition.teams);

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    result.entities.reserve(entity_count);
    for (int32 index{}; index < entity_count; ++index) {
        result.entities.push_back({
            .id = to_utf8(entities.ids[index].value),
            .archetype = to_utf8(entities.archetypes[index].value),
            .team = to_utf8(entities.teams[index].value),
            .position = {entities.positions.xs[index],
                         entities.positions.ys[index],
                         entities.positions.zs[index]},
            .rotation = {entities.rotations.pitches[index],
                         entities.rotations.yaws[index],
                         entities.rotations.rolls[index]},
            .spawn_time_seconds = entities.spawn_times_seconds[index],
        });
    }
    return result;
}

auto to_unreal(::ioj::sim::levels::LevelDefinition definition) -> FLevelDefinition {
    FLevelBuilder builder;
    FLevelMetadata metadata{
        .id = FLevelId{to_fname(definition.metadata.id)},
        .title = to_fstring(definition.metadata.title),
        .description = to_fstring(definition.metadata.description),
    };
    if (definition.metadata.par_time_seconds) {
        metadata.par_time_seconds = *definition.metadata.par_time_seconds;
    }
    builder.set_metadata(metadata);

    for (auto const& level_id : definition.unlock_level_ids) {
        builder.add_unlock_criterion(FLevelUnlockCriterion{
            TInPlaceType<FLevelCompletedUnlockCriterion>{},
            FLevelCompletedUnlockCriterion{.level_id = FLevelId{to_fname(level_id)}}});
    }
    for (auto const& team : definition.teams) {
        builder.add_team(FLevelTeamId{to_fname(team)});
    }
    if (!definition.player_entity_id.empty()) {
        builder.set_player_entity(FLevelEntityId{to_fname(definition.player_entity_id)});
    }

    if (definition.camera) {
        auto const& camera{*definition.camera};
        builder.set_camera(FLevelCameraDefinition{
            .target_entity_ids = to_unreal_ids<FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = FVector{camera.offset_direction.x,
                                        camera.offset_direction.y,
                                        camera.offset_direction.z},
            .distance = camera.distance,
        });
    }
    if (definition.mission) {
        auto const& source{*definition.mission};
        FLevelMissionDefinition mission{
            .mode = source.mode,
            .hero_entity_ids = to_unreal_ids<FLevelEntityId>(source.hero_entity_ids),
            .must_survive_entity_ids =
                to_unreal_ids<FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_unreal_ids<FLevelEntityId>(source.required_kill_entity_ids),
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
        builder.add_mission_event(FLevelMissionObjectiveEvent{
            .time_seconds = source.time_seconds,
            .must_survive_entity_ids =
                to_unreal_ids<FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids =
                to_unreal_ids<FLevelEntityId>(source.required_kill_entity_ids),
            .kill_target_increase = source.kill_target_increase,
        });
    }
    for (auto const& source : definition.entities) {
        builder.add_entity(FEntitySpawnDefinition{
            .id = FLevelEntityId{to_fname(source.id)},
            .archetype = FEntityArchetypeId{to_fname(source.archetype)},
            .team = FLevelTeamId{to_fname(source.team)},
            .position = FVector{source.position.x, source.position.y, source.position.z},
            .rotation = FRotator{source.rotation.pitch, source.rotation.yaw, source.rotation.roll},
            .spawn_time_seconds = source.spawn_time_seconds,
        });
    }
    return builder.finish();
}
} // namespace ml::level_authoring
