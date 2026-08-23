import unittest
from pathlib import Path
from typing import override

from Codegen.generate import collect_files, render_files
from Codegen.manifest import modules
from Codegen.cpp import (
    CppFile,
    FunctionBody,
    FunctionParameter,
    Module,
    Raw,
    RenderContext,
)


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class GenerationTests(unittest.TestCase):
    def test_rendering_is_deterministic(self) -> None:
        modules = (Module("empty", header=CppFile(Path("Empty.h"))),)

        first = render_files(modules)
        second = render_files(modules)

        self.assertEqual(first, second)

    def test_manifest_matches_committed_generated_files(self) -> None:
        for generated_file in render_files(modules()):
            self.assertTrue(generated_file.file.path.exists())
            self.assertEqual(
                generated_file.file.path.read_text(encoding="utf-8"),
                generated_file.content,
            )

    def test_duplicate_output_paths_are_rejected(self) -> None:
        modules = (
            Module("first", header=CppFile(Path("Same.h"))),
            Module("second", header=CppFile(Path("Same.h"))),
        )

        with self.assertRaisesRegex(ValueError, "Duplicate generated output paths"):
            collect_files(modules)
