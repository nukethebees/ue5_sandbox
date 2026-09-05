#include "test_renderer.h"

#include <algorithm>
#include <limits>

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
        auto const type{standard_type(expanded.type)};
        result += "TEST(" + suite + ", " + raw_name(expanded) + ") {\n"
                  "    auto const run = []<std::size_t Count>() {\n"
                  "        constexpr auto storage_count{Count == 0 ? std::size_t{1} : Count};\n";
        for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
            auto const& operand{expanded.operation->operands[index]};
            if (expanded.storage[index] == StorageKind::array) {
                result += "        std::array<" + type + ", storage_count> " + operand.name +
                          "{};\n";
            } else {
                result += "        auto const " + operand.name + "{static_cast<" + type + ">(" +
                          std::to_string(index + 2) + ")};\n";
            }
        }
        auto const destination{expanded.variant->kind == VariantKind::in_place
                                   ? *expanded.variant->target
                                   : expanded.operation->output};
        if (expanded.variant->kind == VariantKind::out_of_place) {
            result += "        std::array<" + type + ", storage_count> " + destination + "{};\n";
        }
        if (out_of_place != variants.end()) {
            result += "        std::array<" + type + ", storage_count> out_of_place{};\n";
        }
        result += "        std::array<" + type + ", storage_count> expected{};\n"
                  "        for (std::size_t i{}; i < Count; ++i) {\n";
        for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
            if (expanded.storage[index] == StorageKind::array) {
                auto const& name{expanded.operation->operands[index].name};
                result += "            " + name + "[i] = static_cast<" + type +
                          ">(((i * 17) + " + std::to_string(index * 3 + 1) + ") % 7 + 1);\n";
            }
        }
        result += "            expected[i] = " +
                  render_expression(expanded.operation->expression,
                                    *expanded.operation,
                                    expanded.storage,
                                    type) +
                  ";\n        }\n";
        if (out_of_place != variants.end()) {
            result += "        " + emission.cpp_namespace + "::" +
                      out_of_place->variant->public_name + "(" +
                      out_of_place_arguments(*out_of_place, type) + ");\n";
        }
        result += "        " + emission.cpp_namespace + "::" + expanded.variant->public_name +
                  "(" + test_arguments(expanded) + ");\n"
                  "        for (std::size_t i{}; i < Count; ++i) {\n"
                  "            if constexpr (std::numeric_limits<" + type +
                  ">::has_quiet_NaN) {\n"
                  "                if (std::isnan(expected[i])) {\n"
                  "                    EXPECT_TRUE(std::isnan(" + destination + "[i]));\n"
                  "                    continue;\n                }\n            }\n"
                  "            EXPECT_EQ(" + destination + "[i], expected[i]);\n"
                  "        }\n";
        if (out_of_place != variants.end()) {
            result += "        EXPECT_EQ(out_of_place, expected);\n";
        }
        result += "    };\n"
                  "    run.template operator()<0>();\n"
                  "    run.template operator()<1>();\n"
                  "    run.template operator()<7>();\n"
                  "    run.template operator()<8>();\n"
                  "    run.template operator()<9>();\n"
                  "    run.template operator()<31>();\n}\n\n";
    }
    return result;
}

}
