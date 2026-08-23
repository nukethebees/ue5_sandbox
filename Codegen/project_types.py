from __future__ import annotations

from collections.abc import Mapping

from Codegen.cpp import (
    CppType,
    MemberFunctionOperation,
    REMOVE_AT_SWAP,
    TypeLike,
    TypeOperation,
    composed_type,
    type_spelling,
)


LOWERCASE_REMOVE_AT_SWAP = MemberFunctionOperation("remove_at_swap")
UNREAL_REMOVE_AT_SWAP = MemberFunctionOperation("RemoveAtSwap")


def qualified_type(value: TypeLike, suffix: str) -> CppType:
    return composed_type(f"{type_spelling(value)}{suffix}", value)


def nested_type(value: TypeLike, name: str) -> CppType:
    return composed_type(f"{type_spelling(value)}::{name}", value)


def dependent_type(
    spelling: str,
    header: str,
    operations: Mapping[str, TypeOperation] | None = None,
) -> CppType:
    return composed_type(spelling, header=header, operations=operations)


def core_type(spelling: str) -> CppType:
    return dependent_type(spelling, "CoreMinimal.h")


F_REGISTRY_ENTITY_HANDLE = dependent_type(
    "FRegistryEntityHandle",
    "SandboxNative/RegistryEntityHandle.h",
)
F_REGISTRY_ENTITY_HANDLE_ARRAY = composed_type(
    "TArray<FRegistryEntityHandle>",
    F_REGISTRY_ENTITY_HANDLE,
    header="Containers/Array.h",
)
E_TEST_DEATH_REASON = dependent_type(
    "ETestDeathReason",
    "Sandbox/batch_game/test_entity_registry/TestDeathReason.h",
)
TEST_ENTITY_UNIQUE_ID = dependent_type(
    "TestEntityUniqueId",
    "Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h",
)
E_TEST_CAPITAL_SHIP_FIGHTERS_TASK = dependent_type(
    "ETestCapitalShipFightersTask",
    "Sandbox/batch_game/TestCapitalShipFightersTask.h",
)
TEST_CAPITAL_SHIP_FIGHTER_ORDER = dependent_type(
    "TestCapitalShipFighterOrder",
    "Sandbox/batch_game/TestCapitalShipFighterOrder.h",
)
TEST_CAPITAL_SHIP_FIGHTER_SPAWN_QUEUE = dependent_type(
    "TestCapitalShipFighterSpawnQueue",
    "Sandbox/batch_game/TestCapitalShipFighterSpawnQueue.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
E_TEST_ENTITY_TYPE = dependent_type(
    "ETestEntityType", "Sandbox/batch_game/TestEntityType.h"
)
E_TEST_TEAM = dependent_type("ETestTeam", "Sandbox/batch_game/TestTeam.h")
F_INDEX_SPAN = dependent_type("FIndexSpan", "Sandbox/utilities/IndexSpan.h")

F_COUNTDOWN_TIMERS = dependent_type(
    "FCountdownTimers",
    "SandboxCore/countdown_timers.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
F_ROTATORS_F = dependent_type(
    "FRotatorsf",
    "SandboxCore/soa_rotators.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
F_VECTORS_3F = dependent_type(
    "FVectors3f",
    "SandboxCore/soa_vectors_3f.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
F_TICK_COUNTDOWN_8 = dependent_type(
    "FTickCountdown8",
    "SandboxCore/tick_countdown.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
F_TICK_COUNTDOWN_16 = dependent_type(
    "FTickCountdown16",
    "SandboxCore/tick_countdown.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)
F_PERIODIC_TICK_COUNTDOWN_16 = dependent_type(
    "FPeriodicTickCountdown16",
    "SandboxCore/periodic_tick_countdown.h",
    {REMOVE_AT_SWAP: LOWERCASE_REMOVE_AT_SWAP},
)

F_INSTANCED_STATIC_MESH_INSTANCE_DATA = dependent_type(
    "FInstancedStaticMeshInstanceData",
    "Components/InstancedStaticMeshComponent.h",
)
F_LINEAR_COLOR = dependent_type("FLinearColor", "Math/Color.h")
U_PRIMITIVE_COMPONENT_CONST_PTR = dependent_type(
    "UPrimitiveComponent const*", "Components/PrimitiveComponent.h"
)

F_VECTOR_2F = core_type("FVector2f")
F_VECTOR_2D = core_type("FVector2d")
F_INT_POINT = core_type("FIntPoint")
F_UINT_POINT = core_type("FUintPoint")
F_VECTOR_3F = core_type("FVector3f")
F_VECTOR_3D = core_type("FVector3d")
F_INT_VECTOR = core_type("FIntVector")
F_UINT_VECTOR_3 = core_type("FUintVector3")
