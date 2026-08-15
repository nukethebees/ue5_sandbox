from __future__ import annotations

from Codegen.nodes import TypeDependency, TypeLike, composed_type, type_spelling


def qualified_type(value: TypeLike, suffix: str) -> TypeDependency:
    return composed_type(f"{type_spelling(value)}{suffix}", value)


def nested_type(value: TypeLike, name: str) -> TypeDependency:
    return composed_type(f"{type_spelling(value)}::{name}", value)


def core_type(spelling: str) -> TypeDependency:
    return TypeDependency(spelling, "CoreMinimal.h")


F_REGISTRY_ENTITY_HANDLE = TypeDependency(
    "FRegistryEntityHandle",
    "Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h",
)
E_TEST_DEATH_REASON = TypeDependency(
    "ETestDeathReason",
    "Sandbox/batch_game/test_entity_registry/TestDeathReason.h",
)
TEST_ENTITY_UNIQUE_ID = TypeDependency(
    "TestEntityUniqueId",
    "Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h",
)
E_TEST_CAPITAL_SHIP_FIGHTERS_TASK = TypeDependency(
    "ETestCapitalShipFightersTask",
    "Sandbox/batch_game/TestCapitalShipFightersTask.h",
)
TEST_CAPITAL_SHIP_FIGHTER_ORDER = TypeDependency(
    "TestCapitalShipFighterOrder",
    "Sandbox/batch_game/TestCapitalShipFighterOrder.h",
)
TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE = TypeDependency(
    "TestCapitalShipFighterSpawnQueue",
    "Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h",
)
E_TEST_ENTITY_TYPE = TypeDependency(
    "ETestEntityType", "Sandbox/batch_game/TestEntityType.h"
)
E_TEST_TEAM = TypeDependency("ETestTeam", "Sandbox/batch_game/TestTeam.h")
F_INDEX_SPAN = TypeDependency("FIndexSpan", "Sandbox/utilities/IndexSpan.h")

F_COUNTDOWN_TIMERS = TypeDependency(
    "FCountdownTimers", "SandboxCore/countdown_timers.h"
)
F_ROTATORS_F = TypeDependency("FRotatorsf", "SandboxCore/soa_rotators.h")
F_VECTORS_3F = TypeDependency("FVectors3f", "SandboxCore/soa_vectors.h")
F_TICK_COUNTDOWN_8 = TypeDependency(
    "FTickCountdown8", "SandboxCore/tick_countdown.h"
)
F_TICK_COUNTDOWN_16 = TypeDependency(
    "FTickCountdown16", "SandboxCore/tick_countdown.h"
)

F_INSTANCED_STATIC_MESH_INSTANCE_DATA = TypeDependency(
    "FInstancedStaticMeshInstanceData",
    "Components/InstancedStaticMeshComponent.h",
)
F_LINEAR_COLOR = TypeDependency("FLinearColor", "Math/Color.h")

F_VECTOR_2F = core_type("FVector2f")
F_VECTOR_2D = core_type("FVector2d")
F_INT_POINT = core_type("FIntPoint")
F_UINT_POINT = core_type("FUintPoint")
F_VECTOR_3F = core_type("FVector3f")
F_VECTOR_3D = core_type("FVector3d")
F_INT_VECTOR = core_type("FIntVector")
F_UINT_VECTOR_3 = core_type("FUintVector3")
