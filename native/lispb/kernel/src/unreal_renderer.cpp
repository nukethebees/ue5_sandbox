#include "unreal_renderer.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace kernel_codegen::detail {
namespace {

auto public_parameter_type(ExpandedVariant const& expanded,
                           std::size_t const operand_index,
                           bool const owning_target) -> std::string {
    auto const& type{expanded.type};
    if (expanded.storage[operand_index] == StorageKind::scalar) {
        return type + " const";
    }
    if (is_target(expanded, operand_index)) {
        return owning_target ? "TArray<" + type + ">&" : "TArrayView<" + type + "> const";
    }
    return "TConstArrayView<" + type + "> const";
}

auto public_parameters(ExpandedVariant const& expanded, bool const owning_target) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result += public_parameter_type(expanded, index, owning_target) + " " +
                  expanded.operation->operands[index].name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        if (!result.empty()) {
            result += ", ";
        }
        result += "TArrayView<" + expanded.type + "> const " + expanded.operation->output;
    }
    return result;
}

auto signature_key(ExpandedVariant const& expanded, bool const owning_target) -> std::string {
    auto result{expanded.variant->public_name + "("};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (index != 0) {
            result += ",";
        }
        result += public_parameter_type(expanded, index, owning_target);
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ",TArrayView<" + expanded.type + "> const";
    }
    return result + ")";
}

auto pointer_restricted(ExpandedVariant const& expanded, std::size_t const operand_index) -> bool {
    return expanded.operation->aliasing == Aliasing::pairwise_disjoint ||
           is_target(expanded, operand_index);
}

auto raw_parameter_type(ExpandedVariant const& expanded, std::size_t const operand_index)
    -> std::string {
    auto const& type{expanded.type};
    if (expanded.storage[operand_index] == StorageKind::scalar) {
        return type + " const";
    }
    auto result{type + (is_target(expanded, operand_index) ? "*" : " const*")};
    if (pointer_restricted(expanded, operand_index)) {
        result += " RESTRICT";
    }
    return result;
}

auto raw_parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        result +=
            raw_parameter_type(expanded, index) + " " + expanded.operation->operands[index].name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", " + expanded.type + "* RESTRICT " + expanded.operation->output;
    }
    return result + ", int32 const count";
}

auto raw_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        auto const& name{expanded.operation->operands[index].name};
        result += expanded.storage[index] == StorageKind::array ? name + ".GetData()" : name;
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result += ", " + expanded.operation->output + ".GetData()";
    }
    return result + ", count";
}

void append_declaration(std::string& output,
                        Emission const& emission,
                        ExpandedVariant const& expanded,
                        bool const owning_target) {
    output += "void " + emission.export_specifier + " " + expanded.variant->public_name + "(\n";
    auto const parameters{public_parameters(expanded, owning_target)};
    std::size_t start{};
    while (start < parameters.size()) {
        auto const separator{parameters.find(", ", start)};
        output += "    " + parameters.substr(start, separator - start);
        if (separator == std::string::npos) {
            output += ") noexcept;\n\n";
            return;
        }
        output += ",\n";
        start = separator + 2;
    }
}

void append_overlap_check(std::string& output,
                          ExpandedVariant const& expanded,
                          std::string const& lhs,
                          std::string const& rhs) {
    output += "    checkf(!ranges_overlap(" + lhs + ".GetData(), static_cast<std::size_t>(" + lhs +
              ".Num()) * sizeof(" + expanded.type + "), " + rhs +
              ".GetData(), static_cast<std::size_t>(" + rhs + ".Num()) * sizeof(" + expanded.type +
              ")),\n";
    output += "           TEXT(\"" + expanded.variant->public_name + ": " + lhs + " and " + rhs +
              " must not overlap\"));\n";
}

