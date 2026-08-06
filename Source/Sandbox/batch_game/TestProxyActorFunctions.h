#pragma once

#include <CoreMinimal.h>

class AActor;
enum class ETestTeam : uint8;

namespace ml {
void set_proxy_actor_name(AActor& actor, FString const& type, ETestTeam team);
}
