from __future__ import annotations

from pathlib import Path

from Codegen.cpp import (
    CppFile,
    ForwardDeclaration,
    FunctionParameter,
    Include,
    IncludeDependencies,
    Module,
    Namespace,
    NewLines,
    Raw,
)
from Codegen.project_types import (
    F_REGISTRY_ENTITY_HANDLE,
    TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE,
    nested_type,
    qualified_type,
)
from Codegen.facade import Facade, FacadeMethod, lower_facade, lower_facade_with_source

from Codegen.manifests.common import (
    CHECK,
    INCLUDE_ORDER,
    SPACE_GAME_CAPITAL_SHIPS_DIR,
    SPACE_GAME_FIGHTERS_DIR,
    SPACE_GAME_LASERS_DIR,
    SPACE_GAME_PRIVATE_DIR,
    SPACE_GAME_PUBLIC_DIR,
    SPACE_GAME_SPINNERS_DIR,
    SPACE_GAME_TURRETS_DIR,
    SPACEGAME_API,
    SPAWNED_ENTITY_HANDLES,
    TEST_CAPITAL_SHIP_FIGHTERS,
    TEST_CAPITAL_SHIP_FIGHTER_ORDER_QUEUE,
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
        export_specifier=SPACEGAME_API,
    )
    header_path = SPACE_GAME_FIGHTERS_DIR / "TestCapitalShipFightersCommandInterface.h"
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
    header_directory: Path,
    namespace: str,
    methods: tuple[FacadeMethod, ...],
) -> Module:
    facade = Facade(
        "PhaseInterface",
        actor_name,
        "actor",
        methods,
        export_specifier=SPACEGAME_API,
        bind_access="private",
        method_access="private",
        friends=(actor_name, "ATestBatchOrchestrator"),
        definitions_in_source=True,
    )
    lowered = lower_facade_with_source(facade)
    header_path = header_directory / f"{actor_name.removeprefix('A')}PhaseInterface.h"
    relative_header_path = header_path.relative_to(SPACE_GAME_PUBLIC_DIR)
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
            path=SPACE_GAME_PRIVATE_DIR / relative_header_path.with_suffix(".cpp"),
            pragma_once=False,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                Include(f"SpaceGame/{relative_header_path.as_posix()}", system=False),
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
            "SpaceGame/ships/player/TestSpaceShip.h",
            SPACE_GAME_PUBLIC_DIR / "ships" / "player",
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
            "SpaceGame/combat/lasers/TestLasers.h",
            SPACE_GAME_LASERS_DIR,
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
            "SpaceGame/ships/capital/TestCapitalShips.h",
            SPACE_GAME_CAPITAL_SHIPS_DIR,
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
            "SpaceGame/ships/fighters/TestCapitalShipFighters.h",
            SPACE_GAME_FIGHTERS_DIR,
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
            "SpaceGame/defences/turrets/TestStaticTurrets.h",
            SPACE_GAME_TURRETS_DIR,
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
            "SpaceGame/defences/spinners/TestTubeSpinners.h",
            SPACE_GAME_SPINNERS_DIR,
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
