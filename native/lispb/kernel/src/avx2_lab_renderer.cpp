#include "avx2_lab_renderer.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace kernel_codegen::detail {
namespace {

struct VectorIntrinsics {
    int width;
    std::string_view set_zero;
    std::string_view set1;
    std::string_view load;
    std::string_view aligned_load;
    std::string_view add;
    std::string_view multiply;
    std::string_view store;
    std::string_view aligned_store;
};

constexpr VectorIntrinsics Avx2{8,
                                "_mm256_setzero_ps",
                                "_mm256_set1_ps",
                                "_mm256_loadu_ps",
                                "_mm256_load_ps",
                                "_mm256_add_ps",
                                "_mm256_mul_ps",
                                "_mm256_storeu_ps",
                                "_mm256_store_ps"};
constexpr VectorIntrinsics Avx512{16,
                                  "_mm512_setzero_ps",
                                  "_mm512_set1_ps",
                                  "_mm512_loadu_ps",
                                  "_mm512_load_ps",
                                  "_mm512_add_ps",
                                  "_mm512_mul_ps",
                                  "_mm512_storeu_ps",
                                  "_mm512_store_ps"};

auto validate_lab_variant(ExpandedVariant const& expanded) -> void {
    auto const kind_matches{(expanded.operation->kind == OperationKind::map &&
                             expanded.variant->kind == VariantKind::out_of_place) ||
                            (expanded.operation->kind == OperationKind::sum &&
                             expanded.variant->kind == VariantKind::sum)};
    if (expanded.type != "float" || !kind_matches ||
        expanded.operation->aliasing != Aliasing::pairwise_disjoint) {
        throw std::invalid_argument{"SIMD lab supports only pairwise-disjoint float maps and sums"};
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
    if (expanded.operation->kind == OperationKind::map) {
        result += ", float* ";
        result += restriction;
        result += expanded.operation->output;
    }
    result += ", ";
    result += count_type;
    return result + " const count";
}

auto implementation_name(ExpandedVariant const& expanded, std::string_view const suffix)
    -> std::string {
    return expanded.operation->name + std::string{suffix};
}

auto soaos_implementation_name(ExpandedVariant const& expanded, std::string_view const suffix)
    -> std::string {
    return expanded.operation->name + std::string{suffix};
}

auto soaos_parameters(ExpandedVariant const& expanded, std::string_view const restriction)
    -> std::string {
    std::string result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (!result.empty()) {
            result += ", ";
        }
        auto const& name{expanded.operation->operands[index].name};
        if (expanded.storage[index] == StorageKind::array) {
            result += "FloatChunk16 const* ";
            result += restriction;
            result += name;
        } else {
            result += "float const " + name;
        }
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += ", FloatChunk16* ";
        result += restriction;
        result += expanded.operation->output;
    }
    return result + ", std::int32_t const chunk_count";
}

auto soaos_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const& operand : expanded.operation->operands) {
        if (!result.empty()) {
            result += ", ";
        }
        result += operand.name;
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += ", " + expanded.operation->output;
    }
    return result + ", chunk_count";
}

auto soaos_function_pointer_parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const storage : expanded.storage) {
        if (!result.empty()) {
            result += ", ";
        }
        if (storage == StorageKind::scalar) {
            result += "float";
        } else {
            result += "FloatChunk16 const*";
        }
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += ", FloatChunk16*";
    }
    return result + ", std::int32_t";
}

auto render_soaos_block() -> std::string {
    return "struct alignas(64) FloatChunk16 {\n"
           "    static constexpr std::int32_t capacity{16};\n\n"
           "    std::array<float, capacity> values{};\n"
           "};\n\n"
           "static_assert(sizeof(FloatChunk16) == 64);\n"
           "static_assert(alignof(FloatChunk16) == 64);\n\n";
}

auto has_floating_point_mode(ExpandedVariant const& expanded, FloatingPointMode const mode)
    -> bool {
    return std::ranges::find(expanded.operation->floating_point_modes, mode) !=
           expanded.operation->floating_point_modes.end();
}

auto function_pointer_parameters(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const storage : expanded.storage) {
        if (!result.empty()) {
            result += ", ";
        }
        result += storage == StorageKind::array ? "float const*" : "float";
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += ", float*";
    }
    return result + ", std::int32_t";
}

auto raw_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result;
    for (auto const& operand : expanded.operation->operands) {
        if (!result.empty()) {
            result += ", ";
        }
        result += operand.name;
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += ", " + expanded.operation->output;
    }
    return result + ", count";
}

class VectorExpressionRenderer {
  public:
    VectorExpressionRenderer(ExpandedVariant const& expanded,
                             VectorIntrinsics const& intrinsics,
                             std::string const& indentation,
                             std::string const& offset)
        : expanded_{expanded}
        , intrinsics_{intrinsics}
        , indentation_{indentation}
        , offset_{offset} {}

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
        statements_ += indentation_ + "auto const " + result + "{" + std::string{intrinsic} + "(" +
                       lhs + ", " + rhs + ")};\n";
        return result;
    }

    auto statements() const -> std::string const& { return statements_; }
  private:
    auto render_reference(Expression const& expression) -> std::string {
        auto const operand{
            std::ranges::find_if(expanded_.operation->operands,
                                 [&](auto const& item) { return item.name == expression.value; })};
        if (operand == expanded_.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }

        auto const index{static_cast<std::size_t>(
            std::distance(expanded_.operation->operands.begin(), operand))};
        if (expanded_.storage[index] == StorageKind::scalar) {
            return operand->name + "_vector";
        }

        auto const result{"vector_" + std::to_string(next_value_++)};
        statements_ += indentation_ + "auto const " + result + "{" + std::string{intrinsics_.load} +
                       "(" + operand->name + " + i" + offset_ + ")};\n";
        return result;
    }

    ExpandedVariant const& expanded_;
    VectorIntrinsics const& intrinsics_;
    std::string indentation_;
    std::string offset_;
    int next_value_{};
    std::string statements_;
};

