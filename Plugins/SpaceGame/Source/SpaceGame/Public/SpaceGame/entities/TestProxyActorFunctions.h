#pragma once

#include <CoreMinimal.h>

class AActor;

namespace ioj {
enum class ETestTeam : uint8;

enum class ProxyLabelUpdate { Placement, Refresh };
SPACEGAME_API void set_proxy_actor_name(AActor& actor,
                                        FString const& type,
                                        ETestTeam team,
                                        ProxyLabelUpdate update);
}
