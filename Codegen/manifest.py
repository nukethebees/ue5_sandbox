from __future__ import annotations

from pathlib import Path

from Codegen.nodes import (
    CppFile,
    ForwardDeclaration,
    FunctionParameter,
    Include,
    IncludeDependencies,
    Member,
    MemberFunctionSpec,
    Module,
    Namespace,
    NewLines,
    Raw,
    Struct,
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
    F_PERIODIC_TICK_COUNTDOWN_16,
    F_REGISTRY_ENTITY_HANDLE,
    F_REGISTRY_ENTITY_HANDLE_ARRAY,
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
from Codegen.facade import Facade, FacadeMethod, lower_facade, lower_facade_with_source
from Codegen.soa import (
    FixedSoAArray,
    ForEachSoAMemberCall,
    HomogeneousSoALayout,
    HomogeneousSoAValueType,
    SoAStruct,
    SoAStructNames,
    SoAStorageOperation,
    lower_homogeneous_soa_permutation_definitions,
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
SANDBOX_CORE_PRIVATE_DIR = (
    PROJECT_ROOT
    / "Plugins"
    / "SandboxCore"
    / "Source"
    / "SandboxCore"
    / "Private"
)
SANDBOX_CORE_TEST_PRIVATE_DIR = (
    PROJECT_ROOT
    / "Plugins"
    / "SandboxCore"
    / "Tests"
    / "SandboxCoreTests"
    / "Private"
)
ALL_STORAGE_OPERATIONS = tuple(SoAStorageOperation)
SANDBOX_API = "SANDBOX_API"
SANDBOX_CORE_API = "SANDBOXCORE_API"
ARRAY_MATH = TypeDependency("ml::subtract_in_place", "SandboxCore/array_math.h")
ARRAY_FILL = TypeDependency("ml::fill", "SandboxCore/array_utils.h")
SOA_VECTOR_FILL = TypeDependency("ml::fill", "SandboxCore/soa_vector_utils.h")
CHECK = TypeDependency("check", "CoreMinimal.h")
TEST_CAPITAL_SHIP_FIGHTERS = TypeDependency(
    "ATestCapitalShipFighters", "Sandbox/batch_game/TestCapitalShipFighters.h"
)
TEST_CAPITAL_SHIP_FIGHTER_ORDER_QUEUE = TypeDependency(
    "TestCapitalShipFighterOrderQueue",
    "Sandbox/batch_game/TestCapitalShipFighterOrderQueue.h",
)
SPAWNED_ENTITY_HANDLES = TypeDependency(
    "SpawnedEntityHandles",
    "Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h",
)
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


def homogeneous_soa_module(
    name: str, header_name: str, layouts: tuple[HomogeneousSoALayout, ...]
) -> Module:
    header_path = SANDBOX_CORE_PUBLIC_DIR / header_name
    return Module(
        name=name,
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lower_homogeneous_soa_layouts(layouts),
            ),
        ),
        source=soa_source_file(
            header_path,
            lower_homogeneous_soa_permutation_definitions(layouts),
            source_path=SANDBOX_CORE_PRIVATE_DIR / header_path.with_suffix(".cpp").name,
            header_include_path=f"SandboxCore/{header_name}",
        ),
    )


def soa_vectors_module() -> tuple[Module, ...]:
    vector_specs = (
        ("FVectors2f", "soa_vectors_2f.h", "float", ("xs", "ys"), F_VECTOR_2F),
        ("FVectors2d", "soa_vectors_2d.h", "double", ("xs", "ys"), F_VECTOR_2D),
        ("FVectors2i32", "soa_vectors_2i32.h", "int32", ("xs", "ys"), F_INT_POINT),
        ("FVectors2u32", "soa_vectors_2u32.h", "uint32", ("xs", "ys"), F_UINT_POINT),
        ("FVectors3f", "soa_vectors_3f.h", "float", ("xs", "ys", "zs"), F_VECTOR_3F),
        ("FVectors3d", "soa_vectors_3d.h", "double", ("xs", "ys", "zs"), F_VECTOR_3D),
        ("FVectors3i32", "soa_vectors_3i32.h", "int32", ("xs", "ys", "zs"), F_INT_VECTOR),
        ("FVectors3u32", "soa_vectors_3u32.h", "uint32", ("xs", "ys", "zs"), F_UINT_VECTOR_3),
    )
    return tuple(vector_soa_module(*spec) for spec in vector_specs)


