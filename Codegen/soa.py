from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from collections.abc import Iterable

from Codegen.nodes import (
    CppType,
    ForwardDeclaration,
    FunctionBody,
    FunctionParameter,
    MemberFunctionOperation,
    MemberFunctionSpec,
    Member,
    NewLines,
    Node,
    Raw,
    REMOVE_AT_SWAP,
    RenderContext,
    Struct,
    TypeDependency,
    TypeLike,
    UsingDeclaration,
    comma_separated,
    composed_type,
    cpp_type,
    type_spelling,
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
TARRAY_REMOVE_AT_SWAP = MemberFunctionOperation("RemoveAtSwap")


@dataclass(frozen=True)
class SoAMember:
    container_type: CppType
    view_type: CppType
    const_view_type: CppType
    name: str
    view_function: str | None = None
    const_view_function: str | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "container_type", cpp_type(self.container_type))
        object.__setattr__(self, "view_type", cpp_type(self.view_type))
        object.__setattr__(self, "const_view_type", cpp_type(self.const_view_type))
        types = (self.container_type, self.view_type, self.const_view_type)
        if any(not type_spelling(value).strip() for value in types) or not self.name.strip():
            raise ValueError("SOA member types and name must not be empty")
        if (self.view_function is None) != (self.const_view_function is None):
            raise ValueError(
                "SOA member view functions must be specified together"
            )
        if self.view_function is not None and (
            not self.view_function.strip() or not self.const_view_function.strip()
        ):
            raise ValueError("SOA member view functions must not be empty")

    def view_expression(self, use_const_view: bool) -> str:
        if self.view_function is not None:
            function = self.const_view_function if use_const_view else self.view_function
            return f"{self.name}.{function}(offset, count)"
        view_type = self.const_view_type if use_const_view else self.view_type
        return f"{type_spelling(view_type)}{{{self.name}}}.Slice(offset, count)"


class ForEachSoAMemberCall(FunctionBody):
    def __init__(self, members: Iterable[SoAMember], method_name: str) -> None:
        self.members = tuple(members)
        self.method_name = method_name
        if not self.members:
            raise ValueError("ForEachSoAMemberCall requires at least one SOA member")
        if not self.method_name.strip():
            raise ValueError("ForEachSoAMemberCall method name must not be empty")

    def render(self, context: RenderContext) -> str:
        function = self.function
        if len(self.members) != len(function.parameters):
            raise ValueError(
                f"ForEachSoAMemberCall for {function.name!r} requires one parameter "
                f"per SOA member ({len(self.members)} members, "
                f"{len(function.parameters)} parameters)"
            )
        return "\n".join(
            Raw(
                f"{member.name}.{self.method_name}("
                f"{function.parameter(parameter).cpp_name});"
            ).render(context)
            for member, parameter in zip(self.members, function.parameters, strict=True)
        )


class ForEachSoAMemberFreeFunctionCall(FunctionBody):
    def __init__(self, members: Iterable[SoAMember], free_function: str) -> None:
        self.members = tuple(members)
        self.free_function = free_function
        if not self.members:
            raise ValueError(
                "ForEachSoAMemberFreeFunctionCall requires at least one SOA member"
            )
        if not self.free_function.strip():
            raise ValueError(
                "ForEachSoAMemberFreeFunctionCall free function must not be empty"
            )

    def dependencies(self) -> Iterable[TypeDependency]:
        return (CONTAINER_OPS,)

    def render(self, context: RenderContext) -> str:
        function = self.function
        parameters = tuple(
            function.parameter(parameter).cpp_name for parameter in function.parameters
        )
        return "\n".join(
            Raw(
                f"{self.free_function}({comma_separated((member.name, *parameters))});"
            ).render(context)
            for member in self.members
        )


