#include "test_renderer.h"

#include "reference_evaluator.h"

#include <algorithm>
#include <array>

namespace kernel_codegen::detail {
namespace {

auto test_argument(ExpandedVariant const& expanded,
                   std::size_t const operand_index) -> std::string {
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

auto out_of_place_arguments(ExpandedVariant const& expanded, std::string const& type)
    -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        auto const& name{expanded.operation->operands[index].name};
        if (expanded.storage[index] == StorageKind::scalar) {
            result += name;
        } else {
            result += "std::span<" + type + " const>{" + name + ".data(), Count}";
        }
    }
    return result + ", std::span<" + type + ">{out_of_place.data(), Count}";
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

void append_expectation(std::string& output,
                        std::string const& type,
                        std::string const& actual) {
    if (type == "float" || type == "double") {
        output += "            if (std::isnan(expected[i])) {\n"
                  "                EXPECT_TRUE(std::isnan(" +
                  actual +
                  "));\n"
                  "            } else if (std::isinf(expected[i])) {\n"
                  "                EXPECT_EQ(" +
                  actual +
                  ", expected[i]);\n"
                  "            } else {\n                " +
                  (type == "float" ? "EXPECT_FLOAT_EQ(" : "EXPECT_DOUBLE_EQ(") + actual +
                  ", expected[i]);\n            }\n";
    } else {
        output += "            EXPECT_EQ(" + actual + ", expected[i]);\n";
    }
}

void append_case(std::string& output,
                 Emission const& emission,
                 ExpandedVariant const& expanded,
                 ExpandedVariant const* const out_of_place,
                 std::size_t const count) {
    auto const type{standard_type(expanded.type)};
    auto const fixture{make_reference_fixture(expanded, count)};
    output += "    {\n        constexpr std::size_t Count{" + std::to_string(count) + "};\n";
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        auto const& operand{expanded.operation->operands[index]};
        if (expanded.storage[index] == StorageKind::array) {
            output += "        std::array<" + type + ", Count> " + operand.name +
                      initializer(fixture.operands[index]) + ";\n";
        } else {
            output += "        auto const " + operand.name + "{" +
                      fixture.operands[index][0] + "};\n";
        }
    }

    auto const destination{expanded.variant->kind == VariantKind::in_place
                               ? *expanded.variant->target
                               : expanded.operation->output};
    if (expanded.variant->kind == VariantKind::out_of_place) {
        output += "        std::array<" + type + ", Count> " + destination + "{};\n";
    }
    if (out_of_place != nullptr) {
        output += "        std::array<" + type + ", Count> out_of_place{};\n";
    }
    output += "        std::array<" + type + ", Count> expected" +
              initializer(fixture.expected) + ";\n";

    if (out_of_place != nullptr) {
        output += "        " + emission.cpp_namespace + "::" + out_of_place->variant->public_name +
                  "(" + out_of_place_arguments(*out_of_place, type) + ");\n";
    }
    output += "        " + emission.cpp_namespace + "::" + expanded.variant->public_name + "(" +
              test_arguments(expanded) + ");\n"
              "        for (std::size_t i{}; i < Count; ++i) {\n";
    append_expectation(output, expanded.type, destination + "[i]");
    if (out_of_place != nullptr) {
        append_expectation(output, expanded.type, "out_of_place[i]");
    }
    output += "        }\n    }\n";
}

}

auto render_standard_tests(Emission const& emission,
                           KernelModule const& module,
                           std::vector<ExpandedVariant> const& variants) -> std::string {
    std::string result{generated_warning};
    result += "#include \"" + emission.header_include + "\"\n\n#include <gtest/gtest.h>\n\n"
              "#include <array>\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n"
              "#include <limits>\n#include <span>\n\n";
    auto const suite{"Generated_" + module.name};
    for (auto const& expanded : variants) {
        auto const out_of_place{std::ranges::find_if(variants, [&](auto const& candidate) {
            return expanded.variant->kind == VariantKind::in_place &&
                   candidate.operation == expanded.operation && candidate.type == expanded.type &&
                   candidate.storage == expanded.storage &&
                   candidate.variant->kind == VariantKind::out_of_place;
        })};
        result += "TEST(" + suite + ", " + raw_name(expanded) + ") {\n";
        auto const* comparison{out_of_place == variants.end() ? nullptr : &*out_of_place};
        for (auto const count : std::array<std::size_t, 6>{0, 1, 7, 8, 9, 31}) {
            append_case(result, emission, expanded, comparison, count);
        }
        result += "}\n\n";
    }
    return result;
}

}