class SoaosVectorExpressionRenderer {
  public:
    SoaosVectorExpressionRenderer(ExpandedVariant const& expanded,
                                  VectorIntrinsics const& intrinsics,
                                  std::string const& indentation,
                                  std::string const& block_index,
                                  int const lane_offset)
        : expanded_{expanded}
        , intrinsics_{intrinsics}
        , indentation_{indentation}
        , block_index_{block_index}
        , lane_offset_{lane_offset} {}

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
        statements_ += indentation_ + "auto const " + result + "{" + std::string{intrinsic} + "(" +
                       lhs + ", " + rhs + ")};\n";
        return result;
    }

    auto statements() const -> std::string const& { return statements_; }
  private:
    auto render_reference(Expression const& expression) -> std::string {
        auto const operand{
            std::ranges::find_if(expanded_.operation->operands,
                                 [&](auto const& item) { return item.name == expression.value; })};
        if (operand == expanded_.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }

        auto const index{static_cast<std::size_t>(
            std::distance(expanded_.operation->operands.begin(), operand))};
        if (expanded_.storage[index] == StorageKind::scalar) {
            return operand->name + "_vector";
        }

        auto const result{"vector_" + std::to_string(next_value_++)};
        auto const offset{lane_offset_ == 0 ? std::string{} : " + " + std::to_string(lane_offset_)};
        statements_ += indentation_ + "auto const " + result + "{" +
                       std::string{intrinsics_.aligned_load} + "(" + operand->name + "[" +
                       block_index_ + "].values.data()" + offset + ")};\n";
        return result;
    }

    ExpandedVariant const& expanded_;
    VectorIntrinsics const& intrinsics_;
    std::string indentation_;
    std::string block_index_;
    int lane_offset_;
    int next_value_{};
    std::string statements_;
};

auto render_soaos_scalar_expression(Expression const& expression,
                                    ExpandedVariant const& expanded,
                                    std::string_view const block_index,
                                    std::string_view const lane_index) -> std::string {
    if (expression.kind == ExpressionKind::reference) {
        auto const operand{
            std::ranges::find_if(expanded.operation->operands,
                                 [&](auto const& item) { return item.name == expression.value; })};
        if (operand == expanded.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }
        auto const index{
            static_cast<std::size_t>(std::distance(expanded.operation->operands.begin(), operand))};
        if (expanded.storage[index] == StorageKind::scalar) {
            return operand->name;
        }
        return operand->name + "[" + std::string{block_index} + "].values[" +
               std::string{lane_index} + "]";
    }
    if (expression.kind != ExpressionKind::binary ||
        (expression.value != "+" && expression.value != "*")) {
        throw std::invalid_argument{
            "SIMD lab supports only operand references, addition, and multiplication"};
    }
    return "(" +
           render_soaos_scalar_expression(
               expression.arguments[0], expanded, block_index, lane_index) +
           " " + expression.value + " " +
           render_soaos_scalar_expression(
               expression.arguments[1], expanded, block_index, lane_index) +
           ")";
}

auto render_soaos_vector_expression(ExpandedVariant const& expanded,
                                    VectorIntrinsics const& intrinsics,
                                    std::string const& indentation,
                                    std::string const& block_index,
                                    int const lane_offset) -> std::pair<std::string, std::string> {
    SoaosVectorExpressionRenderer expression_renderer{
        expanded, intrinsics, indentation, block_index, lane_offset};
    auto const value{expression_renderer.render(expanded.operation->expression)};
    return {expression_renderer.statements(), value};
}

auto render_vector_chunk(ExpandedVariant const& expanded,
                         VectorIntrinsics const& intrinsics,
                         std::string const& indentation,
                         int const offset) -> std::string {
    auto const offset_expression{offset == 0 ? std::string{} : " + " + std::to_string(offset)};
    VectorExpressionRenderer expression_renderer{
        expanded, intrinsics, indentation, offset_expression};
    auto const result_value{expression_renderer.render(expanded.operation->expression)};
    return expression_renderer.statements() + indentation + std::string{intrinsics.store} + "(" +
           expanded.operation->output + " + i" + offset_expression + ", " + result_value + ");\n";
}

