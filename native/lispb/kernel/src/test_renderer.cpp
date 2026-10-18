#include "test_renderer.h"

#include "lowering.h"
#include "reference_evaluator.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/node_list_builder.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/strings.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace kernel_codegen::detail {
namespace {

auto test_argument(Document const& document,
                   ExpandedVariantId const id,
                   std::size_t const operand_index) -> std::string {
    auto const& expanded{document.expanded_variants};
    auto const name{document.text(document.operands.name(
        document.operations.operands(expanded.operation(id))[operand_index]))};
    if (expanded.storage(id)[operand_index] == StorageKind::scalar) {
        return name;
    }
    auto const type{standard_type(expanded.type(id))};
    return is_target(document, id, operand_index)
             ? "std::span<" + type + ">{" + name + ".data(), Count}"
             : "std::span<" + type + " const>{" + name + ".data(), Count}";
}

auto test_arguments(Document const& document, ExpandedVariantId const id) -> std::string {
    auto const& expanded{document.expanded_variants};
    std::string result;
    for (std::size_t index{}; index < document.operations.operands(expanded.operation(id)).size();
         ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result += test_argument(document, id, index);
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        result += ", std::span<" + standard_type(expanded.type(id)) + ">{" +
                  document.text(document.operations.output(expanded.operation(id))) +
                  ".data(), Count}";
    }
    return result;
}

auto initializer(std::vector<std::string> const& values) -> std::string {
    std::string result{"{"};
    for (std::size_t index{}; index < values.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += values[index];
    }
    return result + "}";
}

void append_case(std::string& output,
                 EmissionId const emission,
                 Document const& document,
                 ExpandedVariantId const id,
                 std::size_t const count) {
    auto const& expanded{document.expanded_variants};
    auto const type{standard_type(expanded.type(id))};
    auto const fixture{make_reference_fixture(document, id, count)};
    output += "    for (std::size_t const Count : {0, 1, 7, 8, 9, 31}) {\n";
    for (std::size_t index{}; index < document.operations.operands(expanded.operation(id)).size();
         ++index) {
        auto const operand{document.operations.operands(expanded.operation(id))[index]};
        if (expanded.storage(id)[index] == StorageKind::array) {
            output += "        std::array<" + type + ", " + std::to_string(count) + "> " +
                      document.text(document.operands.name(operand)) +
                      initializer(fixture.operands[index]) + ";\n";
        } else {
            output += "        auto const " + document.text(document.operands.name(operand)) + "{" +
                      fixture.operands[index][0] + "};\n";
        }
    }

    auto const destination{
        document.variants.kind(expanded.variant(id)) == VariantKind::in_place
            ? document.text(document.operands.name(*document.variants.target(expanded.variant(id))))
            : document.text(document.operations.output(expanded.operation(id)))};
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        output += "        std::array<" + type + ", " + std::to_string(count) + "> " + destination +
                  "{};\n";
    }
    output += "        std::array<" + type + ", " + std::to_string(count) + "> expected" +
              initializer(fixture.expected) + ";\n";

    output += "        " + document.text(document.emissions.cpp_namespace(emission)) +
              "::" + document.text(document.variants.name(expanded.variant(id))) + "(" +
              test_arguments(document, id) +
              ");\n"
              "        for (std::size_t i{}; i < Count; ++i) {\n";
    output += "            expect_value(" + destination + "[i], expected[i]);\n";
    output += "        }\n    }\n";
}

}

auto lower_standard_tests(lispb::codegen::CppIR& cpp,
                          EmissionId const emission,
                          ModuleId const module,
                          Document const& document) -> lispb::codegen::CppFileId {
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
    auto const expect_value{
        cpp.function(cpp.strings.intern("expect_value"), cpp.types.intern("void"))};
    cpp.function_flags(expect_value).set_define_in_header(true);
    cpp.set_function_template(
        expect_value,
        cpp.template_declaration({cpp.type_template_parameter(cpp.strings.intern("T"))}));
    cpp.set_function_parameters(
        expect_value,
        {cpp.parameter(cpp.types.intern("T const"), cpp.strings.intern("actual")),
         cpp.parameter(cpp.types.intern("T const"), cpp.strings.intern("expected"))});
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
    std::string cases;
    auto const range{document.modules.expanded_variants(module)};
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        cases += "TEST(" + suite + ", " + raw_name(document, id) + ") {\n";
        append_case(cases, emission, document, id, 31);
        cases += "}\n\n";
    }
    file_nodes.add(
        cpp.namespace_node(cpp.strings.intern(suite),
                           {cpp.header_function(expect_value), cpp.lines(2), cpp.raw(cases)}));
    auto const file{cpp.files.append(*document.emissions.tests(emission), file_nodes.build())};
    auto& flags{cpp.files.flags[cpp.files.index(file)]};
    flags.set_pragma_once(false);
    flags.set_format_generated(true);
    return file;
}

}
