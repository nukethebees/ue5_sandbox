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
        throw std::invalid_argument{
            "SIMD lab supports only pairwise-disjoint float maps and sums"};
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

auto title_name(std::string_view const name) -> std::string {
    std::string result;
    bool capitalize{true};
    for (auto const character : name) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        result += capitalize && character >= 'a' && character <= 'z'
                      ? static_cast<char>(character - 'a' + 'A')
                      : character;
        capitalize = false;
    }
    return result;
}

auto soaos_block_name(ExpandedVariant const& expanded) -> std::string {
    return title_name(expanded.operation->name) + "SoAoSBlock";
}

auto soaos_implementation_name(ExpandedVariant const& expanded,
                               std::string_view const suffix) -> std::string {
    return expanded.operation->name + "_soaos" + std::string{suffix};
}

auto soaos_parameters(ExpandedVariant const& expanded,
                      std::string_view const restriction) -> std::string {
    auto result{soaos_block_name(expanded)};
    if (expanded.operation->kind == OperationKind::sum) {
        result += " const";
    }
    result += "* ";
    result += restriction;
    result += "blocks";
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::scalar) {
            result += ", float const " + expanded.operation->operands[index].name;
        }
    }
    return result + ", std::int32_t const block_count";
}

auto soaos_arguments(ExpandedVariant const& expanded) -> std::string {
    std::string result{"blocks"};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::scalar) {
            result += ", " + expanded.operation->operands[index].name;
        }
    }
    return result + ", block_count";
}

auto soaos_function_pointer_parameters(ExpandedVariant const& expanded) -> std::string {
    auto result{soaos_block_name(expanded)};
    if (expanded.operation->kind == OperationKind::sum) {
        result += " const";
    }
    result += "*";
    for (auto const storage : expanded.storage) {
        if (storage == StorageKind::scalar) {
            result += ", float";
        }
    }
    return result + ", std::int32_t";
}

auto render_soaos_block(ExpandedVariant const& expanded) -> std::string {
    auto result{"struct alignas(64) " + soaos_block_name(expanded) + " {\n"
                "    static constexpr std::int32_t capacity{16};\n\n"};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::array) {
            result += "    std::array<float, capacity> " +
                      expanded.operation->operands[index].name + "{};\n";
        }
    }
    if (expanded.operation->kind == OperationKind::map) {
        result += "    std::array<float, capacity> " + expanded.operation->output + "{};\n";
    }
    return result + "    std::int32_t size{};\n};\n\n";
}

auto has_floating_point_mode(ExpandedVariant const& expanded,
                             FloatingPointMode const mode) -> bool {
    return std::ranges::find(expanded.operation->floating_point_modes, mode) !=
           expanded.operation->floating_point_modes.end();
}