auto render_scalar_tail_expression(Expression const& expression,
                                   ExpandedVariant const& expanded,
                                   int const offset) -> std::string {
    if (expression.kind == ExpressionKind::reference) {
        auto const operand{
            std::ranges::find_if(expanded.operation->operands,
                                 [&](auto const& item) { return item.name == expression.value; })};
        if (operand == expanded.operation->operands.end()) {
            throw std::invalid_argument{"SIMD lab expression references an unknown operand"};
        }
        auto const index{
            static_cast<std::size_t>(std::distance(expanded.operation->operands.begin(), operand))};
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
        result += "    case " + std::to_string(remaining) +
                  ":\n"
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

auto render_map_autovec_function(ExpandedVariant const& expanded,
                                 std::string_view const suffix,
                                 std::string_view const count_type,
                                 std::string_view const restriction) -> std::string {
    return "void " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) +
           ") noexcept {\n"
           "    for (" +
           std::string{count_type} +
           " i{0}; i < count; ++i) {\n"
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

auto scalar_loop_controls() -> std::string_view {
    return "#if defined(__clang__)\n"
           "    #pragma clang loop vectorize(disable) interleave(disable) unroll(disable)\n"
           "#elif defined(_MSC_VER)\n"
           "    #pragma loop(no_vector)\n"
           "#endif\n";
}

auto render_map_scalar_function(ExpandedVariant const& expanded,
                                std::string_view const suffix,
                                std::string_view const count_type,
                                std::string_view const restriction) -> std::string {
    return "void " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept {\n" +
           std::string{scalar_loop_controls()} + "    for (" + std::string{count_type} +
           " i{0}; i < count; ++i) {\n"
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

auto render_map_vector_function(ExpandedVariant const& expanded,
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
                  std::to_string(chunk_width) +
                  ")};\n"
                  "    for (; i < unrolled_count; i += " +
                  std::to_string(chunk_width) + ") {\n";
        for (int offset{}; offset < chunk_width; offset += intrinsics.width) {
            result += "        {\n" +
                      render_vector_chunk(expanded, intrinsics, "            ", offset) +
                      "        }\n";
        }
        result += "    }\n\n";
    }
    result += "    " + std::string{count_type} + " const vectorized_count{count - (count % " +
              std::to_string(intrinsics.width) +
              ")};\n"
              "    for (; i < vectorized_count; i += " +
              std::to_string(intrinsics.width) + ") {\n" +
              render_vector_chunk(expanded, intrinsics, "        ", 0) + "    }\n\n" +
              render_scalar_tail(expanded, intrinsics.width) + "}\n\n";
    return result;
}

auto render_sum_autovec_function(ExpandedVariant const& expanded,
                                 std::string_view const suffix,
                                 std::string_view const count_type,
                                 std::string_view const restriction) -> std::string {
    return "float " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) +
           ") noexcept {\n"
           "    float result{};\n"
           "    for (" +
           std::string{count_type} +
           " i{0}; i < count; ++i) {\n"
           "        result += " +
           render_expression(expanded.operation->expression,
                             *expanded.operation,
                             expanded.storage,
                             expanded.type) +
           ";\n"
           "    }\n"
           "    return result;\n"
           "}\n\n";
}

auto render_sum_scalar_function(ExpandedVariant const& expanded,
                                std::string_view const suffix,
                                std::string_view const count_type,
                                std::string_view const restriction) -> std::string {
    return "float " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) +
           ") noexcept {\n"
           "    float result{};\n" +
           std::string{scalar_loop_controls()} + "    for (" + std::string{count_type} +
           " i{0}; i < count; ++i) {\n"
           "        result += " +
           render_expression(expanded.operation->expression,
                             *expanded.operation,
                             expanded.storage,
                             expanded.type) +
           ";\n"
           "    }\n"
           "    return result;\n"
           "}\n\n";
}

auto render_sum_vector_function(ExpandedVariant const& expanded,
                                VectorIntrinsics const& intrinsics,
                                std::string_view const suffix,
                                int const unroll,
                                std::string_view const count_type,
                                std::string_view const restriction) -> std::string {
    auto result{"float " + implementation_name(expanded, suffix) + "(" +
                raw_parameters(expanded, count_type, restriction) + ") noexcept {\n"};
    for (int accumulator{}; accumulator < unroll; ++accumulator) {
        result += "    auto accumulator_" + std::to_string(accumulator) + "{" +
                  std::string{intrinsics.set_zero} + "()};\n";
    }
    result += "    " + std::string{count_type} + " i{};\n";
    if (unroll > 1) {
        auto const chunk_width{intrinsics.width * unroll};
        result += "    " + std::string{count_type} + " const unrolled_count{count - (count % " +
                  std::to_string(chunk_width) +
                  ")};\n"
                  "    for (; i < unrolled_count; i += " +
                  std::to_string(chunk_width) + ") {\n";
        for (int accumulator{}; accumulator < unroll; ++accumulator) {
            auto const offset{accumulator * intrinsics.width};
            auto const offset_expression{offset == 0 ? std::string{}
                                                     : " + " + std::to_string(offset)};
            result += "        {\n";
            VectorExpressionRenderer expression_renderer{
                expanded, intrinsics, "            ", offset_expression};
            auto const value{expression_renderer.render(expanded.operation->expression)};
            result += expression_renderer.statements() + "            accumulator_" +
                      std::to_string(accumulator) + " = " + std::string{intrinsics.add} +
                      "(accumulator_" + std::to_string(accumulator) + ", " + value +
                      ");\n"
                      "        }\n";
        }
        result += "    }\n\n";
        for (int stride{1}; stride < unroll; stride *= 2) {
            for (int accumulator{}; accumulator + stride < unroll; accumulator += stride * 2) {
                result += "    accumulator_" + std::to_string(accumulator) + " = " +
                          std::string{intrinsics.add} + "(accumulator_" +
                          std::to_string(accumulator) + ", accumulator_" +
                          std::to_string(accumulator + stride) + ");\n";
            }
        }
    }
    result += "    " + std::string{count_type} + " const vectorized_count{count - (count % " +
              std::to_string(intrinsics.width) +
              ")};\n"
              "    for (; i < vectorized_count; i += " +
              std::to_string(intrinsics.width) +
              ") {\n"
              "        {\n";
    VectorExpressionRenderer expression_renderer{expanded, intrinsics, "            ", {}};
    auto const value{expression_renderer.render(expanded.operation->expression)};
    result += expression_renderer.statements() +
              "            accumulator_0 = " + std::string{intrinsics.add} + "(accumulator_0, " +
              value +
              ");\n"
              "        }\n"
              "    }\n\n";
    result +=
        "    float lanes[" + std::to_string(intrinsics.width) +
        "]{};\n"
        "    " +
        std::string{intrinsics.store} +
        "(lanes, accumulator_0);\n"
        "    float result{};\n"
        "    for (int lane{}; lane < " +
        std::to_string(intrinsics.width) +
        "; ++lane) {\n"
        "        result += lanes[lane];\n"
        "    }\n"
        "    for (; i < count; ++i) {\n"
        "        result += " +
        render_expression(
            expanded.operation->expression, *expanded.operation, expanded.storage, expanded.type) +
        ";\n"
        "    }\n"
        "    return result;\n"
        "}\n\n";
    return result;
}

auto render_soaos_map_loop_function(ExpandedVariant const& expanded,
                                    std::string_view const suffix,
                                    bool const force_scalar) -> std::string {
    auto result{
        "void " + soaos_implementation_name(expanded, suffix) + "(" +
        soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") +
        ") noexcept {\n"
        "#if defined(__clang__)\n"
        "    #pragma clang loop vectorize(disable) interleave(disable)\n"
        "#elif defined(_MSC_VER)\n"
        "    #pragma loop(no_vector)\n"
        "#endif\n"
        "    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {\n"};
    if (force_scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "        for (std::int32_t lane{}; lane < FloatChunk16::capacity; ++lane) {\n"
              "            " +
              expanded.operation->output + "[chunk_index].values[lane] = " +
              render_soaos_scalar_expression(
                  expanded.operation->expression, expanded, "chunk_index", "lane") +
              ";\n"
              "        }\n"
              "    }\n"
              "}\n\n";
    return result;
}

auto render_soaos_sum_loop_function(ExpandedVariant const& expanded,
                                    std::string_view const suffix,
                                    bool const force_scalar,
                                    bool const lane_accumulators = false) -> std::string {
    auto result{"float " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"};
    result += lane_accumulators ? "    std::array<float, FloatChunk16::capacity> results{};\n"
                                : "    float result{};\n";
    result += "#if defined(__clang__)\n"
              "    #pragma clang loop vectorize(disable) interleave(disable)\n"
              "#elif defined(_MSC_VER)\n"
              "    #pragma loop(no_vector)\n"
              "#endif\n"
              "    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {\n";
    if (force_scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "        for (std::int32_t lane{}; lane < FloatChunk16::capacity; ++lane) {\n"
              "            " +
              std::string{lane_accumulators ? "results[lane]" : "result"} + " += " +
              render_soaos_scalar_expression(
                  expanded.operation->expression, expanded, "chunk_index", "lane") +
              ";\n"
              "        }\n"
              "    }\n";
    if (lane_accumulators) {
        result += "    float result{};\n"
                  "    for (auto const value : results) {\n"
                  "        result += value;\n"
                  "    }\n";
    }
    return result + "    return result;\n"
                    "}\n\n";
}

auto render_soaos_map_block(ExpandedVariant const& expanded,
                            VectorIntrinsics const& intrinsics,
                            std::string const& indentation,
                            std::string const& chunk_index) -> std::string {
    std::string result;
    for (int lane_offset{}; lane_offset < 16; lane_offset += intrinsics.width) {
        auto const nested_indentation{indentation + "    "};
        auto const [statements, value]{render_soaos_vector_expression(
            expanded, intrinsics, nested_indentation, chunk_index, lane_offset)};
        auto const offset{lane_offset == 0 ? std::string{} : " + " + std::to_string(lane_offset)};
        result += indentation + "{\n" + statements + nested_indentation +
                  std::string{intrinsics.aligned_store} + "(" + expanded.operation->output + "[" +
                  chunk_index + "].values.data()" + offset + ", " + value + ");\n" + indentation +
                  "}\n";
    }
    return result;
}

auto render_soaos_map_vector_function(ExpandedVariant const& expanded,
                                      VectorIntrinsics const& intrinsics,
                                      std::string_view const suffix,
                                      int const chunks_per_iteration) -> std::string {
    auto result{"void " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::scalar) {
            auto const& name{expanded.operation->operands[index].name};
            result += "    auto const " + name + "_vector{" + std::string{intrinsics.set1} + "(" +
                      name + ")};\n";
        }
    }
    result += "    std::int32_t chunk_index{};\n";
    if (chunks_per_iteration > 1) {
        result += "    std::int32_t const unrolled_count{chunk_count - (chunk_count % " +
                  std::to_string(chunks_per_iteration) +
                  ")};\n"
                  "    for (; chunk_index < unrolled_count; chunk_index += " +
                  std::to_string(chunks_per_iteration) + ") {\n";
        for (int chunk_offset{}; chunk_offset < chunks_per_iteration; ++chunk_offset) {
            auto const chunk_expression{chunk_offset == 0
                                            ? std::string{"chunk_index"}
                                            : "chunk_index + " + std::to_string(chunk_offset)};
            result += render_soaos_map_block(expanded, intrinsics, "        ", chunk_expression);
        }
        result += "    }\n\n";
    }
    result += "    for (; chunk_index < chunk_count; ++chunk_index) {\n" +
              render_soaos_map_block(expanded, intrinsics, "        ", "chunk_index") +
              "    }\n"
              "}\n\n";
    return result;
}

auto render_soaos_sum_vector_function(ExpandedVariant const& expanded,
                                      VectorIntrinsics const& intrinsics,
                                      std::string_view const suffix,
                                      int const unroll) -> std::string {
    auto result{"float " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"};
    for (int accumulator{}; accumulator < unroll; ++accumulator) {
        result += "    auto accumulator_" + std::to_string(accumulator) + "{" +
                  std::string{intrinsics.set_zero} + "()};\n";
    }
    result += "    std::int32_t chunk_index{};\n";
    auto const vectors_per_block{16 / intrinsics.width};
    auto const blocks_per_iteration{unroll / vectors_per_block};
    if (blocks_per_iteration > 1) {
        result += "    std::int32_t const unrolled_count{chunk_count - (chunk_count % " +
                  std::to_string(blocks_per_iteration) +
                  ")};\n"
                  "    for (; chunk_index < unrolled_count; chunk_index += " +
                  std::to_string(blocks_per_iteration) + ") {\n";
        for (int block_offset{}; block_offset < blocks_per_iteration; ++block_offset) {
            auto const block_expression{block_offset == 0
                                            ? std::string{"chunk_index"}
                                            : "chunk_index + " + std::to_string(block_offset)};
            for (int vector_offset{}; vector_offset < vectors_per_block; ++vector_offset) {
                auto const accumulator{block_offset * vectors_per_block + vector_offset};
                auto const indentation{std::string{"            "}};
                auto const [statements, value]{
                    render_soaos_vector_expression(expanded,
                                                   intrinsics,
                                                   indentation,
                                                   block_expression,
                                                   vector_offset * intrinsics.width)};
                result += "        {\n" + statements + indentation + "accumulator_" +
                          std::to_string(accumulator) + " = " + std::string{intrinsics.add} +
                          "(accumulator_" + std::to_string(accumulator) + ", " + value + ");\n";
                result += "        }\n";
            }
        }
        result += "    }\n\n";
    }
    result += "    for (; chunk_index < chunk_count; ++chunk_index) {\n";
    for (int vector_offset{}; vector_offset < vectors_per_block; ++vector_offset) {
        auto const indentation{std::string{"            "}};
        auto const [statements, value]{render_soaos_vector_expression(
            expanded, intrinsics, indentation, "chunk_index", vector_offset * intrinsics.width)};
        result += "        {\n" + statements + indentation +
                  "accumulator_0 = " + std::string{intrinsics.add} + "(accumulator_0, " + value +
                  ");\n"
                  "        }\n";
    }
    result += "    }\n\n";
    for (int stride{1}; stride < unroll; stride *= 2) {
        for (int accumulator{}; accumulator + stride < unroll; accumulator += stride * 2) {
            result += "    accumulator_" + std::to_string(accumulator) + " = " +
                      std::string{intrinsics.add} + "(accumulator_" + std::to_string(accumulator) +
                      ", accumulator_" + std::to_string(accumulator + stride) + ");\n";
        }
    }
    result += "    float lanes[" + std::to_string(intrinsics.width) +
              "]{};\n"
              "    " +
              std::string{intrinsics.store} +
              "(lanes, accumulator_0);\n"
              "    float result{};\n"
              "    for (int lane{}; lane < " +
              std::to_string(intrinsics.width) +
              "; ++lane) {\n"
              "        result += lanes[lane];\n"
              "    }\n"
              "    return result;\n"
              "}\n\n";
    return result;
}

auto native_restrict_definition() -> std::string_view;
auto render_declaration(ExpandedVariant const& expanded,
                        std::string_view suffix,
                        std::string_view count_type,
                        std::string_view restriction) -> std::string;
auto wrap_namespace(std::string_view cpp_namespace, std::string const& content) -> std::string;

enum class Vector3Layout { aos, chunked };

struct GroupedComponent {
    std::string_view group;
    int component;
};

auto grouped_component(Emission const& emission, std::string_view const operand)
    -> GroupedComponent {
    for (auto const& group : emission.vector3_groups) {
        for (int component{}; component < 3; ++component) {
            if (group.components[static_cast<std::size_t>(component)] == operand) {
                return {group.name, component};
            }
        }
    }
    throw std::invalid_argument{"vector3 expression references an ungrouped operand"};
}

auto vector3_member(int const component, bool const plural) -> std::string_view {
    constexpr std::array singular{"x", "y", "z"};
    constexpr std::array plural_names{"xs", "ys", "zs"};
    return plural ? plural_names[static_cast<std::size_t>(component)]
                  : singular[static_cast<std::size_t>(component)];
}

auto vector3_aos_parameters(Emission const& emission,
                            ExpandedVariant const& expanded,
                            std::string_view const restriction) -> std::string {
    std::string result;
    for (auto const& group : emission.vector3_groups) {
        if (!result.empty()) {
            result += ", ";
        }
        result += "Float3 const* ";
        result += restriction;
        result += group.name;
    }
    result += ", float* ";
    result += restriction;
    result += expanded.operation->output;
    return result + ", std::int32_t const count";
}

auto vector3_chunk_parameters(Emission const& emission,
                              ExpandedVariant const& expanded,
                              std::string_view const restriction) -> std::string {
    std::string result;
    for (auto const& group : emission.vector3_groups) {
        if (!result.empty()) {
            result += ", ";
        }
        result += "Float3Chunk16 const* ";
        result += restriction;
        result += group.name;
    }
    result += ", FloatChunk16* ";
    result += restriction;
    result += expanded.operation->output;
    return result + ", std::int32_t const chunk_count";
}

auto render_vector3_types() -> std::string {
    return "struct Float3 {\n"
           "    float x{};\n"
           "    float y{};\n"
           "    float z{};\n"
           "};\n\n"
           "static_assert(sizeof(Float3) == 12);\n\n"
           "struct alignas(64) Float3Chunk16 {\n"
           "    static constexpr std::int32_t capacity{16};\n\n"
           "    std::array<float, capacity> xs{};\n"
           "    std::array<float, capacity> ys{};\n"
           "    std::array<float, capacity> zs{};\n"
           "};\n\n"
           "static_assert(sizeof(Float3Chunk16) == 192);\n"
           "static_assert(alignof(Float3Chunk16) == 64);\n\n"
           "struct alignas(64) FloatChunk16 {\n"
           "    static constexpr std::int32_t capacity{16};\n\n"
           "    std::array<float, capacity> values{};\n"
           "};\n\n"
           "static_assert(sizeof(FloatChunk16) == 64);\n"
           "static_assert(alignof(FloatChunk16) == 64);\n\n";
}

auto render_vector3_scalar_expression(Expression const& expression,
                                      Emission const& emission,
                                      Vector3Layout const layout,
                                      std::string_view const outer_index,
                                      std::string_view const lane_index = {}) -> std::string {
    if (expression.kind == ExpressionKind::reference) {
        auto const component{grouped_component(emission, expression.value)};
        auto result{
            std::string{component.group} + "[" + std::string{outer_index} + "]." +
            std::string{vector3_member(component.component, layout == Vector3Layout::chunked)}};
        if (layout == Vector3Layout::chunked) {
            result += "[" + std::string{lane_index} + "]";
        }
        return result;
    }
    if (expression.kind != ExpressionKind::binary ||
        (expression.value != "+" && expression.value != "*")) {
        throw std::invalid_argument{
            "vector3 SIMD lab supports only references, addition, and multiplication"};
    }
    return "(" +
           render_vector3_scalar_expression(
               expression.arguments[0], emission, layout, outer_index, lane_index) +
           " " + expression.value + " " +
           render_vector3_scalar_expression(
               expression.arguments[1], emission, layout, outer_index, lane_index) +
           ")";
}

class Vector3ExpressionRenderer {
  public:
    Vector3ExpressionRenderer(Emission const& emission,
                              VectorIntrinsics const& intrinsics,
                              Vector3Layout const layout,
                              std::string_view const outer_index,
                              int const lane_offset)
        : emission_{emission}
        , intrinsics_{intrinsics}
        , layout_{layout}
        , outer_index_{outer_index}
        , lane_offset_{lane_offset} {}

    auto render(Expression const& expression) -> std::string {
        if (expression.kind == ExpressionKind::reference) {
            return render_reference(expression.value);
        }
        if (expression.kind != ExpressionKind::binary ||
            (expression.value != "+" && expression.value != "*")) {
            throw std::invalid_argument{
                "vector3 SIMD lab supports only references, addition, and multiplication"};
        }
        auto const lhs{render(expression.arguments[0])};
        auto const rhs{render(expression.arguments[1])};
        auto const value{"vector_" + std::to_string(next_value_++)};
        auto const intrinsic{expression.value == "+" ? intrinsics_.add : intrinsics_.multiply};
        statements_ += "        auto const " + value + "{" + std::string{intrinsic} + "(" + lhs +
                       ", " + rhs + ")};\n";
        return value;
    }

    auto statements() const -> std::string const& { return statements_; }
  private:
    auto render_reference(std::string_view const operand) -> std::string {
        auto const component{grouped_component(emission_, operand)};
        auto const value{"vector_" + std::to_string(next_value_++)};
        if (layout_ == Vector3Layout::aos) {
            auto const gather{intrinsics_.width == 8 ? "_mm256_i32gather_ps"
                                                     : "_mm512_i32gather_ps"};
            auto const base{"reinterpret_cast<float const*>(" + std::string{component.group} +
                            " + " + std::string{outer_index_} + ") + " +
                            std::to_string(component.component)};
            auto const arguments{intrinsics_.width == 8 ? base + ", gather_indices, 4"
                                                        : "gather_indices, " + base + ", 4"};
            statements_ += "        auto const " + value + "{" + gather + "(" + arguments + ")};\n";
        } else {
            auto const offset{lane_offset_ == 0 ? std::string{}
                                                : " + " + std::to_string(lane_offset_)};
            statements_ += "        auto const " + value + "{" +
                           std::string{intrinsics_.aligned_load} + "(" +
                           std::string{component.group} + "[" + std::string{outer_index_} + "]." +
                           std::string{vector3_member(component.component, true)} + ".data()" +
                           offset + ")};\n";
        }
        return value;
    }

    Emission const& emission_;
    VectorIntrinsics const& intrinsics_;
    Vector3Layout layout_;
    std::string_view outer_index_;
    int lane_offset_;
    int next_value_{};
    std::string statements_;
};

auto render_vector3_aos_loop_function(Emission const& emission,
                                      ExpandedVariant const& expanded,
                                      bool const scalar) -> std::string {
    auto result{"void " + expanded.operation->name + "(" +
                vector3_aos_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
                ") noexcept {\n"};
    if (scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "    for (std::int32_t i{}; i < count; ++i) {\n"
              "        " +
              expanded.operation->output + "[i] = " +
              render_vector3_scalar_expression(
                  expanded.operation->expression, emission, Vector3Layout::aos, "i") +
              ";\n"
              "    }\n"
              "}\n\n";
    return result;
}

auto render_vector3_chunk_loop_function(Emission const& emission,
                                        ExpandedVariant const& expanded,
                                        bool const scalar) -> std::string {
    auto result{"void " + expanded.operation->name + "(" +
                vector3_chunk_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
                ") noexcept {\n"};
    if (!scalar) {
        result += "#if defined(__clang__)\n"
                  "    #pragma clang loop vectorize(disable) interleave(disable)\n"
                  "#elif defined(_MSC_VER)\n"
                  "    #pragma loop(no_vector)\n"
                  "#endif\n";
    }
    result += "    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {\n";
    if (scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "        for (std::int32_t lane{}; lane < Float3Chunk16::capacity; ++lane) {\n"
              "            " +
              expanded.operation->output + "[chunk_index].values[lane] = " +
              render_vector3_scalar_expression(expanded.operation->expression,
                                               emission,
                                               Vector3Layout::chunked,
                                               "chunk_index",
                                               "lane") +
              ";\n"
              "        }\n"
              "    }\n"
              "}\n\n";
    return result;
}

auto render_gather_indices(VectorIntrinsics const& intrinsics) -> std::string {
    std::string result{"    auto const gather_indices{"};
    result += intrinsics.width == 8 ? "_mm256_setr_epi32(" : "_mm512_setr_epi32(";
    for (int lane{}; lane < intrinsics.width; ++lane) {
        if (lane != 0) {
            result += ", ";
        }
        result += std::to_string(lane * 3);
    }
    return result + ")};\n";
}

auto render_vector3_aos_vector_function(Emission const& emission,
                                        ExpandedVariant const& expanded,
                                        VectorIntrinsics const& intrinsics) -> std::string {
    Vector3ExpressionRenderer renderer{emission, intrinsics, Vector3Layout::aos, "i", 0};
    auto const value{renderer.render(expanded.operation->expression)};
    return "void " + expanded.operation->name + "(" +
           vector3_aos_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
           ") noexcept {\n" + render_gather_indices(intrinsics) +
           "    std::int32_t i{};\n"
           "    std::int32_t const vectorized_count{count - (count % " +
           std::to_string(intrinsics.width) +
           ")};\n"
           "    for (; i < vectorized_count; i += " +
           std::to_string(intrinsics.width) + ") {\n" + renderer.statements() + "        " +
           std::string{intrinsics.store} + "(" + expanded.operation->output + " + i, " + value +
           ");\n"
           "    }\n"
           "    for (; i < count; ++i) {\n"
           "        " +
           expanded.operation->output + "[i] = " +
           render_vector3_scalar_expression(
               expanded.operation->expression, emission, Vector3Layout::aos, "i") +
           ";\n"
           "    }\n"
           "}\n\n";
}

auto render_vector3_chunk_vector_function(Emission const& emission,
                                          ExpandedVariant const& expanded,
                                          VectorIntrinsics const& intrinsics) -> std::string {
    auto result{
        "void " + expanded.operation->name + "(" +
        vector3_chunk_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
        ") noexcept {\n"
        "    for (std::int32_t chunk_index{}; chunk_index < chunk_count; ++chunk_index) {\n"};
    for (int offset{}; offset < 16; offset += intrinsics.width) {
        Vector3ExpressionRenderer renderer{
            emission, intrinsics, Vector3Layout::chunked, "chunk_index", offset};
        auto const value{renderer.render(expanded.operation->expression)};
        auto const lane_offset{offset == 0 ? std::string{} : " + " + std::to_string(offset)};
        result += "        {\n" + renderer.statements() + "        " +
                  std::string{intrinsics.aligned_store} + "(" + expanded.operation->output +
                  "[chunk_index].values.data()" + lane_offset + ", " + value +
                  ");\n"
                  "        }\n";
    }
    return result + "    }\n}\n\n";
}

auto render_vector3_declaration(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    return render_declaration(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") + "void " +
           expanded.operation->name + "(" +
           vector3_aos_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
           ") noexcept;\n\n"
           "void " +
           expanded.operation->name + "(" +
           vector3_chunk_parameters(emission, expanded, "ML_KERNEL_LAB_RESTRICT ") +
           ") noexcept;\n\n";
}

auto render_vector3_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    auto result{std::string{generated_warning} +
                "#pragma once\n\n#include <array>\n#include <cstdint>\n\n" +
                std::string{native_restrict_definition()} + "namespace " + emission.cpp_namespace +
                " {\n\n" + render_vector3_types()};
    for (auto const backend : {"backend::scalar",
                               "backend::autovec_avx2",
                               "backend::avx2",
                               "backend::autovec_avx512",
                               "backend::avx512"}) {
        result += wrap_namespace(backend, render_vector3_declaration(emission, expanded));
    }
    return result + "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_vector3_source(Emission const& emission,
                           ExpandedVariant const& expanded,
                           VectorIntrinsics const& intrinsics,
                           bool const include_scalar) -> std::string {
    std::string functions;
    if (include_scalar) {
        functions += wrap_namespace(
            "backend::scalar",
            render_map_scalar_function(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
                render_vector3_aos_loop_function(emission, expanded, true) +
                render_vector3_chunk_loop_function(emission, expanded, true));
    }
    auto const autovec_namespace{intrinsics.width == 8 ? "backend::autovec_avx2"
                                                       : "backend::autovec_avx512"};
    functions += wrap_namespace(
        autovec_namespace,
        render_map_autovec_function(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
            render_vector3_aos_loop_function(emission, expanded, false) +
            render_vector3_chunk_loop_function(emission, expanded, false));
    auto const explicit_namespace{intrinsics.width == 8 ? "backend::avx2" : "backend::avx512"};
    functions +=
        wrap_namespace(explicit_namespace,
                       render_map_vector_function(
                           expanded, intrinsics, "", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
                           render_vector3_aos_vector_function(emission, expanded, intrinsics) +
                           render_vector3_chunk_vector_function(emission, expanded, intrinsics));
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" + functions +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
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
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float " : "void "};
    return std::string{return_type} + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept;\n\n";
}

auto render_chunk_declaration(ExpandedVariant const& expanded) -> std::string {
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float " : "void "};
    return std::string{return_type} + soaos_implementation_name(expanded, "") + "(" +
           soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept;\n\n";
}

auto wrap_namespace(std::string_view const cpp_namespace, std::string const& content)
    -> std::string {
    return "namespace " + std::string{cpp_namespace} + " {\n\n" + content + "}\n\n";
}

auto render_backend_declarations(ExpandedVariant const& expanded,
                                 std::string_view const cpp_namespace,
                                 bool const include_chunks) -> std::string {
    auto content{render_declaration(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ")};
    if (include_chunks) {
        content += render_chunk_declaration(expanded);
    }
    return wrap_namespace(cpp_namespace, content);
}

auto append_chunk_function(std::string result,
                           std::optional<int> const soaos_lanes,
                           std::string const& function) -> std::string {
    if (soaos_lanes) {
        result += function;
    }
    return result;
}

}

auto render_avx2_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (expanded.operation->kind != OperationKind::map) {
        throw std::invalid_argument{"unreal-avx2-lab supports only map operations"};
    }
    return generated_warning +
           std::string{"#pragma once\n\n#include \"CoreTypes.h\"\n\nnamespace "} +
           emission.cpp_namespace + " {\n\n" +
           render_declaration(expanded, "_autovec_avx2", "int32", "RESTRICT ") +
           render_declaration(expanded, "_avx2", "int32", "RESTRICT ") +
           render_declaration(expanded, "_avx2_unrolled", "int32", "RESTRICT ") + "}\n";
}

auto render_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (expanded.operation->kind != OperationKind::map) {
        throw std::invalid_argument{"unreal-avx2-lab supports only map operations"};
    }
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\nnamespace " + emission.cpp_namespace + " {\n\n" +
           render_map_autovec_function(expanded, "_autovec_avx2", "int32", "RESTRICT ") +
           render_map_vector_function(expanded, Avx2, "_avx2", 1, "int32", "RESTRICT ") +
           render_map_vector_function(expanded, Avx2, "_avx2_unrolled", 4, "int32", "RESTRICT ") +
           "}\n";
}

auto render_native_simd_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (!emission.vector3_groups.empty()) {
        return render_vector3_header(emission, expanded);
    }
    auto result{std::string{generated_warning} + "#pragma once\n\n" +
                (emission.soaos_lanes ? "#include <array>\n" : "") + "#include <cstdint>\n\n" +
                std::string{native_restrict_definition()} + "namespace " + emission.cpp_namespace +
                " {\n\n"};
    if (emission.soaos_lanes) {
        result += render_soaos_block();
    }
    auto const chunks{emission.soaos_lanes.has_value()};
    if (expanded.operation->kind == OperationKind::map) {
        result += render_backend_declarations(expanded, "backend::scalar", chunks);
        result += render_backend_declarations(expanded, "backend::autovec_avx2", chunks);
        result += render_backend_declarations(expanded, "backend::avx2", chunks);
        result += render_backend_declarations(expanded, "backend::avx2_unrolled", chunks);
        result += render_backend_declarations(expanded, "backend::autovec_avx512", chunks);
        result += render_backend_declarations(expanded, "backend::avx512", chunks);
    } else {
        if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
            result += render_backend_declarations(expanded, "strict::backend::scalar", chunks);
            result +=
                render_backend_declarations(expanded, "strict::backend::autovec_avx2", chunks);
            result +=
                render_backend_declarations(expanded, "strict::backend::autovec_avx512", chunks);
        }
        if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
            result +=
                render_backend_declarations(expanded, "relaxed::backend::autovec_avx2", chunks);
            result += render_backend_declarations(expanded, "relaxed::backend::avx2", chunks);
            result +=
                render_backend_declarations(expanded, "relaxed::backend::avx2_unrolled", chunks);
            result +=
                render_backend_declarations(expanded, "relaxed::backend::autovec_avx512", chunks);
            result += render_backend_declarations(expanded, "relaxed::backend::avx512", chunks);
            result +=
                render_backend_declarations(expanded, "relaxed::backend::avx512_unrolled", chunks);
        }
    }
    if (emission.dispatch_source) {
        auto declarations{
            std::string{"enum class X86SimdBackend : std::uint8_t {\n"
                        "    avx2,\n"
                        "    avx512,\n"
                        "};\n\n"} +
            render_declaration(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ")};
        if (chunks) {
            declarations += render_chunk_declaration(expanded);
        }
        declarations += "auto get_backend() noexcept -> X86SimdBackend;\n\n";
        result += wrap_namespace(
            expanded.operation->kind == OperationKind::sum ? "relaxed::dispatch" : "dispatch",
            declarations);
    }
    return result + "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (!emission.vector3_groups.empty()) {
        return render_vector3_source(emission, expanded, Avx2, true);
    }
    auto functions{std::string{}};
    if (expanded.operation->kind == OperationKind::map) {
        functions += wrap_namespace(
            "backend::scalar",
            append_chunk_function(
                render_map_scalar_function(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                emission.soaos_lanes,
                render_soaos_map_loop_function(expanded, "", true)));
        functions += wrap_namespace(
            "backend::autovec_avx2",
            append_chunk_function(render_map_autovec_function(
                                      expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                                  emission.soaos_lanes,
                                  render_soaos_map_loop_function(expanded, "", false)));
        functions += wrap_namespace(
            "backend::avx2",
            append_chunk_function(
                render_map_vector_function(
                    expanded, Avx2, "", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                emission.soaos_lanes,
                render_soaos_map_vector_function(expanded, Avx2, "", 1)));
        functions += wrap_namespace(
            "backend::avx2_unrolled",
            append_chunk_function(
                render_map_vector_function(
                    expanded, Avx2, "", 4, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                emission.soaos_lanes,
                render_soaos_map_vector_function(expanded, Avx2, "", 2)));
    } else {
        if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
            functions += wrap_namespace(
                "strict::backend::scalar",
                append_chunk_function(render_sum_scalar_function(
                                          expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                                      emission.soaos_lanes,
                                      render_soaos_sum_loop_function(expanded, "", true)));
            functions += wrap_namespace(
                "strict::backend::autovec_avx2",
                append_chunk_function(render_sum_autovec_function(
                                          expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                                      emission.soaos_lanes,
                                      render_soaos_sum_loop_function(expanded, "", false)));
        }
        if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
            functions += wrap_namespace(
                "relaxed::backend::avx2",
                append_chunk_function(
                    render_sum_vector_function(
                        expanded, Avx2, "", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                    emission.soaos_lanes,
                    render_soaos_sum_vector_function(expanded, Avx2, "", 1)));
            functions += wrap_namespace(
                "relaxed::backend::avx2_unrolled",
                append_chunk_function(
                    render_sum_vector_function(
                        expanded, Avx2, "", 4, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                    emission.soaos_lanes,
                    render_soaos_sum_vector_function(expanded, Avx2, "", 4)));
        }
    }
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" + functions +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx512_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (!emission.vector3_groups.empty()) {
        return render_vector3_source(emission, expanded, Avx512, false);
    }
    auto functions{std::string{}};
    if (expanded.operation->kind == OperationKind::map) {
        functions += wrap_namespace(
            "backend::autovec_avx512",
            append_chunk_function(render_map_autovec_function(
                                      expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                                  emission.soaos_lanes,
                                  render_soaos_map_loop_function(expanded, "", false)));
        functions += wrap_namespace(
            "backend::avx512",
            append_chunk_function(
                render_map_vector_function(
                    expanded, Avx512, "", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                emission.soaos_lanes,
                render_soaos_map_vector_function(expanded, Avx512, "", 1)));
    } else {
        if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
            functions += wrap_namespace(
                "strict::backend::autovec_avx512",
                append_chunk_function(render_sum_autovec_function(
                                          expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                                      emission.soaos_lanes,
                                      render_soaos_sum_loop_function(expanded, "", false)));
        }
        if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
            functions += wrap_namespace(
                "relaxed::backend::avx512",
                append_chunk_function(
                    render_sum_vector_function(
                        expanded, Avx512, "", 1, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                    emission.soaos_lanes,
                    render_soaos_sum_vector_function(expanded, Avx512, "", 1)));
            functions += wrap_namespace(
                "relaxed::backend::avx512_unrolled",
                append_chunk_function(
                    render_sum_vector_function(
                        expanded, Avx512, "", 4, "std::int32_t", "ML_KERNEL_LAB_RESTRICT "),
                    emission.soaos_lanes,
                    render_soaos_sum_vector_function(expanded, Avx512, "", 4)));
        }
    }
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" + functions +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_relaxed_autovec_source(Emission const& emission,
                                          ExpandedVariant const& expanded,
                                          std::string_view const instruction_set) -> std::string {
    validate_lab_variant(expanded);
    if (expanded.operation->kind != OperationKind::sum ||
        !has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        throw std::invalid_argument{
            "relaxed native autovectorization supports only opted-in sum operations"};
    }
    auto const soaos_function{emission.soaos_lanes
                                  ? render_soaos_sum_loop_function(expanded, "", false, true)
                                  : std::string{}};
    auto const functions{
        render_sum_autovec_function(expanded, "", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
        soaos_function};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#if defined(_MSC_VER) && !defined(__clang__)\n#pragma "
           "fp_contract(off)\n#endif\n\n" +
           std::string{native_restrict_definition()} + "namespace " + emission.cpp_namespace +
           " {\n\n" +
           wrap_namespace("relaxed::backend::autovec_" + std::string{instruction_set}, functions) +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_simd_dispatch_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float" : "void"};
    auto const return_prefix{expanded.operation->kind == OperationKind::sum ? "return " : ""};
    auto const chunk_type{emission.soaos_lanes
                              ? "using ChunkKernel = " + std::string{return_type} + " (*)(" +
                                    soaos_function_pointer_parameters(expanded) + ") noexcept;\n\n"
                              : std::string{}};
    auto const chunk_field{emission.soaos_lanes ? "    ChunkKernel chunk_kernel;\n" : ""};
    auto const backend_prefix{expanded.operation->kind == OperationKind::sum
                                  ? "::" + emission.cpp_namespace + "::relaxed::backend::"
                                  : "::" + emission.cpp_namespace + "::backend::"};
    auto const avx512_name{backend_prefix + "avx512::" + expanded.operation->name};
    auto const avx2_name{backend_prefix + "avx2::" + expanded.operation->name};
    auto const chunk_avx512{emission.soaos_lanes ? ", " + avx512_name : std::string{}};
    auto const chunk_avx2{emission.soaos_lanes ? ", " + avx2_name : std::string{}};
    auto const chunk_dispatch{emission.soaos_lanes
                                  ? std::string{return_type} + " " +
                                        soaos_implementation_name(expanded, "") + "(" +
                                        soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") +
                                        ") noexcept {\n"
                                        "    " +
                                        std::string{return_prefix} + "selection().chunk_kernel(" +
                                        soaos_arguments(expanded) +
                                        ");\n"
                                        "}\n\n"
                                  : std::string{}};
    auto const dispatch_namespace{
        expanded.operation->kind == OperationKind::sum ? "relaxed::dispatch" : "dispatch"};
    auto const body{"namespace {\n\nusing Kernel = " + std::string{return_type} + " (*)(" +
                    function_pointer_parameters(expanded) + ") noexcept;\n\n" + chunk_type +
                    "struct Selection {\n"
                    "    X86SimdBackend backend;\n"
                    "    Kernel kernel;\n" +
                    chunk_field +
                    "};\n\n"
                    "auto select_backend() noexcept -> Selection {\n"
                    "    auto const features{cpu_features::GetX86Info().features};\n"
                    "    if (features.avx512f && features.avx512cd && features.avx512bw &&\n"
                    "        features.avx512dq && features.avx512vl) {\n"
                    "        return {X86SimdBackend::avx512, " +
                    avx512_name + chunk_avx512 +
                    "};\n"
                    "    }\n"
                    "    return {X86SimdBackend::avx2, " +
                    avx2_name + chunk_avx2 +
                    "};\n"
                    "}\n\n"
                    "auto selection() noexcept -> Selection const& {\n"
                    "    static auto const value{select_backend()};\n"
                    "    return value;\n"
                    "}\n\n}\n\n" +
                    std::string{return_type} + " " + implementation_name(expanded, "") + "(" +
                    raw_parameters(expanded, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
                    ") noexcept {\n"
                    "    " +
                    std::string{return_prefix} + "selection().kernel(" + raw_arguments(expanded) +
                    ");\n"
                    "}\n\n" +
                    chunk_dispatch +
                    "auto get_backend() noexcept -> X86SimdBackend {\n"
                    "    return selection().backend;\n"
                    "}\n\n"};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <cpuinfo_x86.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" +
           wrap_namespace(dispatch_namespace, body) + "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

}
