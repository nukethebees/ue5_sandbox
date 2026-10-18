#pragma once

#include <ioj/levels/level_mission_mode.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/missions/TestMissionMode.h>

#include <CoreMinimal.h>

class AActor;
class USpaceGameLevelConfig;

namespace ioj::editor {
struct FS7LevelResolvedActor {
    ml::EResolvedLevelArchetype archetype{};
    ::ioj::Team team{};
};

auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor>;
auto s7_level_actor_class(ml::EResolvedLevelArchetype archetype,
                          USpaceGameLevelConfig const& config) -> UClass*;
void configure_s7_level_actor(AActor& actor,
                              ml::EResolvedLevelArchetype archetype,
                              ETestTeam team,
                              USpaceGameLevelConfig& config,
                              FTransform const& transform,
                              FName label);
auto canonical_s7_level_entity_id(FStringView text, FStringView fallback) -> FName;
auto is_canonical_s7_level_entity_id(FStringView id) -> bool;
auto is_canonical_s7_level_entity_id(FName id) -> bool;
auto s7_mission_mode(ETestMissionMode mode) -> levels::LevelMissionMode;
auto s7_mission_mode(levels::LevelMissionMode mode) -> ETestMissionMode;
}
