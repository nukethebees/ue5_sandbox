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
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace kernel_codegen::detail {
namespace {

auto span_argument(lispb::codegen::CppIR& cpp,
                   std::string const& type,
                   lispb::IdentifierId const name) -> lispb::codegen::Expr {
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

auto initializer(lispb::codegen::CppIR& cpp, std::vector<std::string> const& values)
    -> lispb::codegen::Expr {
    std::vector<lispb::codegen::Expr> elements;
    elements.reserve(values.size());
    for (auto const& value : values) {
        elements.push_back(cpp.raw_expression(value));
    }
    return cpp.init_list(elements);
}

void append_case(lispb::codegen::CppIR& cpp,
                 lispb::codegen::Nodes& output,
                 EmissionId const emission,
                 Document const& document,
                 ExpandedVariantId const id,
                 ReferenceFixture const& fixture) {
    auto const& expanded{document.expanded_variants};
    auto const type{standard_type(expanded.type(id))};
    auto const count{fixture.expected.size()};
    auto const array_type{
        cpp.types.intern("std::array<" + type + ", " + std::to_string(count) + ">")};
    lispb::codegen::Nodes body;
    for (std::size_t index{}; index < document.operations.operands(expanded.operation(id)).size();
         ++index) {
        auto const operand{document.operations.operands(expanded.operation(id))[index]};
        if (expanded.storage(id)[index] == StorageKind::array) {
            body.push_back(cpp.variable(array_type,
                                        document.operands.name(operand),
                                        initializer(cpp, fixture.operands[index])));
        } else {
            body.push_back(cpp.variable(cpp.types.intern("auto const"),
                                        document.operands.name(operand),
                                        cpp.raw_expression(fixture.operands[index][0])));
        }
    }

    auto const destination{
        document.variants.kind(expanded.variant(id)) == VariantKind::in_place
            ? document.operands.name(*document.variants.target(expanded.variant(id)))
            : document.operations.output(expanded.operation(id))};
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        body.push_back(cpp.variable(array_type, destination));
    }
    body.push_back(cpp.variable(array_type, "expected", initializer(cpp, fixture.expected)));

    auto const function{cpp.named(document.text(document.emissions.cpp_namespace(emission)) + "::" +
                                  document.text(document.variants.name(expanded.variant(id))))};
    body.push_back(cpp.expression_statement(cpp.call(function, test_arguments(cpp, document, id))));

    auto const index{cpp.named("i")};
    auto const actual{cpp.subscript(cpp.named(destination), index)};
    auto const expected{cpp.subscript(cpp.named("expected"), index)};
    auto const check{
        cpp.expression_statement(cpp.call(cpp.named("expect_value"), {actual, expected}))};
    body.push_back(cpp.for_statement(
        cpp.variable("std::size_t", "i"),
        cpp.binary(lispb::codegen::BinaryOperator::less, index, cpp.named("Count")),
        cpp.unary(lispb::codegen::UnaryOperator::preincrement, index),
        cpp.block({check})));

    output.push_back(cpp.raw("for (std::size_t const Count : {0, 1, 7, 8, 9, 31})"));
    output.push_back(cpp.block(body));
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
        .add(cpp.include(cpp.strings.add("gtest/gtest.h"), lispb::codegen::IncludeStyle::system),
             2);
    for (auto const include :
         {"array", "cmath", "cstddef", "cstdint", "limits", "span", "type_traits"}) {
        file_nodes.add(cpp.include(cpp.strings.add(include), lispb::codegen::IncludeStyle::system));
    }
    file_nodes.new_lines(2);
    auto const suite{"Generated_" + document.text(document.modules.name(module))};
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
    lispb::codegen::Nodes cases{cpp.header_function(expect_value), cpp.lines(2)};
    auto const range{document.modules.expanded_variants(module)};
    assert(fixtures.size() == range.count);
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        cases.push_back(cpp.raw("TEST(" + suite + ", " + raw_name(document, id) + ")"));
        cases.push_back(cpp.lines(1));
        lispb::codegen::Nodes body;
        append_case(cpp, body, emission, document, id, fixtures[variant_index - range.offset]);
        cases.push_back(cpp.block(body));
        cases.push_back(cpp.lines(2));
    }
    file_nodes.add(cpp.namespace_node(cpp.strings.intern(suite), cases));
    auto const file{cpp.files.append(*document.emissions.tests(emission), file_nodes.build())};
    auto& flags{cpp.files.flags[cpp.files.index(file)]};
    flags.set_pragma_once(false);
    flags.set_format_generated(true);
    return file;
}

}
