#pragma once
#include <ioj/levels/entity_archetype.h>
#include <SpaceGame/entities/TestProxyActorFunctions.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <CoreMinimal.h>

class AActor;
class ULevel;

namespace ioj {
class USpaceGameLevelConfig;
}

namespace ioj::editor {
struct FS7LevelResolvedActor {
    levels::EntityArchetype archetype{};
    Team team{};
    double spawn_time_seconds{};
    bool export_to_script{};
};

void update_s7_proxy_label(AActor& actor, ProxyLabelUpdate update = ProxyLabelUpdate::Refresh);
void refresh_s7_proxy_labels(ULevel const& level);
auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor>;
auto s7_level_actor_class(levels::EntityArchetype archetype, USpaceGameLevelConfig const& config)
    -> UClass*;
auto canonical_s7_level_entity_id(FStringView text, FStringView fallback) -> FName;
auto is_canonical_s7_level_entity_id(FStringView id) -> bool;
}
