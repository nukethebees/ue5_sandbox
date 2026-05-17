#include "TestFlySeekDestroyEvade.h"

ATestFlySeekDestroyEvade::ATestFlySeekDestroyEvade() {}

void ATestFlySeekDestroyEvade::OnConstruction(FTransform const& t) {
    Super::OnConstruction(t);

    transition_to_state();
}
void ATestFlySeekDestroyEvade::BeginPlay() {
    Super::BeginPlay();

    set_state(ETestFlySeekDestroyEvadeState::searching);
}
void ATestFlySeekDestroyEvade::Tick(float dt) {
    Super::Tick(dt);

    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            handle_search(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            handle_chase(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            handle_attack(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            handle_evade(dt);
            break;
        }
    }
}

void ATestFlySeekDestroyEvade::set_config(FTestFlySeekDestroyEvadeConfig const& new_config) {
    config = new_config;
}
void ATestFlySeekDestroyEvade::set_movement(
    FTestFlySeekDestroyEvadeMovementConfig const& new_config) {
    turn_speed_deg_per_s = new_config.turn_speed_deg_per_s;
    speed = new_config.speed;
}

void ATestFlySeekDestroyEvade::set_state(ETestFlySeekDestroyEvadeState new_state) {
    state = new_state;
    transition_to_state();
}
void ATestFlySeekDestroyEvade::transition_to_state() {
    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            set_movement(config.search.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            set_movement(config.chase.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            set_movement(config.attack.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            set_movement(config.evade.movement);
            break;
        }
    }
}

void ATestFlySeekDestroyEvade::handle_search(float dt) {}
void ATestFlySeekDestroyEvade::handle_chase(float dt) {}
void ATestFlySeekDestroyEvade::handle_attack(float dt) {}
void ATestFlySeekDestroyEvade::handle_evade(float dt) {}