class ForEachSoAMemberOperationCall(FunctionBody):
    def __init__(
        self,
        members: Iterable[SoAMember],
        operation: str,
        fallback_function: str,
    ) -> None:
        self.members = tuple(members)
        self.operation = operation
        self.fallback_function = fallback_function
        if not self.members:
            raise ValueError(
                "ForEachSoAMemberOperationCall requires at least one SOA member"
            )
        if not self.operation.strip():
            raise ValueError("SOA member operation must not be empty")
        if not self.fallback_function.strip():
            raise ValueError("SOA member operation fallback must not be empty")

    def dependencies(self) -> Iterable[TypeDependency]:
        return (CONTAINER_OPS,)

    def render(self, context: RenderContext) -> str:
        function = self.function
        parameters = tuple(
            function.parameter(parameter).cpp_name for parameter in function.parameters
        )
        calls: list[str] = []
        for member in self.members:
            implementation = member.container_type.operation(self.operation)
            call = (
                implementation.call(member.name, parameters)
                if implementation is not None
                else Raw(
                    f"{self.fallback_function}("
                    f"{comma_separated((member.name, *parameters))});"
                )
            )
            calls.append(call.render(context))
        return "\n".join(calls)


class ForEachSoAMemberPairFreeFunctionCall(FunctionBody):
    def __init__(
        self,
        members: Iterable[SoAMember],
        free_function: str,
        other_parameter: FunctionParameter,
        required_dependencies: Iterable[TypeDependency] = (),
    ) -> None:
        self.members = tuple(members)
        self.free_function = free_function
        self.other_parameter = other_parameter
        self.required_dependencies = (CONTAINER_OPS, *required_dependencies)
        if not self.members:
            raise ValueError(
                "ForEachSoAMemberPairFreeFunctionCall requires at least one SOA member"
            )
        if not self.free_function.strip():
            raise ValueError(
                "ForEachSoAMemberPairFreeFunctionCall free function must not be empty"
            )

    def dependencies(self) -> Iterable[TypeDependency]:
        return self.required_dependencies

    def render(self, context: RenderContext) -> str:
        function = self.function
        other = function.parameter(self.other_parameter)
        other_index = function.parameters.index(self.other_parameter)
        before_other = tuple(
            function.parameter(parameter).cpp_name
            for parameter in function.parameters[:other_index]
        )
        after_other = tuple(
            function.parameter(parameter).cpp_name
            for parameter in function.parameters[other_index + 1 :]
        )
        return "\n".join(
            Raw(
                f"{self.free_function}("
                f"{comma_separated((member.name, *before_other, f'{other.cpp_name}.{member.name}', *after_other))}"
                f");"
            ).render(context)
            for member in self.members
        )


class ForEachSoAMemberIndexCopy(FunctionBody):
    def __init__(
        self, members: Iterable[SoAMember], other_parameter: FunctionParameter
    ) -> None:
        self.members = tuple(members)
        self.other_parameter = other_parameter
        if not self.members:
            raise ValueError("ForEachSoAMemberIndexCopy requires at least one SOA member")

    def render(self, context: RenderContext) -> str:
        function = self.function
        other_index = function.parameters.index(self.other_parameter)
        other = function.parameter(self.other_parameter).cpp_name
        dst_i = function.parameter(function.parameters[other_index - 1]).cpp_name
        src_i = function.parameter(function.parameters[other_index + 1]).cpp_name
        return "\n".join(
            Raw(
                f"{member.name}[{dst_i}] = {other}.{member.name}[{src_i}];"
            ).render(context)
            for member in self.members
        )


class ForEachSoAMemberRangeCopy(FunctionBody):
    def __init__(
        self, members: Iterable[SoAMember], other_parameter: FunctionParameter
    ) -> None:
        self.members = tuple(members)
        self.other_parameter = other_parameter
        if not self.members:
            raise ValueError("ForEachSoAMemberRangeCopy requires at least one SOA member")

    def render(self, context: RenderContext) -> str:
        function = self.function
        other_index = function.parameters.index(self.other_parameter)
        other = function.parameter(self.other_parameter).cpp_name
        dst_i = function.parameter(function.parameters[other_index - 1]).cpp_name
        src_i = function.parameter(function.parameters[other_index + 1]).cpp_name
        count = function.parameter(function.parameters[other_index + 2]).cpp_name
        assignments = "\n".join(
            f"            {member.name}[{dst_i} + i] = {other}.{member.name}[{src_i} + i];"
            for member in self.members
        )
        return f"        for (auto i{{0}}; i < {count}; ++i) {{\n{assignments}\n        }}"


