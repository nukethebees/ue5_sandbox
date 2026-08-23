from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Iterable

from Codegen.cpp import (
    ForwardDeclaration,
    MemberFunctionOperation,
    NewLines,
    Node,
    TypeDependency,
)


STD_FORWARD = TypeDependency("std::forward", "utility")
STD_REMOVE_CONST = TypeDependency("std::remove_const_t", "type_traits")
TARRAY = TypeDependency("TArray", "Containers/Array.h")
TARRAY_VIEW = TypeDependency("TArrayView", "Containers/ArrayView.h")
ALLOW_SHRINKING = TypeDependency(
    "EAllowShrinking", "Containers/AllowShrinking.h"
)
ARRAY_CHECKS = TypeDependency(
    "ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h"
)
CONTAINER_OPS = TypeDependency("ml::num", "SandboxCore/container_ops.h")
SOA_CONCEPTS = TypeDependency(
    "ml::SupportsApplyArrayPairsWith", "SandboxCore/soa_concepts.h"
)
SOA_PERMUTATION = TypeDependency("ml::apply_permutation", "SandboxCore/soa_permutation.h")
FILL_INDICES = TypeDependency("ml::fill_indices", "SandboxCore/array_utils.h")
CHECK = TypeDependency("check", "CoreMinimal.h")
FIXED_STORAGE = TypeDependency("ml::TFixedStorage", "SandboxCore/fixed_storage.h")
MOVE_TEMP = TypeDependency("MoveTemp", "Templates/UnrealTemplate.h")
TARRAY_REMOVE_AT_SWAP = MemberFunctionOperation("RemoveAtSwap")

from Codegen.soa.dynamic import (
    common_soa_function_specs,
    permutation_source_nodes,
    separate,
    storage_operation_spec,
    storage_operation_specs,
    storage_struct,
    view_struct,
)
from Codegen.soa.fixed import fixed_soa_nodes
from Codegen.soa.model import OUT_OF_LINE_STORAGE_OPERATIONS, SoAStruct

@dataclass(frozen=True)
class SoAStructLowering:
    header_nodes: tuple[Node, ...]
    source_nodes: tuple[Node, ...]


def lower_soa_struct_with_source(soa: SoAStruct) -> SoAStructLowering:
    operation_specs = storage_operation_specs(soa)
    const_view_specs = common_soa_function_specs(soa, True)
    view_specs = common_soa_function_specs(soa)
    storage_specs = common_soa_function_specs(soa)
    fixed_nodes = fixed_soa_nodes(soa)
    header_nodes = (
        ForwardDeclaration(soa.names.view_name),
        NewLines(1),
        ForwardDeclaration(soa.names.const_view_name),
        NewLines(2),
        view_struct(soa, soa.names.const_view_name, True),
        NewLines(2),
        view_struct(soa, soa.names.view_name, False),
        NewLines(2),
        storage_struct(soa, operation_specs),
        *((NewLines(2), *fixed_nodes) if fixed_nodes else ()),
    )
    source_functions = (
        *(
            spec.definition_node(soa.names.const_view_name)
            for spec in const_view_specs
            if not spec.is_inline
        ),
        *(
            spec.definition_node(soa.names.view_name)
            for spec in view_specs
            if not spec.is_inline
        ),
        *(
            storage_operation_spec(soa, operation).definition_node(soa.names.name)
            for operation in soa.storage_operations
            if operation in OUT_OF_LINE_STORAGE_OPERATIONS
        ),
        *permutation_source_nodes(
            (member.name for member in soa.members), soa.names.name
        ),
        *(
            spec.definition_node(soa.names.name)
            for spec in storage_specs
            if not spec.is_inline
        ),
    )
    source_nodes = separate((*soa.source_nodes, *source_functions), 2)
    return SoAStructLowering(header_nodes, source_nodes)


def lower_soa_struct(soa: SoAStruct) -> tuple[Node, ...]:
    return lower_soa_struct_with_source(soa).header_nodes


@dataclass(frozen=True)
class SoAStructsLowering:
    header_nodes: tuple[Node, ...]
    source_nodes: tuple[Node, ...]


def lower_soa_structs_with_source(
    soa_structs: Iterable[SoAStruct],
) -> SoAStructsLowering:
    lowerings = tuple(lower_soa_struct_with_source(soa) for soa in soa_structs)
    header_nodes: list[Node] = []
    source_nodes: list[Node] = []
    for lowering in lowerings:
        if header_nodes:
            header_nodes.append(NewLines(2))
        header_nodes.extend(lowering.header_nodes)
        if lowering.source_nodes:
            if source_nodes:
                source_nodes.append(NewLines(2))
            source_nodes.extend(lowering.source_nodes)
    return SoAStructsLowering(tuple(header_nodes), tuple(source_nodes))


def lower_soa_structs(soa_structs: Iterable[SoAStruct]) -> tuple[Node, ...]:
    return lower_soa_structs_with_source(soa_structs).header_nodes
