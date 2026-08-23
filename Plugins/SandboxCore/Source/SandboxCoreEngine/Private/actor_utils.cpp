#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
auto is_actor_in_world(UWorld const& world, AActor const* const target) -> bool {
    if (!IsValid(target)) {
        return false;
    }

    for (TActorIterator<AActor> it{&world}; it; ++it) {
        if (*it == target) {
            return true;
        }
    }

    return false;
}
}