void append_public_definition(std::string& output,
                              ExpandedVariant const& expanded,
                              bool const owning_target) {
    output += "void " + expanded.variant->public_name + "(" +
              public_parameters(expanded, owning_target) + ") noexcept {\n";
    if (owning_target) {
        output += "    " + expanded.variant->public_name + "(";
        for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
            if (index != 0) {
                output += ", ";
            }
            auto const& name{expanded.operation->operands[index].name};
            output += is_target(expanded, index) ? "TArrayView<" + expanded.type + ">{" + name + "}"
                                                 : name;
        }
        output += ");\n}\n\n";
        return;
    }

    auto const source{count_source(expanded)};
    output += "    int32 const count{" + source + ".Num()};\n";
    auto const arrays{array_names(expanded)};
    for (auto const& array : arrays) {
        if (array != source) {
            output += "    checkf(" + array + ".Num() == count, TEXT(\"" +
                      expanded.variant->public_name + ": " + array + " and " + source +
                      " must have equal lengths\"));\n";
        }
    }
    if (expanded.operation->aliasing == Aliasing::pairwise_disjoint) {
        for (std::size_t lhs{}; lhs < arrays.size(); ++lhs) {
            for (std::size_t rhs{lhs + 1}; rhs < arrays.size(); ++rhs) {
                append_overlap_check(output, expanded, arrays[lhs], arrays[rhs]);
            }
        }
    } else if (expanded.variant->kind == VariantKind::out_of_place) {
        for (auto const& array : arrays) {
            if (array != expanded.operation->output) {
                append_overlap_check(output, expanded, expanded.operation->output, array);
            }
        }
    } else {
        for (auto const& array : arrays) {
            if (array != *expanded.variant->target) {
                append_overlap_check(output, expanded, *expanded.variant->target, array);
            }
        }
    }
    output += "    " + raw_name(expanded) + "(" + raw_arguments(expanded) + ");\n}\n\n";
}

}

auto render_unreal_header(Emission const& emission, std::vector<ExpandedVariant> const& variants)
    -> std::string {
    std::string result{generated_warning};
    result += "#pragma once\n\n#include \"Containers/Array.h\"\n"
              "#include \"Containers/ArrayView.h\"\n#include \"CoreTypes.h\"\n\n";
    result += "namespace " + emission.cpp_namespace + " {\n\n";
    std::set<std::string> signatures;
    for (auto const& expanded : variants) {
        auto const key{signature_key(expanded, false)};
        if (!signatures.insert(key).second) {
            throw std::invalid_argument{"Duplicate generated kernel signature: " + key};
        }
        append_declaration(result, emission, expanded, false);
        if (expanded.variant->kind == VariantKind::in_place) {
            auto const owning_key{signature_key(expanded, true)};
            if (!signatures.insert(owning_key).second) {
                throw std::invalid_argument{"Duplicate generated kernel signature: " + owning_key};
            }
            append_declaration(result, emission, expanded, true);
        }
    }
    return result + "}\n";
}

auto render_unreal_source(KernelModule const& module,
                          Emission const& emission,
                          std::vector<ExpandedVariant> const& variants) -> std::string {
    std::string result{generated_warning};
    result += "#include \"" + emission.header_include +
              "\"\n\n#include \"CoreMinimal.h\"\n\n"
              "#include <cstddef>\n#include <cstdint>\n";
    if (std::ranges::any_of(module.operations, [](auto const& operation) {
            return contains_constant(operation.expression);
        })) {
        result += "#include <limits>\n";
    }
    result += "\nnamespace {\n\n"
              "auto ranges_overlap(void const* const lhs, std::size_t const lhs_size,\n"
              "                    void const* const rhs, std::size_t const rhs_size) noexcept -> "
              "bool {\n"
              "    if (lhs_size == 0 || rhs_size == 0) {\n        return false;\n    }\n\n"
              "    auto const lhs_address{reinterpret_cast<std::uintptr_t>(lhs)};\n"
              "    auto const rhs_address{reinterpret_cast<std::uintptr_t>(rhs)};\n"
              "    return lhs_address <= rhs_address ? rhs_address - lhs_address < lhs_size\n"
              "                                  : lhs_address - rhs_address < rhs_size;\n}\n\n";
    for (auto const& expanded : variants) {
        result += "void " + raw_name(expanded) + "(" + raw_parameters(expanded) +
                  ") noexcept {\n"
                  "    for (int32 i{0}; i < count; ++i) {\n";
        auto const destination{expanded.variant->kind == VariantKind::in_place
                                   ? *expanded.variant->target
                                   : expanded.operation->output};
        result += "        " + destination + "[i] = " +
                  render_expression(expanded.operation->expression,
                                    *expanded.operation,
                                    expanded.storage,
                                    expanded.type) +
                  ";\n    }\n}\n\n";
    }
    result += "}\n\nnamespace " + emission.cpp_namespace + " {\n\n";
    for (auto const& expanded : variants) {
        append_public_definition(result, expanded, false);
        if (expanded.variant->kind == VariantKind::in_place) {
            append_public_definition(result, expanded, true);
        }
    }
    return result + "}\n";
}

}
