from __future__ import annotations

from collections.abc import Iterable

from Codegen.cpp import (
    FunctionParameter,
    MemberFunctionOperation,
    MemberFunctionSpec,
    Member,
    NewLines,
    Node,
    Raw,
    REMOVE_AT_SWAP,
    Struct,
    TypeDependency,
    UsingDeclaration,
    comma_separated,
    composed_type,
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

from Codegen.soa.model import SoAMember, SoAStruct, SoAStorageOperation
from Codegen.soa.operations import (
    ForEachSoAFieldPermutationCall,
    ForEachSoAMemberFreeFunctionCall,
    ForEachSoAMemberIndexCopy,
    ForEachSoAMemberOperationCall,
    ForEachSoAMemberPairFreeFunctionCall,
    ForEachSoAMemberRangeCopy,
)

def separate(nodes: Iterable[Node], newline_count: int) -> tuple[Node, ...]:
    node_list = tuple(nodes)
    separated: list[Node] = []
    for index, node in enumerate(node_list):
        if index:
            separated.append(NewLines(newline_count))
        separated.append(node)
    return tuple(separated)


def _apply_arrays_function(members: tuple[SoAMember, ...]) -> Node:
    body = ["return std::forward<TFunc>(func)("]
    final_index = len(members) - 1
    for index, member in enumerate(members):
        comma = "," if index != final_index else ""
        body.append(f"    self.{member.name}{comma}")
    body.append(");")
    return MemberFunctionSpec(
        "apply_arrays",
        "auto",
        (
            FunctionParameter("this auto&&", "self"),
            FunctionParameter("TFunc&&", "func"),
        ),
        Raw("\n".join(body), (STD_FORWARD,)),
        suffix=" -> decltype(auto)",
        is_inline=True,
        template_parameters="typename TFunc",
    ).header_node()


def _apply_array_pairs_function(members: tuple[SoAMember, ...]) -> Node:
    body = ["return std::forward<TFunc>(func)("]
    final_index = len(members) - 1
    for index, member in enumerate(members):
        comma = "," if index != final_index else ""
        body.append(f"    self.{member.name}, other.{member.name}{comma}")
    body.append(");")
    return MemberFunctionSpec(
        "apply_array_pairs",
        "auto",
        (
            FunctionParameter("this Self&&", "self"),
            FunctionParameter("Other&&", "other"),
            FunctionParameter("TFunc&&", "func"),
        ),
        Raw("\n".join(body), (STD_FORWARD,)),
        suffix="\n    -> decltype(auto)",
        is_inline=True,
        template_parameters="typename Self, typename Other, typename TFunc",
    ).header_node()


def storage_operation_spec(
    soa: SoAStruct, operation: SoAStorageOperation
) -> MemberFunctionSpec:
    members = soa.members
    match operation:
        case SoAStorageOperation.RESET:
            return MemberFunctionSpec(
                "reset",
                "void",
                (),
                ForEachSoAMemberFreeFunctionCall(members, "ml::reset"),
            )
        case SoAStorageOperation.RESERVE:
            return MemberFunctionSpec(
                "reserve",
                "void",
                (FunctionParameter("int32 const", "count"),),
                ForEachSoAMemberFreeFunctionCall(members, "ml::reserve"),
            )
        case SoAStorageOperation.ADD_UNINITIALISED:
            return MemberFunctionSpec(
                "add_uninitialised",
                "void",
                (FunctionParameter("int32 const", "count"),),
                ForEachSoAMemberFreeFunctionCall(members, "ml::add_uninitialised"),
            )
        case SoAStorageOperation.ADD_DEFAULTED:
            return MemberFunctionSpec(
                "add_defaulted",
                "void",
                (FunctionParameter("int32 const", "count"),),
                ForEachSoAMemberFreeFunctionCall(members, "ml::add_defaulted"),
            )
        case SoAStorageOperation.REMOVE_AT_SWAP:
            return MemberFunctionSpec(
                "remove_at_swap",
                "void",
                (
                    FunctionParameter("int32 const", "index"),
                    FunctionParameter("int32 const", "count"),
                    FunctionParameter(
                        composed_type("EAllowShrinking const", ALLOW_SHRINKING),
                        "allow_shrinking",
                    ),
                ),
                ForEachSoAMemberOperationCall(
                    members, REMOVE_AT_SWAP, "ml::remove_at_swap"
                ),
                is_inline=True,
            )
        case SoAStorageOperation.SET_NUM:
            return MemberFunctionSpec(
                "set_num",
                "void",
                (
                    FunctionParameter("int32 const", "count"),
                    FunctionParameter(
                        composed_type("EAllowShrinking const", ALLOW_SHRINKING),
                        "allow_shrinking",
                    ),
                ),
                ForEachSoAMemberFreeFunctionCall(members, "ml::set_num"),
            )
        case SoAStorageOperation.COPY_ELEMENT:
            other = FunctionParameter("Other const&", "other")
            return MemberFunctionSpec(
                "copy_element",
                "void",
                (
                    FunctionParameter("int32 const", "dst_i"),
                    other,
                    FunctionParameter("int32 const", "src_i"),
                ),
                (
                    ForEachSoAMemberIndexCopy(members, other)
                    if soa.copy_element_memberwise
                    else ForEachSoAMemberPairFreeFunctionCall(
                        members, "ml::copy_element", other
                    )
                ),
                is_inline=True,
                template_parameters="typename Other",
            )
        case SoAStorageOperation.APPEND_FROM:
            other = FunctionParameter("Other const&", "other")
            return MemberFunctionSpec(
                "append_from",
                "void",
                (other,),
                ForEachSoAMemberPairFreeFunctionCall(
                    members,
                    "ml::append_from",
                    other,
                    (SOA_CONCEPTS,),
                ),
                is_inline=True,
                template_parameters="typename Other",
                requires_clause=f"ml::SupportsApplyArrayPairsWith<{soa.names.name}, Other>",
            )


def storage_operation_specs(soa: SoAStruct) -> tuple[MemberFunctionSpec, ...]:
    specs: list[MemberFunctionSpec] = []
    for operation in soa.storage_operations:
        specs.append(storage_operation_spec(soa, operation))
        if operation is SoAStorageOperation.COPY_ELEMENT:
            other = FunctionParameter("Other const&", "other")
            specs.append(
                MemberFunctionSpec(
                    "copy_elements",
                    "void",
                    (
                        FunctionParameter("int32 const", "dst_i"),
                        other,
                        FunctionParameter("int32 const", "src_i"),
                        FunctionParameter("int32 const", "count"),
                    ),
                    ForEachSoAMemberPairFreeFunctionCall(
                        soa.members, "ml::copy_elements", other
                    )
                    if not soa.copy_element_memberwise
                    else ForEachSoAMemberRangeCopy(soa.members, other),
                    is_inline=True,
                    template_parameters="typename Other",
                )
            )
            specs.append(
                MemberFunctionSpec(
                    "copy_to_tail",
                    "void",
                    (other,),
                    Raw(
                        "auto const count{other.num()};\n"
                        "check(num() >= count);\n"
                        "copy_elements(num() - count, other, 0, count);",
                        (CHECK,),
                    ),
                    is_inline=True,
                    template_parameters="typename Other",
                )
            )
    return tuple(specs)


def _storage_operation_nodes(specs: Iterable[MemberFunctionSpec]) -> tuple[Node, ...]:
    return separate((spec.header_node() for spec in specs), 2)


def _permutation_function_specs(fields: Iterable[str]) -> tuple[MemberFunctionSpec, ...]:
    indices = FunctionParameter(composed_type("TArrayView<int32>", TARRAY_VIEW), "indices")
    scratch_indices = FunctionParameter(
        composed_type("TArrayView<int32>", TARRAY_VIEW), "scratch_indices"
    )
    compare = FunctionParameter("Compare&&", "compare")
    apply_body = ForEachSoAFieldPermutationCall(fields)
    sort_body = Raw(
        "\n".join(
            (
                "validate_array_sizes();",
                "auto const n{num()};",
                "check(scratch_indices.Num() == n);",
                "ml::fill_indices(scratch_indices);",
                "// indices[new_index] is the old row index that belongs at new_index.",
                "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) {",
                "    return compare(*this, lhs, rhs);",
                "});",
                "apply_permutation(scratch_indices);",
            )
        ),
        (CHECK, FILL_INDICES),
    )
    nttp_sort_body = Raw(
        "\n".join(
            (
                "validate_array_sizes();",
                "auto const n{num()};",
                "check(scratch_indices.Num() == n);",
                "ml::fill_indices(scratch_indices);",
                "// indices[new_index] is the old row index that belongs at new_index.",
                "scratch_indices.Sort([this](int32 const lhs, int32 const rhs) {",
                "    return Compare(*this, lhs, rhs);",
                "});",
                "apply_permutation(scratch_indices);",
            )
        ),
        (CHECK, FILL_INDICES),
    )
    return (
        MemberFunctionSpec(
            "apply_permutation",
            "void",
            (indices,),
            apply_body,
        ),
        MemberFunctionSpec(
            "sort",
            "void",
            (compare, scratch_indices),
            sort_body,
            is_inline=True,
            template_parameters="typename Compare",
        ),
        MemberFunctionSpec(
            "sort",
            "void",
            (scratch_indices,),
            nttp_sort_body,
            is_inline=True,
            template_parameters="auto Compare",
        ),
    )


def permutation_function_nodes(fields: Iterable[str]) -> tuple[Node, ...]:
    return separate(
        (spec.header_node() for spec in _permutation_function_specs(fields)), 2
    )


def permutation_source_nodes(fields: Iterable[str], owner_name: str) -> tuple[Node, ...]:
    apply_permutation, *_ = _permutation_function_specs(fields)
    return (apply_permutation.definition_node(owner_name),)


def _view_projection_body(
    result_type: str,
    members: tuple[SoAMember, ...],
    use_const_view: bool,
) -> Raw:
    lines = [f"return {result_type}{{"]
    lines.extend(
        f"    {member.view_expression(use_const_view)},"
        for member in members
    )
    lines.append("};")
    return Raw("\n".join(lines))


def common_soa_function_specs(
    soa: SoAStruct, is_const_view: bool = False
) -> tuple[MemberFunctionSpec, ...]:
    offset = FunctionParameter("int32 const", "offset")
    count = FunctionParameter("int32 const", "count")
    validate_lines = ["ml::fatal_if_nums_not_equal({"]
    validate_lines.extend(f"    ml::num({member.name})," for member in soa.members)
    validate_lines.append("});")
    functions: list[MemberFunctionSpec] = []
    if not is_const_view:
        functions.extend(
            (
                MemberFunctionSpec(
                    "get_view",
                    "auto",
                    (),
                    Raw("return get_view(0, num());"),
                    suffix=" -> View",
                ),
                MemberFunctionSpec(
                    "get_view",
                    "auto",
                    (offset, count),
                    _view_projection_body("View", soa.members, False),
                    suffix=" -> View",
                ),
            )
        )
    if soa.equivalent_type is not None:
        functions.extend(
            (
                MemberFunctionSpec(
                    "operator[]",
                    soa.equivalent_type,
                    (FunctionParameter("int32 const", "index"),),
                    Raw(
                        f"return {{{comma_separated(member.unchecked_index_expression('index') for member in soa.members)}}};"
                    ),
                    suffix=" const",
                    is_inline=True,
                ),
                MemberFunctionSpec(
                    "at",
                    soa.equivalent_type,
                    (FunctionParameter("int32 const", "index"),),
                    Raw(
                        "validate_array_sizes();\n"
                        "check(index >= 0);\n"
                        "check(index < num());\n"
                        "return (*this)[index];",
                        (CHECK,),
                    ),
                    suffix=" const",
                    is_inline=True,
                ),
            )
        )
    functions.extend(
        (
            MemberFunctionSpec(
                "get_view",
                "auto",
                (),
                Raw("return get_view(0, num());"),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "get_view",
                "auto",
                (offset, count),
                _view_projection_body("ConstView", soa.members, True),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "get_const_view",
                "auto",
                (),
                Raw("return get_const_view(0, num());"),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "get_const_view",
                "auto",
                (offset, count),
                _view_projection_body("ConstView", soa.members, True),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "num",
                "auto",
                (),
                Raw(f"return ml::num({soa.members[0].name});", (CONTAINER_OPS,)),
                suffix=" const noexcept -> int32",
            ),
            MemberFunctionSpec(
                "is_empty",
                "auto",
                (),
                Raw("return num() == 0;"),
                suffix=" const noexcept -> bool",
            ),
            MemberFunctionSpec(
                "validate_array_sizes",
                "void",
                (),
                Raw("\n".join(validate_lines), (ARRAY_CHECKS, CONTAINER_OPS)),
                suffix=" const",
            ),
        )
    )
    if not is_const_view:
        functions.extend(
            (
                MemberFunctionSpec(
                    "slice",
                    "auto",
                    (offset, count),
                    Raw("return get_view(offset, count);"),
                    suffix=" -> View",
                ),
                MemberFunctionSpec(
                    "left",
                    "auto",
                    (count,),
                    Raw("return slice(0, count);"),
                    suffix=" -> View",
                ),
                MemberFunctionSpec(
                    "right",
                    "auto",
                    (count,),
                    Raw("return slice(num() - count, count);"),
                    suffix=" -> View",
                ),
            )
        )
    functions.extend(
        (
            MemberFunctionSpec(
                "slice",
                "auto",
                (offset, count),
                Raw("return get_view(offset, count);"),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "left",
                "auto",
                (count,),
                Raw("return slice(0, count);"),
                suffix=" const -> ConstView",
            ),
            MemberFunctionSpec(
                "right",
                "auto",
                (count,),
                Raw("return slice(num() - count, count);"),
                suffix=" const -> ConstView",
            ),
        )
    )
    return tuple(functions)


def view_struct(soa: SoAStruct, name: str, use_const_view_types: bool) -> Struct:
    members = tuple(
        Member(
            member.const_view_type if use_const_view_types else member.view_type,
            member.name,
        )
        for member in soa.members
    )
    return Struct(
        name,
        (
            UsingDeclaration("View", soa.names.view_name),
            NewLines(1),
            UsingDeclaration("ConstView", soa.names.const_view_name),
            *(
                (
                    NewLines(1),
                    UsingDeclaration("equivalent_type", soa.equivalent_type),
                )
                if soa.equivalent_type is not None
                else ()
            ),
            NewLines(2),
            _apply_arrays_function(soa.members),
            NewLines(2),
            *separate(
                (function.header_node() for function in common_soa_function_specs(
                    soa, use_const_view_types
                )),
                1,
            ),
            NewLines(2),
            *separate(members, 1),
        ),
        export_specifier=soa.storage_export_specifier,
    )


def storage_struct(
    soa: SoAStruct, operation_specs: Iterable[MemberFunctionSpec]
) -> Struct:
    members = tuple(
        Member(member.container_type, member.name) for member in soa.members
    )
    struct_nodes: list[Node] = [
        UsingDeclaration("View", soa.names.view_name),
        NewLines(1),
        UsingDeclaration("ConstView", soa.names.const_view_name),
    ]
    if soa.equivalent_type is not None:
        struct_nodes.extend(
            (NewLines(1), UsingDeclaration("equivalent_type", soa.equivalent_type))
        )
    if soa.nodes:
        struct_nodes.extend((NewLines(2), *soa.nodes))
    operations = _storage_operation_nodes(operation_specs)
    if operations:
        struct_nodes.extend((NewLines(2), *operations))
    struct_nodes.extend((NewLines(2), *permutation_function_nodes(member.name for member in soa.members)))
    struct_nodes.extend((NewLines(2), _apply_arrays_function(soa.members)))
    struct_nodes.extend((NewLines(2), _apply_array_pairs_function(soa.members)))
    struct_nodes.extend(
        (
            NewLines(2),
            *separate(
                (function.header_node() for function in common_soa_function_specs(soa)),
                1,
            ),
        )
    )
    struct_nodes.extend((NewLines(2), *separate(members, 1)))
    return Struct(
        soa.names.name,
        struct_nodes,
        export_specifier=soa.storage_export_specifier,
    )