auto strict_autovec_suffix(ExpandedVariant const& expanded,
                           std::string_view const instruction_set) -> std::string {
    auto const policy{expanded.operation->kind == OperationKind::sum ? "_autovec_strict_"
                                                                     : "_autovec_"};
    return std::string{policy} + std::string{instruction_set};
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
        : expanded_{expanded},
          intrinsics_{intrinsics},
          indentation_{indentation},
          block_index_{block_index},
          lane_offset_{lane_offset} {}

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
        statements_ += indentation_ + "auto const " + result + "{" +
                       std::string{intrinsic} + "(" + lhs + ", " + rhs + ")};\n";
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
        auto const offset{lane_offset_ == 0 ? std::string{}
                                            : " + " + std::to_string(lane_offset_)};
        statements_ += indentation_ + "auto const " + result + "{" +
                       std::string{intrinsics_.aligned_load} + "(blocks[" + block_index_ + "]." +
                       operand->name + ".data()" + offset + ")};\n";
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
        return "blocks[" + std::string{block_index} + "]." + operand->name + "[" +
               std::string{lane_index} + "]";
    }
    if (expression.kind != ExpressionKind::binary ||
        (expression.value != "+" && expression.value != "*")) {
        throw std::invalid_argument{
            "SIMD lab supports only operand references, addition, and multiplication"};
    }
    return "(" + render_soaos_scalar_expression(
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

auto render_map_autovec_function(ExpandedVariant const& expanded,
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

auto scalar_loop_controls() -> std::string_view {
    return "#if defined(__clang__)\n"
           "    #pragma clang loop vectorize(disable) interleave(disable) unroll(disable)\n"
           "#elif defined(_MSC_VER)\n"
           "    #pragma loop(no_vector)\n"
           "#endif\n";
}

auto render_map_scalar_function(ExpandedVariant const& expanded,
                                std::string_view const count_type,
                                std::string_view const restriction) -> std::string {
    return "void " + implementation_name(expanded, "_scalar") + "(" +
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

auto render_sum_autovec_function(ExpandedVariant const& expanded,
                                 std::string_view const suffix,
                                 std::string_view const count_type,
                                 std::string_view const restriction) -> std::string {
    return "float " + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept {\n"
           "    float result{};\n"
           "    for (" +
           std::string{count_type} + " i{0}; i < count; ++i) {\n"
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
                                std::string_view const count_type,
                                std::string_view const restriction) -> std::string {
    return "float " + implementation_name(expanded, "_scalar") + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept {\n"
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
        result += "    " + std::string{count_type} +
                  " const unrolled_count{count - (count % " +
                  std::to_string(chunk_width) + ")};\n"
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
                      "(accumulator_" + std::to_string(accumulator) + ", " + value + ");\n"
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
    result += "    " + std::string{count_type} +
              " const vectorized_count{count - (count % " +
              std::to_string(intrinsics.width) + ")};\n"
              "    for (; i < vectorized_count; i += " +
              std::to_string(intrinsics.width) + ") {\n"
              "        {\n";
    VectorExpressionRenderer expression_renderer{expanded, intrinsics, "            ", {}};
    auto const value{expression_renderer.render(expanded.operation->expression)};
    result += expression_renderer.statements() +
              "            accumulator_0 = " + std::string{intrinsics.add} +
              "(accumulator_0, " + value + ");\n"
              "        }\n"
              "    }\n\n";
    result += "    float lanes[" + std::to_string(intrinsics.width) + "]{};\n"
              "    " +
              std::string{intrinsics.store} + "(lanes, accumulator_0);\n"
              "    float result{};\n"
              "    for (int lane{}; lane < " +
              std::to_string(intrinsics.width) + "; ++lane) {\n"
              "        result += lanes[lane];\n"
              "    }\n"
              "    for (; i < count; ++i) {\n"
              "        result += " +
              render_expression(expanded.operation->expression,
                                *expanded.operation,
                                expanded.storage,
                                expanded.type) +
              ";\n"
              "    }\n"
              "    return result;\n"
              "}\n\n";
    return result;
}

auto render_soaos_map_loop_function(ExpandedVariant const& expanded,
                                    std::string_view const suffix,
                                    bool const force_scalar) -> std::string {
    auto result{"void " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"
                "#if defined(__clang__)\n"
                "    #pragma clang loop vectorize(disable) interleave(disable)\n"
                "#elif defined(_MSC_VER)\n"
                "    #pragma loop(no_vector)\n"
                "#endif\n"
                "    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {\n"};
    if (force_scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "        for (std::int32_t lane{}; lane < " + soaos_block_name(expanded) +
              "::capacity; ++lane) {\n"
              "            blocks[block_index]." +
              expanded.operation->output + "[lane] = " +
              render_soaos_scalar_expression(
                  expanded.operation->expression, expanded, "block_index", "lane") +
              ";\n"
              "        }\n"
              "    }\n"
              "}\n\n";
    return result;
}

auto render_soaos_sum_loop_function(ExpandedVariant const& expanded,
                                    std::string_view const suffix,
                                    bool const force_scalar) -> std::string {
    auto result{"float " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"
                "    float result{};\n"
                "#if defined(__clang__)\n"
                "    #pragma clang loop vectorize(disable) interleave(disable)\n"
                "#elif defined(_MSC_VER)\n"
                "    #pragma loop(no_vector)\n"
                "#endif\n"
                "    for (std::int32_t block_index{}; block_index < block_count; ++block_index) {\n"};
    if (force_scalar) {
        result += std::string{scalar_loop_controls()};
    }
    result += "        for (std::int32_t lane{}; lane < " + soaos_block_name(expanded) +
              "::capacity; ++lane) {\n"
              "            result += " +
              render_soaos_scalar_expression(
                  expanded.operation->expression, expanded, "block_index", "lane") +
              ";\n"
              "        }\n"
              "    }\n"
              "    return result;\n"
              "}\n\n";
    return result;
}

auto render_soaos_map_block(ExpandedVariant const& expanded,
                            VectorIntrinsics const& intrinsics,
                            std::string const& indentation,
                            std::string const& block_index) -> std::string {
    std::string result;
    for (int lane_offset{}; lane_offset < 16; lane_offset += intrinsics.width) {
        auto const nested_indentation{indentation + "    "};
        auto const [statements, value]{render_soaos_vector_expression(
            expanded, intrinsics, nested_indentation, block_index, lane_offset)};
        auto const offset{lane_offset == 0 ? std::string{}
                                           : " + " + std::to_string(lane_offset)};
        result += indentation + "{\n" + statements + nested_indentation +
                  std::string{intrinsics.aligned_store} +
                  "(blocks[" + block_index + "]." + expanded.operation->output + ".data()" +
                  offset + ", " + value + ");\n" + indentation + "}\n";
    }
    return result;
}

auto render_soaos_map_vector_function(ExpandedVariant const& expanded,
                                      VectorIntrinsics const& intrinsics,
                                      std::string_view const suffix,
                                      int const blocks_per_iteration) -> std::string {
    auto result{"void " + soaos_implementation_name(expanded, suffix) + "(" +
                soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"};
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::scalar) {
            auto const& name{expanded.operation->operands[index].name};
            result += "    auto const " + name + "_vector{" + std::string{intrinsics.set1} +
                      "(" + name + ")};\n";
        }
    }
    result += "    std::int32_t block_index{};\n";
    if (blocks_per_iteration > 1) {
        result += "    std::int32_t const unrolled_count{block_count - (block_count % " +
                  std::to_string(blocks_per_iteration) + ")};\n"
                  "    for (; block_index < unrolled_count; block_index += " +
                  std::to_string(blocks_per_iteration) + ") {\n";
        for (int block_offset{}; block_offset < blocks_per_iteration; ++block_offset) {
            auto const block_expression{block_offset == 0
                                            ? std::string{"block_index"}
                                            : "block_index + " + std::to_string(block_offset)};
            result += render_soaos_map_block(
                expanded, intrinsics, "        ", block_expression);
        }
        result += "    }\n\n";
    }
    result += "    for (; block_index < block_count; ++block_index) {\n" +
              render_soaos_map_block(expanded, intrinsics, "        ", "block_index") +
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
    result += "    std::int32_t block_index{};\n";
    auto const vectors_per_block{16 / intrinsics.width};
    auto const blocks_per_iteration{unroll / vectors_per_block};
    if (blocks_per_iteration > 1) {
        result += "    std::int32_t const unrolled_count{block_count - (block_count % " +
                  std::to_string(blocks_per_iteration) + ")};\n"
                  "    for (; block_index < unrolled_count; block_index += " +
                  std::to_string(blocks_per_iteration) + ") {\n";
        for (int block_offset{}; block_offset < blocks_per_iteration; ++block_offset) {
            auto const block_expression{block_offset == 0
                                            ? std::string{"block_index"}
                                            : "block_index + " + std::to_string(block_offset)};
            for (int vector_offset{}; vector_offset < vectors_per_block; ++vector_offset) {
                auto const accumulator{block_offset * vectors_per_block + vector_offset};
                auto const indentation{std::string{"            "}};
                auto const [statements, value]{render_soaos_vector_expression(
                    expanded,
                    intrinsics,
                    indentation,
                    block_expression,
                    vector_offset * intrinsics.width)};
                result += "        {\n" + statements + indentation + "accumulator_" +
                          std::to_string(accumulator) + " = " +
                          std::string{intrinsics.add} + "(accumulator_" +
                          std::to_string(accumulator) + ", " + value + ");\n";
                result += "        }\n";
            }
        }
        result += "    }\n\n";
    }
    result += "    for (; block_index < block_count; ++block_index) {\n";
    for (int vector_offset{}; vector_offset < vectors_per_block; ++vector_offset) {
        auto const indentation{std::string{"            "}};
        auto const [statements, value]{render_soaos_vector_expression(expanded,
                                                                       intrinsics,
                                                                       indentation,
                                                                       "block_index",
                                                                       vector_offset *
                                                                           intrinsics.width)};
        result += "        {\n" + statements + indentation + "accumulator_0 = " +
                  std::string{intrinsics.add} + "(accumulator_0, " + value + ");\n"
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
    result += "    float lanes[" + std::to_string(intrinsics.width) + "]{};\n"
              "    " +
              std::string{intrinsics.store} + "(lanes, accumulator_0);\n"
              "    float result{};\n"
              "    for (int lane{}; lane < " +
              std::to_string(intrinsics.width) + "; ++lane) {\n"
              "        result += lanes[lane];\n"
              "    }\n"
              "    return result;\n"
              "}\n\n";
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
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float " : "void "};
    return std::string{return_type} + implementation_name(expanded, suffix) + "(" +
           raw_parameters(expanded, count_type, restriction) + ") noexcept;\n\n";
}

auto render_soaos_declaration(ExpandedVariant const& expanded,
                              std::string_view const suffix) -> std::string {
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float " : "void "};
    return std::string{return_type} + soaos_implementation_name(expanded, suffix) + "(" +
           soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept;\n\n";
}

auto render_soaos_declarations(ExpandedVariant const& expanded) -> std::string {
    auto result{render_soaos_declaration(expanded, "_scalar")};
    if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
        result += render_soaos_declaration(expanded, strict_autovec_suffix(expanded, "avx2"));
    }
    if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        result += render_soaos_declaration(expanded, "_autovec_relaxed_avx2");
    }
    result += render_soaos_declaration(expanded, "_avx2");
    result += render_soaos_declaration(expanded, "_avx2_unrolled");
    if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
        result += render_soaos_declaration(expanded, strict_autovec_suffix(expanded, "avx512"));
    }
    if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        result += render_soaos_declaration(expanded, "_autovec_relaxed_avx512");
    }
    result += render_soaos_declaration(expanded, "_avx512");
    if (expanded.operation->kind == OperationKind::sum) {
        result += render_soaos_declaration(expanded, "_avx512_unrolled");
    }
    return result + render_soaos_declaration(expanded, "_dispatch");
}

auto render_soaos_avx2_functions(ExpandedVariant const& expanded) -> std::string {
    if (expanded.operation->kind == OperationKind::sum) {
        return render_soaos_sum_loop_function(expanded, "_scalar", true) +
               (has_floating_point_mode(expanded, FloatingPointMode::strict)
                    ? render_soaos_sum_loop_function(
                          expanded, strict_autovec_suffix(expanded, "avx2"), false)
                    : std::string{}) +
               render_soaos_sum_vector_function(expanded, Avx2, "_avx2", 1) +
               render_soaos_sum_vector_function(expanded, Avx2, "_avx2_unrolled", 4);
    }
    return render_soaos_map_loop_function(expanded, "_scalar", true) +
           render_soaos_map_loop_function(expanded, "_autovec_avx2", false) +
           render_soaos_map_vector_function(expanded, Avx2, "_avx2", 1) +
           render_soaos_map_vector_function(expanded, Avx2, "_avx2_unrolled", 2);
}

auto render_soaos_avx512_functions(ExpandedVariant const& expanded) -> std::string {
    if (expanded.operation->kind == OperationKind::sum) {
        return (has_floating_point_mode(expanded, FloatingPointMode::strict)
                    ? render_soaos_sum_loop_function(
                          expanded, strict_autovec_suffix(expanded, "avx512"), false)
                    : std::string{}) +
               render_soaos_sum_vector_function(expanded, Avx512, "_avx512", 1) +
               render_soaos_sum_vector_function(expanded, Avx512, "_avx512_unrolled", 4);
    }
    return render_soaos_map_loop_function(expanded, "_autovec_avx512", false) +
           render_soaos_map_vector_function(expanded, Avx512, "_avx512", 1);
}

}

auto render_avx2_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    if (expanded.operation->kind != OperationKind::map) {
        throw std::invalid_argument{"unreal-avx2-lab supports only map operations"};
    }
    return generated_warning + std::string{"#pragma once\n\n#include \"CoreTypes.h\"\n\nnamespace "} +
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
           render_map_vector_function(
               expanded, Avx2, "_avx2_unrolled", 4, "int32", "RESTRICT ") +
           "}\n";
}

