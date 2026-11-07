#pragma once

#include <ioj/levels/entity_archetype.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <CoreMinimal.h>

class AActor;
class USpaceGameLevelConfig;

namespace ioj::editor {
struct FS7LevelResolvedActor {
    levels::EntityArchetype archetype{};
    Team team{};
    double spawn_time_seconds{};
};

auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor>;
auto s7_level_actor_class(levels::EntityArchetype archetype, USpaceGameLevelConfig const& config)
    -> UClass*;
void configure_s7_level_actor(AActor& actor,
                              levels::EntityArchetype archetype,
                              ETestTeam team,
                              USpaceGameLevelConfig& config,
                              FTransform const& transform,
                              FName label);
auto canonical_s7_level_entity_id(FStringView text, FStringView fallback) -> FName;
auto is_canonical_s7_level_entity_id(FStringView id) -> bool;
}
