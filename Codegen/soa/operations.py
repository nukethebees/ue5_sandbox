from __future__ import annotations

from collections.abc import Iterable
from typing import override

from Codegen.cpp import (
    FunctionBody,
    FunctionParameter,
    Raw,
    RenderContext,
    TypeDependency,
    comma_separated,
)
from Codegen.soa.model import SoAMember


CONTAINER_OPS = TypeDependency("ml::num", "SandboxCore/container_ops.h")
SOA_PERMUTATION = TypeDependency("ml::apply_permutation", "SandboxCore/soa_permutation.h")
CHECK = TypeDependency("check", "CoreMinimal.h")


class ForEachSoAMemberCall(FunctionBody):
    def __init__(self, members: Iterable[SoAMember], method_name: str) -> None:
        self.members = tuple(members)
        self.method_name = method_name
        if not self.members:
            raise ValueError("ForEachSoAMemberCall requires at least one SOA member")
        if not self.method_name.strip():
            raise ValueError("ForEachSoAMemberCall method name must not be empty")

    @override
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

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return (CONTAINER_OPS,)

    @override
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

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return (CONTAINER_OPS,)

    @override
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

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return self.required_dependencies

    @override
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

    @override
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

    @override
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

    @override
    def dependencies(self) -> Iterable[TypeDependency]:
        return (SOA_PERMUTATION, CHECK)

    @override
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
