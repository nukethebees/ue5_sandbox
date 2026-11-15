#include "SandboxGameShared/utilities/actor_utils.h"

#include <Components/SceneComponent.h>
#include <GameFramework/Actor.h>

namespace ioj {
void set_actor_component_mobility(AActor& actor, EComponentMobility::Type mobility) {
    TInlineComponentArray<USceneComponent*> components;
    actor.GetComponents(components);

    for (auto* const component : components) {
        if (component != nullptr) {
            component->SetMobility(mobility);
        }
    }
}
}
