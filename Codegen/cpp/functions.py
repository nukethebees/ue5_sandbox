from __future__ import annotations

from abc import ABC
from collections.abc import Iterable
from dataclasses import dataclass
from typing import override

from Codegen.cpp.rendering import Node, RenderContext
from Codegen.cpp.types import (
    CppType, TypeDependency, TypeLike, comma_separated, cpp_type,
    type_dependencies, type_spelling, uses_trailing_return_type,
)


@dataclass(frozen=True)
class FunctionDeclaration(Node):
    function: MemberFunctionSpec | FreeFunctionSpec

    @override
    def children(self) -> Iterable[Node]:
        return self.function.parameters

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return self.function.type_dependencies()

    @override
    def render(self, context: RenderContext) -> str:
        declaration = self.function.declaration_signature().rstrip(";") + ";"
        return context.apply_indent(declaration)


@dataclass(frozen=True)
class Function(Node):
    function: MemberFunctionSpec | FreeFunctionSpec
    owner_name: str | None = None
    is_header: bool = False

    @override
    def children(self) -> Iterable[Node]:
        return (*self.function.parameters, self.function.body)

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return self.function.type_dependencies()

    @override
    def render(self, context: RenderContext) -> str:
        body = self.function.body.render(context.indent())
        return (
            f"{context.apply_indent(self.function.definition_signature(self.owner_name, self.is_header))} {{\n"
            f"{body}\n"
            f"{context.apply_indent('}')}"
        )


@dataclass
class FunctionParameter(Node):
    type_name: CppType
    name: str
    default_value: str | None = None

    def __init__(
        self,
        type_name: TypeLike,
        name: str,
        default_value: str | None = None,
    ) -> None:
        self.type_name = cpp_type(type_name)
        self.name = name
        self.default_value = default_value

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return self.type_name.type_dependencies

    @override
    def render(self, context: RenderContext) -> str:
        return context.apply_indent(self.declaration_text())

    def declaration_text(self) -> str:
        default_value = f" = {self.default_value}" if self.default_value else ""
        return f"{type_spelling(self.type_name)} {self.name}{default_value}"

    def definition_text(self) -> str:
        return f"{type_spelling(self.type_name)} {self.name}"


@dataclass(frozen=True)
class ParameterRef(Node):
    parameter: FunctionParameter

    @property
    def cpp_name(self) -> str:
        return self.parameter.name

    @override
    def render(self, context: RenderContext) -> str:
        return context.apply_indent(self.cpp_name)


class FunctionBody(Node, ABC):
    @property
    def function(self) -> MemberFunctionSpec | FreeFunctionSpec:
        function = getattr(self, "_function", None)
        if function is None:
            raise ValueError("Function body must be bound to a function before rendering")
        return function

    def bind(self, function: MemberFunctionSpec | FreeFunctionSpec) -> None:
        owner = getattr(self, "_function", None)
        if owner is not None and owner is not function:
            raise ValueError("Function body is already bound to a different function")
        object.__setattr__(self, "_function", function)


