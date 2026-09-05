#include "standard_renderer.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace kernel_codegen::detail {
namespace {

auto parameter_type(ExpandedVariant const& expanded,
                    std::size_t const operand_index) -> std::string {
    auto const type{standard_type(expanded.type)};
    if (expanded.storage[operand_index] == StorageKind::scalar) {
        return type;
    }
    return is_target(expanded, operand_index) ? "std::span<" + type + ">"
                                              : "std::span<" + type + " const>";
}

auto parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result += parameter_type(expanded, index) + " " +
                  expanded.operation->operands[index].name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", std::span<" + standard_type(expanded.type) + "> " +
                  expanded.operation->output;
    }
    return result;
}

auto signature_key(ExpandedVariant const& expanded) -> std::string {
    auto result{expanded.variant->public_name + "("};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (index != 0) {
            result += ",";
        }
        result += parameter_type(expanded, index);
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ",std::span<" + standard_type(expanded.type) + ">";
    }
    return result + ")";
}

auto raw_parameter_type(ExpandedVariant const& expanded,
                        std::size_t const operand_index) -> std::string {
    auto const type{standard_type(expanded.type)};
    if (expanded.storage[operand_index] == StorageKind::scalar) {
        return type;
    }
    return type + (is_target(expanded, operand_index) ? "*" : " const*");
}

auto raw_parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result += raw_parameter_type(expanded, index) + " " +
                  expanded.operation->operands[index].name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", " + standard_type(expanded.type) + "* " + expanded.operation->output;
    }
    return result + ", std::size_t const count";
}

auto arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        auto const& name{expanded.operation->operands[index].name};
        result += expanded.storage[index] == StorageKind::array ? name + ".data()" : name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", " + expanded.operation->output + ".data()";
    }
    return result + ", count";
}

void append_overlap_check(std::string& output,
                          std::string const& lhs,
                          std::string const& rhs) {
    output += "    require(!ranges_overlap(" + lhs + ".data(), " + lhs + ".size_bytes(), " + rhs +
              ".data(), " + rhs + ".size_bytes()));\n";
}

void append_public_definition(std::string& output, ExpandedVariant const& expanded) {
    output += "void " + expanded.variant->public_name + "(" + parameters(expanded) +
              ") noexcept {\n";
    auto const source{count_source(expanded)};
    output += "    auto const count{" + source + ".size()};\n";
    auto const arrays{array_names(expanded)};
    for (auto const& array : arrays) {
        if (array != source) {
            output += "    require(" + array + ".size() == count);\n";
        }
    }
    if (expanded.operation->aliasing == Aliasing::pairwise_disjoint) {
        for (std::size_t lhs{}; lhs < arrays.size(); ++lhs) {
            for (std::size_t rhs{lhs + 1}; rhs < arrays.size(); ++rhs) {
                append_overlap_check(output, arrays[lhs], arrays[rhs]);
            }
        }
    } else if (expanded.variant->kind == VariantKind::out_of_place) {
        for (auto const& array : arrays) {
            if (array != expanded.operation->output) {
                append_overlap_check(output, expanded.operation->output, array);
            }
        }
    } else {
        for (auto const& array : arrays) {
            if (array != *expanded.variant->target) {
                append_overlap_check(output, *expanded.variant->target, array);
            }
        }
    }
    output += "    " + raw_name(expanded) + "(" + arguments(expanded) + ");\n}\n\n";
}

}

auto render_standard_header(Emission const& emission,
                            std::vector<ExpandedVariant> const& variants) -> std::string {
    std::string result{generated_warning};
    result += "#pragma once\n\n#include <cstdint>\n#include <span>\n\nnamespace " +
              emission.cpp_namespace + " {\n\n";
    std::set<std::string> signatures;
    for (auto const& expanded : variants) {
        auto const key{signature_key(expanded)};
        if (!signatures.insert(key).second) {
            throw std::invalid_argument{"Duplicate generated kernel signature: " + key};
        }
        result += "void " + expanded.variant->public_name + "(" + parameters(expanded) +
                  ") noexcept;\n\n";
    }
    return result + "}\n";
}

auto render_standard_source(KernelModule const& module,
                            Emission const& emission,
                            std::vector<ExpandedVariant> const& variants) -> std::string {
    std::string result{generated_warning};
    result += "#include \"" + emission.header_include + "\"\n\n"
              "#include <cstddef>\n#include <cstdint>\n#include <cstdlib>\n";
    if (std::ranges::any_of(module.operations, [](auto const& operation) {
            return contains_constant(operation.expression);
        })) {
        result += "#include <limits>\n";
    }
    result += "\nnamespace {\n\n[[noreturn]] void invariant_failed() noexcept { std::abort(); }\n\n"
              "void require(bool const condition) noexcept {\n"
              "    if (!condition) {\n        invariant_failed();\n    }\n}\n\n"
              "auto ranges_overlap(void const* const lhs, std::size_t const lhs_size,\n"
              "                    void const* const rhs, std::size_t const rhs_size) noexcept -> bool {\n"
              "    if (lhs_size == 0 || rhs_size == 0) {\n        return false;\n    }\n\n"
              "    auto const lhs_address{reinterpret_cast<std::uintptr_t>(lhs)};\n"
              "    auto const rhs_address{reinterpret_cast<std::uintptr_t>(rhs)};\n"
              "    return lhs_address <= rhs_address ? rhs_address - lhs_address < lhs_size\n"
              "                                      : lhs_address - rhs_address < rhs_size;\n}\n\n";
    for (auto const& expanded : variants) {
        result += "void " + raw_name(expanded) + "(" + raw_parameters(expanded) +
                  ") noexcept {\n    for (std::size_t i{}; i < count; ++i) {\n";
        auto const destination{expanded.variant->kind == VariantKind::in_place
                                   ? *expanded.variant->target
                                   : expanded.operation->output};
        result += "        " + destination + "[i] = " +
                  render_expression(expanded.operation->expression,
                                    *expanded.operation,
                                    expanded.storage,
                                    standard_type(expanded.type)) +
                  ";\n    }\n}\n\n";
    }
    result += "}\n\nnamespace " + emission.cpp_namespace + " {\n\n";
    for (auto const& expanded : variants) {
        append_public_definition(result, expanded);
    }
    return result + "}\n";
}

}
