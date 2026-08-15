from __future__ import annotations

from pathlib import Path

from Codegen.nodes import CppFile, Include, Module, Namespace, NewLines, Raw
from Codegen.soa import SoAStruct, soa_member, tarray_member


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def fighter_soa_module() -> Module:
    members = (
        tarray_member("entity_handles", "FRegistryEntityHandle"),
        tarray_member("integral_biases", "uint32"),
        tarray_member("float_biases", "float"),
        tarray_member("tasks", "ETestCapitalShipFightersTask"),
        soa_member("locations", "FVectors3f"),
        soa_member("desired_move_locations", "FVectors3f"),
        soa_member("aim_directions", "FVectors3f"),
        soa_member("desired_aiming_directions", "FVectors3f"),
        soa_member("movement_directions", "FVectors3f"),
        soa_member("velocities", "FVectors3f"),
        tarray_member("move_distances", "float"),
        tarray_member("speeds", "float"),
        tarray_member("teams", "ETestTeam"),
        tarray_member("healths", "int32"),
        soa_member("awareness_scan_countdowns", "FTickCountdown8"),
        soa_member("attack_reposition_countdowns", "FTickCountdown16"),
        soa_member("attack_cooldowns", "FTickCountdown16"),
        tarray_member("target_handles", "FRegistryEntityHandle"),
        soa_member("target_locations", "FVectors3f"),
        soa_member("target_velocities", "FVectors3f"),
        soa_member("target_directions", "FVectors3f"),
        tarray_member("intercept_times", "float"),
        tarray_member("target_distance_sq", "float"),
        tarray_member("target_distances", "float"),
        tarray_member("target_radii", "float"),
    )
    return Module(
        name="test_capital_ship_fighters_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "TestCapitalShipFightersSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestCapitalShipFightersTask.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_vectors.h"),
                Include("SandboxCore/tick_countdown.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("Containers/ArrayView.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(
                Namespace(
                    "ml::test_capital_ship_fighters",
                    (
                        SoAStruct(
                            "EntityData",
                            "EntityDataView",
                            "EntityDataConstView",
                            members,
                        ),
                    ),
                ),
            ),
        ),
    )


def capital_ships_soa_module() -> Module:
    spawn_data = SoAStruct(
        "SpawnData",
        "SpawnDataView",
        "SpawnDataConstView",
        (
            tarray_member("target_handles", "FRegistryEntityHandle"),
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            tarray_member("teams", "ETestTeam"),
            tarray_member("healths", "int32"),
            tarray_member("initial_spawn_delays", "float"),
            tarray_member("spawn_cooldowns", "float"),
        ),
    )
    entity_tick_data = SoAStruct(
        "EntityTickData",
        "EntityTickDataView",
        "EntityTickDataConstView",
        (
            tarray_member("ships_ready_to_spawn_fighters_buffer", "int32"),
            soa_member("fighter_queue", "TestCapitalShipFighterSpawnQueue"),
        ),
    )
    entity_data = SoAStruct(
        "EntityData",
        "EntityDataView",
        "EntityDataConstView",
        (
            tarray_member("handles", "FRegistryEntityHandle"),
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            soa_member("fighter_spawn_timers", "FCountdownTimers"),
            tarray_member("fighter_spawn_cooldowns", "float"),
            tarray_member("teams", "ETestTeam"),
            tarray_member("healths", "int32"),
            tarray_member("capital_fighter_handle_spans", "FIndexSpan"),
            tarray_member("target_handles", "FRegistryEntityHandle"),
        ),
    )
    return Module(
        name="test_capital_ships_soa",
        header=CppFile(
            path=PROJECT_ROOT / "Source" / "Sandbox" / "batch_game" / "TestCapitalShipsSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                Include("Sandbox/utilities/IndexSpan.h"),
                NewLines(2),
                Include("SandboxCore/countdown_timers.h"),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_rotators.h"),
                Include("SandboxCore/soa_vectors.h"),
                NewLines(2),
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(
                Namespace("ml::test_capital_ships", (spawn_data, entity_tick_data, entity_data)),
            ),
        ),
    )


def lasers_soa_module() -> Module:
    entities = SoAStruct(
        "Entities",
        "EntitiesView",
        "EntitiesConstView",
        (
            tarray_member("ismc_data", "FInstancedStaticMeshInstanceData"),
            tarray_member("colours", "FLinearColor"),
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            soa_member("velocities", "FVectors3f"),
            tarray_member("damages", "int32"),
            tarray_member("lifetimes_remaining", "float"),
            tarray_member("instigator_handles", "FRegistryEntityHandle"),
        ),
    )
    hit_details = SoAStruct(
        "HitDetails",
        "HitDetailsView",
        "HitDetailsConstView",
        (soa_member("locations", "FVectors3f"), tarray_member("colours", "FLinearColor")),
    )
    return Module(
        name="test_lasers_soa",
        header=CppFile(
            path=PROJECT_ROOT / "Source" / "Sandbox" / "batch_game" / "TestLasersSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_rotators.h"),
                Include("SandboxCore/soa_vectors.h"),
                NewLines(2),
                Include("Components/InstancedStaticMeshComponent.h"),
                Include("CoreMinimal.h"),
                Include("Math/Color.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(Namespace("ml::test_lasers", (entities, hit_details)),),
        ),
    )


