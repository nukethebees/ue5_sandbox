#include "test_renderer.h"

#include "reference_evaluator.h"

namespace kernel_codegen::detail {
namespace {

auto test_argument(ExpandedVariant const& expanded, std::size_t const operand_index)
    -> std::string {
    auto const& name{expanded.operation->operands[operand_index].name};
    if (expanded.storage[operand_index] == StorageKind::scalar) {
        return name;
    }
    auto const type{standard_type(expanded.type)};
    return is_target(expanded, operand_index)
             ? "std::span<" + type + ">{" + name + ".data(), Count}"
             : "std::span<" + type + " const>{" + name + ".data(), Count}";
}

auto test_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result += test_argument(expanded, index);
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", std::span<" + standard_type(expanded.type) + ">{" +
                  expanded.operation->output + ".data(), Count}";
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
                 Emission const& emission,
                 ExpandedVariant const& expanded,
                 std::size_t const count) {
    auto const type{standard_type(expanded.type)};
    auto const fixture{make_reference_fixture(expanded, count)};
    output += "    for (std::size_t const Count : {0, 1, 7, 8, 9, 31}) {\n";
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        auto const& operand{expanded.operation->operands[index]};
        if (expanded.storage[index] == StorageKind::array) {
            output += "        std::array<" + type + ", " + std::to_string(count) + "> " +
                      operand.name + initializer(fixture.operands[index]) + ";\n";
        } else {
            output +=
                "        auto const " + operand.name + "{" + fixture.operands[index][0] + "};\n";
        }
    }

    auto const destination{expanded.variant->kind == VariantKind::in_place
                               ? *expanded.variant->target
                               : expanded.operation->output};
    if (expanded.variant->kind == VariantKind::out_of_place) {
        output += "        std::array<" + type + ", " + std::to_string(count) + "> " + destination +
                  "{};\n";
    }
    output += "        std::array<" + type + ", " + std::to_string(count) + "> expected" +
              initializer(fixture.expected) + ";\n";

    output += "        " + emission.cpp_namespace + "::" + expanded.variant->public_name + "(" +
              test_arguments(expanded) +
              ");\n"
              "        for (std::size_t i{}; i < Count; ++i) {\n";
    output += "            expect_value(" + destination + "[i], expected[i]);\n";
    output += "        }\n    }\n";
}

}

auto render_standard_tests(Emission const& emission,
                           KernelModule const& module,
                           std::vector<ExpandedVariant> const& variants) -> std::string {
    std::string result{generated_warning};
    result += "#include \"" + emission.header_include +
              "\"\n\n#include <gtest/gtest.h>\n\n"
              "#include <array>\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n"
              "#include <limits>\n#include <span>\n#include <type_traits>\n\n";
    auto const suite{"Generated_" + module.name};
    result += "namespace " + suite + " {\n";
    result += R"(template <typename T>
void expect_value(T const actual, T const expected) {
    if constexpr (std::is_floating_point_v<T>) {
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
}

)";
    for (auto const& expanded : variants) {
        result += "TEST(" + suite + ", " + raw_name(expanded) + ") {\n";
        append_case(result, emission, expanded, 31);
        result += "}\n\n";
    }
    return result + "}\n";
}

}