def vector_soa_module(
    name: str, header_name: str, value_type: str, components: tuple[str, ...], equivalent_type
) -> Module:
    component_parameters = tuple(
        FunctionParameter("value_type const", component[0]) for component in components
    )
    add_components = MemberFunctionSpec(
        "add",
        "auto",
        component_parameters,
        Raw(
            "\n".join(
                (
                    f"auto const index{{{components[0]}.Add({components[0][0]})}};",
                    *(f"{component}.Add({component[0]});" for component in components[1:]),
                    "return index;",
                )
            )
        ),
        suffix=" -> size_type",
        is_inline=True,
    )
    add_equivalent = MemberFunctionSpec(
        "add",
        "auto",
        (FunctionParameter(qualified_type(equivalent_type, " const&"), "value"),),
        Raw(
            f"return add({', '.join(f'value.{axis}' for axis in ('X', 'Y', 'Z')[:len(components)])});"
        ),
        suffix=" -> size_type",
        is_inline=True,
    )
    get_data = MemberFunctionSpec(
        "get_data",
        "auto",
        (),
        Raw(f"return Data{{{', '.join(f'{component}.GetData()' for component in components)}}};"),
        suffix=" -> Data",
        is_inline=True,
    )
    get_const_data = MemberFunctionSpec(
        "get_data",
        "auto",
        (),
        Raw(f"return ConstData{{{', '.join(f'{component}.GetData()' for component in components)}}};"),
        suffix=" const -> ConstData",
        is_inline=True,
    )
    empty = MemberFunctionSpec(
        "empty",
        "void",
        (),
        Raw("\n".join(f"{component}.Empty();" for component in components)),
        is_inline=True,
    )
    set_num_uninitialised = MemberFunctionSpec(
        "set_num_uninitialised",
        "void",
        (FunctionParameter("size_type const", "count"),),
        Raw("\n".join(f"{component}.SetNumUninitialized(count);" for component in components)),
        is_inline=True,
    )
    add_zeroed = MemberFunctionSpec(
        "add_zeroed",
        "void",
        (FunctionParameter("size_type const", "count"),),
        Raw("\n".join(f"{component}.AddZeroed(count);" for component in components)),
        is_inline=True,
    )
    vectors = SoAStruct(
        SoAStructNames(name),
        tuple(tarray_member(component, value_type) for component in components),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_CORE_API,
        nodes=(
            UsingDeclaration("value_type", value_type),
            NewLines(1),
            UsingDeclaration("size_type", "TArray<value_type>::SizeType"),
            NewLines(2),
            Struct("Data", tuple(Member("value_type*", component) for component in components)),
            NewLines(2),
            Struct("ConstData", tuple(Member("value_type const*", component) for component in components)),
            NewLines(2),
            get_data.header_node(),
            NewLines(1),
            get_const_data.header_node(),
            NewLines(2),
            add_components.header_node(),
            NewLines(1),
            add_equivalent.header_node(),
            NewLines(2),
            empty.header_node(),
            NewLines(1),
            set_num_uninitialised.header_node(),
            NewLines(1),
            add_zeroed.header_node(),
        ),
        equivalent_type=equivalent_type,
        copy_element_memberwise=True,
        fixed_storage_name=("TVectors3fFixedStorage" if name == "FVectors3f" else None),
        fixed_arrays=(
            (FixedSoAArray("TFixedVectors3f"),)
            if name == "FVectors3f"
            else ()
        ),
    )
    lowered = lower_soa_structs_with_source((vectors,))
    header_path = SANDBOX_CORE_PUBLIC_DIR / header_name
    return Module(
        name=header_name.removesuffix(".h"),
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(IncludeDependencies(), NewLines(2), *lowered.header_nodes),
        ),
        source=soa_source_file(
            header_path,
            lowered.source_nodes,
            source_path=SANDBOX_CORE_PRIVATE_DIR / header_path.with_suffix(".cpp").name,
            header_include_path=f"SandboxCore/{header_name}",
        ),
    )


