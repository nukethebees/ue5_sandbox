#include <SpaceGameS7/LevelDefinitionReader.h>

#include <sandbox/level_authoring/LevelDefinitionReader.h>

#include <Containers/StringConv.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

#include <string_view>

namespace ml::s7 {
namespace {
auto to_fstring(std::string_view const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}

auto to_fname(std::string const& value) -> FName {
    return FName{to_fstring(value)};
}

template <typename Id>
auto to_ids(std::vector<std::string> const& source) -> TArray<Id> {
    TArray<Id> result;
    result.Reserve(static_cast<int32>(source.size()));
    for (auto const& id : source) {
        result.Add(Id{to_fname(id)});
    }
    return result;
}

auto to_unreal(level_authoring::LevelDefinition definition) -> FLevelDefinition {
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
        auto& camera{*definition.camera};
        builder.set_camera(FLevelCameraDefinition{
            .target_entity_ids = to_ids<FLevelEntityId>(camera.target_entity_ids),
            .offset_direction = FVector{camera.offset_direction.x,
                                        camera.offset_direction.y,
                                        camera.offset_direction.z},
            .distance = camera.distance,
        });
    }
    if (definition.mission) {
        auto& source{*definition.mission};
        FLevelMissionDefinition mission{
            .mode = source.mode,
            .hero_entity_ids = to_ids<FLevelEntityId>(source.hero_entity_ids),
            .must_survive_entity_ids = to_ids<FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids = to_ids<FLevelEntityId>(source.required_kill_entity_ids),
        };
        if (source.time_limit_seconds) {
            mission.time_limit_seconds = *source.time_limit_seconds;
        }
        if (source.kill_count) {
            mission.kill_count = *source.kill_count;
        }
        builder.set_mission(mission);
    }

    for (auto& source : definition.mission_events) {
        builder.add_mission_event(FLevelMissionObjectiveEvent{
            .time_seconds = source.time_seconds,
            .must_survive_entity_ids = to_ids<FLevelEntityId>(source.must_survive_entity_ids),
            .required_kill_entity_ids = to_ids<FLevelEntityId>(source.required_kill_entity_ids),
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

auto to_unreal(level_authoring::LevelDefinitionReadResult native) -> FLevelDefinitionReadResult {
    FLevelDefinitionReadResult result;
    result.script_error = to_fstring(native.script_error);
    result.decode_errors.Reserve(static_cast<int32>(native.decode_errors.size()));
    for (auto& error : native.decode_errors) {
        result.decode_errors.Add(
            {.path = to_fstring(error.path), .message = to_fstring(error.message)});
    }
    result.validation_errors.Reserve(static_cast<int32>(native.validation_errors.size()));
    for (auto& error : native.validation_errors) {
        result.validation_errors.Add({.code = error.code, .message = to_fstring(error.message)});
    }
    if (native.definition) {
        result.definition.Emplace(to_unreal(std::move(*native.definition)));
    }
    return result;
}
} // namespace

FLevelDefinitionReader::FLevelDefinitionReader(FString script_library_root)
    : script_library_root_{MoveTemp(script_library_root)} {}

auto FLevelDefinitionReader::read_source(FStringView const source) const
    -> FLevelDefinitionReadResult {
    auto const converted_source{FTCHARToUTF8{source.GetData(), source.Len()}};
    auto const utf8_source{std::string_view{converted_source.Get(),
                                            static_cast<std::size_t>(converted_source.Length())}};

    auto const converted_root{FTCHARToUTF8{*script_library_root_}};
    auto const utf8_root{
        std::string{converted_root.Get(), static_cast<std::size_t>(converted_root.Length())}};
    return to_unreal(level_authoring::LevelDefinitionReader{utf8_root}.read_source(utf8_source));
}

auto FLevelDefinitionReader::read_file(FStringView const path) const -> FLevelDefinitionReadResult {
    FString source;
    auto const owned_path{FString{path}};
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return {.script_error =
                    FString::Printf(TEXT("Could not read level script '%s'."), *owned_path)};
    }

    if (script_library_root_.IsEmpty()) {
        auto const library_root{FPaths::Combine(FPaths::GetPath(owned_path), TEXT("Libraries"))};
        return FLevelDefinitionReader{library_root}.read_source(source);
    }
    return read_source(source);
}
} // namespace ml::s7
