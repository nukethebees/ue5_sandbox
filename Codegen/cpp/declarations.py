from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from typing import override

from Codegen.cpp.rendering import Node, RenderContext, render_node_sequence
from Codegen.cpp.types import (
    TypeDependency, TypeLike, comma_separated, type_dependencies, type_spelling,
)


@dataclass(frozen=True)
class Include(Node):
    path: str
    system: bool | None = None

    @property
    def is_system(self) -> bool:
        if self.system is not None:
            return self.system
        return "." not in self.path.rsplit("/", 1)[-1]

    @override
    def render(self, context: RenderContext) -> str:
        left, right = ("<", ">") if self.is_system else ('"', '"')
        return context.apply_indent(f"#include {left}{self.path}{right}")


@dataclass(frozen=True)
class IncludeDependencies(Node):
    @override
    def render(self, context: RenderContext) -> str:
        return "\n\n".join(
            "\n".join(include.render(context) for include in group)
            for group in context.include_groups
        )


@dataclass(frozen=True)
class ForwardDeclaration(Node):
    name: str
    kind: str = "struct"

    @override
    def render(self, context: RenderContext) -> str:
        return context.apply_indent(f"{self.kind} {self.name};")


@dataclass(frozen=True)
class UsingDeclaration(Node):
    name: str
    value_type: TypeLike

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return type_dependencies(self.value_type)

    @override
    def render(self, context: RenderContext) -> str:
        return context.apply_indent(f"using {self.name} = {type_spelling(self.value_type)};")


@dataclass(frozen=True)
class Member(Node):
    type_name: TypeLike
    name: str
    initializer: str | None = None

    def __post_init__(self) -> None:
        if not type_spelling(self.type_name).strip() or not self.name.strip():
            raise ValueError("Member type and name must not be empty")

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return type_dependencies(self.type_name)

    @override
    def render(self, context: RenderContext) -> str:
        initializer = f"{{{self.initializer}}}" if self.initializer is not None else ""
        return context.apply_indent(f"{type_spelling(self.type_name)} {self.name}{initializer};")


@dataclass(frozen=True)
class Struct(Node):
    name: str
    nodes: tuple[Node, ...]
    bases: tuple[TypeLike, ...] = ()
    template: str | None = None
    export_specifier: str | None = None
    record_kind: str = "struct"

    def __init__(
        self,
        name: str,
        nodes: Iterable[Node],
        bases: Iterable[TypeLike] = (),
        template: str | None = None,
        export_specifier: str | None = None,
        record_kind: str = "struct",
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "nodes", tuple(nodes))
        object.__setattr__(self, "bases", tuple(bases))
        object.__setattr__(self, "template", template)
        object.__setattr__(self, "export_specifier", export_specifier)
        object.__setattr__(self, "record_kind", record_kind)
        if self.record_kind not in ("struct", "class"):
            raise ValueError("Record kind must be 'struct' or 'class'")
        member_names = [node.name for node in self.nodes if isinstance(node, Member)]
        duplicates = sorted({name for name in member_names if member_names.count(name) > 1})
        if duplicates:
            raise ValueError(f"Struct {self.name!r} has duplicate members: {', '.join(duplicates)}")

    @override
    def children(self) -> Iterable[Node]:
        return self.nodes

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return (
            dependency
            for base in self.bases
            for dependency in type_dependencies(base)
        )

    @override
    def render(self, context: RenderContext) -> str:
        lines: list[str] = []
        if self.template:
            lines.append(context.apply_indent(f"template <{self.template}>"))
        inheritance = (
            f" : {comma_separated(type_spelling(base) for base in self.bases)}"
            if self.bases
            else ""
        )
        export_specifier = f" {self.export_specifier}" if self.export_specifier else ""
        lines.append(
            context.apply_indent(
                f"{self.record_kind}{export_specifier} {self.name}{inheritance} {{"
            )
        )
        lines.append(render_node_sequence(self.nodes, context.indent(), 2))
        lines.append(context.apply_indent("};"))
        return "\n".join(lines)


@dataclass(frozen=True)
class Namespace(Node):
    name: str
    nodes: tuple[Node, ...]

    def __init__(self, name: str, nodes: Iterable[Node]) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "nodes", tuple(nodes))

    @override
    def children(self) -> Iterable[Node]:
        return self.nodes

    @override
    def render(self, context: RenderContext) -> str:
        body = render_node_sequence(self.nodes, context, 2)
        return (
            f"{context.apply_indent(f'namespace {self.name} {{')}\n"
            f"{body}\n"
            f"{context.apply_indent(f'}} // namespace {self.name}')}"
        )