def soa_vectors_umbrella_module() -> Module:
    headers = (
        "soa_vectors_2f.h",
        "soa_vectors_2d.h",
        "soa_vectors_2i32.h",
        "soa_vectors_2u32.h",
        "soa_vectors_3f.h",
        "soa_vectors_3d.h",
        "soa_vectors_3i32.h",
        "soa_vectors_3u32.h",
    )
    return Module(
        name="sandbox_core_soa_vectors",
        header=CppFile(
            path=SANDBOX_CORE_PUBLIC_DIR / "soa_vectors.h",
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=tuple(Include(f"SandboxCore/{header}", system=False) for header in headers),
        ),
    )


def soa_rotators_module() -> Module:
    return homogeneous_soa_module(
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
                storage_export_specifier=SANDBOX_CORE_API,
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


def entity_registry_data_soa_module() -> Module:
    add_disabled = MemberFunctionSpec(
        "add_disabled",
        "void",
        (FunctionParameter("int32 const", "count"),),
        Raw(
            "add_uninitialised(count);\n"
            "auto view{get_view()};\n"
            "auto slice{view.right(count)};\n\n"
            "ml::fill(slice.locations, 0.f);\n"
            "ml::fill(slice.velocities, 0.f);\n"
            "ml::fill(slice.radii, 0.f);\n"
            "ml::fill(slice.healths, 0);\n"
            "ml::fill(slice.teams, ETestTeam::White);\n"
            "ml::fill(slice.entity_types, ETestEntityType::COUNT);\n"
            "ml::fill(slice.alive, uint8{0u});",
            (ARRAY_FILL, SOA_VECTOR_FILL),
        ),
    )
    set_all_alive = MemberFunctionSpec(
        "set_all_alive",
        "void",
        (),
        Raw("ml::fill(alive, uint8{1});", (ARRAY_FILL,)),
    )
    set_all_velocities = MemberFunctionSpec(
        "set_all_velocities",
        "void",
        (FunctionParameter("float const", "value"),),
        Raw("ml::fill(velocities, value);", (SOA_VECTOR_FILL,)),
    )
    set_all_entity_types = MemberFunctionSpec(
        "set_all_entity_types",
        "void",
        (FunctionParameter(qualified_type(E_TEST_ENTITY_TYPE, " const"), "value"),),
        Raw("ml::fill(entity_types, value);", (ARRAY_FILL,)),
    )
    custom_functions = (
        add_disabled,
        set_all_alive,
        set_all_velocities,
        set_all_entity_types,
    )
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            soa_member("locations", F_VECTORS_3F),
            soa_member("velocities", F_VECTORS_3F),
            tarray_member("radii", "float"),
            tarray_member("healths", "int32"),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("entity_types", E_TEST_ENTITY_TYPE),
            tarray_member("alive", "uint8"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        nodes=tuple(function.header_node() for function in custom_functions),
        source_nodes=tuple(
            function.definition_node("EntityData") for function in custom_functions
        ),
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "TestEntityRegistryData.h"
    return Module(
        name="test_entity_registry_data_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::entity_registry", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::entity_registry"),
    )


def capital_ship_fighters_command_interface_module() -> Module:
    facade = Facade(
        "CommandInterface",
        TEST_CAPITAL_SHIP_FIGHTERS,
        "fighters",
        (
            FacadeMethod(
                "queue_spawns",
                "void",
                (
                    FunctionParameter(
                        qualified_type(TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE, " const&"),
                        "queue",
                    ),
                ),
            ),
            FacadeMethod(
                "queue_orders",
                "void",
                (
                    FunctionParameter(
                        qualified_type(TEST_CAPITAL_SHIP_FIGHTER_ORDER_QUEUE, " const&"),
                        "queue",
                    ),
                ),
            ),
            FacadeMethod(
                "self_destruct_fighter",
                "void",
                (FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "handle"),),
            ),
            FacadeMethod(
                "get_new_spawn_entity_data",
                qualified_type(nested_type(TEST_CAPITAL_SHIP_FIGHTERS, "RegistryEntityData"), " const&"),
                (),
                suffix=" const",
            ),
            FacadeMethod(
                "get_new_spawn_entity_handles",
                qualified_type(SPAWNED_ENTITY_HANDLES, " const&"),
                (),
                suffix=" const",
            ),
            FacadeMethod("get_num_instances", "int32", (), suffix=" const noexcept"),
            FacadeMethod(
                "get_target_handle",
                F_REGISTRY_ENTITY_HANDLE,
                (FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "fighter_handle"),),
                suffix=" const noexcept",
            ),
        ),
        validation=Raw("check(IsValid(fighters));", (CHECK,)),
        export_specifier=SANDBOX_API,
    )
    header_path = BATCH_GAME_DIR / "TestCapitalShipFightersCommandInterface.h"
    return Module(
        name="test_capital_ship_fighters_command_interface",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::test_capital_ship_fighters", (lower_facade(facade),)),
            ),
        ),
    )


