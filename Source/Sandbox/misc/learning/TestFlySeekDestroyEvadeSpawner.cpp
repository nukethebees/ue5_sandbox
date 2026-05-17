#include "TestFlySeekDestroyEvadeSpawner.h"

void ATestFlySeekDestroyEvadeSpawner::configure_instance(AActor& instance) {
    Super::configure_instance(instance);

    auto* inst{Cast<ATestFlySeekDestroyEvade>(&instance)};
    if (!inst) {
        return;
    }

    inst->set_config(config);
}
