from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Iterable

from Codegen.nodes import (
    ForwardDeclaration,
    Function,
    FunctionBody,
    Member,
    NewLines,
    Node,
    Raw,
    RenderContext,
    Struct,
    UsingDeclaration,
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
class SoAStruct:
    name: str
    view_name: str
    const_view_name: str
    members: tuple[SoAMember, ...]
    storage_export_specifier: str | None = None
    storage_type_aliases: tuple[tuple[str, str], ...] = ()
    nodes: tuple[Node, ...] = ()
    storage_base: str = "ml::FSoAArrayMixin"
    view_base: str = "ml::FSoAViewMixin"

    def __post_init__(self) -> None:
        object.__setattr__(self, "members", tuple(self.members))
        object.__setattr__(self, "storage_type_aliases", tuple(self.storage_type_aliases))
        object.__setattr__(self, "nodes", tuple(self.nodes))
        if not self.members:
            raise ValueError(f"SOA struct {self.name!r} must contain at least one member")
        for alias_name, alias_value_type in self.storage_type_aliases:
            if not alias_name.strip() or not alias_value_type.strip():
                raise ValueError("SOA storage type aliases must not be empty")


def _separate(nodes: Iterable[Node], newline_count: int) -> tuple[Node, ...]:
    node_list = tuple(nodes)
    separated: list[Node] = []
    for index, node in enumerate(node_list):
        if index:
            separated.append(NewLines(newline_count))
        separated.append(node)
    return tuple(separated)


def _apply_arrays_function(members: tuple[SoAMember, ...]) -> Function:
    body = ["return std::forward<TFunc>(func)("]
    final_index = len(members) - 1
    for index, member in enumerate(members):
        comma = "," if index != final_index else ""
        body.append(f"    self.{member.name}{comma}")
    body.append(");")
    return Function(
        "template <typename TFunc>\n"
        "auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto)",
        Raw("\n".join(body)),
    )


def _apply_array_pairs_function(members: tuple[SoAMember, ...]) -> Function:
    body = ["return std::forward<TFunc>(func)("]
    final_index = len(members) - 1
    for index, member in enumerate(members):
        comma = "," if index != final_index else ""
        body.append(f"    self.{member.name}, other.{member.name}{comma}")
    body.append(");")
    return Function(
        "template <typename Self, typename Other, typename TFunc>\n"
        "auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func)\n"
        "    -> decltype(auto)",
        Raw("\n".join(body)),
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
            UsingDeclaration("View", soa.view_name),
            NewLines(1),
            UsingDeclaration("ConstView", soa.const_view_name),
            NewLines(2),
            _apply_arrays_function(soa.members),
            NewLines(2),
            *_separate(members, 1),
        ),
        bases=(f"public {soa.view_base}",),
    )


def _storage_struct(soa: SoAStruct) -> Struct:
    members = tuple(Member(member.container_type, member.name) for member in soa.members)
    aliases = tuple(UsingDeclaration(name, value_type) for name, value_type in soa.storage_type_aliases)
    struct_nodes: list[Node] = [
        UsingDeclaration("View", soa.view_name),
        NewLines(1),
        UsingDeclaration("ConstView", soa.const_view_name),
    ]
    if aliases:
        struct_nodes.extend((NewLines(2), *_separate(aliases, 1)))
    if soa.nodes:
        struct_nodes.extend((NewLines(2), *soa.nodes))
    struct_nodes.extend((NewLines(2), _apply_arrays_function(soa.members)))
    struct_nodes.extend((NewLines(2), _apply_array_pairs_function(soa.members)))
    struct_nodes.extend((NewLines(2), *_separate(members, 1)))
    return Struct(
        soa.name,
        struct_nodes,
        bases=(f"public {soa.storage_base}",),
        export_specifier=soa.storage_export_specifier,
    )


def lower_soa_struct(soa: SoAStruct) -> tuple[Node, ...]:
    return (
        ForwardDeclaration(soa.const_view_name),
        NewLines(2),
        _view_struct(soa, soa.view_name, False),
        NewLines(2),
        _view_struct(soa, soa.const_view_name, True),
        NewLines(2),
        _storage_struct(soa),
    )


def lower_soa_structs(soa_structs: Iterable[SoAStruct]) -> tuple[Node, ...]:
    nodes: list[Node] = []
    for index, soa in enumerate(soa_structs):
        if index:
            nodes.append(NewLines(2))
        nodes.extend(lower_soa_struct(soa))
    return tuple(nodes)
