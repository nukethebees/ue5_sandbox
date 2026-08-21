from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Iterable

from Codegen.cpp_types import qualified_type
from Codegen.nodes import (
    FunctionBody,
    FunctionParameter,
    Member,
    MemberFunctionSpec,
    NewLines,
    Node,
    Raw,
    RenderContext,
    Struct,
    TypeLike,
    type_spelling,
)


class FacadeForwardingBody(FunctionBody):
    def __init__(
        self,
        target_member_name: str,
        target_function_name: str,
        validation: Node | None,
    ) -> None:
        self.target_member_name = target_member_name
        self.target_function_name = target_function_name
        self.validation = validation

    def children(self) -> Iterable[Node]:
        return (self.validation,) if self.validation is not None else ()

    def render(self, context: RenderContext) -> str:
        arguments = ", ".join(parameter.name for parameter in self.function.parameters)
        call = (
            f"{self.target_member_name}->{self.target_function_name}({arguments});"
        )
        if type_spelling(self.function.return_type) != "void":
            call = f"return {call}"

        nodes: tuple[Node, ...] = (
            *((self.validation,) if self.validation is not None else ()),
            Raw(call),
        )
        return "\n".join(node.render(context) for node in nodes)


@dataclass(frozen=True)
class FacadeMethod:
    name: str
    return_type: TypeLike
    parameters: tuple[FunctionParameter, ...]
    suffix: str = ""
    target_name: str | None = None

    def __init__(
        self,
        name: str,
        return_type: TypeLike,
        parameters: Iterable[FunctionParameter],
        suffix: str = "",
        target_name: str | None = None,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "return_type", return_type)
        object.__setattr__(self, "parameters", tuple(parameters))
        object.__setattr__(self, "suffix", suffix)
        object.__setattr__(self, "target_name", target_name)
        if not self.name.strip():
            raise ValueError("Facade method name must not be empty")
        if self.target_name is not None and not self.target_name.strip():
            raise ValueError("Facade target method name must not be empty")


@dataclass(frozen=True)
class Facade:
    name: str
    target_type: TypeLike
    target_member_name: str
    methods: tuple[FacadeMethod, ...]
    validation: Node | None = None
    export_specifier: str | None = None

    def __init__(
        self,
        name: str,
        target_type: TypeLike,
        target_member_name: str,
        methods: Iterable[FacadeMethod],
        validation: Node | None = None,
        export_specifier: str | None = None,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "target_type", target_type)
        object.__setattr__(self, "target_member_name", target_member_name)
        object.__setattr__(self, "methods", tuple(methods))
        object.__setattr__(self, "validation", validation)
        object.__setattr__(self, "export_specifier", export_specifier)
        if not self.name.strip() or not self.target_member_name.strip():
            raise ValueError("Facade and target member names must not be empty")
        if not self.methods:
            raise ValueError("Facade must expose at least one method")
        method_names = [method.name for method in self.methods]
        duplicates = sorted({name for name in method_names if method_names.count(name) > 1})
        if duplicates:
            raise ValueError(
                f"Facade {self.name!r} has duplicate methods: {', '.join(duplicates)}"
            )
        if self.validation is not None and not isinstance(self.validation, Node):
            raise TypeError("Facade validation must be a codegen node")


def lower_facade(facade: Facade) -> Struct:
    target_reference = qualified_type(facade.target_type, "&")
    bind_target = FunctionParameter(target_reference, "new_target")
    bind = MemberFunctionSpec(
        "bind",
        "void",
        (bind_target,),
        Raw(f"{facade.target_member_name} = &{bind_target.name};"),
        is_inline=True,
    )
    forwarding_functions = tuple(
        MemberFunctionSpec(
            method.name,
            method.return_type,
            method.parameters,
            FacadeForwardingBody(
                facade.target_member_name,
                method.target_name or method.name,
                facade.validation,
            ),
            suffix=method.suffix,
            is_inline=True,
        )
        for method in facade.methods
    )
    nodes: list[Node] = [Raw("public:"), NewLines(1), bind.header_node(), NewLines(2)]
    for function in forwarding_functions:
        nodes.extend((function.header_node(), NewLines(2)))
    nodes.extend(
        (
            Raw("private:"),
            NewLines(1),
            Member(
                qualified_type(facade.target_type, "*"),
                facade.target_member_name,
                initializer="nullptr",
            ),
        )
    )
    return Struct(
        facade.name,
        nodes,
        export_specifier=facade.export_specifier,
        record_kind="class",
    )