def phase_facade_module(
    module_name: str,
    actor_name: str,
    actor_header: str,
    namespace: str,
    methods: tuple[FacadeMethod, ...],
) -> Module:
    facade = Facade(
        "PhaseInterface",
        actor_name,
        "actor",
        methods,
        export_specifier=SANDBOX_API,
        bind_access="private",
        method_access="private",
        friends=(actor_name, "ATestBatchOrchestrator"),
        definitions_in_source=True,
    )
    lowered = lower_facade_with_source(facade)
    header_path = BATCH_GAME_DIR / f"{actor_name.removeprefix('A')}PhaseInterface.h"
    return Module(
        name=module_name,
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                Include("CoreMinimal.h", system=False),
                NewLines(2),
                ForwardDeclaration(actor_name, "class"),
                ForwardDeclaration("ATestBatchOrchestrator", "class"),
                NewLines(2),
                Namespace(namespace, (lowered.header,)),
            ),
        ),
        source=CppFile(
            path=header_path.with_suffix(".cpp"),
            pragma_once=False,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                Include(header_path.name, system=False),
                NewLines(2),
                Include(actor_header, system=False),
                NewLines(2),
                IncludeDependencies(),
                NewLines(2),
                Namespace(namespace, lowered.source_nodes),
            ),
        ),
    )


