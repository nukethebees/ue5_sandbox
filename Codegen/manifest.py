from __future__ import annotations

from pathlib import Path

from Codegen.nodes import (
    CppFile,
    FunctionParameter,
    Include,
    IncludeDependencies,
    MemberFunctionSpec,
    Module,
    Namespace,
    NewLines,
    Raw,
    TypeDependency,
    UsingDeclaration,
)
from Codegen.cpp_types import (
    E_TEST_CAPITAL_SHIP_FIGHTERS_TASK,
    E_TEST_DEATH_REASON,
    E_TEST_ENTITY_TYPE,
    E_TEST_TEAM,
    F_COUNTDOWN_TIMERS,
    F_INDEX_SPAN,
    F_INSTANCED_STATIC_MESH_INSTANCE_DATA,
    F_INT_POINT,
    F_INT_VECTOR,
    F_LINEAR_COLOR,
    F_REGISTRY_ENTITY_HANDLE,
    F_ROTATORS_F,
    F_TICK_COUNTDOWN_8,
    F_TICK_COUNTDOWN_16,
    F_UINT_POINT,
    F_UINT_VECTOR_3,
    F_VECTOR_2D,
    F_VECTOR_2F,
    F_VECTOR_3D,
    F_VECTOR_3F,
    F_VECTORS_3F,
    TEST_CAPITAL_SHIP_FIGHTER_ORDER,
    TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE,
    TEST_ENTITY_UNIQUE_ID,
    U_PRIMITIVE_COMPONENT_CONST_PTR,
    nested_type,
    qualified_type,
)
from Codegen.soa import (
    ForEachSoAMemberCall,
    HomogeneousSoALayout,
    HomogeneousSoAValueType,
    SoAStruct,
    SoAStructNames,
    SoAStorageOperation,
    lower_soa_structs_with_source,
    lower_homogeneous_soa_layouts,
    soa_member,
    tarray_member,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BATCH_GAME_DIR = PROJECT_ROOT / "Source" / "Sandbox" / "batch_game"
TEST_ENTITY_REGISTRY_DIR = BATCH_GAME_DIR / "test_entity_registry"
SANDBOX_CORE_PUBLIC_DIR = (
    PROJECT_ROOT
    / "Plugins"
    / "SandboxCore"
    / "Source"
    / "SandboxCore"
    / "Public"
    / "SandboxCore"
)
ALL_STORAGE_OPERATIONS = tuple(SoAStorageOperation)
SANDBOX_API = "SANDBOX_API"
SANDBOX_CORE_API = "SANDBOXCORE_API"
ARRAY_MATH = TypeDependency("ml::subtract_in_place", "SandboxCore/array_math.h")
INCLUDE_ORDER = (
    "Sandbox/batch_game/",
    "Sandbox/",
    "SandboxCore/",
)


def soa_source_file(
    header_path: Path,
    source_nodes: tuple,
    namespace: str | None = None,
    source_path: Path | None = None,
    header_include_path: str | None = None,
) -> CppFile:
    definitions = (Namespace(namespace, source_nodes),) if namespace else source_nodes
    return CppFile(
        path=source_path or header_path.with_suffix(".cpp"),
        pragma_once=False,
        clang_format_off=True,
        include_order=INCLUDE_ORDER,
        nodes=(
            Include(header_include_path or header_path.name, system=False),
            NewLines(2),
            IncludeDependencies(),
            NewLines(2),
            *definitions,
        ),
    )


def homogeneous_soa_header_module(
    name: str, header_name: str, layouts: tuple[HomogeneousSoALayout, ...]
) -> Module:
    return Module(
        name=name,
        header=CppFile(
            path=SANDBOX_CORE_PUBLIC_DIR / header_name,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lower_homogeneous_soa_layouts(layouts),
            ),
        ),
    )


def soa_vectors_module() -> Module:
    return homogeneous_soa_header_module(
        "sandbox_core_soa_vectors",
        "soa_vectors.h",
        (
            HomogeneousSoALayout(
                "Vectors2",
                ("xs", "ys"),
                (
                    HomogeneousSoAValueType("float", "f", F_VECTOR_2F),
                    HomogeneousSoAValueType("double", "d", F_VECTOR_2D),
                    HomogeneousSoAValueType("int32", "i32", F_INT_POINT),
                    HomogeneousSoAValueType("uint32", "u32", F_UINT_POINT),
                ),
            ),
            HomogeneousSoALayout(
                "Vectors3",
                ("xs", "ys", "zs"),
                (
                    HomogeneousSoAValueType("float", "f", F_VECTOR_3F),
                    HomogeneousSoAValueType("double", "d", F_VECTOR_3D),
                    HomogeneousSoAValueType("int32", "i32", F_INT_VECTOR),
                    HomogeneousSoAValueType("uint32", "u32", F_UINT_VECTOR_3),
                ),
            ),
        ),
    )


