from __future__ import annotations

from pathlib import Path

from Codegen.nodes import (
    CppFile,
    FunctionParameter,
    Include,
    MemberFunctionSpec,
    Module,
    Namespace,
    NewLines,
    Raw,
)
from Codegen.soa import (
    ForEachSoAMemberCall,
    SoAStruct,
    SoAStorageOperation,
    lower_soa_structs,
    soa_member,
    tarray_member,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BATCH_GAME_DIR = PROJECT_ROOT / "Source" / "Sandbox" / "batch_game"
TEST_ENTITY_REGISTRY_DIR = BATCH_GAME_DIR / "test_entity_registry"
ALL_STORAGE_OPERATIONS = tuple(SoAStorageOperation)
COUNTDOWN_TIMERS_STORAGE_OPERATIONS = (
    SoAStorageOperation.RESET,
    SoAStorageOperation.ADD_UNINITIALISED,
    SoAStorageOperation.REMOVE_AT_SWAP,
    SoAStorageOperation.COPY_ELEMENT,
)


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
            path=BATCH_GAME_DIR / "TestCapitalShipFightersSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                Namespace(
                    "ml::test_capital_ship_fighters",
                    lower_soa_structs(
                        (
                            SoAStruct(
                                "EntityData",
                                "EntityDataView",
                                "EntityDataConstView",
                                members,
                                storage_operations=ALL_STORAGE_OPERATIONS,
                            ),
                        )
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    entity_tick_data = SoAStruct(
        "EntityTickData",
        "EntityTickDataView",
        "EntityTickDataConstView",
        (
            tarray_member("ships_ready_to_spawn_fighters_buffer", "int32"),
            soa_member("fighter_queue", "TestCapitalShipFighterSpawnQueue"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
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
        storage_operations=COUNTDOWN_TIMERS_STORAGE_OPERATIONS,
    )
    fighter_reassignment_members = (
        tarray_member("capital_handles", "FRegistryEntityHandle"),
        tarray_member("fighter_handles", "FRegistryEntityHandle"),
    )
    fighter_reassignment = SoAStruct(
        "FighterReassignment",
        "FighterReassignmentView",
        "FighterReassignmentConstView",
        fighter_reassignment_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        nodes=(
            MemberFunctionSpec(
                "add",
                "void",
                (
                    FunctionParameter("FRegistryEntityHandle const", "ch"),
                    FunctionParameter("FRegistryEntityHandle const", "fh"),
                ),
                ForEachSoAMemberCall(fighter_reassignment_members, "Add"),
                is_inline=True,
            ).header_node(),
        ),
    )
    return Module(
        name="test_capital_ships_soa",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestCapitalShipsSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                Namespace(
                    "ml::test_capital_ships",
                    lower_soa_structs(
                        (spawn_data, entity_tick_data, entity_data, fighter_reassignment)
                    ),
                ),
            ),
        ),
    )


def lasers_soa_module() -> Module:
    set_damages = MemberFunctionSpec(
        "set_damages",
        "void",
        (FunctionParameter("int32 const", "value"),),
        Raw(""),
    )
    set_speeds = MemberFunctionSpec(
        "set_speeds",
        "void",
        (FunctionParameter("float const", "value"),),
        Raw(""),
    )
    set_max_distances = MemberFunctionSpec(
        "set_max_distances",
        "void",
        (FunctionParameter("float const", "value"),),
        Raw(""),
    )
    set_colours = MemberFunctionSpec(
        "set_colours",
        "void",
        (FunctionParameter("FLinearColor const", "value"),),
        Raw(""),
    )
    spawn_requests = SoAStruct(
        "SpawnRequests",
        "SpawnRequestsView",
        "SpawnRequestsConstView",
        (
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            soa_member("base_velocities", "FVectors3f"),
            tarray_member("damages", "int32"),
            tarray_member("speeds", "float"),
            tarray_member("max_distances", "float"),
            tarray_member("instigator_handles", "FRegistryEntityHandle"),
            tarray_member("colours", "FLinearColor"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        nodes=(
            set_damages.header_node(),
            NewLines(1),
            set_speeds.header_node(),
            NewLines(1),
            set_max_distances.header_node(),
            NewLines(1),
            set_colours.header_node(),
        ),
    )
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    hit_details = SoAStruct(
        "HitDetails",
        "HitDetailsView",
        "HitDetailsConstView",
        (soa_member("locations", "FVectors3f"), tarray_member("colours", "FLinearColor")),
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="test_lasers_soa",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestLasersSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                Namespace(
                    "ml::test_lasers",
                    lower_soa_structs((spawn_requests, entities, hit_details)),
                ),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="collision_damage_events_soa",
        header=CppFile(
            path=TEST_ENTITY_REGISTRY_DIR / "CollisionDamageEventsSoA.h",
            clang_format_off=True,
            nodes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
                NewLines(2),
                Raw("class AActor;\nclass UActorComponent;"),
                NewLines(2),
                *lower_soa_structs((unresolved, resolved)),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="test_capital_ship_fighter_spawn_queue_soa",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestCapitalShipFighterSpawnQueueSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                *lower_soa_structs((queue,)),
            ),
        ),
    )


def fighter_order_queue_module() -> Module:
    order_queue_members = (
        tarray_member("handles", "FRegistryEntityHandle"),
        tarray_member("orders", "TestCapitalShipFighterOrder"),
        tarray_member("tasks", "ETestCapitalShipFightersTask"),
        tarray_member("targets", "FRegistryEntityHandle"),
    )
    order_queue = SoAStruct(
        "TestCapitalShipFighterOrderQueue",
        "TestCapitalShipFighterOrderQueueView",
        "TestCapitalShipFighterOrderQueueConstView",
        order_queue_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_type_aliases=(
            ("Task", "ETestCapitalShipFightersTask"),
            ("Order", "TestCapitalShipFighterOrder"),
        ),
        nodes=(
            MemberFunctionSpec(
                "add",
                "void",
                (
                    FunctionParameter("FRegistryEntityHandle const", "handle"),
                    FunctionParameter("Order const", "order"),
                    FunctionParameter("Task const", "task"),
                    FunctionParameter("FRegistryEntityHandle const", "target"),
                ),
                ForEachSoAMemberCall(order_queue_members, "Add"),
                is_inline=True,
            ).header_node(),
        ),
    )
    return Module(
        name="test_capital_ship_fighter_order_queue",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestCapitalShipFighterOrderQueue.h",
            clang_format_off=True,
            nodes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/TestCapitalShipFighterOrder.h"),
                Include("Sandbox/batch_game/TestCapitalShipFightersTask.h"),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                Include("Containers/ArrayView.h"),
                Include("utility"),
                NewLines(2),
                *lower_soa_structs((order_queue,)),
            ),
        ),
    )


def entity_death_info_module() -> Module:
    entity_death_info_members = (
        tarray_member("reasons", "ETestDeathReason"),
        tarray_member("victims", "FRegistryEntityHandle"),
        tarray_member("killers", "FRegistryEntityHandle"),
    )
    add_function = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter("ETestDeathReason const", "reason"),
            FunctionParameter("FRegistryEntityHandle const", "victim"),
            FunctionParameter("FRegistryEntityHandle const", "killer"),
        ),
        ForEachSoAMemberCall(entity_death_info_members, "Add"),
    )
    add_without_killer = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter("ETestDeathReason const", "reason"),
            FunctionParameter("FRegistryEntityHandle const", "victim"),
        ),
        Raw("add(reason, victim, FRegistryEntityHandle{});"),
        is_inline=True,
    )
    entity_death_info = SoAStruct(
        "EntityDeathInfo",
        "EntityDeathInfoView",
        "EntityDeathInfoConstView",
        entity_death_info_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        nodes=(
            add_function.declaration_node(),
            NewLines(1),
            add_without_killer.header_node(),
        ),
    )
    return Module(
        name="entity_death_info",
        header=CppFile(
            path=TEST_ENTITY_REGISTRY_DIR / "EntityDeathInfo.h",
            clang_format_off=True,
            nodes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                Include("Sandbox/batch_game/test_entity_registry/TestDeathReason.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("Containers/ArrayView.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("utility"),
                NewLines(2),
                *lower_soa_structs((entity_death_info,)),
            ),
        ),
        source=CppFile(
            path=TEST_ENTITY_REGISTRY_DIR / "EntityDeathInfo.cpp",
            pragma_once=False,
            clang_format_off=True,
            nodes=(
                Include("EntityDeathInfo.h", system=False),
                NewLines(2),
                add_function.definition_node("EntityDeathInfo"),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="test_static_turrets_soa",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestStaticTurretsSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                Namespace("ml::test_static_turrets", lower_soa_structs((entity_data,))),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="test_tube_spinners_soa",
        header=CppFile(
            path=BATCH_GAME_DIR / "TestTubeSpinnersSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                Namespace("ml::test_tube_spinners", lower_soa_structs((entity_data,))),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
    )
    return Module(
        name="direct_damage_events_soa",
        header=CppFile(
            path=TEST_ENTITY_REGISTRY_DIR / "DirectDamageEventsSoA.h",
            clang_format_off=True,
            nodes=(
                Include("Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h"),
                NewLines(2),
                Include("SandboxCore/soa_array_mixin.h"),
                NewLines(2),
                Include("Containers/Array.h"),
                Include("HAL/Platform.h"),
                NewLines(2),
                Include("type_traits"),
                Include("utility"),
                NewLines(2),
                *lower_soa_structs((events,)),
            ),
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
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier="SANDBOX_API",
        storage_type_aliases=(("kills_type", "uint32"),),
    )
    return Module(
        name="test_entity_unique_entity_data_soa",
        header=CppFile(
            path=TEST_ENTITY_REGISTRY_DIR / "TestEntityUniqueEntityDataSoA.h",
            clang_format_off=True,
            nodes=(
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
                NewLines(2),
                *lower_soa_structs((entity_data,)),
            ),
        ),
    )


def modules() -> tuple[Module, ...]:
    return (
        fighter_soa_module(),
        capital_ships_soa_module(),
        lasers_soa_module(),
        collision_damage_events_soa_module(),
        fighter_spawn_queue_soa_module(),
        fighter_order_queue_module(),
        static_turrets_soa_module(),
        tube_spinners_soa_module(),
        direct_damage_events_soa_module(),
        unique_entity_data_soa_module(),
        entity_death_info_module(),
    )
