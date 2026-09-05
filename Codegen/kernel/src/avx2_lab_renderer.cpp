#include "avx2_lab_renderer.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace kernel_codegen::detail {
namespace {

struct VectorIntrinsics {
    int width;
    std::string_view set1;
    std::string_view load;
    std::string_view add;
    std::string_view multiply;
    std::string_view store;
};

constexpr VectorIntrinsics Avx2{8,
                                "_mm256_set1_ps",
                                "_mm256_loadu_ps",
                                "_mm256_add_ps",
                                "_mm256_mul_ps",
                                "_mm256_storeu_ps"};
constexpr VectorIntrinsics Avx512{16,
                                  "_mm512_set1_ps",
                                  "_mm512_loadu_ps",
                                  "_mm512_add_ps",
                                  "_mm512_mul_ps",
                                  "_mm512_storeu_ps"};

auto validate_lab_variant(ExpandedVariant const& expanded) -> void {
    if (expanded.type != "float" || expanded.variant->kind != VariantKind::out_of_place ||
        expanded.operation->aliasing != Aliasing::pairwise_disjoint) {
        throw std::invalid_argument{
            "SIMD lab supports only pairwise-disjoint, out-of-place float variants"};
    }
}

auto raw_parameters(ExpandedVariant const& expanded,
                    std::string_view const count_type,
                    std::string_view const restriction) -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        auto const& name{expanded.operation->operands[index].name};
        if (expanded.storage[index] == StorageKind::array) {
            result += "float const* ";
            result += restriction;
            result += name;
        } else {
            result += "float const " + name;
        }
    }
    result += ", float* ";
    result += restriction;
    result += expanded.operation->output + ", ";
    result += count_type;
    return result + " const count";
}

auto implementation_name(ExpandedVariant const& expanded, std::string_view const suffix)
    -> std::string {
    return expanded.operation->name + std::string{suffix};
}

auto function_pointer_parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const storage : expanded.storage) {
        if (!result.empty()) {
            result += ", ";
        }
        result += storage == StorageKind::array ? "float const*" : "float";
    }
    return result + ", float*, std::int32_t";
}

auto raw_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const& operand : expanded.operation->operands) {
        if (!result.empty()) {
            result += ", ";
        }
        result += operand.name;
    }
    return result + ", " + expanded.operation->output + ", count";
}

class VectorExpressionRenderer {
  public:
    VectorExpressionRenderer(ExpandedVariant const& expanded,
                             VectorIntrinsics const& intrinsics,
                             std::string const& indentation,
                             std::string const& offset)
        : expanded_{expanded},
          intrinsics_{intrinsics},
          indentation_{indentation},
          offset_{offset} {}

    auto render(Expression const& expression) -> std::string {
        if (expression.kind == ExpressionKind::reference) {
            return render_reference(expression);
        }
        if (expression.kind != ExpressionKind::binary ||
            (expression.value != "+" && expression.value != "*")) {
            throw std::invalid_argument{
                "SIMD lab supports only operand references, addition, and multiplication"};
        }

        auto const lhs{render(expression.arguments[0])};
        auto const rhs{render(expression.arguments[1])};
        auto const result{"vector_" + std::to_string(next_value_++)};
        auto const intrinsic{expression.value == "+" ? intrinsics_.add : intrinsics_.multiply};
        statements_ += indentation_ + "auto const " + result + "{" + std::string{intrinsic} +
                       "(" + lhs + ", " + rhs + ")};\n";
        return result;
    }

    auto statements() const -> std::string const& { return statements_; }

  private:
    auto render_reference(Expression const& expression) -> std::string {
        auto const operand{std::ranges::find_if(expanded_.operation->operands, [&](auto const& item) {
            return item.name == expression.value;
        })};
        if (operand == expanded_.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }

        auto const index{static_cast<std::size_t>(
            std::distance(expanded_.operation->operands.begin(), operand))};
        if (expanded_.storage[index] == StorageKind::scalar) {
            return operand->name + "_vector";
        }

        auto const result{"vector_" + std::to_string(next_value_++)};
        statements_ += indentation_ + "auto const " + result + "{" +
                       std::string{intrinsics_.load} + "(" + operand->name + " + i" + offset_ +
                       ")};\n";
        return result;
    }

    ExpandedVariant const& expanded_;
    VectorIntrinsics const& intrinsics_;
    std::string const& indentation_;
    std::string const& offset_;
    int next_value_{};
    std::string statements_;
};

