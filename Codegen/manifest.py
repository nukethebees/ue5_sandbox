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
    UsingDeclaration,
)
from Codegen.soa import (
    ForEachSoAMemberCall,
    SoAStruct,
    SoAStructNames,
    SoAStorageOperation,
    lower_soa_structs_with_source,
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
SANDBOX_API = "SANDBOX_API"


def soa_source_file(
    header_path: Path, source_nodes: tuple, namespace: str | None = None
) -> CppFile:
    definitions = (Namespace(namespace, source_nodes),) if namespace else source_nodes
    return CppFile(
        path=header_path.with_suffix(".cpp"),
        pragma_once=False,
        clang_format_off=True,
        nodes=(
            Include(header_path.name, system=False),
            NewLines(2),
            *definitions,
        ),
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
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = BATCH_GAME_DIR / "TestCapitalShipFightersSoA.h"
    return Module(
        name="test_capital_ship_fighters_soa",
        header=CppFile(
            path=header_path,
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
                    lowered.header_nodes,
                ),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::test_capital_ship_fighters"
        ),
    )


def capital_ships_soa_module() -> Module:
    spawn_data = SoAStruct(
        SoAStructNames("SpawnData"),
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
        storage_export_specifier=SANDBOX_API,
    )
    entity_tick_data = SoAStruct(
        SoAStructNames("EntityTickData"),
        (
            tarray_member("ships_ready_to_spawn_fighters_buffer", "int32"),
            soa_member("fighter_queue", "TestCapitalShipFighterSpawnQueue"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
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
        storage_export_specifier=SANDBOX_API,
    )
    fighter_reassignment_members = (
        tarray_member("capital_handles", "FRegistryEntityHandle"),
        tarray_member("fighter_handles", "FRegistryEntityHandle"),
    )
    fighter_reassignment = SoAStruct(
        SoAStructNames("FighterReassignment"),
        fighter_reassignment_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
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
    lowered = lower_soa_structs_with_source(
        (spawn_data, entity_tick_data, entity_data, fighter_reassignment)
    )
    header_path = BATCH_GAME_DIR / "TestCapitalShipsSoA.h"
    return Module(
        name="test_capital_ships_soa",
        header=CppFile(
            path=header_path,
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
                    lowered.header_nodes,
                ),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::test_capital_ships"),
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
        SoAStructNames("SpawnRequests"),
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
        storage_export_specifier=SANDBOX_API,
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
        SoAStructNames("Entities"),
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
        storage_export_specifier=SANDBOX_API,
    )
    hit_details = SoAStruct(
        SoAStructNames("HitDetails"),
        (soa_member("locations", "FVectors3f"), tarray_member("colours", "FLinearColor")),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((spawn_requests, entities, hit_details))
    header_path = BATCH_GAME_DIR / "TestLasersSoA.h"
    return Module(
        name="test_lasers_soa",
        header=CppFile(
            path=header_path,
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
                    lowered.header_nodes,
                ),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::test_lasers"),
    )


def collision_damage_events_soa_module() -> Module:
    unresolved = SoAStruct(
        SoAStructNames("UnresolvedCollisionDamageEvents"),
        (
            tarray_member("damaged_actors", "AActor*"),
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    resolved = SoAStruct(
        SoAStructNames("CollisionDamageEvents"),
        (
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((unresolved, resolved))
    header_path = TEST_ENTITY_REGISTRY_DIR / "CollisionDamageEventsSoA.h"
    return Module(
        name="collision_damage_events_soa",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def fighter_spawn_queue_soa_module() -> Module:
    queue = SoAStruct(
        SoAStructNames("TestCapitalShipFighterSpawnQueue"),
        (
            soa_member("locations", "FVectors3f"),
            soa_member("rotations", "FRotatorsf"),
            tarray_member("teams", "ETestTeam"),
            tarray_member("targets", "FRegistryEntityHandle"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((queue,))
    header_path = BATCH_GAME_DIR / "TestCapitalShipFighterSpawnQueueSoA.h"
    return Module(
        name="test_capital_ship_fighter_spawn_queue_soa",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def fighter_order_queue_module() -> Module:
    order_queue_members = (
        tarray_member("handles", "FRegistryEntityHandle"),
        tarray_member("orders", "TestCapitalShipFighterOrder"),
        tarray_member("tasks", "ETestCapitalShipFightersTask"),
        tarray_member("targets", "FRegistryEntityHandle"),
    )
    order_queue = SoAStruct(
        SoAStructNames("TestCapitalShipFighterOrderQueue"),
        order_queue_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            UsingDeclaration("Task", "ETestCapitalShipFightersTask"),
            NewLines(1),
            UsingDeclaration("Order", "TestCapitalShipFighterOrder"),
            NewLines(2),
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
    lowered = lower_soa_structs_with_source((order_queue,))
    header_path = BATCH_GAME_DIR / "TestCapitalShipFighterOrderQueue.h"
    return Module(
        name="test_capital_ship_fighter_order_queue",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
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
        SoAStructNames("EntityDeathInfo"),
        entity_death_info_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            add_function.declaration_node(),
            NewLines(1),
            add_without_killer.header_node(),
        ),
    )
    lowered = lower_soa_structs_with_source((entity_death_info,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "EntityDeathInfo.h"
    return Module(
        name="entity_death_info",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(
            header_path,
            (*lowered.source_nodes, NewLines(2), add_function.definition_node("EntityDeathInfo")),
        ),
    )


def static_turrets_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
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
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = BATCH_GAME_DIR / "TestStaticTurretsSoA.h"
    return Module(
        name="test_static_turrets_soa",
        header=CppFile(
            path=header_path,
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
                Namespace("ml::test_static_turrets", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::test_static_turrets"),
    )


def tube_spinners_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            tarray_member("handles", "FRegistryEntityHandle"),
            soa_member("locations", "FVectors3f"),
            tarray_member("yaws", "float"),
            soa_member("laser_cooldowns", "FTickCountdown16"),
            tarray_member("next_fire_point_indices", "int32"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = BATCH_GAME_DIR / "TestTubeSpinnersSoA.h"
    return Module(
        name="test_tube_spinners_soa",
        header=CppFile(
            path=header_path,
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
                Namespace("ml::test_tube_spinners", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::test_tube_spinners"),
    )


def direct_damage_events_soa_module() -> Module:
    events = SoAStruct(
        SoAStructNames("DirectDamageEvents"),
        (
            tarray_member("damaged_entities", "FRegistryEntityHandle"),
            tarray_member("damage_amounts", "int32"),
            tarray_member("instigators", "FRegistryEntityHandle"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((events,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "DirectDamageEventsSoA.h"
    return Module(
        name="direct_damage_events_soa",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def unique_entity_data_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("TestEntityUniqueEntityData"),
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
        storage_export_specifier=SANDBOX_API,
        nodes=(UsingDeclaration("kills_type", "uint32"),),
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "TestEntityUniqueEntityDataSoA.h"
    return Module(
        name="test_entity_unique_entity_data_soa",
        header=CppFile(
            path=header_path,
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
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
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