@dataclass(frozen=True)
class MemberFunctionSpec:
    name: str
    return_type: TypeLike
    parameters: tuple[FunctionParameter, ...]
    body: Node
    suffix: str = ""
    is_static: bool = False
    is_inline: bool = False
    template_parameters: str | None = None
    requires_clause: str | None = None

    def __init__(
        self,
        name: str,
        return_type: TypeLike,
        parameters: Iterable[FunctionParameter],
        body: object,
        suffix: str = "",
        is_static: bool = False,
        is_inline: bool = False,
        template_parameters: str | None = None,
        requires_clause: str | None = None,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "return_type", return_type)
        object.__setattr__(self, "parameters", tuple(parameters))
        if not isinstance(body, Node):
            raise TypeError("Function bodies must be codegen nodes; use Raw for literal C++")
        object.__setattr__(self, "body", body)
        object.__setattr__(self, "suffix", suffix)
        object.__setattr__(self, "is_static", is_static)
        object.__setattr__(self, "is_inline", is_inline)
        object.__setattr__(self, "template_parameters", template_parameters)
        object.__setattr__(self, "requires_clause", requires_clause)
        if isinstance(self.body, FunctionBody):
            self.body.bind(self)

    def declaration_node(self) -> FunctionDeclaration:
        return FunctionDeclaration(self)

    def header_node(self) -> Node:
        if self.is_inline:
            return Function(self, is_header=True)
        return self.declaration_node()

    def definition_node(self, owner_name: str) -> Function:
        return Function(self, owner_name=owner_name)

    def parameter(self, parameter: FunctionParameter) -> ParameterRef:
        if not any(candidate is parameter for candidate in self.parameters):
            raise ValueError("Parameter does not belong to this function")
        return ParameterRef(parameter)

    def type_dependencies(self) -> Iterable[TypeDependency]:
        return type_dependencies(self.return_type)

    def _parameters(self, include_defaults: bool) -> str:
        render = FunctionParameter.declaration_text if include_defaults else FunctionParameter.definition_text
        return comma_separated(render(parameter) for parameter in self.parameters)

    def declaration_signature(self) -> str:
        return self._signature(True)

    def definition_signature(self, owner_name: str | None, is_header: bool) -> str:
        return self._signature(is_header, owner_name)

    def _signature(self, include_defaults: bool, owner_name: str | None = None) -> str:
        static_prefix = "static " if self.is_static and owner_name is None else ""
        qualified_name = f"{owner_name}::{self.name}" if owner_name else self.name
        lines: list[str] = []
        if self.template_parameters:
            lines.append(f"template <{self.template_parameters}>")
        if self.requires_clause:
            lines.append(f"requires {self.requires_clause}")
        if uses_trailing_return_type(self.return_type):
            lines.append(
                f"{static_prefix}auto {qualified_name}({self._parameters(include_defaults)})"
                f"{self.suffix} -> {type_spelling(self.return_type)}"
            )
        else:
            lines.append(
                f"{static_prefix}{type_spelling(self.return_type)} {qualified_name}({self._parameters(include_defaults)})"
                f"{self.suffix}"
            )
        return "\n".join(lines)


@dataclass(frozen=True)
class FreeFunctionSpec:
    name: str
    return_type: TypeLike
    parameters: tuple[FunctionParameter, ...]
    body: Node
    suffix: str = ""
    is_inline: bool = False

    def __init__(
        self,
        name: str,
        return_type: TypeLike,
        parameters: Iterable[FunctionParameter],
        body: object,
        suffix: str = "",
        is_inline: bool = False,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "return_type", return_type)
        object.__setattr__(self, "parameters", tuple(parameters))
        if not isinstance(body, Node):
            raise TypeError("Function bodies must be codegen nodes; use Raw for literal C++")
        object.__setattr__(self, "body", body)
        object.__setattr__(self, "suffix", suffix)
        object.__setattr__(self, "is_inline", is_inline)
        if isinstance(self.body, FunctionBody):
            self.body.bind(self)

    def declaration_node(self) -> FunctionDeclaration:
        return FunctionDeclaration(self)

    def header_node(self) -> Node:
        if self.is_inline:
            return Function(self, is_header=True)
        return self.declaration_node()

    def definition_node(self) -> Function:
        return Function(self)

    def parameter(self, parameter: FunctionParameter) -> ParameterRef:
        if not any(candidate is parameter for candidate in self.parameters):
            raise ValueError("Parameter does not belong to this function")
        return ParameterRef(parameter)

    def type_dependencies(self) -> Iterable[TypeDependency]:
        return type_dependencies(self.return_type)

    def _parameters(self, include_defaults: bool) -> str:
        render = FunctionParameter.declaration_text if include_defaults else FunctionParameter.definition_text
        return comma_separated(render(parameter) for parameter in self.parameters)

    def declaration_signature(self) -> str:
        return self._signature(True, False)

    def definition_signature(self, owner_name: str | None, is_header: bool) -> str:
        if owner_name is not None:
            raise ValueError("Free function definitions must not have an owner name")
        return self._signature(is_header, is_header)

    def _signature(self, include_defaults: bool, include_inline: bool) -> str:
        inline_prefix = "inline " if include_inline and self.is_inline else ""
        if uses_trailing_return_type(self.return_type):
            return (
                f"{inline_prefix}auto {self.name}({self._parameters(include_defaults)})"
                f"{self.suffix} -> {type_spelling(self.return_type)}"
            )
        return f"{inline_prefix}{type_spelling(self.return_type)} {self.name}({self._parameters(include_defaults)}){self.suffix}"
