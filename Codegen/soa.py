from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from collections.abc import Iterable

from Codegen.nodes import (
    ForwardDeclaration,
    FunctionBody,
    FunctionParameter,
    MemberFunctionSpec,
    Member,
    NewLines,
    Node,
    Raw,
    RenderContext,
    Struct,
    UsingDeclaration,
    comma_separated,
)


@dataclass(frozen=True)
class SoAMember:
    container_type: str
    view_type: str
    const_view_type: str
    name: str

    def __post_init__(self) -> None:
        values = (self.container_type, self.view_type, self.const_view_type, self.name)
        if any(not value.strip() for value in values):
                raise ValueError("SOA member types and name must not be empty")


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
            raise ValueError("ForEachSoAMemberFreeFunctionCall requires at least one SOA member")
        if not self.free_function.strip():
            raise ValueError("ForEachSoAMemberFreeFunctionCall free function must not be empty")

    def render(self, context: RenderContext) -> str:
        function = self.function
        parameters = tuple(
            function.parameter(parameter).cpp_name for parameter in function.parameters
        )
        return "\n".join(
            Raw(f"{self.free_function}({comma_separated((member.name, *parameters))});").render(context)
            for member in self.members
        )


class ForEachSoAMemberPairFreeFunctionCall(FunctionBody):
    def __init__(
        self,
        members: Iterable[SoAMember],
        free_function: str,
        other_parameter: FunctionParameter,
    ) -> None:
        self.members = tuple(members)
        self.free_function = free_function
        self.other_parameter = other_parameter
        if not self.members:
            raise ValueError("ForEachSoAMemberPairFreeFunctionCall requires at least one SOA member")
        if not self.free_function.strip():
            raise ValueError("ForEachSoAMemberPairFreeFunctionCall free function must not be empty")

    def render(self, context: RenderContext) -> str:
        function = self.function
        other = function.parameter(self.other_parameter)
        other_index = function.parameters.index(self.other_parameter)
        before_other = tuple(
            function.parameter(parameter).cpp_name for parameter in function.parameters[:other_index]
        )
        after_other = tuple(
            function.parameter(parameter).cpp_name for parameter in function.parameters[other_index + 1 :]
        )
        return "\n".join(
            Raw(
                f"{self.free_function}("
                f"{comma_separated((member.name, *before_other, f'{other.cpp_name}.{member.name}', *after_other))}"
                f");"
            ).render(context)
            for member in self.members
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


def tarray_member(name: str, value_type: str, allocator: str | None = None) -> SoAMember:
    allocator_suffix = f", {allocator}" if allocator else ""
    return SoAMember(
        container_type=f"TArray<{value_type}{allocator_suffix}>",
        view_type=f"TArrayView<{value_type}>",
        const_view_type=f"TConstArrayView<{value_type}>",
        name=name,
    )


def soa_member(
    name: str,
    container_type: str,
    view_type: str | None = None,
    const_view_type: str | None = None,
) -> SoAMember:
    return SoAMember(
        container_type=container_type,
        view_type=view_type or f"{container_type}::View",
        const_view_type=const_view_type or f"{container_type}::ConstView",
        name=name,
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
    storage_base: str = "ml::FSoAArrayMixin"
    view_base: str = "ml::FSoAViewMixin"

    def __post_init__(self) -> None:
        object.__setattr__(self, "members", tuple(self.members))
        object.__setattr__(self, "storage_operations", tuple(self.storage_operations))
        object.__setattr__(self, "nodes", tuple(self.nodes))
        if not self.members:
            raise ValueError(f"SOA struct {self.names.name!r} must contain at least one member")
        if any(not isinstance(operation, SoAStorageOperation) for operation in self.storage_operations):
            raise ValueError("SOA storage operations must be SoAStorageOperation values")
        if len(set(self.storage_operations)) != len(self.storage_operations):
            raise ValueError("SOA storage operations must not contain duplicates")


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
        (FunctionParameter("this auto&&", "self"), FunctionParameter("TFunc&&", "func")),
        Raw("\n".join(body)),
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
        Raw("\n".join(body)),
        suffix="\n    -> decltype(auto)",
        is_inline=True,
        template_parameters="typename Self, typename Other, typename TFunc",
    ).header_node()


def _storage_operation_spec(soa: SoAStruct, operation: SoAStorageOperation) -> MemberFunctionSpec:
    members = soa.members
    match operation:
        case SoAStorageOperation.RESET:
            return MemberFunctionSpec(
                "reset", "void", (), ForEachSoAMemberFreeFunctionCall(members, "ml::reset")
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
                    FunctionParameter("EAllowShrinking const", "allow_shrinking"),
                ),
                ForEachSoAMemberFreeFunctionCall(members, "ml::remove_at_swap"),
                is_inline=True,
            )
        case SoAStorageOperation.SET_NUM:
            return MemberFunctionSpec(
                "set_num",
                "void",
                (
                    FunctionParameter("int32 const", "count"),
                    FunctionParameter("EAllowShrinking const", "allow_shrinking"),
                ),
                ForEachSoAMemberFreeFunctionCall(members, "ml::set_num"),
            )
        case SoAStorageOperation.COPY_ELEMENT:
            other = FunctionParameter(f"{soa.names.name} const&", "other")
            return MemberFunctionSpec(
                "copy_element",
                "void",
                (
                    FunctionParameter("int32 const", "dst_i"),
                    other,
                    FunctionParameter("int32 const", "src_i"),
                ),
                ForEachSoAMemberPairFreeFunctionCall(members, "ml::copy_element", other),
                is_inline=True,
            )
        case SoAStorageOperation.APPEND_FROM:
            other = FunctionParameter("Other const&", "other")
            return MemberFunctionSpec(
                "append_from",
                "void",
                (other,),
                ForEachSoAMemberPairFreeFunctionCall(members, "ml::append_from", other),
                is_inline=True,
                template_parameters="typename Other",
                requires_clause=f"ml::SupportsApplyArrayPairsWith<{soa.names.name}, Other>",
            )
        case _:
            raise ValueError(f"Unsupported SOA storage operation: {operation}")