def collision_damage_events_soa_module() -> Module:
    unresolved = SoAStruct(
        "UnresolvedCollisionDamageEvents",
        "UnresolvedCollisionDamageEventsView",
        "UnresolvedCollisionDamageEventsConstView",
        (
            tarray_member("damaged_actors", "AActor*"),
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
    )
    resolved = SoAStruct(
        "CollisionDamageEvents",
        "CollisionDamageEventsView",
        "CollisionDamageEventsConstView",
        (
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
    )
    return Module(
        name="collision_damage_events_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "test_entity_registry"
            / "CollisionDamageEventsSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(Raw("class AActor;\nclass UActorComponent;"), unresolved, resolved),
        ),
    )


def fighter_spawn_queue_soa_module() -> Module:
    queue = SoAStruct(
        "TestCapitalShipFighterSpawnQueue",
        "TestCapitalShipFighterSpawnQueueView",
        "TestCapitalShipFighterSpawnQueueConstView",
        (
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            tarray_member("teams", "ETestTeam"),
            tarray_member("targets", "FRegistryEntityHandle"),
        ),
    )
    return Module(
        name="test_capital_ship_fighter_spawn_queue_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "TestCapitalShipFighterSpawnQueueSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_rotators.h"),
                Include("SandboxCore/soa_vectors.h"),
                NewLines(2),
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(queue,),
        ),
    )


def static_turrets_soa_module() -> Module:
    entity_data = SoAStruct(
        "EntityData",
        "EntityDataView",
        "EntityDataConstView",
        (
            tarray_member("handles", "FRegistryEntityHandle"),
            soa_member("locations", "FVectors3f"),
            tarray_member("teams", "ETestTeam"),
            soa_member("laser_cooldowns", "FTickCountdown16"),
            tarray_member("target_handles", "FRegistryEntityHandle"),
            soa_member("target_locations", "FVectors3f"),
            soa_member("target_velocities", "FVectors3f"),
            tarray_member("healths", "int32"),
        ),
    )
    return Module(
        name="test_static_turrets_soa",
        header=CppFile(
            path=PROJECT_ROOT / "Source" / "Sandbox" / "batch_game" / "TestStaticTurretsSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_vectors.h"),
                Include("SandboxCore/tick_countdown.h"),
                NewLines(2),
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(Namespace("ml::test_static_turrets", (entity_data,)),),
        ),
    )


def tube_spinners_soa_module() -> Module:
    entity_data = SoAStruct(
        "EntityData",
        "EntityDataView",
        "EntityDataConstView",
        (
            tarray_member("handles", "FRegistryEntityHandle"),
            soa_member("locations", "FVectors3f"),
            tarray_member("yaws", "float"),
            soa_member("laser_cooldowns", "FTickCountdown16"),
            tarray_member("next_fire_point_indices", "int32"),
        ),
    )
    return Module(
        name="test_tube_spinners_soa",
        header=CppFile(
            path=PROJECT_ROOT / "Source" / "Sandbox" / "batch_game" / "TestTubeSpinnersSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                Include("SandboxCore/soa_vectors.h"),
                Include("SandboxCore/tick_countdown.h"),
                NewLines(2),
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(Namespace("ml::test_tube_spinners", (entity_data,)),),
        ),
    )


def direct_damage_events_soa_module() -> Module:
    events = SoAStruct(
        "DirectDamageEvents",
        "DirectDamageEventsView",
        "DirectDamageEventsConstView",
        (
            tarray_member("damaged_entities", "FRegistryEntityHandle"),
            tarray_member("damage_amounts", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
    )
    return Module(
        name="direct_damage_events_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "test_entity_registry"
            / "DirectDamageEventsSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(events,),
        ),
    )


def unique_entity_data_soa_module() -> Module:
    entity_data = SoAStruct(
        "TestEntityUniqueEntityData",
        "TestEntityUniqueEntityDataView",
        "TestEntityUniqueEntityDataConstView",
        (
            tarray_member("registry_indices", "FRegistryEntityHandle::index_type"),
            tarray_member("registry_generations", "FRegistryEntityHandle::generation_type"),
            tarray_member("entity_types", "ETestEntityType"),
            tarray_member("teams", "ETestTeam"),
            tarray_member("kills", "uint32"),
            tarray_member("alive", "uint8"),
            tarray_member("killed_by", "TestEntityUniqueId"),
            tarray_member("death_reason", "ETestDeathReason"),
        ),
        storage_export_specifier="SANDBOX_API",
        storage_type_aliases=("using kills_type = uint32",),
    )
    return Module(
        name="test_entity_unique_entity_data_soa",
        header=CppFile(
            path=PROJECT_ROOT
            / "Source"
            / "Sandbox"
            / "batch_game"
            / "test_entity_registry"
            / "TestEntityUniqueEntityDataSoA.h",
            clang_format_off=True,
            includes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/test_entity_registry/TestDeathReason.h"),
                Include("Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h"),
                Include("Sandbox/batch_game/TestEntityType.h"),
                Include("Sandbox/batch_game/TestTeam.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
            ),
            nodes=(entity_data,),
        ),
    )


def modules() -> tuple[Module, ...]:
    return (
        fighter_soa_module(),
        capital_ships_soa_module(),
        lasers_soa_module(),
        collision_damage_events_soa_module(),
        fighter_spawn_queue_soa_module(),
        static_turrets_soa_module(),
        tube_spinners_soa_module(),
        direct_damage_events_soa_module(),
        unique_entity_data_soa_module(),
    )
