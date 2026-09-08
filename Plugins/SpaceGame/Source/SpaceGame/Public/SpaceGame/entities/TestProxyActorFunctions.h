#pragma once

#include <CoreMinimal.h>

class AActor;
class USpaceGameLevelConfig;
enum class ETestTeam : uint8;

namespace ml {
void set_proxy_actor_name(AActor& actor, FString const& type, ETestTeam team);
#if WITH_EDITOR
auto resolve_proxy_level_config(AActor const& actor) -> USpaceGameLevelConfig*;
#endif
}
