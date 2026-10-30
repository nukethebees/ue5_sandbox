#include "test_renderer.h"

#include "lowering.h"
#include "reference_evaluator.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/unary_operator.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/strings.h>

#include <cassert>
#include <cctype>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kernel_codegen::detail {
namespace {

auto pascal_case(std::string_view const name) -> std::string {
    std::string result;
    bool uppercase{true};
    for (unsigned char const character : name) {
        if (!std::isalnum(character)) {
            uppercase = true;
        } else {
            result += uppercase ? static_cast<char>(std::toupper(character))
                                : static_cast<char>(character);
            uppercase = false;
        }
    }
    return result;
}

auto span_argument(lispb::codegen::CppIR& cpp, std::string const& type, lispb::StringId const name)
    -> lispb::codegen::Expr {
    auto const data{cpp.call(cpp.member_access(cpp.named(name), "data"))};
    return cpp.init_list({data, cpp.named("Count")}, cpp.types.intern("std::span<" + type + ">"));
}

auto test_argument(lispb::codegen::CppIR& cpp,
                   Document const& document,
                   ExpandedVariantId const id,
                   std::size_t const operand_index) -> lispb::codegen::Expr {
    auto const& expanded{document.expanded_variants};
    auto const name{document.operands.name(
        document.operations.operands(expanded.operation(id))[operand_index])};
    if (expanded.storage(id)[operand_index] == StorageKind::scalar) {
        return cpp.named(name);
    }
    auto const type{standard_type(expanded.type(id))};
    return span_argument(
        cpp, is_target(document, id, operand_index) ? type : type + " const", name);
}

auto test_arguments(lispb::codegen::CppIR& cpp,
                    Document const& document,
                    ExpandedVariantId const id) -> std::vector<lispb::codegen::Expr> {
    auto const& expanded{document.expanded_variants};
    std::vector<lispb::codegen::Expr> result;
    for (std::size_t index{}; index < document.operations.operands(expanded.operation(id)).size();
         ++index) {
        result.push_back(test_argument(cpp, document, id, index));
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        result.push_back(span_argument(cpp,
                                       standard_type(expanded.type(id)),
                                       document.operations.output(expanded.operation(id))));
    }
    return result;
}

auto reference_expression(lispb::codegen::CppIR& cpp,
                          lispb::codegen::CppTypeId const type,
                          ReferenceValue const& value) -> lispb::codegen::Expr {
    if (auto const* literal{std::get_if<std::string>(&value)}) {
        if (cpp.types.spelling(type) == "float") {
            return cpp.literal(*literal +
                               (literal->find_first_of(".eE") == std::string::npos ? ".0f" : "f"));
        }
        return cpp.literal(*literal);
    }

    auto const constant{std::get<ConstantKind>(value)};
    auto const prefix{"std::numeric_limits<" + std::string{cpp.types.spelling(type)} + ">::"};
    if (constant == ConstantKind::negative_infinity) {
        return cpp.raw_expr("-" + prefix + "infinity()");
    }
    return cpp.call(cpp.named(prefix + (constant == ConstantKind::nan ? "quiet_NaN" : "infinity")));
}

auto array_initializer(lispb::codegen::CppIR& cpp,
                       lispb::codegen::CppTypeId const type,
                       std::vector<ReferenceValue> const& values) -> lispb::codegen::Expr {
    std::vector<lispb::codegen::Expr> elements;
    elements.reserve(values.size());
    for (auto const& value : values) {
        elements.push_back(reference_expression(cpp, type, value));
    }
    return cpp.init_list(elements);
}

struct FixtureArray {
    lispb::codegen::CppTypeId type;
    lispb::StringId name;
    lispb::codegen::CppTypeId symbol;
    std::vector<ReferenceValue> const* values;
};

auto fixture_array(lispb::codegen::CppIR& cpp,
                   std::vector<FixtureArray>& arrays,
                   std::string_view const suite,
                   lispb::codegen::CppTypeId const type,
                   std::vector<ReferenceValue> const& values) -> lispb::codegen::Expr {
    for (auto const& array : arrays) {
        if (array.type == type && *array.values == values) {
            return cpp.type_reference(array.symbol);
        }
    }

    auto const name{"fixture_values_" + std::to_string(arrays.size())};
    auto const symbol{cpp.types.named(std::string{suite} + "::" + name)};
    arrays.push_back({type, cpp.strings.intern(name), symbol, &values});
    return cpp.type_reference(symbol);
}

void append_case(lispb::codegen::CppIR& cpp,
                 lispb::codegen::Nodes& output,
                 std::vector<FixtureArray>& arrays,
                 std::string_view const suite,
                 EmissionId const emission,
                 Document const& document,
                 ExpandedVariantId const id,
                 ReferenceFixture const& fixture) {
    auto const& expanded{document.expanded_variants};
    auto const type{standard_type(expanded.type(id))};
    auto const element_type{cpp.types.intern(type)};
    auto const count{fixture.expected.size()};
    auto const array_type{
        cpp.types.intern("std::array<" + type + ", " + std::to_string(count) + ">")};
    auto& body{output};
    for (std::size_t index{}; index < document.operations.operands(expanded.operation(id)).size();
         ++index) {
        auto const operand{document.operations.operands(expanded.operation(id))[index]};
        if (expanded.storage(id)[index] == StorageKind::array) {
            body.push_back(cpp.variable(
                cpp.types.intern("auto"),
                document.operands.name(operand),
                fixture_array(cpp, arrays, suite, element_type, fixture.operands[index])));
        } else {
            body.push_back(cpp.variable(
                cpp.types.intern("auto const"),
                document.operands.name(operand),
                cpp.static_cast_expr(
                    element_type,
                    reference_expression(cpp, element_type, fixture.operands[index][0]))));
        }
    }

    auto const destination{
        document.variants.kind(expanded.variant(id)) == VariantKind::in_place
            ? document.operands.name(*document.variants.target(expanded.variant(id)))
            : document.operations.output(expanded.operation(id))};
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        body.push_back(cpp.variable(array_type, destination));
    }
    body.push_back(cpp.variable("auto const&",
                                "expected",
                                fixture_array(cpp, arrays, suite, element_type, fixture.expected)));

