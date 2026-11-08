#pragma once

#include <CoreMinimal.h>

class AActor;

namespace ioj {
enum class ETestTeam : uint8;

void set_proxy_actor_name(AActor& actor, FString const& type, ETestTeam team);
}