def soa_rotators_module() -> Module:
    return homogeneous_soa_header_module(
        "sandbox_core_soa_rotators",
        "soa_rotators.h",
        (
            HomogeneousSoALayout(
                "Rotators",
                ("pitches", "yaws", "rolls"),
                (
                    HomogeneousSoAValueType("float", "f"),
                    HomogeneousSoAValueType("double", "d"),
                ),
            ),
        ),
    )


def countdown_timers_module() -> Module:
    tick = MemberFunctionSpec(
        "tick",
        "void",
        (FunctionParameter("float const", "dt"),),
        Raw("ml::subtract_in_place(remaining_times, dt);", (ARRAY_MATH,)),
        suffix=" noexcept",
    )
    countdown_timers = SoAStruct(
        SoAStructNames("FCountdownTimers"),
        (tarray_member("remaining_times", "float"),),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_CORE_API,
        nodes=(tick.header_node(),),
        source_nodes=(tick.definition_node("FCountdownTimers"),),
    )
    lowered = lower_soa_structs_with_source((countdown_timers,))
    header_path = SANDBOX_CORE_PUBLIC_DIR / "countdown_timers.h"
    source_path = (
        PROJECT_ROOT
        / "Plugins"
        / "SandboxCore"
        / "Source"
        / "SandboxCore"
        / "Private"
        / "countdown_timers.cpp"
    )
    return Module(
        name="sandbox_core_countdown_timers",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(
            header_path,
            lowered.source_nodes,
            source_path=source_path,
            header_include_path="SandboxCore/countdown_timers.h",
        ),
    )


