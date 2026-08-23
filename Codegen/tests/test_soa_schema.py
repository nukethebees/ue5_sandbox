import unittest
from typing import override
from Codegen.cpp import (
    CppType,
    FunctionBody,
    FunctionParameter,
    MemberFunctionOperation,
    MemberFunctionSpec,
    Raw,
    REMOVE_AT_SWAP,
    RenderContext,
    TypeDependency,
    type_spelling,
)
from Codegen.soa import (
    ForEachSoAMemberOperationCall,
    SoAStruct,
    SoAStructNames,
    lower_soa_struct,
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


class SoASchemaTests(unittest.TestCase):
    def test_type_directed_operation_uses_registered_member_function(self) -> None:
        function = MemberFunctionSpec(
            "remove_at_swap",
            "void",
            (
                FunctionParameter("int32 const", "index"),
                FunctionParameter("int32 const", "count"),
                FunctionParameter("EAllowShrinking const", "allow_shrinking"),
            ),
            ForEachSoAMemberOperationCall(
                (tarray_member("values", "int32"),),
                REMOVE_AT_SWAP,
                "ml::remove_at_swap",
            ),
        )

        self.assertIn(
            "values.RemoveAtSwap(index, count, allow_shrinking);",
            function.definition_node("FData").render(RenderContext()),
        )

    def test_type_directed_operation_uses_generic_fallback(self) -> None:
        function = MemberFunctionSpec(
            "remove_at_swap",
            "void",
            (
                FunctionParameter("int32 const", "index"),
                FunctionParameter("int32 const", "count"),
                FunctionParameter("EAllowShrinking const", "allow_shrinking"),
            ),
            ForEachSoAMemberOperationCall(
                (soa_member("values", "FNestedValues"),),
                REMOVE_AT_SWAP,
                "ml::remove_at_swap",
            ),
        )

        self.assertIn(
            "ml::remove_at_swap(values, index, count, allow_shrinking);",
            function.definition_node("FData").render(RenderContext()),
        )

    def test_operation_lookup_does_not_use_type_dependencies(self) -> None:
        dependency = TypeDependency("FSharedDependency", "SharedDependency.h")
        operation = MemberFunctionOperation("RemoveAtSwap")
        direct_type = CppType(
            "FValues",
            (dependency,),
            {REMOVE_AT_SWAP: operation},
        )
        fallback_type = CppType("FValues", (dependency,))

        self.assertIs(direct_type.operation(REMOVE_AT_SWAP), operation)
        self.assertIsNone(fallback_type.operation(REMOVE_AT_SWAP))

    def test_tarray_types_share_construction_without_template_awareness(self) -> None:
        floats = tarray_member("floats", "float").container_type
        tasks = tarray_member("tasks", "ETask").container_type

        self.assertIsNot(floats, tasks)
        self.assertEqual(floats.spelling, "TArray<float>")
        self.assertEqual(tasks.spelling, "TArray<ETask>")
        self.assertIs(
            floats.operation(REMOVE_AT_SWAP), tasks.operation(REMOVE_AT_SWAP)
        )

    def test_function_parameter_stores_a_cpp_type(self) -> None:
        parameter = FunctionParameter("int32 const", "count")

        self.assertIsInstance(parameter.type_name, CppType)
        self.assertEqual(parameter.type_name.spelling, "int32 const")

    def test_tarray_member_derives_storage_and_view_types(self) -> None:
        member = tarray_member("values", "int32")

        self.assertEqual(type_spelling(member.container_type), "TArray<int32>")
        self.assertEqual(type_spelling(member.view_type), "TArrayView<int32>")
        self.assertEqual(type_spelling(member.const_view_type), "TConstArrayView<int32>")

    def test_soa_struct_names_derive_and_override_view_names(self) -> None:
        default_names = SoAStructNames("FData")
        overridden_names = SoAStructNames("FData", "TDataView", "TDataConstView")

        self.assertEqual(default_names.view_name, "FDataView")
        self.assertEqual(default_names.const_view_name, "FDataConstView")
        self.assertEqual(overridden_names.view_name, "TDataView")
        self.assertEqual(overridden_names.const_view_name, "TDataConstView")

        lowered = lower_soa_struct(
            SoAStruct(overridden_names, (tarray_member("values", "int32"),))
        )
        rendered = "\n".join(node.render(RenderContext()) for node in lowered)
        self.assertIn("struct TDataView", rendered)
        self.assertIn("struct TDataConstView", rendered)

    def test_soa_struct_names_reject_empty_names(self) -> None:
        with self.assertRaisesRegex(ValueError, "SOA struct name"):
            SoAStructNames("")
        with self.assertRaisesRegex(ValueError, "SOA view struct names"):
            SoAStructNames("FData", "")

    def test_invalid_members_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "must not be empty"):
            tarray_member("", "int32")

        member = tarray_member("values", "int32")
        with self.assertRaisesRegex(ValueError, "duplicate members"):
            SoAStruct(SoAStructNames("FData"), (member, member))

        with self.assertRaisesRegex(ValueError, "at least one member"):
            SoAStruct(SoAStructNames("FData"), ())
