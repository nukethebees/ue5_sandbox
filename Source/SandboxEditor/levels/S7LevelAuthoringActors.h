#pragma once

#include <SpaceGame/levels/LevelEntityResolution.h>

#include <CoreMinimal.h>

class AActor;
class USpaceGameLevelConfig;

namespace ml::editor {
struct FS7LevelResolvedActor {
    EResolvedLevelArchetype archetype{};
    FLevelTeamId team{};
};

auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor>;
auto s7_level_actor_class(EResolvedLevelArchetype archetype, USpaceGameLevelConfig const& config)
    -> UClass*;
void configure_s7_level_actor(AActor& actor,
                              EResolvedLevelArchetype archetype,
                              ETestTeam team,
                              USpaceGameLevelConfig& config,
                              FTransform const& transform,
                              FName label);
auto canonical_s7_level_entity_id(FStringView text, FStringView fallback) -> FName;
}