    auto const function{cpp.named(document.text(document.emissions.cpp_namespace(emission)) + "::" +
                                  document.text(document.variants.name(expanded.variant(id))))};
    body.push_back(cpp.expr_stmt(cpp.call(function, test_arguments(cpp, document, id))));

    auto const expect_prefix{
        cpp.type_reference(cpp.types.named(std::string{suite} + "::expect_prefix"))};
    body.push_back(
        cpp.expr_stmt(cpp.call(expect_prefix,
                               {cpp.call(cpp.member_access(cpp.named(destination), "data")),
                                cpp.call(cpp.member_access(cpp.named("expected"), "data")),
                                cpp.named("Count")})));
}

}

auto lower_standard_tests(lispb::codegen::CppIR& cpp,
                          EmissionId const emission,
                          ModuleId const module,
                          Document const& document,
                          std::span<ReferenceFixture const> const fixtures)
    -> lispb::codegen::CppFileId {
    lispb::codegen::NodeListBuilder file_nodes{cpp};
    file_nodes
        .add(cpp.include(document.emissions.header_include(emission),
                         lispb::codegen::IncludeStyle::quoted),
             2)
        .add(cpp.include(cpp.strings.intern("gtest/gtest.h"), lispb::codegen::IncludeStyle::system),
             2);
    for (auto const include : {"array",
                               "cmath",
                               "cstddef",
                               "cstdint",
                               "limits",
                               "span",
                               "string",
                               "tuple",
                               "type_traits"}) {
        file_nodes.add(
            cpp.include(cpp.strings.intern(include), lispb::codegen::IncludeStyle::system));
    }
    file_nodes.new_lines(2);
    auto const suite{"Generated" + pascal_case(document.text(document.modules.name(module)))};
    auto const namespace_name{suite + "Tests"};
    auto const expect_value{cpp.function("expect_value", "void")};
    cpp.function_flags(expect_value).set_define_in_header(true);
    cpp.set_function_template(
        expect_value,
        cpp.template_declaration({cpp.type_template_parameter(cpp.strings.intern("T"))}));
    cpp.set_function_parameters(
        expect_value, {cpp.parameter("T const", "actual"), cpp.parameter("T const", "expected")});
    cpp.set_function_body(expect_value, {cpp.raw(R"(if constexpr (std::is_floating_point_v<T>) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual));
    } else if (std::isinf(expected)) {
        EXPECT_EQ(actual, expected);
    } else if constexpr (std::is_same_v<T, float>) {
        EXPECT_FLOAT_EQ(actual, expected);
    } else {
        EXPECT_DOUBLE_EQ(actual, expected);
    }
} else {
    EXPECT_EQ(actual, expected);
}
)")});
    auto const expect_prefix{cpp.function("expect_prefix", "void")};
    cpp.function_flags(expect_prefix).set_define_in_header(true);
    cpp.set_function_template(
        expect_prefix,
        cpp.template_declaration({cpp.type_template_parameter(cpp.strings.intern("T"))}));
    cpp.set_function_parameters(expect_prefix,
                                {cpp.parameter("T const*", "actual"),
                                 cpp.parameter("T const*", "expected"),
                                 cpp.parameter("std::size_t const", "count")});
    auto const index{cpp.named("i")};
    cpp.set_function_body(
        expect_prefix,
        {cpp.for_stmt(
            cpp.variable("std::size_t", "i"),
            cpp.binary(lispb::codegen::BinaryOperator::less, index, cpp.named("count")),
            cpp.unary(lispb::codegen::UnaryOperator::preincrement, index),
            cpp.block({cpp.expr_stmt(cpp.call(cpp.named("SCOPED_TRACE"), {index})),
                       cpp.expr_stmt(cpp.call(cpp.named("expect_value"),
                                              {cpp.subscript(cpp.named("actual"), index),
                                               cpp.subscript(cpp.named("expected"), index)}))}))});
    lispb::codegen::Nodes cases;
    std::vector<lispb::codegen::Expr> case_values;
    std::vector<FixtureArray> arrays;
    auto const range{document.modules.expanded_variants(module)};
    assert(fixtures.size() == range.count);
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        auto const name{raw_name(document, id)};
        auto const check{cpp.function(name, "void")};
        cpp.function_flags(check).set_define_in_header(true);
        cpp.set_function_parameters(check, {cpp.parameter("std::size_t const", "Count")});
        lispb::codegen::Nodes body;
        append_case(cpp,
                    body,
                    arrays,
                    namespace_name,
                    emission,
                    document,
                    id,
                    fixtures[variant_index - range.offset]);
        cpp.set_function_body(check, body);
        cases.push_back(cpp.header_function(check));
        cases.push_back(cpp.lines(2));
        case_values.push_back(
            cpp.init_list({cpp.string_literal(pascal_case(name)), cpp.named(name)}));
    }
    lispb::codegen::Nodes declarations{cpp.header_function(expect_value),
                                       cpp.lines(2),
                                       cpp.header_function(expect_prefix),
                                       cpp.lines(2)};
    for (auto const& array : arrays) {
        declarations.push_back(
            cpp.variable(cpp.types.intern("inline constexpr std::array<" +
                                          std::string{cpp.types.spelling(array.type)} + ", " +
                                          std::to_string(array.values->size()) + ">"),
                         array.name,
                         array_initializer(cpp, array.type, *array.values)));
        declarations.push_back(cpp.lines(2));
    }
    declarations.insert(declarations.end(), cases.begin(), cases.end());
    declarations.push_back(cpp.using_declaration(cpp.strings.intern("KernelCheck"),
                                                 cpp.types.intern("void (*)(std::size_t)")));
    declarations.push_back(cpp.lines(2));
    declarations.push_back(cpp.record(
        cpp.strings.intern("KernelCase"),
        {cpp.instance_member(cpp.types.intern("char const*"), cpp.strings.intern("name")),
         cpp.instance_member(cpp.types.intern("KernelCheck"), cpp.strings.intern("run"))}));
    declarations.push_back(cpp.lines(2));
    declarations.push_back(
        cpp.variable(cpp.types.intern("inline constexpr std::array<KernelCase, " +
                                      std::to_string(case_values.size()) + ">"),
                     cpp.strings.intern("cases"),
                     cpp.init_list({cpp.init_list(case_values)})));
    declarations.push_back(cpp.lines(2));
    declarations.push_back(cpp.using_declaration(
        cpp.strings.intern(suite),
        cpp.types.intern("::testing::TestWithParam<std::tuple<KernelCase, std::size_t>>")));
    declarations.push_back(cpp.lines(2));
    declarations.push_back(cpp.raw("TEST_P(" + suite + R"(, MatchesReference) {
    auto const& [test, count]{GetParam()};
    test.run(count);
}
INSTANTIATE_TEST_SUITE_P(Kernels, )" +
                                   suite +
                                   ", ::testing::Combine(::testing::ValuesIn(cases), "
                                   "::testing::Values<std::size_t>(0, 1, 7, 8, 9, 31)), " +
                                   R"(([](auto const& info) {
    auto const& [test, count]{info.param};
    return std::string{test.name} + "Count" + std::to_string(count);
}));
)"));
    file_nodes.add(cpp.namespace_node(cpp.strings.intern(namespace_name), declarations));
    auto const file{cpp.files.append(*document.emissions.tests(emission), file_nodes.build())};
    auto& flags{cpp.files.flags[cpp.files.index(file)]};
    flags.set_pragma_once(false);
    flags.set_format_generated(true);
    return file;
}

}
