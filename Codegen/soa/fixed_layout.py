from __future__ import annotations

from dataclasses import dataclass

from Codegen.cpp import CppType, comma_separated, type_spelling
from Codegen.soa.model import ArraySoAMember, FixedSoAContainer, SoAMember, SoAStruct


@dataclass(frozen=True)
class FixedSoALeaf:
    path: tuple[str, ...]
    element_type: CppType

    @property
    def argument_name(self) -> str:
        return "_".join(self.path)


@dataclass(frozen=True)
class FixedSoAMemberLayout:
    member: SoAMember
    leaves: tuple[FixedSoALeaf, ...]
    nested: FixedSoALayout | None = None

    @property
    def storage_type(self) -> str:
        if isinstance(self.member, ArraySoAMember):
            return f"ml::TFixedStorage<{type_spelling(self.member.element_type)}, Capacity>"
        if self.nested is None:
            raise ValueError(f"Nested member {self.member.name!r} has no fixed layout")
        return f"{self.nested.storage_name}<Capacity>"


@dataclass(frozen=True)
class FixedSoALayout:
    schema: SoAStruct
    storage_name: str
    containers: tuple[FixedSoAContainer, ...]
    members: tuple[FixedSoAMemberLayout, ...]
    leaves: tuple[FixedSoALeaf, ...]


def build_fixed_layout(
    soa: SoAStruct, prefix: tuple[str, ...] = (), ancestors: tuple[int, ...] = ()
) -> FixedSoALayout:
    if soa.fixed is None:
        raise ValueError(f"SOA struct {soa.names.name!r} has no fixed configuration")
    if id(soa) in ancestors:
        raise ValueError(f"Fixed SOA schema {soa.names.name!r} contains a cycle")

    member_layouts: list[FixedSoAMemberLayout] = []
    leaves: list[FixedSoALeaf] = []
    next_ancestors = (*ancestors, id(soa))
    for member in soa.members:
        member_path = (*prefix, member.name)
        if isinstance(member, ArraySoAMember):
            member_leaves = (FixedSoALeaf(member_path, member.element_type),)
            member_layout = FixedSoAMemberLayout(member, member_leaves)
        else:
            if member.fixed_schema is None or member.fixed_schema.fixed is None:
                raise ValueError(
                    f"Fixed SOA storage {soa.fixed.storage_name!r} cannot represent "
                    f"member {member.name!r} without a fixed schema"
                )
            nested = build_fixed_layout(member.fixed_schema, member_path, next_ancestors)
            member_leaves = nested.leaves
            member_layout = FixedSoAMemberLayout(member, member_leaves, nested)
        member_layouts.append(member_layout)
        leaves.extend(member_leaves)
    return FixedSoALayout(
        soa,
        soa.fixed.storage_name,
        soa.fixed.containers,
        tuple(member_layouts),
        tuple(leaves),
    )


def fixed_member_view(member: SoAMember, is_const: bool) -> str:
    if isinstance(member, ArraySoAMember):
        view = "TConstArrayView" if is_const else "TArrayView"
        return (
            f"{view}<{type_spelling(member.element_type)}>{{"
            f"{member.name}_.data() + offset, count}}"
        )
    function = "get_const_view" if is_const else "get_view"
    return f"{member.name}_.{function}(offset, count)"


def fixed_construct_lines(
    layout: FixedSoALayout, operation: str, argument_names: tuple[str, ...] = ()
) -> tuple[str, ...]:
    lines: list[str] = []
    argument_index = 0
    for member_layout in layout.members:
        member = member_layout.member
        leaf_count = len(member_layout.leaves)
        args = argument_names[argument_index : argument_index + leaf_count]
        argument_index += leaf_count
        name = f"{member.name}_"
        if operation == "default":
            call = (
                f"{name}.construct_at(index);"
                if isinstance(member, ArraySoAMember)
                else f"{name}.default_construct_at(index);"
            )
        elif operation == "arguments":
            call = f"{name}.construct_at(index, {comma_separated(args)});"
        elif operation == "copy":
            call = (
                f"{name}.construct_at(index, other.{name}[other_index]);"
                if isinstance(member, ArraySoAMember)
                else f"{name}.copy_construct_at(index, other.{name}, other_index);"
            )
        elif operation == "move":
            call = (
                f"{name}.construct_at(index, MoveTemp(other.{name}[other_index]));"
                if isinstance(member, ArraySoAMember)
                else f"{name}.move_construct_at(index, other.{name}, other_index);"
            )
        elif operation == "view":
            call = (
                f"{name}.construct_at(index, source.{member.name}[source_index]);"
                if isinstance(member, ArraySoAMember)
                else f"{name}.construct_from_view_at(index, source.{member.name}, source_index);"
            )
        else:
            raise ValueError(f"Unsupported fixed construction operation: {operation}")
        lines.append(call)
    return tuple(lines)
