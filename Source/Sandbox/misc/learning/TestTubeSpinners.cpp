#include "TestTubeSpinners.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <Components/InstancedStaticMeshComponent.h>

ATestTubeSpinners::ATestTubeSpinners()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);
}

// Actor life cycle
void ATestTubeSpinners::clear_runtime_state() {
    instances->ClearInstances();
    laser_cooldowns.Reset();
    indices_ready_to_fire.Reset();
}
void ATestTubeSpinners::BeginPlay() {
    Super::BeginPlay();

    if (actor_config == nullptr) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestTubeSpinners: actor_config is nullptr."));
        SetActorTickEnabled(false);
        return;
    }

    clear_runtime_state();
    configure_ismc();
    register_all_proxies_in_level();
}
void ATestTubeSpinners::Tick(float dt) {
    Super::Tick(dt);

    laser_cooldowns.tick(dt);
    rotate_instances(dt);
    fire_lasers();
}

// Accessors
auto ATestTubeSpinners::get_num_instances() const noexcept -> int32 {
    return laser_cooldowns.Num();
}

// Spawning
void ATestTubeSpinners::register_all_proxies_in_level() {
    return;
}

// Movement
void ATestTubeSpinners::rotate_instances(float const dt) {}

// Visuals
void ATestTubeSpinners::configure_ismc() {
    return;
}

// Firing
void ATestTubeSpinners::fire_lasers() {
    return;
}

// Debugging
bool ATestTubeSpinners::array_sizes_consistent() const {
    return false;
}