auto render_vector_chunk(ExpandedVariant const& expanded,
                         VectorIntrinsics const& intrinsics,
                         std::string const& indentation,
                         int const offset) -> std::string {
    auto const offset_expression{offset == 0 ? std::string{} : " + " + std::to_string(offset)};
    VectorExpressionRenderer expression_renderer{
        expanded, intrinsics, indentation, offset_expression};
    auto const result_value{expression_renderer.render(expanded.operation->expression)};
    return expression_renderer.statements() + indentation + std::string{intrinsics.store} + "(" +
           expanded.operation->output + " + i" + offset_expression + ", " + result_value +
           ");\n";
}

auto render_scalar_tail_expression(Expression const& expression,
                                   ExpandedVariant const& expanded,
                                   int const offset) -> std::string {
    if (expression.kind == ExpressionKind::reference) {
        auto const operand{
            std::ranges::find_if(expanded.operation->operands, [&](auto const& item) {
                return item.name == expression.value;
            })};
        if (operand == expanded.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }
        auto const index{static_cast<std::size_t>(
            std::distance(expanded.operation->operands.begin(), operand))};
        if (expanded.storage[index] == StorageKind::scalar) {
            return operand->name;
        }
        return operand->name + "[i" +
               (offset == 0 ? std::string{} : " + " + std::to_string(offset)) + "]";
    }
    if (expression.kind != ExpressionKind::binary ||
        (expression.value != "+" && expression.value != "*")) {
        throw std::invalid_argument{
            "SIMD lab supports only operand references, addition, and multiplication"};
    }
    return "(" + render_scalar_tail_expression(expression.arguments[0], expanded, offset) + " " +
           expression.value + " " +
           render_scalar_tail_expression(expression.arguments[1], expanded, offset) + ")";
}

auto render_scalar_tail(ExpandedVariant const& expanded, int const width) -> std::string {
    auto result{std::string{"    switch (count - i) {\n"}};
    for (int remaining{width - 1}; remaining > 0; --remaining) {
        auto const offset{remaining - 1};
        auto const index_expression{offset == 0 ? std::string{"i"}
                                                : "i + " + std::to_string(offset)};
        result += "    case " + std::to_string(remaining) + ":\n"
                  "        " +
                  expanded.operation->output + "[" + index_expression + "] = " +
                  render_scalar_tail_expression(expanded.operation->expression, expanded, offset) +
                  ";\n"
                  "        [[fallthrough]];\n";
    }
    return result + "    case 0:\n"
                    "        break;\n"
                    "    default:\n"
                    "        break;\n"
                    "    }\n";
}

auto render_autovec_function(ExpandedVariant const& expanded,
                             std::string_view const suffix,
                             std::string_view const count_type,
                             std::string_view const restriction) -> std::string {
    return "void " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept {\n"
           "    for (" +
           std::string{count_type} + " i{0}; i < count; ++i) {\n"
           "        " +
           expanded.operation->output + "[i] = " +
           render_expression(expanded.operation->expression,
                             *expanded.operation,
                             expanded.storage,
                             expanded.type) +
           ";\n"
           "    }\n"
           "}\n\n";
}

auto render_vector_function(ExpandedVariant const& expanded,
                            VectorIntrinsics const& intrinsics,
                            std::string_view const suffix,
                            int const unroll,
                            std::string_view const count_type,
                            std::string_view const restriction) -> std::string {
    auto result{"void " + implementation_name(expanded, suffix) + "(" +
                raw_parameters(expanded, count_type, restriction) + ") noexcept {\n"};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::scalar) {
            auto const& name{expanded.operation->operands[index].name};
            result += "    auto const " + name + "_vector{" + std::string{intrinsics.set1} + "(" +
                      name + ")};\n";
        }
    }
    result += "    " + std::string{count_type} + " i{};\n";
    if (unroll > 1) {
        auto const chunk_width{intrinsics.width * unroll};
        result += "    " + std::string{count_type} + " const unrolled_count{count - (count % " +
                  std::to_string(chunk_width) + ")};\n"
                  "    for (; i < unrolled_count; i += " +
                  std::to_string(chunk_width) + ") {\n";
        for (int offset{}; offset < chunk_width; offset += intrinsics.width) {
            result += "        {\n" + render_vector_chunk(expanded, intrinsics, "            ", offset) +
                      "        }\n";
        }
        result += "    }\n\n";
    }
    result += "    " + std::string{count_type} + " const vectorized_count{count - (count % " +
              std::to_string(intrinsics.width) + ")};\n"
              "    for (; i < vectorized_count; i += " +
              std::to_string(intrinsics.width) + ") {\n" +
              render_vector_chunk(expanded, intrinsics, "        ", 0) + "    }\n\n" +
              render_scalar_tail(expanded, intrinsics.width) + "}\n\n";
    return result;
}

