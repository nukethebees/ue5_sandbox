from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Iterable
from typing import override

from Codegen.project_types import qualified_type
from Codegen.cpp import (
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

    @override
    def children(self) -> Iterable[Node]:
        return (self.validation,) if self.validation is not None else ()

    @override
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
    bind_access: str = "public"
    method_access: str = "public"
    friends: tuple[str, ...] = ()
    definitions_in_source: bool = False

    def __init__(
        self,
        name: str,
        target_type: TypeLike,
        target_member_name: str,
        methods: Iterable[FacadeMethod],
        validation: object | None = None,
        export_specifier: str | None = None,
        bind_access: str = "public",
        method_access: str = "public",
        friends: Iterable[str] = (),
        definitions_in_source: bool = False,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "target_type", target_type)
        object.__setattr__(self, "target_member_name", target_member_name)
        object.__setattr__(self, "methods", tuple(methods))
        if validation is not None and not isinstance(validation, Node):
            raise TypeError("Facade validation must be a codegen node")
        object.__setattr__(self, "validation", validation)
        object.__setattr__(self, "export_specifier", export_specifier)
        object.__setattr__(self, "bind_access", bind_access)
        object.__setattr__(self, "method_access", method_access)
        object.__setattr__(self, "friends", tuple(friends))
        object.__setattr__(self, "definitions_in_source", definitions_in_source)
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
        if self.bind_access not in ("public", "private"):
            raise ValueError("Facade bind access must be 'public' or 'private'")
        if self.method_access not in ("public", "private"):
            raise ValueError("Facade method access must be 'public' or 'private'")
        if any(not friend.strip() for friend in self.friends):
            raise ValueError("Facade friend names must not be empty")


@dataclass(frozen=True)
class LoweredFacade:
    header: Struct
    source_nodes: tuple[Node, ...]


def facade_functions(
    facade: Facade,
) -> tuple[MemberFunctionSpec, tuple[MemberFunctionSpec, ...]]:
    target_reference = qualified_type(facade.target_type, "&")
    bind_target = FunctionParameter(target_reference, "new_target")
    bind = MemberFunctionSpec(
        "bind",
        "void",
        (bind_target,),
        Raw(f"{facade.target_member_name} = &{bind_target.name};"),
        is_inline=not facade.definitions_in_source,
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
            is_inline=not facade.definitions_in_source,
        )
        for method in facade.methods
    )
    return bind, forwarding_functions


def append_section(nodes: list[Node], access: str, section_nodes: Iterable[Node]) -> None:
    section_nodes = tuple(section_nodes)
    if not section_nodes:
        return
    nodes.extend((Raw(f"{access}:"), NewLines(1)))
    for index, node in enumerate(section_nodes):
        nodes.append(node)
        if index < len(section_nodes) - 1:
            nodes.append(NewLines(2))


def lower_facade(facade: Facade) -> Struct:
    bind, forwarding_functions = facade_functions(facade)
    nodes: list[Node] = []
    public_nodes: list[Node] = []
    if facade.bind_access == "public":
        public_nodes.append(bind.header_node())
    if facade.method_access == "public":
        public_nodes.extend(function.header_node() for function in forwarding_functions)
    append_section(
        nodes,
        "public",
        public_nodes,
    )
    private_nodes: list[Node] = [Raw(f"friend class {friend};") for friend in facade.friends]
    if facade.bind_access == "private":
        private_nodes.append(bind.header_node())
    if facade.method_access == "private":
        private_nodes.extend(function.header_node() for function in forwarding_functions)
    private_nodes.append(
        Member(
            qualified_type(facade.target_type, "*"),
            facade.target_member_name,
            initializer="nullptr",
        )
    )
    append_section(
        nodes,
        "private",
        private_nodes,
    )
    return Struct(
        facade.name,
        nodes,
        export_specifier=facade.export_specifier,
        record_kind="class",
    )


def lower_facade_with_source(facade: Facade) -> LoweredFacade:
    if not facade.definitions_in_source:
        raise ValueError("Facade definitions must be configured for source output")
    bind, forwarding_functions = facade_functions(facade)
    source_nodes: list[Node] = [bind.definition_node(facade.name), NewLines(2)]
    for index, function in enumerate(forwarding_functions):
        source_nodes.append(function.definition_node(facade.name))
        if index < len(forwarding_functions) - 1:
            source_nodes.append(NewLines(2))
    return LoweredFacade(
        header=lower_facade(facade),
        source_nodes=tuple(source_nodes),
    )