class ForEachSoAFieldPermutationCall(FunctionBody):
    def __init__(self, fields: Iterable[str]) -> None:
        self.fields = tuple(fields)
        if not self.fields or any(not field.strip() for field in self.fields):
            raise ValueError("ForEachSoAFieldPermutationCall requires named fields")

    def dependencies(self) -> Iterable[TypeDependency]:
        return (SOA_PERMUTATION, CHECK)

    def render(self, context: RenderContext) -> str:
        function = self.function
        if len(function.parameters) != 1:
            raise ValueError(
                f"ForEachSoAFieldPermutationCall for {function.name!r} requires one indices parameter"
            )
        indices = function.parameter(function.parameters[0]).cpp_name
        return "\n".join(
            (
                Raw("validate_array_sizes();").render(context),
                Raw(f"check({indices}.Num() == num());").render(context),
                *(Raw(f"ml::apply_permutation({field}, {indices});").render(context)
                  for field in self.fields),
            )
        )


class SoAStorageOperation(Enum):
    RESET = "reset"
    RESERVE = "reserve"
    ADD_UNINITIALISED = "add_uninitialised"
    ADD_DEFAULTED = "add_defaulted"
    REMOVE_AT_SWAP = "remove_at_swap"
    SET_NUM = "set_num"
    COPY_ELEMENT = "copy_element"
    APPEND_FROM = "append_from"


OUT_OF_LINE_STORAGE_OPERATIONS = frozenset(
    (
        SoAStorageOperation.RESET,
        SoAStorageOperation.RESERVE,
        SoAStorageOperation.ADD_UNINITIALISED,
        SoAStorageOperation.ADD_DEFAULTED,
        SoAStorageOperation.SET_NUM,
    )
)


def tarray_member(
    name: str, value_type: TypeLike, allocator: TypeLike | None = None
) -> SoAMember:
    allocator_suffix = f", {type_spelling(allocator)}" if allocator else ""
    element = type_spelling(value_type)
    contained_types = (value_type, allocator) if allocator else (value_type,)
    return SoAMember(
        container_type=composed_type(
            f"TArray<{element}{allocator_suffix}>",
            *contained_types,
            header="Containers/Array.h",
            operations={REMOVE_AT_SWAP: TARRAY_REMOVE_AT_SWAP},
        ),
        view_type=composed_type(
            f"TArrayView<{element}>", value_type, header="Containers/ArrayView.h"
        ),
        const_view_type=composed_type(
            f"TConstArrayView<{element}>", value_type, header="Containers/ArrayView.h"
        ),
        name=name,
    )


def soa_member(
    name: str,
    container_type: TypeLike,
    view_type: TypeLike | None = None,
    const_view_type: TypeLike | None = None,
) -> SoAMember:
    container_spelling = type_spelling(container_type)
    return SoAMember(
        container_type=container_type,
        view_type=view_type
        or composed_type(f"{container_spelling}::View", container_type),
        const_view_type=const_view_type
        or composed_type(f"{container_spelling}::ConstView", container_type),
        name=name,
        view_function="get_view",
        const_view_function="get_const_view",
    )


@dataclass(frozen=True)
class SoAStructNames:
    name: str
    view_name: str | None = None
    const_view_name: str | None = None

    def __post_init__(self) -> None:
        if not self.name.strip():
            raise ValueError("SOA struct name must not be empty")
        if self.view_name is None:
            object.__setattr__(self, "view_name", f"{self.name}View")
        if self.const_view_name is None:
            object.__setattr__(self, "const_view_name", f"{self.name}ConstView")
        if not self.view_name.strip() or not self.const_view_name.strip():
            raise ValueError("SOA view struct names must not be empty")


