#include "lowering.h"

#include <algorithm>
#include <stdexcept>

namespace kernel_codegen::detail {
namespace {

auto type_set(KernelModule const& module, std::string const& name) -> TypeSet const& {
    auto const found{std::ranges::find_if(
        module.type_sets, [&](auto const& candidate) { return candidate.name == name; })};
    if (found == module.type_sets.end()) {
        throw std::invalid_argument{"Unknown kernel type-set: " + name};
    }
    return *found;
}

void expand_storage(Operation const& operation,
                    Variant const& variant,
                    std::size_t const operand_index,
                    std::vector<StorageKind>& storage,
                    std::vector<std::vector<StorageKind>>& output) {
    if (operand_index == operation.operands.size()) {
        if (variant.kind == VariantKind::out_of_place &&
            std::ranges::find(storage, StorageKind::array) == storage.end()) {
            return;
        }
        output.push_back(storage);
        return;
    }

    auto const& operand{operation.operands[operand_index]};
    for (auto const kind : operand.storage) {
        if (variant.kind == VariantKind::in_place && operand.name == *variant.target &&
            kind != StorageKind::array) {
            continue;
        }
        storage.push_back(kind);
        expand_storage(operation, variant, operand_index + 1, storage, output);
        storage.pop_back();
    }
}

}

auto expand(KernelModule const& module) -> std::vector<ExpandedVariant> {
    std::vector<ExpandedVariant> result;
    for (auto const& operation : module.operations) {
        for (auto const& variant : operation.variants) {
            std::vector<std::vector<StorageKind>> combinations;
            std::vector<StorageKind> storage;
            expand_storage(operation, variant, 0, storage, combinations);
            for (auto const& type : type_set(module, operation.type_set).types) {
                for (auto const& combination : combinations) {
                    result.push_back(ExpandedVariant{&operation, &variant, type, combination});
                }
            }
        }
    }
    return result;
}

auto is_target(ExpandedVariant const& expanded, std::size_t const operand_index) -> bool {
    return expanded.variant->kind == VariantKind::in_place &&
           expanded.operation->operands[operand_index].name == *expanded.variant->target;
}

auto raw_name(ExpandedVariant const& expanded) -> std::string {
    auto result{expanded.operation->name + "_" + expanded.type};
    for (auto const storage : expanded.storage) {
        result += storage == StorageKind::array ? "_array" : "_scalar";
    }
    if (expanded.variant->kind == VariantKind::in_place) {
        result += "_in_place_raw";
    } else if (expanded.variant->kind == VariantKind::sum) {
        result += "_sum_raw";
    } else {
        result += "_raw";
    }
    return result;
}

auto contains_constant(Expression const& expression) -> bool {
    return expression.kind == ExpressionKind::constant ||
           std::ranges::any_of(expression.arguments, contains_constant);
}

auto array_names(ExpandedVariant const& expanded) -> std::vector<std::string> {
    std::vector<std::string> result;
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::array) {
            result.push_back(expanded.operation->operands[index].name);
        }
    }
    if (expanded.variant->kind == VariantKind::out_of_place) {
        result.push_back(expanded.operation->output);
    }
    return result;
}

auto count_source(ExpandedVariant const& expanded) -> std::string {
    if (expanded.variant->kind == VariantKind::in_place) {
        return *expanded.variant->target;
    }
    for (std::size_t index{}; index < expanded.operation->operands.size(); ++index) {
        if (expanded.storage[index] == StorageKind::array) {
            return expanded.operation->operands[index].name;
        }
    }
    throw std::logic_error{"out-of-place kernel has no array input"};
}

auto standard_type(std::string_view const type) -> std::string {
    if (type == "int32") {
        return "std::int32_t";
    }
    if (type == "uint32") {
        return "std::uint32_t";
    }
    return std::string{type};
}

auto render_expression(Expression const& expression,
                       Operation const& operation,
                       std::vector<StorageKind> const& storage,
                       std::string const& concrete_type) -> std::string {
    if (expression.kind == ExpressionKind::literal) {
        return "static_cast<" + concrete_type + ">(" + expression.value + ")";
    }
    if (expression.kind == ExpressionKind::reference) {
        auto const found{std::ranges::find_if(operation.operands, [&](auto const& operand) {
            return operand.name == expression.value;
        })};
        auto const index{static_cast<std::size_t>(found - operation.operands.begin())};
        return expression.value + (storage[index] == StorageKind::array ? "[i]" : "");
    }
    if (expression.kind == ExpressionKind::constant) {
        switch (expression.constant) {
            case ConstantKind::nan:
                return "std::numeric_limits<" + concrete_type + ">::quiet_NaN()";
            case ConstantKind::infinity:
                return "std::numeric_limits<" + concrete_type + ">::infinity()";
            case ConstantKind::negative_infinity:
                return "(-std::numeric_limits<" + concrete_type + ">::infinity())";
        }
    }
    return "(" + render_expression(expression.arguments[0], operation, storage, concrete_type) +
           " " + expression.value + " " +
           render_expression(expression.arguments[1], operation, storage, concrete_type) + ")";
}

}
