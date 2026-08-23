import unittest
from typing import override
from Codegen.cpp import (
    FunctionBody,
    FunctionParameter,
    NewLines,
    Raw,
    RenderContext,
    Struct,
)
from Codegen.soa import (
    HomogeneousSoALayout,
    HomogeneousSoAValueType,
    lower_homogeneous_soa_layouts,
    lower_homogeneous_soa_permutation_definitions,
)


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class HomogeneousSoATests(unittest.TestCase):
    def test_homogeneous_soa_layout_lowers_to_generic_struct_nodes(self) -> None:
        layout = HomogeneousSoALayout(
            "Values",
            ("xs", "ys"),
            (
                HomogeneousSoAValueType(
                    "float", "f", "FVector2f", ("FVector2f",)
                ),
            ),
        )

        nodes = lower_homogeneous_soa_layouts((layout,))
        rendered = "\n".join(node.render(RenderContext()) for node in nodes)

        self.assertTrue(all(isinstance(node, (NewLines, Raw, Struct)) for node in nodes))
        self.assertIn("template <typename T>", rendered)
        self.assertIn("struct TValuesEquivalentType<float>", rendered)
        self.assertIn("struct TValuesView", rendered)
        self.assertIn("struct FValuesf", rendered)
        self.assertIn("auto is_empty() const -> bool", rendered)
        self.assertIn(
            "    template <typename Other>\n"
            "    auto copy_elements(size_type const dst_i, Other const& src, size_type const src_i, size_type const count) -> void",
            rendered,
        )
        self.assertIn("for (auto i{0}; i < count; ++i)", rendered)
        self.assertIn("xs[dst_i + i] = src.xs[src_i + i];", rendered)
        self.assertIn("ys[dst_i + i] = src.ys[src_i + i];", rendered)
        self.assertIn(
            "    template <typename Other>\n    auto copy_to_tail(Other const& src) -> void",
            rendered,
        )
        self.assertIn("copy_elements(num() - count, src, 0, count);", rendered)
        self.assertIn("using equivalent_type = FVector2f;", rendered)
        self.assertIn("auto operator[](size_type const index) const -> equivalent_type", rendered)
        self.assertIn("auto operator[](size_type const index) const -> FVector2f", rendered)
        self.assertIn("auto add(value_type const x, value_type const y) -> size_type", rendered)

        source_nodes = lower_homogeneous_soa_permutation_definitions((layout,))
        source = "\n".join(node.render(RenderContext()) for node in source_nodes)
        self.assertIn("void FValuesf::apply_permutation(TArrayView<int32> indices)", source)
        self.assertIn("ml::apply_permutation(xs, indices);", source)
        self.assertIn("ml::apply_permutation(ys, indices);", source)
