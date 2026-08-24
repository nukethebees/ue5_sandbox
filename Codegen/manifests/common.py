from __future__ import annotations

from pathlib import Path

from Codegen.cpp import (
    CppFile,
    Include,
    IncludeDependencies,
    Module,
    Namespace,
    NewLines,
    Node,
    TypeDependency,
)
from Codegen.soa import (
    HomogeneousSoALayout,
    SoAStorageOperation,
    lower_homogeneous_soa_permutation_definitions,
    lower_homogeneous_soa_layouts,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
SPACE_GAME_PUBLIC_DIR = (
    PROJECT_ROOT
    / "Plugins"
    / "SpaceGame"
    / "Source"
    / "SpaceGame"
    / "Public"
    / "SpaceGame"
)
SPACE_GAME_PRIVATE_DIR = (
    PROJECT_ROOT / "Plugins" / "SpaceGame" / "Source" / "SpaceGame" / "Private"
)
SPACE_GAME_ENTITIES_DIR = SPACE_GAME_PUBLIC_DIR / "entities"
SPACE_GAME_FIGHTERS_DIR = SPACE_GAME_PUBLIC_DIR / "ships" / "fighters"
SPACE_GAME_CAPITAL_SHIPS_DIR = SPACE_GAME_PUBLIC_DIR / "ships" / "capital"
SPACE_GAME_LASERS_DIR = SPACE_GAME_PUBLIC_DIR / "combat" / "lasers"
SPACE_GAME_TURRETS_DIR = SPACE_GAME_PUBLIC_DIR / "defences" / "turrets"
SPACE_GAME_SPINNERS_DIR = SPACE_GAME_PUBLIC_DIR / "defences" / "spinners"
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
SPACEGAME_API = "SPACEGAME_API"
SANDBOX_CORE_API = "SANDBOXCORE_API"
ARRAY_MATH = TypeDependency("ml::subtract_in_place", "SandboxCore/array_math.h")
ARRAY_FILL = TypeDependency("ml::fill", "SandboxCore/array_utils.h")
SOA_VECTOR_FILL = TypeDependency("ml::fill", "SandboxCore/soa_vector_utils.h")
CHECK = TypeDependency("check", "CoreMinimal.h")
TEST_CAPITAL_SHIP_FIGHTERS = TypeDependency(
    "ATestCapitalShipFighters", "SpaceGame/ships/fighters/TestCapitalShipFighters.h"
)
TEST_CAPITAL_SHIP_FIGHTER_ORDER_QUEUE = TypeDependency(
    "TestCapitalShipFighterOrderQueue",
    "SpaceGame/ships/fighters/TestCapitalShipFighterOrderQueue.h",
)
SPAWNED_ENTITY_HANDLES = TypeDependency(
    "SpawnedEntityHandles",
    "SpaceGame/entities/TestEntityRegistry.h",
)
INCLUDE_ORDER = (
    "SpaceGame/",
    "Sandbox/",
    "SandboxCore/",
)

__all__ = (
    "PROJECT_ROOT",
    "SPACE_GAME_PUBLIC_DIR",
    "SPACE_GAME_PRIVATE_DIR",
    "SPACE_GAME_ENTITIES_DIR",
    "SPACE_GAME_FIGHTERS_DIR",
    "SPACE_GAME_CAPITAL_SHIPS_DIR",
    "SPACE_GAME_LASERS_DIR",
    "SPACE_GAME_TURRETS_DIR",
    "SPACE_GAME_SPINNERS_DIR",
    "SANDBOX_CORE_PUBLIC_DIR",
    "SANDBOX_CORE_PRIVATE_DIR",
    "SANDBOX_CORE_TEST_PRIVATE_DIR",
    "ALL_STORAGE_OPERATIONS",
    "SPACEGAME_API",
    "SANDBOX_CORE_API",
    "ARRAY_MATH",
    "ARRAY_FILL",
    "SOA_VECTOR_FILL",
    "CHECK",
    "TEST_CAPITAL_SHIP_FIGHTERS",
    "TEST_CAPITAL_SHIP_FIGHTER_ORDER_QUEUE",
    "SPAWNED_ENTITY_HANDLES",
    "INCLUDE_ORDER",
    "soa_source_file",
    "homogeneous_soa_module",
)


def soa_source_file(
    header_path: Path,
    source_nodes: tuple[Node, ...],
    namespace: str | None = None,
    source_path: Path | None = None,
    header_include_path: str | None = None,
) -> CppFile:
    if source_path is None and header_path.is_relative_to(SPACE_GAME_PUBLIC_DIR):
        relative_header_path = header_path.relative_to(SPACE_GAME_PUBLIC_DIR)
        source_path = SPACE_GAME_PRIVATE_DIR / relative_header_path.with_suffix(".cpp")
        header_include_path = f"SpaceGame/{relative_header_path.as_posix()}"
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
