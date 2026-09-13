#include "NativeLevelDefinitionConversion.h"

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

template <typename Id>
auto to_ids(TConstArrayView<Id> const source) -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(source.Num());
    for (auto const id : source) {
        result.push_back(to_utf8(id.value));
    }
    return result;
}
} // namespace

auto to_native(FLevelDefinition const& definition) -> LevelDefinition {
    LevelDefinition result;
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
        LevelMissionDefinition native_mission{
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
} // namespace ml::level_authoring
