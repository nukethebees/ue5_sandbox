#include "lowering.h"

#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/variant_kind.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace kernel_codegen::detail {

auto ExpandedVariants::expand(Document const& document, ModuleId const module)
    -> lispb::codegen::sexpr::HandleRange {
    auto const begin{static_cast<std::uint32_t>(operations_.size())};
    std::vector<StorageKind> storage;
    for (auto const operation : document.modules.operations(module)) {
        for (auto const variant : document.operations.variants(operation)) {
            auto const first{operations_.size()};
            expand_storage(document, operation, variant, 0, storage);
            auto const end{operations_.size()};
            auto const types{document.type_sets.types(document.operations.type_set(operation))};
            // Reuse each storage combination for every concrete scalar type.
            for (auto const type : types.subspan(1)) {
                for (auto index{first}; index < end; ++index) {
                    operations_.push_back(operation);
                    variants_.push_back(variant);
                    types_.push_back(type);
                    auto const range{storage_ranges_[index]};
                    storage_ranges_.push_back(range);
                }
            }
        }
    }
    return {begin, static_cast<std::uint32_t>(operations_.size()) - begin};
}

void ExpandedVariants::clear() {
    operations_.clear();
    variants_.clear();
    types_.clear();
    storage_ranges_.clear();
    storage_.clear();
}

void ExpandedVariants::expand_storage(Document const& document,
                                      OperationId const operation,
                                      VariantId const variant,
                                      std::size_t const operand_index,
                                      std::vector<StorageKind>& storage) {
    auto const operands{document.operations.operands(operation)};
    if (operand_index == operands.size()) {
        if (document.variants.kind(variant) == VariantKind::out_of_place &&
            std::ranges::find(storage, StorageKind::array) == storage.end()) {
            return;
        }
        operations_.push_back(operation);
        variants_.push_back(variant);
        types_.push_back(document.type_sets.types(document.operations.type_set(operation)).front());
        storage_ranges_.push_back({static_cast<std::uint32_t>(storage_.size()),
                                   static_cast<std::uint32_t>(storage.size())});
        storage_.insert(storage_.end(), storage.begin(), storage.end());
        return;
    }

    auto const operand{operands[operand_index]};
    for (auto const kind : document.operands.storage(operand)) {
        if (document.variants.target(variant) == operand && kind != StorageKind::array) {
            continue;
        }
        storage.push_back(kind);
        expand_storage(document, operation, variant, operand_index + 1, storage);
        storage.pop_back();
    }
}

auto ExpandedVariants::storage(ExpandedVariantId const id) const -> std::span<StorageKind const> {
    auto const range{storage_ranges_[id.value]};
    return std::span{storage_}.subspan(range.offset, range.count);
}

auto is_target(Document const& document,
               ExpandedVariantId const id,
               std::size_t const operand_index) -> bool {
    auto const& expanded{document.expanded_variants};
    return document.variants.target(expanded.variant(id)) ==
           document.operations.operands(expanded.operation(id))[operand_index];
}

auto operand_names(Document const& document,
                   ExpandedVariantId const id,
                   std::size_t const operand_index) -> std::vector<std::string> {
    auto const& expanded{document.expanded_variants};
    auto const operation{expanded.operation(id)};
    auto const operand{document.operations.operands(operation)[operand_index]};
    auto const name{document.text(document.operands.name(operand))};
    if (!document.operands.is_component(operand)) {
        return {name};
    }

    std::vector<std::string> result;
    auto const components{document.operations.components(operation)};
    result.reserve(components.size());
    for (auto const component : components) {
        result.push_back(name + "_" + document.text(component));
    }
    return result;
}

auto output_names(Document const& document, ExpandedVariantId const id)
    -> std::vector<std::string> {
    auto const& expanded{document.expanded_variants};
    auto const operation{expanded.operation(id)};
    auto const name{document.text(document.operations.output(operation))};
    if (operation.kind() != OperationKind::component_map) {
        return {name};
    }

    std::vector<std::string> result;
    auto const components{document.operations.components(operation)};
    result.reserve(components.size());
    for (auto const component : components) {
        result.push_back(name + "_" + document.text(component));
    }
    return result;
}

auto raw_name(Document const& document, ExpandedVariantId const id) -> std::string {
    auto const& expanded{document.expanded_variants};
    auto result{document.text(document.operations.name(expanded.operation(id))) + "_" +
                std::string{to_serialized_string(expanded.type(id))}};
    for (auto const storage : expanded.storage(id)) {
        result += storage == StorageKind::array ? "_array" : "_scalar";
    }
    result += document.variants.kind(expanded.variant(id)) == VariantKind::in_place
                ? "_in_place_raw"
                : "_raw";
    return result;
}

auto contains_constant(Expressions const& expressions, ExpressionId const expression) -> bool {
    return expression.kind() == ExpressionKind::constant ||
           (expression.kind() == ExpressionKind::binary &&
            (contains_constant(expressions, expressions.left(expression)) ||
             contains_constant(expressions, expressions.right(expression))));
}

auto array_names(Document const& document, ExpandedVariantId const id) -> std::vector<std::string> {
    auto const& expanded{document.expanded_variants};
    std::vector<std::string> result;
    auto const storage{expanded.storage(id)};
    for (std::size_t index{}; index < storage.size(); ++index) {
        if (storage[index] == StorageKind::array) {
            auto const names{operand_names(document, id, index)};
            result.insert(result.end(), names.begin(), names.end());
        }
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        auto const names{output_names(document, id)};
        result.insert(result.end(), names.begin(), names.end());
    }
    return result;
}

auto count_source(Document const& document, ExpandedVariantId const id) -> std::string {
    auto const& expanded{document.expanded_variants};
    if (auto const target{document.variants.target(expanded.variant(id))}) {
        return operand_names(document, id, document.operand_index(expanded.operation(id), *target))
            .front();
    }
    auto const storage{expanded.storage(id)};
    for (std::size_t index{}; index < storage.size(); ++index) {
        if (storage[index] == StorageKind::array) {
            return operand_names(document, id, index).front();
        }
    }
    throw std::logic_error{"out-of-place kernel has no array input"};
}

auto standard_type(ScalarType const type) -> std::string {
    if (type == ScalarType::int32) {
        return "std::int32_t";
    }
    if (type == ScalarType::uint32) {
        return "std::uint32_t";
    }
    return std::string{to_serialized_string(type)};
}

}