def phase_facade_modules() -> tuple[Module, ...]:
    tick_time = FunctionParameter("float const", "dt")
    return (
        phase_facade_module(
            "test_space_ship_phase_interface",
            "ATestSpaceShip",
            "Sandbox/batch_game/TestSpaceShip.h",
            "ml::test_space_ship",
            (
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("update_timers", "void", (tick_time,)),
                FacadeMethod("move", "void", (tick_time,)),
                FacadeMethod("queue_commands", "void", ()),
                FacadeMethod("resolve_damage_events", "void", ()),
                FacadeMethod("update_entity_registry", "void", ()),
                FacadeMethod("sync_from_registry", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
        phase_facade_module(
            "test_lasers_phase_interface",
            "ATestLasers",
            "Sandbox/batch_game/TestLasers.h",
            "ml::test_lasers",
            (
                FacadeMethod("clear_runtime_state", "void", ()),
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("simulate", "void", (tick_time,)),
                FacadeMethod("commit_spawns", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
        phase_facade_module(
            "test_capital_ships_phase_interface",
            "ATestCapitalShips",
            "Sandbox/batch_game/TestCapitalShips.h",
            "ml::test_capital_ships",
            (
                FacadeMethod("clear_runtime_state", "void", ()),
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("update_timers", "void", (tick_time,)),
                FacadeMethod("make_decisions", "void", ()),
                FacadeMethod("resolve_damage_events", "void", ()),
                FacadeMethod("update_entity_registry", "void", ()),
                FacadeMethod("sync_from_registry", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
        phase_facade_module(
            "test_capital_ship_fighters_phase_interface",
            "ATestCapitalShipFighters",
            "Sandbox/batch_game/TestCapitalShipFighters.h",
            "ml::test_capital_ship_fighters",
            (
                FacadeMethod("clear_runtime_state", "void", ()),
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("update_timers", "void", (tick_time,)),
                FacadeMethod("make_decisions", "void", ()),
                FacadeMethod("move", "void", (tick_time,)),
                FacadeMethod("queue_commands", "void", ()),
                FacadeMethod("resolve_damage_events", "void", ()),
                FacadeMethod("update_entity_registry", "void", ()),
                FacadeMethod("sync_from_registry", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
        phase_facade_module(
            "test_static_turrets_phase_interface",
            "ATestStaticTurrets",
            "Sandbox/batch_game/TestStaticTurrets.h",
            "ml::test_static_turrets",
            (
                FacadeMethod("clear_runtime_state", "void", ()),
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("update_timers", "void", (tick_time,)),
                FacadeMethod("make_decisions", "void", ()),
                FacadeMethod("queue_commands", "void", ()),
                FacadeMethod("resolve_damage_events", "void", ()),
                FacadeMethod("update_entity_registry", "void", ()),
                FacadeMethod("sync_from_registry", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
        phase_facade_module(
            "test_tube_spinners_phase_interface",
            "ATestTubeSpinners",
            "Sandbox/batch_game/TestTubeSpinners.h",
            "ml::test_tube_spinners",
            (
                FacadeMethod("clear_runtime_state", "void", ()),
                FacadeMethod("begin_play", "void", ()),
                FacadeMethod("begin_tick", "void", ()),
                FacadeMethod("update_timers", "void", (tick_time,)),
                FacadeMethod("move", "void", (tick_time,)),
                FacadeMethod("queue_commands", "void", ()),
                FacadeMethod("update_entity_registry", "void", ()),
                FacadeMethod("update_visual_data", "void", ()),
                FacadeMethod("commit_visual_data", "void", ()),
                FacadeMethod("end_tick", "void", ()),
            ),
        ),
    )


def registry_entity_handles_soa_module() -> Module:
    add = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter("int32 const", "index"),
            FunctionParameter("int32 const", "generation"),
        ),
        Raw("registry_indices.Add(index);\ngenerations.Add(generation);"),
    )
    append_to = MemberFunctionSpec(
        "append_to",
        "void",
        (FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE_ARRAY, "&"), "out"),),
        Raw(
            "auto const count{num()};\n"
            "out.Reserve(out.Num() + count);\n"
            "for (int32 i{0}; i < count; ++i) {\n"
            "    out.Emplace(registry_indices[i], generations[i]);\n"
            "}"
        ),
        suffix=" const",
    )
    to_array = MemberFunctionSpec(
        "to_array",
        F_REGISTRY_ENTITY_HANDLE_ARRAY,
        (),
        Raw(
            "TArray<FRegistryEntityHandle> out;\n"
            "append_to(out);\n"
            "return out;"
        ),
        suffix=" const",
    )
    handles = SoAStruct(
        SoAStructNames("FRegistryEntityHandles"),
        (
            tarray_member("registry_indices", "int32"),
            tarray_member("generations", "int32"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            add.header_node(),
            NewLines(1),
            append_to.header_node(),
            NewLines(1),
            to_array.header_node(),
        ),
        source_nodes=(
            add.definition_node("FRegistryEntityHandles"),
            NewLines(2),
            append_to.definition_node("FRegistryEntityHandles"),
            NewLines(2),
            to_array.definition_node("FRegistryEntityHandles"),
        ),
        equivalent_type=F_REGISTRY_ENTITY_HANDLE,
    )
    lowered = lower_soa_structs_with_source((handles,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "RegistryEntityHandles.h"
    return Module(
        name="registry_entity_handles_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(IncludeDependencies(), NewLines(2), *lowered.header_nodes),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def fixed_soa_test_types_module() -> Module:
    string_type = TypeDependency("FString", "CoreMinimal.h")
    shared_ptr_type = TypeDependency("TSharedPtr<int32>", "Templates/SharedPointer.h")
    child = SoAStruct(
        SoAStructNames("FTestFixedChild"),
        (
            tarray_member("names", string_type),
            tarray_member("references", shared_ptr_type),
        ),
        fixed_storage_name="TTestFixedChildStorage",
    )
    rows = SoAStruct(
        SoAStructNames("FTestFixedRows"),
        (
            soa_member("children", "FTestFixedChild", fixed_schema=child),
            tarray_member("ids", "int32"),
        ),
        fixed_storage_name="TTestFixedRowsStorage",
        fixed_arrays=(
            FixedSoAArray("TTestFixedRowsArray"),
            FixedSoAArray("TTestFixedRowsArrayAlternate"),
        ),
    )
    lowered = lower_soa_structs_with_source((child, rows))
    header_path = SANDBOX_CORE_TEST_PRIVATE_DIR / "fixed_soa_test_types.h"
    return Module(
        name="fixed_soa_test_types",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::fixed_soa_tests", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::fixed_soa_tests"
        ),
    )


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