auto render_native_simd_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto result{std::string{generated_warning} +
                "#pragma once\n\n" +
                (emission.soaos_lanes ? "#include <array>\n" : "") +
                "#include <cstdint>\n\n" +
                std::string{native_restrict_definition()} + "namespace " +
                emission.cpp_namespace + " {\n\nenum class X86SimdBackend : std::uint8_t {\n"
                                         "    avx2,\n"
                                         "    avx512,\n"
                                         "};\n\n"};
    if (emission.soaos_lanes) {
        result += render_soaos_block(expanded);
    }
    result += render_declaration(
        expanded, "_scalar", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ");
    if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
        result += render_declaration(expanded,
                                     strict_autovec_suffix(expanded, "avx2"),
                                     "std::int32_t",
                                     "ML_KERNEL_LAB_RESTRICT ");
    }
    if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        result += render_declaration(expanded,
                                     "_autovec_relaxed_avx2",
                                     "std::int32_t",
                                     "ML_KERNEL_LAB_RESTRICT ");
    }
    result += render_declaration(
        expanded, "_avx2", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ");
    result += render_declaration(
        expanded, "_avx2_unrolled", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ");
    if (has_floating_point_mode(expanded, FloatingPointMode::strict)) {
        result += render_declaration(expanded,
                                     strict_autovec_suffix(expanded, "avx512"),
                                     "std::int32_t",
                                     "ML_KERNEL_LAB_RESTRICT ");
    }
    if (has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        result += render_declaration(expanded,
                                     "_autovec_relaxed_avx512",
                                     "std::int32_t",
                                     "ML_KERNEL_LAB_RESTRICT ");
    }
    result +=
        render_declaration(expanded, "_avx512", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           (expanded.operation->kind == OperationKind::sum
                ? render_declaration(expanded,
                                     "_avx512_unrolled",
                                     "std::int32_t",
                                     "ML_KERNEL_LAB_RESTRICT ")
                : std::string{}) +
           render_declaration(
               expanded, "_dispatch", "std::int32_t", "ML_KERNEL_LAB_RESTRICT ");
    if (emission.soaos_lanes) {
        result += render_soaos_declarations(expanded);
    }
    return result + "auto get_" + expanded.operation->name +
           "_backend() noexcept -> X86SimdBackend;\n\n}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto const functions{expanded.operation->kind == OperationKind::sum
                             ? render_sum_scalar_function(expanded,
                                                          "std::int32_t",
                                                          "ML_KERNEL_LAB_RESTRICT ") +
                                   (has_floating_point_mode(expanded, FloatingPointMode::strict)
                                        ? render_sum_autovec_function(
                                              expanded,
                                              strict_autovec_suffix(expanded, "avx2"),
                                              "std::int32_t",
                                              "ML_KERNEL_LAB_RESTRICT ")
                                        : std::string{}) +
                                   render_sum_vector_function(expanded,
                                                              Avx2,
                                                              "_avx2",
                                                              1,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ") +
                                   render_sum_vector_function(expanded,
                                                              Avx2,
                                                              "_avx2_unrolled",
                                                              4,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ")
                             : render_map_scalar_function(expanded,
                                                          "std::int32_t",
                                                          "ML_KERNEL_LAB_RESTRICT ") +
                                   render_map_autovec_function(expanded,
                                                           "_autovec_avx2",
                                                           "std::int32_t",
                                                           "ML_KERNEL_LAB_RESTRICT ") +
                                   render_map_vector_function(expanded,
                                                              Avx2,
                                                              "_avx2",
                                                              1,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ") +
                                   render_map_vector_function(expanded,
                                                              Avx2,
                                                              "_avx2_unrolled",
                                                              4,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ")};
    auto const soaos_functions{emission.soaos_lanes ? render_soaos_avx2_functions(expanded)
                                                    : std::string{}};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" + functions + soaos_functions +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_avx512_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto const functions{expanded.operation->kind == OperationKind::sum
                             ? (has_floating_point_mode(expanded, FloatingPointMode::strict)
                                    ? render_sum_autovec_function(
                                          expanded,
                                          strict_autovec_suffix(expanded, "avx512"),
                                          "std::int32_t",
                                          "ML_KERNEL_LAB_RESTRICT ")
                                    : std::string{}) +
                                   render_sum_vector_function(expanded,
                                                              Avx512,
                                                              "_avx512",
                                                              1,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ") +
                                   render_sum_vector_function(expanded,
                                                              Avx512,
                                                              "_avx512_unrolled",
                                                              4,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ")
                             : render_map_autovec_function(expanded,
                                                           "_autovec_avx512",
                                                           "std::int32_t",
                                                           "ML_KERNEL_LAB_RESTRICT ") +
                                   render_map_vector_function(expanded,
                                                              Avx512,
                                                              "_avx512",
                                                              1,
                                                              "std::int32_t",
                                                              "ML_KERNEL_LAB_RESTRICT ")};
    auto const soaos_functions{emission.soaos_lanes ? render_soaos_avx512_functions(expanded)
                                                    : std::string{}};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <immintrin.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\n\n" + functions + soaos_functions +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_relaxed_autovec_source(Emission const& emission,
                                          ExpandedVariant const& expanded,
                                          std::string_view const suffix) -> std::string {
    validate_lab_variant(expanded);
    if (expanded.operation->kind != OperationKind::sum ||
        !has_floating_point_mode(expanded, FloatingPointMode::relaxed)) {
        throw std::invalid_argument{
            "relaxed native autovectorization supports only opted-in sum operations"};
    }
    auto const soaos_function{
        emission.soaos_lanes
            ? render_soaos_sum_loop_function(expanded, suffix, false)
            : std::string{}};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#if defined(_MSC_VER) && !defined(__clang__)\n#pragma fp_contract(off)\n#endif\n\n" +
           std::string{native_restrict_definition()} + "namespace " + emission.cpp_namespace +
           " {\n\n" +
           render_sum_autovec_function(
               expanded, suffix, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           soaos_function +
           "}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

auto render_native_simd_dispatch_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string {
    validate_lab_variant(expanded);
    auto const dispatch_name{implementation_name(expanded, "_dispatch")};
    auto const avx2_name{implementation_name(expanded, "_avx2")};
    auto const avx512_name{implementation_name(expanded, "_avx512")};
    auto const return_type{expanded.operation->kind == OperationKind::sum ? "float" : "void"};
    auto const return_prefix{expanded.operation->kind == OperationKind::sum ? "return " : ""};
    auto const soaos_type{emission.soaos_lanes
                              ? "using SoaosKernel = " + std::string{return_type} + " (*)(" +
                                    soaos_function_pointer_parameters(expanded) + ") noexcept;\n\n"
                              : std::string{}};
    auto const soaos_field{emission.soaos_lanes ? "    SoaosKernel soaos_kernel;\n" : ""};
    auto const soaos_avx512{emission.soaos_lanes
                                ? ", " + soaos_implementation_name(expanded, "_avx512")
                                : std::string{}};
    auto const soaos_avx2{emission.soaos_lanes
                              ? ", " + soaos_implementation_name(expanded, "_avx2")
                              : std::string{}};
    auto const soaos_dispatch{
        emission.soaos_lanes
            ? std::string{return_type} + " " +
                  soaos_implementation_name(expanded, "_dispatch") + "(" +
                  soaos_parameters(expanded, "ML_KERNEL_LAB_RESTRICT ") + ") noexcept {\n"
                  "    " +
                  std::string{return_prefix} + "selection().soaos_kernel(" +
                  soaos_arguments(expanded) + ");\n"
                  "}\n\n"
            : std::string{}};
    return std::string{generated_warning} + "#include \"" + emission.header_include +
           "\"\n\n#include <cpuinfo_x86.h>\n\n" + std::string{native_restrict_definition()} +
           "namespace " + emission.cpp_namespace + " {\nnamespace {\n\nusing Kernel = " +
           std::string{return_type} + " (*)(" +
           function_pointer_parameters(expanded) + ") noexcept;\n\n" + soaos_type +
           "struct Selection {\n"
           "    X86SimdBackend backend;\n"
           "    Kernel kernel;\n"
           + soaos_field +
           "};\n\n"
           "auto select_backend() noexcept -> Selection {\n"
           "    auto const features{cpu_features::GetX86Info().features};\n"
           "    if (features.avx512f && features.avx512cd && features.avx512bw &&\n"
           "        features.avx512dq && features.avx512vl) {\n"
           "        return {X86SimdBackend::avx512, " + avx512_name + soaos_avx512 + "};\n"
           "    }\n"
           "    return {X86SimdBackend::avx2, " + avx2_name + soaos_avx2 + "};\n"
           "}\n\n"
           "auto selection() noexcept -> Selection const& {\n"
           "    static auto const value{select_backend()};\n"
           "    return value;\n"
           "}\n\n}\n\n"
           + std::string{return_type} + " " + dispatch_name + "(" +
           raw_parameters(expanded, "std::int32_t", "ML_KERNEL_LAB_RESTRICT ") +
           ") noexcept {\n"
           "    " + std::string{return_prefix} + "selection().kernel(" + raw_arguments(expanded) +
           ");\n"
           "}\n\n"
           + soaos_dispatch +
           "auto get_" + expanded.operation->name +
           "_backend() noexcept -> X86SimdBackend {\n"
           "    return selection().backend;\n"
           "}\n\n}\n\n#undef ML_KERNEL_LAB_RESTRICT\n";
}

}
