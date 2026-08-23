from Codegen.cpp import Module
from Codegen.manifests.batch_game import (
    capital_ships_soa_module,
    fighter_order_queue_module,
    fighter_soa_module,
    fighter_spawn_queue_soa_module,
    laser_collision_data_soa_module,
    lasers_soa_module,
    static_turrets_soa_module,
    tube_spinners_soa_module,
)
from Codegen.manifests.core import (
    countdown_timers_module,
    soa_rotators_module,
    soa_vectors_module,
    soa_vectors_umbrella_module,
)
from Codegen.manifests.entity_registry import (
    collision_damage_events_soa_module,
    direct_damage_events_soa_module,
    entity_death_info_module,
    entity_registry_data_soa_module,
    registry_entity_handles_soa_module,
    unique_entity_data_soa_module,
)
from Codegen.manifests.facades import (
    capital_ship_fighters_command_interface_module,
    phase_facade_modules,
)
from Codegen.manifests.test_fixtures import fixed_soa_test_types_module


def modules() -> tuple[Module, ...]:
    return (
        countdown_timers_module(),
        *soa_vectors_module(),
        soa_vectors_umbrella_module(),
        soa_rotators_module(),
        fighter_soa_module(),
        capital_ships_soa_module(),
        lasers_soa_module(),
        laser_collision_data_soa_module(),
        collision_damage_events_soa_module(),
        fighter_spawn_queue_soa_module(),
        fighter_order_queue_module(),
        capital_ship_fighters_command_interface_module(),
        *phase_facade_modules(),
        static_turrets_soa_module(),
        tube_spinners_soa_module(),
        direct_damage_events_soa_module(),
        unique_entity_data_soa_module(),
        entity_registry_data_soa_module(),
        registry_entity_handles_soa_module(),
        entity_death_info_module(),
        fixed_soa_test_types_module(),
    )