def _storage_operation_specs(soa: SoAStruct) -> tuple[MemberFunctionSpec, ...]:
    return tuple(_storage_operation_spec(soa, operation) for operation in soa.storage_operations)


def _storage_operation_nodes(specs: Iterable[MemberFunctionSpec]) -> tuple[Node, ...]:
    return _separate(
        (spec.header_node() for spec in specs), 2
    )


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
            NewLines(2),
            _apply_arrays_function(soa.members),
            NewLines(2),
            *_separate(members, 1),
        ),
        bases=(f"public {soa.view_base}",),
    )


def _storage_struct(soa: SoAStruct, operation_specs: Iterable[MemberFunctionSpec]) -> Struct:
    members = tuple(Member(member.container_type, member.name) for member in soa.members)
    struct_nodes: list[Node] = [
        UsingDeclaration("View", soa.names.view_name),
        NewLines(1),
        UsingDeclaration("ConstView", soa.names.const_view_name),
    ]
    if soa.nodes:
        struct_nodes.extend((NewLines(2), *soa.nodes))
    operations = _storage_operation_nodes(operation_specs)
    if operations:
        struct_nodes.extend((NewLines(2), *operations))
    struct_nodes.extend((NewLines(2), _apply_arrays_function(soa.members)))
    struct_nodes.extend((NewLines(2), _apply_array_pairs_function(soa.members)))
    struct_nodes.extend((NewLines(2), *_separate(members, 1)))
    return Struct(
        soa.names.name,
        struct_nodes,
        bases=(f"public {soa.storage_base}",),
        export_specifier=soa.storage_export_specifier,
    )


@dataclass(frozen=True)
class SoAStructLowering:
    header_nodes: tuple[Node, ...]
    source_nodes: tuple[Node, ...]


def lower_soa_struct_with_source(soa: SoAStruct) -> SoAStructLowering:
    operation_specs = _storage_operation_specs(soa)
    header_nodes = (
        ForwardDeclaration(soa.names.const_view_name),
        NewLines(2),
        _view_struct(soa, soa.names.view_name, False),
        NewLines(2),
        _view_struct(soa, soa.names.const_view_name, True),
        NewLines(2),
        _storage_struct(soa, operation_specs),
    )
    source_nodes = _separate(
        (
            spec.definition_node(soa.names.name)
            for operation, spec in zip(soa.storage_operations, operation_specs, strict=True)
            if operation in OUT_OF_LINE_STORAGE_OPERATIONS
        ),
        2,
    )
    return SoAStructLowering(header_nodes, source_nodes)


def lower_soa_struct(soa: SoAStruct) -> tuple[Node, ...]:
    return lower_soa_struct_with_source(soa).header_nodes


@dataclass(frozen=True)
class SoAStructsLowering:
    header_nodes: tuple[Node, ...]
    source_nodes: tuple[Node, ...]


def lower_soa_structs_with_source(soa_structs: Iterable[SoAStruct]) -> SoAStructsLowering:
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
