#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <algorithm>
#include <format>

namespace ioj::levels::authoring {
namespace {
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
    return FName{to_fstring(std::string{value})};
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

auto to_native(FLevelDraft const& definition) -> std::expected<LevelDefinition, Diagnostics> {
    LevelDefinitionComponents result;
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
            to_utf8(criterion.Get<FLevelCompletedUnlockCriterion>().level_id.value));
    }
    result.player_entity_id = EntityId{to_utf8(definition.player_entity_id.value)};

    if (definition.collision_grid.IsSet()) {
        auto const& grid{definition.collision_grid.GetValue()};
        LevelCollisionGridDefinition native_grid;
        if (grid.level_size.IsSet()) {
            auto const size{grid.level_size.GetValue()};
            native_grid.level_size = Vector3d{size.X, size.Y, size.Z};
        }
        if (grid.cell_size.IsSet()) {
            auto const size{grid.cell_size.GetValue()};
            native_grid.cell_size = Vector3d{size.X, size.Y, size.Z};
        }
        result.collision_grid = MoveTemp(native_grid);
    }

    if (definition.camera.IsSet()) {
        auto const& camera{definition.camera.GetValue()};
        result.camera = {
            .target_entity_ids = to_ids<EntityId, FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = {camera.offset_direction.X,
                                 camera.offset_direction.Y,
                                 camera.offset_direction.Z},
            .distance = camera.distance,
        };
    }
    if (definition.mission.IsSet()) {
        result.mission = definition.mission.GetValue();
    }
    auto const& entities{definition.entities};
    auto const entity_count{entities.Num()};
    result.entities.reserve(entity_count);
    for (int32 index{}; index < entity_count; ++index) {
        auto const& entity{entities[index]};
        auto const archetype_name{to_utf8(entity.archetype.value)};
        auto const archetype{try_parse_serialized_entity_archetype(archetype_name)};
        if (!archetype) {
            errors.emplace_back(archetype_name.empty() ? DiagnosticCode::EmptyArchetypeId
                                                       : DiagnosticCode::UnsupportedArchetype,
                                std::format("level.entities[{}].archetype", index),
                                std::format("Unknown archetype '{}'", archetype_name));
        }
        result.entities.push_back({
            .id = EntityId{to_utf8(entity.id.value)},
            .archetype = archetype.value_or(EntityArchetype::PlayerFighter),
            .team = entity.team,
            .position = {entity.position.X, entity.position.Y, entity.position.Z},
            .rotation = {entity.rotation.Pitch, entity.rotation.Yaw, entity.rotation.Roll},
            .spawn_time_seconds = entity.spawn_time_seconds,
        });
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return LevelDefinition::make(std::move(result));
}

auto to_unreal(LevelId const& id) -> FLevelId {
    return FLevelId{FName{to_fstring(id.value)}};
}

auto to_unreal(LevelDefinition const& level) -> FLevelDraft {
    auto const& definition{level};
    FLevelBuilder builder;
    FLevelMetadata metadata{
        .id = FLevelId{to_fname(definition.metadata().id.value)},
        .title = to_fstring(definition.metadata().title),
        .description = to_fstring(definition.metadata().description),
    };
    if (definition.metadata().par_time_seconds) {
        metadata.par_time_seconds = *definition.metadata().par_time_seconds;
    }
    builder.set_metadata(metadata);

    if (definition.collision_grid()) {
        auto const& grid{*definition.collision_grid()};
        FLevelCollisionGridDefinition unreal_grid;
        if (grid.level_size) {
            unreal_grid.level_size = ioj::to_unreal(to_float(*grid.level_size));
        }
        if (grid.cell_size) {
            unreal_grid.cell_size = ioj::to_unreal(to_float(*grid.cell_size));
        }
        builder.set_collision_grid(unreal_grid);
    }

    for (auto const& level_id : definition.unlock_level_ids()) {
        builder.add_unlock_criterion(FLevelUnlockCriterion{
            TInPlaceType<FLevelCompletedUnlockCriterion>{},
            FLevelCompletedUnlockCriterion{.level_id = FLevelId{to_fname(level_id.value)}}});
    }
    if (!definition.player_entity_id().empty()) {
        builder.set_player_entity(FLevelEntityId{to_fname(definition.player_entity_id().value)});
    }

    if (definition.camera()) {
        auto const& camera{*definition.camera()};
        builder.set_camera(FLevelCameraDefinition{
            .target_entity_ids = to_unreal_ids<FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = FVector{camera.offset_direction.x(),
                                        camera.offset_direction.y(),
                                        camera.offset_direction.z()},
            .distance = camera.distance,
        });
    }
    if (definition.mission()) {
        builder.set_mission(*definition.mission());
    }
    for (auto const& source : definition.entities()) {
        builder.add_entity(FEntitySpawnDefinition{
            .id = FLevelEntityId{to_fname(source.id.value)},
            .archetype = FEntityArchetypeId{to_fname(to_serialized_string(source.archetype))},
            .team = source.team,
            .position = FVector{source.position.x(), source.position.y(), source.position.z()},
            .rotation = FRotator{source.rotation.pitch, source.rotation.yaw, source.rotation.roll},
            .spawn_time_seconds = source.spawn_time_seconds,
        });
    }
    return builder.finish();
}
} // namespace ioj::levels::authoring
