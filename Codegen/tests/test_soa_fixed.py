import unittest
from typing import override
from Codegen.cpp import (
    FunctionBody,
    FunctionParameter,
    Raw,
    RenderContext,
)
from Codegen.soa import (
    FixedSoAConfig,
    FixedSoAContainer,
    SoAStruct,
    SoAStructNames,
    lower_soa_struct,
    soa_member,
    tarray_member,
)
from Codegen.soa.fixed import build_fixed_layout


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class FixedSoATests(unittest.TestCase):
    def test_fixed_layout_normalises_recursive_leaves_once_in_declaration_order(self) -> None:
        child = SoAStruct(
            SoAStructNames("FChild"),
            (tarray_member("names", "FString"), tarray_member("weights", "float")),
            fixed=FixedSoAConfig("TChildStorage"),
        )
        parent = SoAStruct(
            SoAStructNames("FParent"),
            (
                tarray_member("ids", "int32"),
                soa_member("children", "FChild", fixed_schema=child),
            ),
            fixed=FixedSoAConfig("TParentStorage"),
        )

        layout = build_fixed_layout(parent)

        self.assertEqual(
            tuple(leaf.path for leaf in layout.leaves),
            (("ids",), ("children", "names"), ("children", "weights")),
        )
        self.assertEqual(
            tuple(leaf.argument_name for leaf in layout.leaves),
            ("ids", "children_names", "children_weights"),
        )
        self.assertEqual(layout.members[1].storage_type, "TChildStorage<Capacity>")

    def test_fixed_soa_arrays_generate_multiple_owners_with_one_size_each(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData"),
            (tarray_member("values", "int32"), tarray_member("weights", "float")),
            fixed=FixedSoAConfig(
                "TDataFixedStorage",
                (
                    FixedSoAContainer("TFirstFixedData"),
                    FixedSoAContainer("TSecondFixedData"),
                ),
            ),
        )

        rendered = "\n".join(node.render(RenderContext()) for node in lower_soa_struct(soa))

        self.assertIn("struct TDataFixedStorage", rendered)
        self.assertIn("ml::TFixedStorage<int32, Capacity> values_;", rendered)
        self.assertIn("ml::TFixedStorage<float, Capacity> weights_;", rendered)
        self.assertIn("struct TFirstFixedData", rendered)
        self.assertIn("struct TSecondFixedData", rendered)
        self.assertEqual(rendered.count("size_type size_{};"), 2)
        self.assertNotIn("TFixedArray<", rendered)

    def test_fixed_soa_storage_recurses_without_nested_sizes(self) -> None:
        child = SoAStruct(
            SoAStructNames("FChild"),
            (tarray_member("names", "FString"),),
            fixed=FixedSoAConfig("TChildFixedStorage"),
        )
        parent = SoAStruct(
            SoAStructNames("FParent"),
            (
                soa_member("children", "FChild", fixed_schema=child),
                tarray_member("ids", "int32"),
            ),
            fixed=FixedSoAConfig(
                "TParentFixedStorage",
                (FixedSoAContainer("TParentFixedArray"),),
            ),
        )

        rendered = "\n".join(
            node.render(RenderContext()) for node in lower_soa_struct(parent)
        )

        self.assertIn("TChildFixedStorage<Capacity> children_;", rendered)
        self.assertIn("children_.construct_at(index", rendered)
        self.assertEqual(rendered.count("size_type size_{};"), 1)
        self.assertIn("new_children_names", rendered)

    def test_fixed_soa_storage_rejects_opaque_nested_members(self) -> None:
        with self.assertRaisesRegex(ValueError, "without an element type or fixed schema"):
            SoAStruct(
                SoAStructNames("FData"),
                (soa_member("nested", "FNested"),),
                fixed=FixedSoAConfig("TDataFixedStorage"),
            )

    def test_fixed_soa_config_requires_storage_and_unique_names(self) -> None:
        with self.assertRaisesRegex(ValueError, "storage name"):
            FixedSoAConfig("", (FixedSoAContainer("TFixedData"),))
        with self.assertRaisesRegex(ValueError, "must not contain duplicates"):
            FixedSoAConfig(
                "TDataFixedStorage",
                (
                    FixedSoAContainer("TFixedData"),
                    FixedSoAContainer("TFixedData"),
                ),
            )
