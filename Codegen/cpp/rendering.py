from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from typing import TYPE_CHECKING, override

from Codegen.cpp.types import TypeDependency

if TYPE_CHECKING:
    from Codegen.cpp.declarations import Include


@dataclass(frozen=True)
class RenderContext:
    indent_level: int = 0
    indent_text: str = "    "
    include_groups: tuple[tuple[Include, ...], ...] = ()

    def indent(self) -> RenderContext:
        return RenderContext(self.indent_level + 1, self.indent_text, self.include_groups)

    def with_include_groups(
        self, include_groups: Iterable[Iterable[Include]]
    ) -> RenderContext:
        return RenderContext(
            self.indent_level,
            self.indent_text,
            tuple(tuple(group) for group in include_groups),
        )

    def apply_indent(self, text: str) -> str:
        prefix = self.indent_text * self.indent_level
        return "\n".join(f"{prefix}{line}" if line else "" for line in text.splitlines())


class Node(ABC):
    def children(self) -> Iterable[Node]:
        return ()

    def dependencies(self) -> Iterable[TypeDependency]:
        return ()

    @abstractmethod
    def render(self, context: RenderContext) -> str:
        raise NotImplementedError


def walk_tree(root: Node) -> Iterator[Node]:
    yield root
    for child in root.children():
        yield from walk_tree(child)


@dataclass(frozen=True)
class Raw(Node):
    text: str
    required_dependencies: tuple[TypeDependency, ...] = ()

    def __init__(
        self, text: str, required_dependencies: Iterable[TypeDependency] = ()
    ) -> None:
        object.__setattr__(self, "text", text)
        object.__setattr__(self, "required_dependencies", tuple(required_dependencies))

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return self.required_dependencies

    @override
    def render(self, context: RenderContext) -> str:
        return context.apply_indent(self.text)


@dataclass(frozen=True)
class NewLines(Node):
    count: int

    def __post_init__(self) -> None:
        if self.count <= 0:
            raise ValueError("NewLines count must be positive")

    @override
    def render(self, context: RenderContext) -> str:
        return "\n" * self.count


def render_node_sequence(
    nodes: Iterable[Node], context: RenderContext, default_newlines: int
) -> str:
    output = ""
    for node in nodes:
        if isinstance(node, NewLines):
            output = output.rstrip("\n") + node.render(context)
            continue
        rendered_node = node.render(context)
        if not rendered_node:
            continue
        if output and not output.endswith("\n"):
            output += "\n" * default_newlines
        output += rendered_node
    return output