auto native_restrict_definition() -> std::string_view {
    return "#if defined(_MSC_VER)\n"
           "#define ML_KERNEL_LAB_RESTRICT __restrict\n"
           "#else\n"
           "#define ML_KERNEL_LAB_RESTRICT __restrict__\n"
           "#endif\n\n";
}

auto render_declaration(ExpandedVariant const& expanded,
                        std::string_view const suffix,
                        std::string_view const count_type,
                        std::string_view const restriction) -> std::string {
    return "void " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept;\n\n";
}

}

auto render_avx2_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    return generated_warning + std::string{"#pragma once\n\n#include \"CoreTypes.h\"\n\nnamespace "} +
           emission.cpp_namespace + " {\n\n" +
           render_declaration(expanded, "_autovec_avx2", "int32", "RESTRICT ") +
           render_declaration(expanded, "_avx2", "int32", "RESTRICT ") +
           render_declaration(expanded, "_avx2_unrolled", "int32", "RESTRICT ") + "}\n";
}

auto render_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\nnamespace " + emission.cpp_namespace + " {\n\n" +
           render_autovec_function(expanded, "_autovec_avx2", "int32", "RESTRICT ") +
           render_vector_function(expanded, Avx2, "_avx2", 1, "int32", "RESTRICT ") +
           render_vector_function(
               expanded, Avx2, "_avx2_unrolled", 4, "int32", "RESTRICT ") +
           "}\n";
}

auto render_native_simd_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    return std::string{generated_warning} +
           "#pragma once\n\n#include <cstdint>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\nenum class X86SimdBackend : std::uint8_t {\n"
           "    avx2,\n"
           "    avx512,\n"
           "};\n\n" +
           render_declaration(
               expanded, "_autovec_avx2", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_declaration(
               expanded, "_avx2", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_declaration(
               expanded, "_avx2_unrolled", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_declaration(
               expanded, "_autovec_avx512", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_declaration(
               expanded, "_avx512", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_declaration(
               expanded, "_dispatch", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           "auto get_" + expanded.operation->name +
           "_backend() noexcept -> X86SimdBackend;\n\n}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" +
           render_autovec_function(
               expanded, "_autovec_avx2", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_vector_function(
               expanded, Avx2, "_avx2", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_vector_function(expanded,
                                  Avx2,
                                  "_avx2_unrolled",
                                  4,
                                  "std::int32_t",
                                  "ML_KERNEL_LAB_RESTRICT ") +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx512_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" +
           render_autovec_function(
               expanded, "_autovec_avx512", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           render_vector_function(
               expanded, Avx512, "_avx512", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_simd_dispatch_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto const dispatch_name{implementation_name(expanded, "_dispatch")};
    auto const avx2_name{implementation_name(expanded, "_avx2")};
    auto const avx512_name{implementation_name(expanded, "_avx512")};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <cpuinfo_x86.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\nnamespace {\n\nusing Kernel = void (*)(" +
           function_pointer_parameters(expanded) + ") noexcept;\n\n"
           "struct Selection {\n"
           "    X86SimdBackend backend;\n"
           "    Kernel kernel;\n"
           "};\n\n"
           "auto select_backend() noexcept -> Selection {\n"
           "    auto const features{cpu_features::GetX86Info().features};\n"
           "    if (features.avx512f && features.avx512cd && features.avx512bw &&\n"
           "        features.avx512dq && features.avx512vl) {\n"
           "        return {X86SimdBackend::avx512, " + avx512_name + "};\n"
           "    }\n"
           "    return {X86SimdBackend::avx2, " + avx2_name + "};\n"
           "}\n\n"
           "auto selection() noexcept -> Selection const& {\n"
           "    static auto const value{select_backend()};\n"
           "    return value;\n"
           "}\n\n}\n\n"
           "void " + dispatch_name + "(" +
           raw_parameters(expanded, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           ") noexcept {\n"
           "    selection().kernel(" + raw_arguments(expanded) + ");\n"
           "}\n\n"
           "auto get_" + expanded.operation->name +
           "_backend() noexcept -> X86SimdBackend {\n"
           "    return selection().backend;\n"
           "}\n\n}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

}
