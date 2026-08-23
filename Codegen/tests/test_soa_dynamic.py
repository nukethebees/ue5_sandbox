import unittest
from typing import override
from Codegen.project_types import F_VECTORS_3F
from Codegen.cpp import (
    ForwardDeclaration,
    FunctionBody,
    FunctionParameter,
    Member,
    MemberFunctionSpec,
    NewLines,
    Node,
    Raw,
    REMOVE_AT_SWAP,
    RenderContext,
    Struct,
    UsingDeclaration,
)
from Codegen.soa import (
    ForEachSoAMemberCall,
    ForEachSoAMemberOperationCall,
    SoAStruct,
    SoAStructNames,
    SoAStorageOperation,
    ForEachSoAMemberFreeFunctionCall,
    ForEachSoAMemberPairFreeFunctionCall,
    lower_soa_struct,
    lower_soa_structs_with_source,
    soa_member,
    tarray_member,
)


class ReturnParameterBody(FunctionBody):
    def __init__(self, parameter: FunctionParameter) -> None:
        self.parameter_to_return = parameter

    @override
    def render(self, context: RenderContext) -> str:
        parameter = self.function.parameter(self.parameter_to_return)
        return Raw(f"return {parameter.cpp_name};").render(context)


class DynamicSoATests(unittest.TestCase):
    def test_generated_soa_container_registers_its_remove_at_swap(self) -> None:
        function = MemberFunctionSpec(
            "remove_at_swap",
            "void",
            (
                FunctionParameter("int32 const", "index"),
                FunctionParameter("int32 const", "count"),
                FunctionParameter("EAllowShrinking const", "allow_shrinking"),
            ),
            ForEachSoAMemberOperationCall(
                (soa_member("locations", F_VECTORS_3F),),
                REMOVE_AT_SWAP,
                "ml::remove_at_swap",
            ),
        )

        self.assertIn(
            "locations.remove_at_swap(index, count, allow_shrinking);",
            function.definition_node("FData").render(RenderContext()),
        )

    def test_member_free_function_call_uses_bound_function_parameters(self) -> None:
        count = FunctionParameter("int32 const", "count")
        function = MemberFunctionSpec(
            "add_defaulted",
            "void",
            (count,),
            ForEachSoAMemberFreeFunctionCall(
                (tarray_member("handles", "int32"), tarray_member("weights", "float")),
                "ml::add_defaulted",
            ),
        )

        self.assertEqual(
            function.definition_node("FData").render(RenderContext()),
            """void FData::add_defaulted(int32 const count) {
    ml::add_defaulted(handles, count);
    ml::add_defaulted(weights, count);
}""",
        )

        count.name = "new_count"
        self.assertIn("new_count);", function.definition_node("FData").render(RenderContext()))

    def test_member_pair_free_function_call_expands_other_members(self) -> None:
        dst_i = FunctionParameter("int32 const", "dst_i")
        other = FunctionParameter("FData const&", "other")
        src_i = FunctionParameter("int32 const", "src_i")
        function = MemberFunctionSpec(
            "copy_element",
            "void",
            (dst_i, other, src_i),
            ForEachSoAMemberPairFreeFunctionCall(
                (tarray_member("handles", "int32"), tarray_member("weights", "float")),
                "ml::copy_element",
                other,
            ),
        )

        self.assertEqual(
            function.definition_node("FData").render(RenderContext()),
            """void FData::copy_element(int32 const dst_i, FData const& other, int32 const src_i) {
    ml::copy_element(handles, dst_i, other.handles, src_i);
    ml::copy_element(weights, dst_i, other.weights, src_i);
}""",
        )

    def test_soa_copy_element_operation_also_generates_copy_elements(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData"),
            (
                tarray_member("values", "int32"),
                soa_member("nested_values", "FNestedValues"),
            ),
            storage_operations=(SoAStorageOperation.COPY_ELEMENT,),
        )

        header = lower_soa_struct(soa)[-1].render(RenderContext())

        self.assertIn(
            "    template <typename Other>\n"
            "    void copy_elements(int32 const dst_i, Other const& other, int32 const src_i, int32 const count)",
            header,
        )
        self.assertIn(
            "ml::copy_elements(values, dst_i, other.values, src_i, count);", header
        )
        self.assertIn(
            "ml::copy_elements(nested_values, dst_i, other.nested_values, src_i, count);",
            header,
        )
        self.assertIn(
            "    template <typename Other>\n    void copy_to_tail(Other const& other)", header
        )
        self.assertIn("check(num() >= count);", header)
        self.assertIn("copy_elements(num() - count, other, 0, count);", header)

    def test_member_pair_free_function_call_rejects_an_unowned_other_parameter(self) -> None:
        function = MemberFunctionSpec(
            "append_from",
            "void",
            (FunctionParameter("FData const&", "other"),),
            ForEachSoAMemberPairFreeFunctionCall(
                (tarray_member("values", "int32"),),
                "ml::append_from",
                FunctionParameter("FData const&", "other"),
            ),
        )

        with self.assertRaisesRegex(ValueError, "does not belong"):
            function.definition_node("FData").render(RenderContext())

    def test_member_function_spec_renders_templated_requires_clause(self) -> None:
        other = FunctionParameter("Other const&", "other")
        function = MemberFunctionSpec(
            "append_from",
            "void",
            (other,),
            ForEachSoAMemberPairFreeFunctionCall(
                (tarray_member("values", "int32"),), "ml::append_from", other
            ),
            is_inline=True,
            template_parameters="typename Other",
            requires_clause="ml::SupportsApplyArrayPairsWith<FData, Other>",
        )

        self.assertEqual(
            function.header_node().render(RenderContext()),
            """template <typename Other>
requires ml::SupportsApplyArrayPairsWith<FData, Other>
void append_from(Other const& other) {
    ml::append_from(values, other.values);
}""",
        )

    def test_soa_storage_operations_generate_direct_member_functions(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData"),
            (tarray_member("values", "int32"),),
            storage_operations=(
                SoAStorageOperation.ADD_DEFAULTED,
                SoAStorageOperation.REMOVE_AT_SWAP,
                SoAStorageOperation.COPY_ELEMENT,
                SoAStorageOperation.APPEND_FROM,
            ),
        )

        lowered = lower_soa_structs_with_source((soa,))
        header = lowered.header_nodes[-1].render(RenderContext())
        source = "\n".join(node.render(RenderContext()) for node in lowered.source_nodes)

        self.assertIn("void add_defaulted(int32 const count);", header)
        self.assertIn("ml::add_defaulted(", source)
        self.assertIn("void remove_at_swap(", header)
        self.assertIn("values.RemoveAtSwap(", header)
        self.assertIn("auto is_empty() const noexcept -> bool;", header)
        self.assertIn("ml::copy_element(", header)
        self.assertIn("ml::copy_elements(", header)
        self.assertIn("template <typename Other>", header)
        self.assertLess(header.index("void add_defaulted"), header.index("apply_arrays"))

    def test_soa_storage_operations_share_header_and_source_specs(self) -> None:
        first = SoAStruct(
            SoAStructNames("FFirst"),
            (tarray_member("values", "int32"),),
            storage_operations=(
                SoAStorageOperation.RESET,
                SoAStorageOperation.COPY_ELEMENT,
            ),
        )
        second = SoAStruct(
            SoAStructNames("FSecond"),
            (tarray_member("weights", "float"),),
            storage_operations=(SoAStorageOperation.ADD_DEFAULTED,),
        )

        lowered = lower_soa_structs_with_source((first, second))
        header = "\n".join(node.render(RenderContext()) for node in lowered.header_nodes)
        source = "\n".join(node.render(RenderContext()) for node in lowered.source_nodes)

        self.assertIn("void reset();", header)
        self.assertIn("void add_defaulted(int32 const count);", header)
        self.assertIn("void FFirst::reset()", source)
        self.assertIn("void FSecond::add_defaulted(int32 const count)", source)
        self.assertNotIn("copy_element", source)

    def test_soa_sort_generates_callable_permutation_api(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData"),
            (
                tarray_member("keys", "int32"),
                soa_member("locations", "FVectors3f"),
                tarray_member("values", "float"),
            ),
        )

        lowered = lower_soa_structs_with_source((soa,))
        header = "\n".join(node.render(RenderContext()) for node in lowered.header_nodes)
        source = "\n".join(node.render(RenderContext()) for node in lowered.source_nodes)

        self.assertIn("void apply_permutation(TArrayView<int32> indices);", header)
        self.assertNotIn("ml::apply_permutation(keys, indices);", header)
        self.assertIn("template <typename Compare>", header)
        self.assertIn("void sort(Compare&& compare, TArrayView<int32> scratch_indices)", header)
        self.assertIn("check(scratch_indices.Num() == n);", header)
        self.assertIn("ml::fill_indices(scratch_indices);", header)
        self.assertIn("scratch_indices.Sort", header)
        self.assertIn("apply_permutation(scratch_indices);", header)
        self.assertNotIn("auto indices{scratch_indices};", header)
        self.assertIn("indices[new_index] is the old row index", header)
        self.assertIn("return compare(*this, lhs, rhs);", header)
        self.assertIn("template <auto Compare>", header)
        self.assertIn("void sort(TArrayView<int32> scratch_indices)", header)
        self.assertIn("return Compare(*this, lhs, rhs);", header)
        self.assertNotIn("TFunction", header)
        self.assertNotIn("std::function", header)
        self.assertIn("void FData::apply_permutation(TArrayView<int32> indices)", source)
        self.assertIn("check(indices.Num() == num());", source)
        self.assertIn("ml::apply_permutation(keys, indices);", source)
        self.assertIn("ml::apply_permutation(locations, indices);", source)
        self.assertIn("ml::apply_permutation(values, indices);", source)

    def test_for_each_soa_member_call_uses_bound_function_parameters(self) -> None:
        handles = tarray_member("handles", "FRegistryEntityHandle")
        weights = tarray_member("weights", "float")
        handle = FunctionParameter("FRegistryEntityHandle const", "handle")
        weight = FunctionParameter("float", "weight")
        function = MemberFunctionSpec(
            "add",
            "void",
            (handle, weight),
            ForEachSoAMemberCall((handles, weights), "Add"),
        )

        self.assertEqual(
            function.definition_node("FData").render(RenderContext()),
            """void FData::add(FRegistryEntityHandle const handle, float weight) {
    handles.Add(handle);
    weights.Add(weight);
}""",
        )

        weight.name = "strength"
        self.assertEqual(
            function.definition_node("FData").render(RenderContext()),
            """void FData::add(FRegistryEntityHandle const handle, float strength) {
    handles.Add(handle);
    weights.Add(strength);
}""",
        )

    def test_for_each_soa_member_call_rejects_mismatched_parameters(self) -> None:
        function = MemberFunctionSpec(
            "add",
            "void",
            (FunctionParameter("int32", "value"),),
            ForEachSoAMemberCall(
                (tarray_member("values", "int32"), tarray_member("weights", "float")),
                "Add",
            ),
        )

        with self.assertRaisesRegex(ValueError, "one parameter per SOA member"):
            function.definition_node("FData").render(RenderContext())

    def test_soa_struct_lowers_to_ordered_generic_declarations(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData", "TDataView", "TDataConstView"),
            (
                tarray_member("values", "int32"),
                soa_member("vectors", "FVectors3f"),
            ),
            equivalent_type="FValue",
        )

        lowered = lower_soa_struct(soa)
        rendered = "\n\n".join(
            node.render(RenderContext())
            for node in lowered
            if not isinstance(node, NewLines)
        )

        self.assertLess(rendered.index("self.values"), rendered.index("self.vectors"))
        self.assertIn("self.values, other.values", rendered)
        self.assertIn("self.vectors, other.vectors", rendered)
        self.assertEqual(rendered.count("auto operator[](int32 const index) const -> FValue"), 3)
        self.assertIn("auto get_view() -> View", rendered)
        self.assertIn("auto slice(int32 const offset, int32 const count) -> View", rendered)
        self.assertIn("auto left(int32 const count) -> View", rendered)
        self.assertIn("auto right(int32 const count) -> View", rendered)
        self.assertNotIn("FSoAViewMixin", rendered)
        self.assertNotIn("FSoAArrayMixin", rendered)
        self.assertNotIn("std::conditional_t", rendered)
        self.assertNotIsInstance(soa, Node)
        self.assertIsInstance(lowered[0], ForwardDeclaration)
        self.assertTrue(all(isinstance(node, (ForwardDeclaration, NewLines, Struct)) for node in lowered))

    def test_struct_rejects_duplicate_member_names(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate members"):
            Struct("FData", (Member("int32", "value"), Member("float", "value")))

    def test_storage_export_and_nodes_are_rendered(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FExportedData"),
            (tarray_member("values", "int32"),),
            storage_export_specifier="SANDBOX_API",
            nodes=(UsingDeclaration("CountType", "int32"),),
        )

        rendered = lower_soa_struct(soa)[-1].render(RenderContext())

        self.assertIn("struct SANDBOX_API FExportedData", rendered)
        self.assertIn("using CountType = int32;", rendered)

    def test_soa_lowering_inserts_nodes_before_generated_functions_and_members(self) -> None:
        soa = SoAStruct(
            SoAStructNames("FData"),
            (tarray_member("values", "FValue"),),
            nodes=(
                MemberFunctionSpec(
                    "reset_values", "void", (), Raw("values.Reset();"), is_inline=True
                ).header_node(),
                NewLines(1),
                MemberFunctionSpec("validate_values", "void", (), Raw("")).declaration_node(),
            ),
        )

        rendered = lower_soa_struct(soa)[-1].render(RenderContext())

        self.assertLess(rendered.index("void reset_values()"), rendered.index("TArray<FValue> values"))
        self.assertLess(rendered.index("void validate_values"), rendered.index("apply_arrays"))
        self.assertLess(rendered.index("apply_array_pairs"), rendered.index("TArray<FValue> values"))
