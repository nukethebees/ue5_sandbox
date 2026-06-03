#include <SandboxCore/collision_settings.h>

#include <Components/PrimitiveComponent.h>
#include "CoreMinimal.h"

namespace ml {
auto copy_collision_settings(UPrimitiveComponent const& component) -> FCollisionSettings {
    return {
        .enabled = component.GetCollisionEnabled(),
        .object_type = component.GetCollisionObjectType(),
        .response = component.GetCollisionResponseToChannels(),
        .can_generate_overlap_events = component.GetGenerateOverlapEvents(),
        .can_character_step_up_on = component.CanCharacterStepUpOn,
        .notify_rigid_body_collision = component.BodyInstance.bNotifyRigidBodyCollision,
    };
}
void apply_collision_settings(UPrimitiveComponent& component, FCollisionSettings const& settings) {
    component.SetCollisionEnabled(settings.enabled);
    component.SetCollisionObjectType(settings.object_type);
    component.SetCollisionResponseToChannels(settings.response);
    component.SetGenerateOverlapEvents(settings.can_generate_overlap_events);
    component.CanCharacterStepUpOn = settings.can_character_step_up_on;
    component.SetNotifyRigidBodyCollision(static_cast<bool>(settings.notify_rigid_body_collision));
}
}