@dataclass(frozen=True)
class SoAStruct:
    names: SoAStructNames
    members: tuple[SoAMember, ...]
    storage_export_specifier: str | None = None
    storage_operations: tuple[SoAStorageOperation, ...] = ()
    nodes: tuple[Node, ...] = ()
    source_nodes: tuple[Node, ...] = ()
    equivalent_type: TypeLike | None = None
    copy_element_memberwise: bool = False

    def __post_init__(self) -> None:
        object.__setattr__(self, "members", tuple(self.members))
        object.__setattr__(self, "storage_operations", tuple(self.storage_operations))
        object.__setattr__(self, "nodes", tuple(self.nodes))
        object.__setattr__(self, "source_nodes", tuple(self.source_nodes))
        if not self.members:
            raise ValueError(
                f"SOA struct {self.names.name!r} must contain at least one member"
            )
        if any(
            not isinstance(operation, SoAStorageOperation)
            for operation in self.storage_operations
        ):
            raise ValueError(
                "SOA storage operations must be SoAStorageOperation values"
            )
        if len(set(self.storage_operations)) != len(self.storage_operations):
            raise ValueError("SOA storage operations must not contain duplicates")


@dataclass(frozen=True)
class HomogeneousSoAValueType:
    cpp_type: TypeLike
    suffix: str
    equivalent_type: TypeLike | None = None
    input_types: tuple[TypeLike, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "input_types", tuple(self.input_types))
        if not type_spelling(self.cpp_type).strip() or not self.suffix.strip():
            raise ValueError("Homogeneous SOA value type and suffix must not be empty")
        if any(not type_spelling(value).strip() for value in self.input_types):
            raise ValueError("Homogeneous SOA input types must not be empty")


@dataclass(frozen=True)
class HomogeneousSoALayout:
    name: str
    components: tuple[str, ...]
    value_types: tuple[HomogeneousSoAValueType, ...]
    storage_export_specifier: str | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "components", tuple(self.components))
        object.__setattr__(self, "value_types", tuple(self.value_types))
        if not self.name.strip():
            raise ValueError("Homogeneous SOA layout name must not be empty")
        if not self.components or any(
            not component.strip() for component in self.components
        ):
            raise ValueError("Homogeneous SOA components must not be empty")
        if len(set(self.components)) != len(self.components):
            raise ValueError("Homogeneous SOA components must not contain duplicates")
        if not self.value_types:
            raise ValueError("Homogeneous SOA layout must contain value types")
        equivalent_types = tuple(value.equivalent_type for value in self.value_types)
        if any(equivalent_types) and not all(equivalent_types):
            raise ValueError(
                "Homogeneous SOA layouts must define equivalent types for every value type"
            )

    @property
    def view_name(self) -> str:
        return f"T{self.name}View"

    def storage_name(self, value_type: HomogeneousSoAValueType) -> str:
        return f"F{self.name}{value_type.suffix}"

    @property
    def has_equivalent_type(self) -> bool:
        return all(value.equivalent_type is not None for value in self.value_types)

    @property
    def equivalent_type_trait_name(self) -> str:
        return f"T{self.name}EquivalentType"


def _separate(nodes: Iterable[Node], newline_count: int) -> tuple[Node, ...]:
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


