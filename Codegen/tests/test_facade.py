import unittest
from pathlib import Path
from typing import override

from Codegen.facade import Facade, FacadeMethod, lower_facade, lower_facade_with_source
from Codegen.cpp import (
    CppFile,
    FunctionBody,
    FunctionParameter,
    IncludeDependencies,
    Namespace,
    NewLines,
    Raw,
    RenderContext,
    TypeDependency,
)


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class FacadeTests(unittest.TestCase):
    def test_facade_generates_bound_checked_forwarding_methods(self) -> None:
        target = TypeDependency("FTarget", "Project/Target.h")
        facade = Facade(
            "TargetInterface",
            target,
            "target",
            (
                FacadeMethod(
                    "set_value",
                    "void",
                    (FunctionParameter("int32 const", "value"),),
                ),
                FacadeMethod(
                    "get_value",
                    "FValue const&",
                    (),
                    suffix=" const noexcept",
                ),
            ),
            validation=Raw("check(target != nullptr);", (TypeDependency("check", "CoreMinimal.h"),)),
            export_specifier="PROJECT_API",
        )
        file = CppFile(
            Path("TargetInterface.h"),
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("project", (lower_facade(facade),)),
            ),
        )

        rendered = file.render()
        self.assertIn('#include "Project/Target.h"', rendered)
        self.assertIn('#include "CoreMinimal.h"', rendered)
        self.assertIn("class PROJECT_API TargetInterface", rendered)
        self.assertIn("void bind(FTarget& new_target)", rendered)
        self.assertIn("target = &new_target;", rendered)
        self.assertIn("void set_value(int32 const value)", rendered)
        self.assertIn("target->set_value(value);", rendered)
        self.assertIn("auto get_value() const noexcept -> FValue const&", rendered)
        self.assertIn("return target->get_value();", rendered)
        self.assertIn("private:\n    FTarget* target{nullptr};", rendered)

    def test_facade_can_define_private_methods_in_a_source_file(self) -> None:
        facade = Facade(
            "PhaseInterface",
            "ATarget",
            "target",
            (FacadeMethod("update", "void", (FunctionParameter("float const", "dt"),)),),
            validation=Raw("check(IsValid(target));"),
            bind_access="private",
            method_access="private",
            friends=("ATarget", "ATestBatchOrchestrator"),
            definitions_in_source=True,
        )
        lowered = lower_facade_with_source(facade)

        header = lowered.header.render(RenderContext())
        source = "\n".join(node.render(RenderContext()) for node in lowered.source_nodes)

        self.assertIn("friend class ATarget;", header)
        self.assertIn("friend class ATestBatchOrchestrator;", header)
        self.assertIn("void bind(ATarget& new_target);", header)
        self.assertIn("void update(float const dt);", header)
        self.assertNotIn("void update(float const dt) {", header)
        self.assertIn("void PhaseInterface::bind(ATarget& new_target)", source)
        self.assertIn("void PhaseInterface::update(float const dt)", source)
        self.assertIn("check(IsValid(target));", source)