def fighter_soa_module() -> Module:
    members = (
        tarray_member("entity_handles", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("integral_biases", "uint32"),
        tarray_member("float_biases", "float"),
        tarray_member("tasks", E_TEST_CAPITAL_SHIP_FIGHTERS_TASK),
        soa_member("locations", F_VECTORS_3F),
        soa_member("desired_move_locations", F_VECTORS_3F),
        soa_member("aim_directions", F_VECTORS_3F),
        soa_member("desired_aiming_directions", F_VECTORS_3F),
        soa_member("movement_directions", F_VECTORS_3F),
        soa_member("velocities", F_VECTORS_3F),
        tarray_member("move_distances", "float"),
        tarray_member("speeds", "float"),
        tarray_member("teams", E_TEST_TEAM),
        tarray_member("healths", "int32"),
        soa_member("awareness_scan_countdowns", F_TICK_COUNTDOWN_8),
        soa_member("attack_reposition_countdowns", F_TICK_COUNTDOWN_16),
        soa_member("attack_cooldowns", F_TICK_COUNTDOWN_16),
        tarray_member("target_handles", F_REGISTRY_ENTITY_HANDLE),
        soa_member("target_locations", F_VECTORS_3F),
        soa_member("target_velocities", F_VECTORS_3F),
        soa_member("target_directions", F_VECTORS_3F),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
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
            tarray_member("target_handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("locations", F_VECTORS_3F),
            soa_member("rotations", F_ROTATORS_F),
            tarray_member("teams", E_TEST_TEAM),
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
            soa_member("fighter_queue", TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            tarray_member("handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("locations", F_VECTORS_3F),
            soa_member("rotations", F_ROTATORS_F),
            soa_member("fighter_spawn_timers", F_COUNTDOWN_TIMERS),
            tarray_member("fighter_spawn_cooldowns", "float"),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("healths", "int32"),
            tarray_member("capital_fighter_handle_spans", F_INDEX_SPAN),
            tarray_member("target_handles", F_REGISTRY_ENTITY_HANDLE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    fighter_reassignment_members = (
        tarray_member("capital_handles", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("fighter_handles", F_REGISTRY_ENTITY_HANDLE),
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
                    FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "ch"),
                    FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "fh"),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace(
                    "ml::test_capital_ships",
                    lowered.header_nodes,
                ),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::test_capital_ships"
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
        (FunctionParameter(qualified_type(F_LINEAR_COLOR, " const"), "value"),),
        Raw(""),
    )
    spawn_requests = SoAStruct(
        SoAStructNames("SpawnRequests"),
        (
            soa_member("locations", F_VECTORS_3F),
            soa_member("rotations", F_ROTATORS_F),
            soa_member("base_velocities", F_VECTORS_3F),
            tarray_member("damages", "int32"),
            tarray_member("speeds", "float"),
            tarray_member("max_distances", "float"),
            tarray_member("instigator_handles", F_REGISTRY_ENTITY_HANDLE),
            tarray_member("colours", F_LINEAR_COLOR),
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
            tarray_member("ismc_data", F_INSTANCED_STATIC_MESH_INSTANCE_DATA),
            tarray_member("colours", F_LINEAR_COLOR),
            soa_member("locations", F_VECTORS_3F),
            soa_member("rotations", F_ROTATORS_F),
            soa_member("velocities", F_VECTORS_3F),
            tarray_member("damages", "int32"),
            tarray_member("lifetimes_remaining", "float"),
            tarray_member("instigator_handles", F_REGISTRY_ENTITY_HANDLE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    hit_details = SoAStruct(
        SoAStructNames("HitDetails"),
        (
            soa_member("locations", F_VECTORS_3F),
            tarray_member("colours", F_LINEAR_COLOR),
        ),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace(
                    "ml::test_lasers",
                    lowered.header_nodes,
                ),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::test_lasers"),
    )


def laser_collision_data_soa_module() -> Module:
    component_hit_ranges = SoAStruct(
        SoAStructNames("ComponentHitRanges"),
        (
            tarray_member("components", U_PRIMITIVE_COMPONENT_CONST_PTR),
            tarray_member("counts", "int32"),
            tarray_member("offsets", "int32"),
            tarray_member("next_write_indices", "int32"),
        ),
        storage_operations=(SoAStorageOperation.RESET,),
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((component_hit_ranges,))
    header_path = BATCH_GAME_DIR / "TestLaserCollisionDataSoA.h"
    return Module(
        name="test_laser_collision_data_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
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
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
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
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
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
            soa_member("locations", F_VECTORS_3F),
            soa_member("rotations", F_ROTATORS_F),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("targets", F_REGISTRY_ENTITY_HANDLE),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def fighter_order_queue_module() -> Module:
    order_queue_members = (
        tarray_member("handles", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("orders", TEST_CAPITAL_SHIP_FIGHTER_ORDER),
        tarray_member("tasks", E_TEST_CAPITAL_SHIP_FIGHTERS_TASK),
        tarray_member("targets", F_REGISTRY_ENTITY_HANDLE),
    )
    order_queue = SoAStruct(
        SoAStructNames("TestCapitalShipFighterOrderQueue"),
        order_queue_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            UsingDeclaration("Task", E_TEST_CAPITAL_SHIP_FIGHTERS_TASK),
            NewLines(1),
            UsingDeclaration("Order", TEST_CAPITAL_SHIP_FIGHTER_ORDER),
            NewLines(2),
            MemberFunctionSpec(
                "add",
                "void",
                (
                    FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "handle"),
                    FunctionParameter("Order const", "order"),
                    FunctionParameter("Task const", "task"),
                    FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "target"),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def entity_death_info_module() -> Module:
    entity_death_info_members = (
        tarray_member("reasons", E_TEST_DEATH_REASON),
        tarray_member("victims", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("killers", F_REGISTRY_ENTITY_HANDLE),
    )
    add_function = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter(qualified_type(E_TEST_DEATH_REASON, " const"), "reason"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "victim"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "killer"),
        ),
        ForEachSoAMemberCall(entity_death_info_members, "Add"),
    )
    add_without_killer = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter(qualified_type(E_TEST_DEATH_REASON, " const"), "reason"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "victim"),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(
            header_path,
            (
                *lowered.source_nodes,
                NewLines(2),
                add_function.definition_node("EntityDeathInfo"),
            ),
        ),
    )


def static_turrets_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            tarray_member("handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("locations", F_VECTORS_3F),
            tarray_member("teams", E_TEST_TEAM),
            soa_member("laser_cooldowns", F_TICK_COUNTDOWN_16),
            tarray_member("target_handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("target_locations", F_VECTORS_3F),
            soa_member("target_velocities", F_VECTORS_3F),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::test_static_turrets", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::test_static_turrets"
        ),
    )


def tube_spinners_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            tarray_member("handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("locations", F_VECTORS_3F),
            tarray_member("yaws", "float"),
            soa_member("laser_cooldowns", F_TICK_COUNTDOWN_16),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::test_tube_spinners", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::test_tube_spinners"
        ),
    )


def direct_damage_events_soa_module() -> Module:
    events = SoAStruct(
        SoAStructNames("DirectDamageEvents"),
        (
            tarray_member("damaged_entities", F_REGISTRY_ENTITY_HANDLE),
            tarray_member("damage_amounts", "int32"),
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
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
            tarray_member("registry_indices", nested_type(F_REGISTRY_ENTITY_HANDLE, "index_type")),
            tarray_member(
                "registry_generations", nested_type(F_REGISTRY_ENTITY_HANDLE, "generation_type")
            ),
            tarray_member("entity_types", E_TEST_ENTITY_TYPE),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("kills", "uint32"),
            tarray_member("alive", "uint8"),
            tarray_member("killed_by", TEST_ENTITY_UNIQUE_ID),
            tarray_member("death_reason", E_TEST_DEATH_REASON),
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
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def modules() -> tuple[Module, ...]:
    return (
        countdown_timers_module(),
        soa_vectors_module(),
        soa_rotators_module(),
        fighter_soa_module(),
        capital_ships_soa_module(),
        lasers_soa_module(),
        laser_collision_data_soa_module(),
        collision_damage_events_soa_module(),
        fighter_spawn_queue_soa_module(),
        fighter_order_queue_module(),
        static_turrets_soa_module(),
        tube_spinners_soa_module(),
        direct_damage_events_soa_module(),
        unique_entity_data_soa_module(),
        entity_death_info_module(),
    )
