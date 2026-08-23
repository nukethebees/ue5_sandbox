from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass, field
from pathlib import Path
from typing import override

from Codegen.cpp.declarations import Include
from Codegen.cpp.rendering import (
    Node, RenderContext, render_node_sequence, walk_tree,
)
from Codegen.cpp.types import GENERATED_FILE_WARNING, TypeDependency


@dataclass(frozen=True)
class CppFile(Node):
    path: Path
    nodes: tuple[Node, ...] = field(default_factory=tuple)
    pragma_once: bool = True
    clang_format_off: bool = False
    prologue: tuple[str, ...] = field(default_factory=tuple)
    epilogue: tuple[str, ...] = field(default_factory=tuple)
    include_order: tuple[str, ...] = field(default_factory=tuple)

    def __post_init__(self) -> None:
        object.__setattr__(self, "path", Path(self.path))
        object.__setattr__(self, "nodes", tuple(self.nodes))
        object.__setattr__(self, "prologue", tuple(self.prologue))
        object.__setattr__(self, "epilogue", tuple(self.epilogue))
        object.__setattr__(self, "include_order", tuple(self.include_order))

    @override
    def children(self) -> Iterable[Node]:
        return self.nodes

    def _include_groups(self) -> tuple[tuple[Include, ...], ...]:
        includes: set[Include] = set()

        def collect(dependency: TypeDependency) -> None:
            if dependency.header is not None:
                includes.add(Include(dependency.header))
            for contained in dependency.dependencies:
                collect(contained)

        for node in walk_tree(self):
            for dependency in node.dependencies():
                collect(dependency)

        remaining = set(includes)
        groups: list[tuple[Include, ...]] = []
        for prefix in self.include_order:
            group = tuple(sorted(
                (include for include in remaining if include.path.startswith(prefix)),
                key=lambda include: include.path,
            ))
            if group:
                groups.append(group)
                remaining.difference_update(group)

        quoted = tuple(sorted(
            (include for include in remaining if not include.is_system),
            key=lambda include: include.path,
        ))
        system = tuple(sorted(
            (include for include in remaining if include.is_system),
            key=lambda include: include.path,
        ))
        if quoted:
            groups.append(quoted)
        if system:
            groups.append(system)
        return tuple(groups)

    @override
    def render(self, context: RenderContext | None = None) -> str:
        if context is None:
            context = RenderContext()

        if context.indent_level != 0:
            raise ValueError("CppFile must be rendered at the root indentation level")

        context = context.with_include_groups(self._include_groups())

        sections: list[str] = [GENERATED_FILE_WARNING]
        sections.extend(self.prologue)
        if self.pragma_once:
            sections.append("#pragma once")
        if self.nodes:
            sections.append(render_node_sequence(self.nodes, context, 1))
        sections.extend(self.epilogue)
        rendered = "\n\n".join(sections).rstrip()
        if self.clang_format_off:
            return f"// clang-format off\n{rendered}\n// clang-format on\n"
        return rendered + "\n"


@dataclass(frozen=True)
class Module:
    name: str
    header: CppFile | None = None
    source: CppFile | None = None

    def files(self) -> tuple[CppFile, ...]:
        return tuple(file for file in (self.header, self.source) if file is not None)