def _storage_operation_spec(
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
        case _:
            raise ValueError(f"Unsupported SOA storage operation: {operation}")


def _storage_operation_specs(soa: SoAStruct) -> tuple[MemberFunctionSpec, ...]:
    specs: list[MemberFunctionSpec] = []
    for operation in soa.storage_operations:
        specs.append(_storage_operation_spec(soa, operation))
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
    return _separate((spec.header_node() for spec in specs), 2)


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


def _permutation_function_nodes(fields: Iterable[str]) -> tuple[Node, ...]:
    return _separate(
        (spec.header_node() for spec in _permutation_function_specs(fields)), 2
    )


def _permutation_source_nodes(fields: Iterable[str], owner_name: str) -> tuple[Node, ...]:
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


def _common_soa_function_specs(
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
        functions.append(
            MemberFunctionSpec(
                "operator[]",
                soa.equivalent_type,
                (FunctionParameter("int32 const", "index"),),
                Raw(
                    "validate_array_sizes();\n"
                    f"return {{{comma_separated(f'{member.name}[index]' for member in soa.members)}}};"
                ),
                suffix=" const",
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


def _view_struct(soa: SoAStruct, name: str, use_const_view_types: bool) -> Struct:
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
            *_separate(
                (function.header_node() for function in _common_soa_function_specs(
                    soa, use_const_view_types
                )),
                1,
            ),
            NewLines(2),
            *_separate(members, 1),
        ),
        export_specifier=soa.storage_export_specifier,
    )


def _storage_struct(
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
    struct_nodes.extend((NewLines(2), *_permutation_function_nodes(member.name for member in soa.members)))
    struct_nodes.extend((NewLines(2), _apply_arrays_function(soa.members)))
    struct_nodes.extend((NewLines(2), _apply_array_pairs_function(soa.members)))
    struct_nodes.extend(
        (
            NewLines(2),
            *_separate(
                (function.header_node() for function in _common_soa_function_specs(soa)),
                1,
            ),
        )
    )
    struct_nodes.extend((NewLines(2), *_separate(members, 1)))
    return Struct(
        soa.names.name,
        struct_nodes,
        export_specifier=soa.storage_export_specifier,
    )


@dataclass(frozen=True)
class SoAStructLowering:
    header_nodes: tuple[Node, ...]
    source_nodes: tuple[Node, ...]


def lower_soa_struct_with_source(soa: SoAStruct) -> SoAStructLowering:
    operation_specs = _storage_operation_specs(soa)
    const_view_specs = _common_soa_function_specs(soa, True)
    view_specs = _common_soa_function_specs(soa)
    storage_specs = _common_soa_function_specs(soa)
    header_nodes = (
        ForwardDeclaration(soa.names.view_name),
        NewLines(1),
        ForwardDeclaration(soa.names.const_view_name),
        NewLines(2),
        _view_struct(soa, soa.names.const_view_name, True),
        NewLines(2),
        _view_struct(soa, soa.names.view_name, False),
        NewLines(2),
        _storage_struct(soa, operation_specs),
    )
    source_functions = (
        *(spec.definition_node(soa.names.const_view_name) for spec in const_view_specs),
        *(spec.definition_node(soa.names.view_name) for spec in view_specs),
        *(
            _storage_operation_spec(soa, operation).definition_node(soa.names.name)
            for operation in soa.storage_operations
            if operation in OUT_OF_LINE_STORAGE_OPERATIONS
        ),
        *_permutation_source_nodes(
            (member.name for member in soa.members), soa.names.name
        ),
        *(spec.definition_node(soa.names.name) for spec in storage_specs),
    )
    source_nodes = _separate((*soa.source_nodes, *source_functions), 2)
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


def _homogeneous_body(
    lines: Iterable[str], dependencies: Iterable[TypeDependency] = ()
) -> Raw:
    return Raw("\n".join(lines), dependencies)


def _homogeneous_view_functions(layout: HomogeneousSoALayout) -> tuple[Node, ...]:
    components = comma_separated(layout.components)
    view_name = layout.view_name
    offset = FunctionParameter("size_type const", "offset")
    count = FunctionParameter("size_type const", "count")
    func = FunctionParameter("TFunc&&", "func")
    functions: list[Node] = [
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw("return View{" + components + "};"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw("return ConstView{" + components + "};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (),
            Raw("return ConstView{" + components + "};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (offset, count),
            Raw("return get_const_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "apply_arrays",
            "auto",
            (func,),
            Raw(
                f"return std::forward<TFunc>(func)({components});",
                (STD_FORWARD,),
            ),
            suffix=" -> decltype(auto)",
            is_inline=True,
            template_parameters="typename TFunc",
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "apply_arrays",
            "auto",
            (func,),
            Raw(
                f"return std::forward<TFunc>(func)({components});",
                (STD_FORWARD,),
            ),
            suffix=" const -> decltype(auto)",
            is_inline=True,
            template_parameters="typename TFunc",
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "num",
            "auto",
            (),
            Raw(f"return {layout.components[0]}.Num();"),
            suffix=" const -> size_type",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "slice",
            "auto",
            (offset, count),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Slice(offset, count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "left",
            "auto",
            (count,),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Left(count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "right",
            "auto",
            (count,),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Right(count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
    ]
    if layout.has_equivalent_type:
        functions.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "operator[]",
                    "equivalent_type",
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        f"return {{{comma_separated(f'{component}[index]' for component in layout.components)}}};"
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
            )
        )
    return tuple(functions)


def _homogeneous_view_struct(layout: HomogeneousSoALayout) -> Struct:
    return Struct(
        layout.view_name,
        (
            UsingDeclaration(
                "size_type", composed_type("TArrayView<T>::SizeType", TARRAY_VIEW)
            ),
            NewLines(1),
            UsingDeclaration(
                "value_type",
                composed_type("std::remove_const_t<T>", STD_REMOVE_CONST),
            ),
            NewLines(1),
            UsingDeclaration("View", f"{layout.view_name}<T>"),
            NewLines(1),
            UsingDeclaration("ConstView", f"{layout.view_name}<value_type const>"),
            *(
                (
                    NewLines(1),
                    UsingDeclaration(
                        "equivalent_type",
                        f"typename {layout.equivalent_type_trait_name}<value_type>::type",
                    ),
                )
                if layout.has_equivalent_type
                else ()
            ),
            NewLines(2),
            *_separate(
                (
                    Member(composed_type("TArrayView<T>", TARRAY_VIEW), component)
                    for component in layout.components
                ),
                1,
            ),
            NewLines(2),
            *_homogeneous_view_functions(layout),
        ),
        template="typename T",
    )


def _homogeneous_data_struct(
    layout: HomogeneousSoALayout, name: str, pointer_type: str
) -> Struct:
    return Struct(
        name, (Member(pointer_type, component) for component in layout.components)
    )


def _homogeneous_storage_functions(
    layout: HomogeneousSoALayout, value_type: HomogeneousSoAValueType
) -> tuple[Node, ...]:
    name = layout.storage_name(value_type)
    components = comma_separated(layout.components)
    offset = FunctionParameter("size_type const", "offset")
    count = FunctionParameter("size_type const", "count")
    allow_shrinking = FunctionParameter(
        composed_type("EAllowShrinking const", ALLOW_SHRINKING), "allow_shrinking"
    )
    func = FunctionParameter("TFunc&&", "func")
    get_data = (
        MemberFunctionSpec(
            "get_data",
            "auto",
            (),
            Raw(
                f"return Data{{{comma_separated(f'{component}.GetData()' for component in layout.components)}}};"
            ),
            suffix=" -> Data",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_data",
            "auto",
            (),
            Raw(
                f"return ConstData{{{comma_separated(f'{component}.GetData()' for component in layout.components)}}};"
            ),
            suffix=" const -> ConstData",
            is_inline=True,
        ).header_node(),
    )
    views = (
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw(f"return View{{{components}}};"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw(f"return ConstView{{{components}}};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (),
            Raw(f"return ConstView{{{components}}};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (offset, count),
            Raw("return get_const_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
    )
    operations = (
        ("reset", (), "Reset"),
        ("empty", (), "Empty"),
        ("reserve", (count,), "Reserve"),
        ("set_num", (count, allow_shrinking), "SetNum"),
        ("set_num_uninitialised", (count,), "SetNumUninitialized"),
        ("add_uninitialised", (count,), "AddUninitialized"),
        (
            "remove_at_swap",
            (FunctionParameter("size_type const", "index"), count, allow_shrinking),
            "RemoveAtSwap",
        ),
        ("add_zeroed", (count,), "AddZeroed"),
        ("add_defaulted", (count,), "AddDefaulted"),
    )
    operation_nodes: list[Node] = []
    for index, (operation_name, parameters, array_operation) in enumerate(operations):
        if index:
            operation_nodes.append(NewLines(1))
        arguments = comma_separated(parameter.name for parameter in parameters)
        operation_nodes.append(
            MemberFunctionSpec(
                operation_name,
                "auto",
                parameters,
                _homogeneous_body(
                    f"{component}.{array_operation}({arguments});"
                    for component in layout.components
                ),
                suffix=" -> void",
                is_inline=True,
            ).header_node()
        )
    nodes: list[Node] = [*get_data, NewLines(2), *views, NewLines(2)]
    nodes.extend(
        (
            MemberFunctionSpec(
                "apply_arrays",
                "auto",
                (func,),
                Raw(
                    f"return std::forward<TFunc>(func)({components});",
                    (STD_FORWARD,),
                ),
                suffix=" -> decltype(auto)",
                is_inline=True,
                template_parameters="typename TFunc",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "apply_arrays",
                "auto",
                (func,),
                Raw(
                    f"return std::forward<TFunc>(func)({components});",
                    (STD_FORWARD,),
                ),
                suffix=" const -> decltype(auto)",
                is_inline=True,
                template_parameters="typename TFunc",
            ).header_node(),
            NewLines(2),
            MemberFunctionSpec(
                "num",
                "auto",
                (),
                Raw(f"return {layout.components[0]}.Num();"),
                suffix=" const -> size_type",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "validate_array_sizes",
                "void",
                (),
                _homogeneous_body(
                    f"check({component}.Num() == {layout.components[0]}.Num());"
                    for component in layout.components[1:]
                ),
                suffix=" const",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "is_empty",
                "auto",
                (),
                Raw("return num() == 0;"),
                suffix=" const -> bool",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_element",
                "auto",
                (
                    FunctionParameter("size_type const", "dst_i"),
                    FunctionParameter("Other const&", "src"),
                    FunctionParameter("size_type const", "src_i"),
                ),
                _homogeneous_body(
                    f"{component}[dst_i] = src.{component}[src_i];"
                    for component in layout.components
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_elements",
                "auto",
                (
                    FunctionParameter("size_type const", "dst_i"),
                    FunctionParameter("Other const&", "src"),
                    FunctionParameter("size_type const", "src_i"),
                    FunctionParameter("size_type const", "count"),
                ),
                Raw(
                    "for (auto i{0}; i < count; ++i) {\n"
                    + "\n".join(
                        f"    {component}[dst_i + i] = src.{component}[src_i + i];"
                        for component in layout.components
                    )
                    + "\n}"
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_to_tail",
                "auto",
                (FunctionParameter("Other const&", "src"),),
                Raw(
                    "auto const count{src.num()};\n"
                    "check(num() >= count);\n"
                    "copy_elements(num() - count, src, 0, count);",
                    (CHECK,),
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "append_from",
                "void",
                (FunctionParameter("Other const&", "other"),),
                _homogeneous_body(
                    f"{component}.Append(other.{component});"
                    for component in layout.components
                ),
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
        )
    )
    nodes.extend((NewLines(2), *_permutation_function_nodes(layout.components)))
    if value_type.equivalent_type:
        nodes.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "operator[]",
                    value_type.equivalent_type,
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        "validate_array_sizes();\n"
                        f"return {{{comma_separated(f'{component}[index]' for component in layout.components)}}};"
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
            )
        )
    if value_type.input_types:
        component_parameters = tuple(
            FunctionParameter("value_type const", component[0])
            for component in layout.components
        )
        nodes.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "add",
                    "auto",
                    component_parameters,
                    _homogeneous_body(
                        (
                            f"auto const index{{{layout.components[0]}.Add({layout.components[0][0]})}};",
                            *(
                                f"{component}.Add({component[0]});"
                                for component in layout.components[1:]
                            ),
                            "return index;",
                        )
                    ),
                    suffix=" -> size_type",
                    is_inline=True,
                ).header_node(),
                *(
                    node
                    for input_type in value_type.input_types
                    for node in (
                        NewLines(1),
                        MemberFunctionSpec(
                            "add",
                            "auto",
                            (
                                FunctionParameter(
                                    composed_type(
                                        f"{type_spelling(input_type)} const&", input_type
                                    ),
                                    "value",
                                ),
                            ),
                            Raw(
                                f"return add({comma_separated(f'value.{axis}' for axis in ('X', 'Y', 'Z')[: len(layout.components)])});"
                            ),
                            suffix=" -> size_type",
                            is_inline=True,
                        ).header_node(),
                    )
                ),
            )
        )
    nodes.extend((NewLines(2), *operation_nodes))
    return tuple(nodes)


def _homogeneous_storage_struct(
    layout: HomogeneousSoALayout, value_type: HomogeneousSoAValueType
) -> Struct:
    aliases: list[Node] = [UsingDeclaration("value_type", value_type.cpp_type)]
    if value_type.equivalent_type:
        aliases.extend(
            (NewLines(1), UsingDeclaration("equivalent_type", value_type.equivalent_type))
        )
    aliases.extend(
        (
            NewLines(1),
            UsingDeclaration(
                "size_type", composed_type("TArray<value_type>::SizeType", TARRAY)
            ),
            NewLines(1),
            UsingDeclaration("View", f"{layout.view_name}<value_type>"),
            NewLines(1),
            UsingDeclaration("ConstView", f"{layout.view_name}<value_type const>"),
        )
    )
    return Struct(
        layout.storage_name(value_type),
        (
            *aliases,
            NewLines(2),
            _homogeneous_data_struct(layout, "Data", "value_type*"),
            NewLines(2),
            _homogeneous_data_struct(layout, "ConstData", "value_type const*"),
            NewLines(2),
            *_separate(
                (
                    Member(composed_type("TArray<value_type>", TARRAY), component)
                    for component in layout.components
                ),
                1,
            ),
            NewLines(2),
            *_homogeneous_storage_functions(layout, value_type),
        ),
        export_specifier=layout.storage_export_specifier,
    )


def _homogeneous_equivalent_type_nodes(
    layout: HomogeneousSoALayout,
) -> tuple[Node, ...]:
    if not layout.has_equivalent_type:
        return ()

    specialisations = tuple(
        Raw(
            "\n".join(
                (
                    "template <>",
                    f"struct {layout.equivalent_type_trait_name}<{type_spelling(value.cpp_type)}> {{",
                    f"    using type = {type_spelling(value.equivalent_type)};",
                    "};",
                )
            ),
        )
        for value in layout.value_types
    )
    return (
        Raw(f"template <typename T>\nstruct {layout.equivalent_type_trait_name};"),
        NewLines(2),
        *_separate(specialisations, 2),
    )


def lower_homogeneous_soa_layouts(
    layouts: Iterable[HomogeneousSoALayout],
) -> tuple[Node, ...]:
    layout_list = tuple(layouts)
    nodes: list[Node] = []
    for layout in layout_list:
        if nodes:
            nodes.append(NewLines(2))
        nodes.extend(_homogeneous_equivalent_type_nodes(layout))
        if layout.has_equivalent_type:
            nodes.append(NewLines(2))
        nodes.append(_homogeneous_view_struct(layout))
    for layout in layout_list:
        for value_type in layout.value_types:
            nodes.extend((NewLines(2), _homogeneous_storage_struct(layout, value_type)))
    return tuple(nodes)


def lower_homogeneous_soa_permutation_definitions(
    layouts: Iterable[HomogeneousSoALayout],
) -> tuple[Node, ...]:
    nodes: list[Node] = []
    for layout in layouts:
        for value_type in layout.value_types:
            if nodes:
                nodes.append(NewLines(2))
            nodes.extend(
                _permutation_source_nodes(
                    layout.components, layout.storage_name(value_type)
                )
            )
    return tuple(nodes)
