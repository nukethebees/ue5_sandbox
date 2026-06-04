#include "TestTubeSpinnerProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestTubeSpinnersConfig.h"

#include <Components/ArrowComponent.h>
#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>

ATestTubeSpinnerProxy::ATestTubeSpinnerProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
}

#if WITH_EDITOR
void ATestTubeSpinnerProxy::add_fire_point() {}
void ATestTubeSpinnerProxy::pop_fire_point() {}

void ATestTubeSpinnerProxy::save_configuration_to_asset() {
    if (!actor_config) {
        return;
    }

    actor_config->Modify();

    // Save code here

    actor_config->MarkPackageDirty();
}

void ATestTubeSpinnerProxy::apply_asset_configuration() {
    if (!actor_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestTubeSpinnerProxy::apply_asset_configuration: actor_config is nullptr."));
        return;
    }

    mesh->SetStaticMesh(actor_config->mesh);
}
void ATestTubeSpinnerProxy::apply_asset_configuration_to_all_instances() {
    auto* world{GetWorld()};
    if (!world) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestTubeSpinnerProxy::apply_asset_configuration_to_all_instances: world is "
                    "nullptr."));
        return;
    }

    for (auto it{TActorIterator<ThisClass>(world)}; it; ++it) {
        if (!IsValid(*it)) {
            continue;
        }

        it->apply_asset_configuration();
    }
}

#endif
