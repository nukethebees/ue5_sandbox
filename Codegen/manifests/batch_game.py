from __future__ import annotations

from Codegen.cpp import (
    CppFile,
    FunctionParameter,
    IncludeDependencies,
    MemberFunctionSpec,
    Module,
    Namespace,
    NewLines,
    Raw,
    UsingDeclaration,
)
from Codegen.project_types import (
    E_TEST_CAPITAL_SHIP_FIGHTERS_TASK,
    E_TEST_TEAM,
    F_COUNTDOWN_TIMERS,
    F_INDEX_SPAN,
    F_INSTANCED_STATIC_MESH_INSTANCE_DATA,
    F_LINEAR_COLOR,
    F_PERIODIC_TICK_COUNTDOWN_16,
    F_REGISTRY_ENTITY_HANDLE,
    F_ROTATORS_F,
    F_TICK_COUNTDOWN_8,
    F_TICK_COUNTDOWN_16,
    F_VECTORS_3F,
    TEST_CAPITAL_SHIP_FIGHTER_ORDER,
    TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE,
    U_PRIMITIVE_COMPONENT_CONST_PTR,
    qualified_type,
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

from Codegen.manifests.common import (
    ALL_STORAGE_OPERATIONS,
    INCLUDE_ORDER,
    SPACE_GAME_CAPITAL_SHIPS_DIR,
    SPACE_GAME_FIGHTERS_DIR,
    SPACE_GAME_LASERS_DIR,
    SPACE_GAME_SPINNERS_DIR,
    SPACE_GAME_TURRETS_DIR,
    SPACEGAME_API,
    soa_source_file,
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
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = SPACE_GAME_FIGHTERS_DIR / "TestCapitalShipFightersSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
    )
    entity_tick_data = SoAStruct(
        SoAStructNames("EntityTickData"),
        (
            tarray_member("ships_ready_to_spawn_fighters_buffer", "int32"),
            soa_member("fighter_queue", TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SPACEGAME_API,
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
        storage_export_specifier=SPACEGAME_API,
    )
    fighter_reassignment_members = (
        tarray_member("capital_handles", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("fighter_handles", F_REGISTRY_ENTITY_HANDLE),
    )
    fighter_reassignment = SoAStruct(
        SoAStructNames("FighterReassignment"),
        fighter_reassignment_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SPACEGAME_API,
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
    header_path = SPACE_GAME_CAPITAL_SHIPS_DIR / "TestCapitalShipsSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
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
        storage_export_specifier=SPACEGAME_API,
    )
    hit_details = SoAStruct(
        SoAStructNames("HitDetails"),
        (
            soa_member("locations", F_VECTORS_3F),
            tarray_member("colours", F_LINEAR_COLOR),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((spawn_requests, entities, hit_details))
    header_path = SPACE_GAME_LASERS_DIR / "TestLasersSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((component_hit_ranges,))
    header_path = SPACE_GAME_LASERS_DIR / "TestLaserCollisionDataSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((queue,))
    header_path = SPACE_GAME_FIGHTERS_DIR / "TestCapitalShipFighterSpawnQueueSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
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
    header_path = SPACE_GAME_FIGHTERS_DIR / "TestCapitalShipFighterOrderQueue.h"
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


def static_turrets_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            tarray_member("handles", F_REGISTRY_ENTITY_HANDLE),
            tarray_member("integral_biases", "uint32"),
            soa_member("locations", F_VECTORS_3F),
            soa_member("fire_point_locations", F_VECTORS_3F),
            tarray_member("teams", E_TEST_TEAM),
            soa_member("laser_cooldowns", F_TICK_COUNTDOWN_16),
            tarray_member("laser_damages", "int32"),
            soa_member("target_refresh_countdowns", F_PERIODIC_TICK_COUNTDOWN_16),
            tarray_member("target_handles", F_REGISTRY_ENTITY_HANDLE),
            soa_member("target_locations", F_VECTORS_3F),
            soa_member("target_velocities", F_VECTORS_3F),
            tarray_member("healths", "int32"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = SPACE_GAME_TURRETS_DIR / "TestStaticTurretsSoA.h"
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
        storage_export_specifier=SPACEGAME_API,
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = SPACE_GAME_SPINNERS_DIR / "TestTubeSpinnersSoA.h"
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
