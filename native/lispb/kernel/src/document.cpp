#include <kernel_codegen/document.h>

#include <lispb/strings.h>

#include <cassert>

namespace kernel_codegen::detail {

namespace {

template <typename Container>
auto is_tail(lispb::codegen::sexpr::HandleRange const range, Container const& container) noexcept
    -> bool {
    auto const size{container.size()};
    return range.offset <= size && range.count == size - range.offset;
}

}
auto Operands::add(lispb::StringId const name,
                   lispb::codegen::sexpr::SourceSpan const span,
                   bool const is_component) -> OperandId {
    auto const id{OperandId{static_cast<std::uint32_t>(name_.size())}};
    name_.push_back(name);
    span_.push_back(span);
    is_component_.push_back(static_cast<std::uint8_t>(is_component));
    storage_ranges_.push_back({static_cast<std::uint32_t>(storage_.size()), 0});
    return id;
}
auto Operands::storage(OperandId const id) const -> std::span<StorageKind const> {
    auto const range{storage_ranges_[id.value]};
    return std::span{storage_}.subspan(range.offset, range.count);
}
void Operands::append_storage(OperandId const id, StorageKind const value) {
    auto& range{storage_ranges_[id.value]};
    assert(is_tail(range, storage_));
    storage_.push_back(value);
    ++range.count;
}
void Operands::clear() {
    name_.clear();
    span_.clear();
    is_component_.clear();
    storage_ranges_.clear();
    storage_.clear();
}

auto Variants::add(VariantKind const kind,
                   lispb::StringId const name,
                   std::optional<OperandId> const target,
                   lispb::codegen::sexpr::SourceSpan const span) -> VariantId {
    auto const id{VariantId{static_cast<std::uint32_t>(kind_.size())}};
    kind_.push_back(kind);
    name_.push_back(name);
    target_.push_back(target);
    span_.push_back(span);
    return id;
}
void Variants::clear() {
    kind_.clear();
    name_.clear();
    target_.clear();
    span_.clear();
}

auto TypeSets::add(lispb::StringId const name, lispb::codegen::sexpr::SourceSpan const span)
    -> TypeSetId {
    auto const id{TypeSetId{static_cast<std::uint32_t>(name_.size())}};
    name_.push_back(name);
    span_.push_back(span);
    types_ranges_.push_back({static_cast<std::uint32_t>(types_.size()), 0});
    return id;
}
auto TypeSets::types(TypeSetId const id) const -> std::span<ScalarType const> {
    auto const range{types_ranges_[id.value]};
    return std::span{types_}.subspan(range.offset, range.count);
}
void TypeSets::append_types(TypeSetId const id, ScalarType const value) {
    auto& range{types_ranges_[id.value]};
    assert(is_tail(range, types_));
    types_.push_back(value);
    ++range.count;
}
void TypeSets::clear() {
    name_.clear();
    span_.clear();
    types_ranges_.clear();
    types_.clear();
}

auto Operations::add(OperationKind const kind,
                     lispb::StringId const name,
                     lispb::codegen::sexpr::SourceSpan const span) -> OperationId {
    assert(name_.size() <= OperationId::index_mask);
    auto const id{OperationId{(static_cast<std::uint32_t>(kind) << OperationId::kind_shift) |
                              static_cast<std::uint32_t>(name_.size())}};
    name_.push_back(name);
    span_.push_back(span);
    type_set_.push_back({});
    output_.push_back({});
    expression_.push_back({});
    aliasing_.push_back(Aliasing::output_disjoint);
    operands_ranges_.push_back({static_cast<std::uint32_t>(operands_.size()), 0});
    components_ranges_.push_back({static_cast<std::uint32_t>(components_.size()), 0});
    variants_ranges_.push_back({static_cast<std::uint32_t>(variants_.size()), 0});
    return id;
}
auto Operations::operands(OperationId const id) const -> std::span<OperandId const> {
    auto const range{operands_ranges_[id.index()]};
    return std::span{operands_}.subspan(range.offset, range.count);
}
void Operations::append_operands(OperationId const id, OperandId const value) {
    auto& range{operands_ranges_[id.index()]};
    assert(is_tail(range, operands_));
    operands_.push_back(value);
    ++range.count;
}
auto Operations::components(OperationId const id) const -> std::span<lispb::StringId const> {
    auto const range{components_ranges_[id.index()]};
    return std::span{components_}.subspan(range.offset, range.count);
}
void Operations::append_components(OperationId const id, lispb::StringId const value) {
    auto& range{components_ranges_[id.index()]};
    assert(is_tail(range, components_));
    components_.push_back(value);
    ++range.count;
}
auto Operations::variants(OperationId const id) const -> std::span<VariantId const> {
    auto const range{variants_ranges_[id.index()]};
    return std::span{variants_}.subspan(range.offset, range.count);
}
void Operations::append_variants(OperationId const id, VariantId const value) {
    auto& range{variants_ranges_[id.index()]};
    assert(is_tail(range, variants_));
    variants_.push_back(value);
    ++range.count;
}
void Operations::clear() {
    name_.clear();
    span_.clear();
    type_set_.clear();
    output_.clear();
    expression_.clear();
    aliasing_.clear();
    operands_ranges_.clear();
    operands_.clear();
    components_ranges_.clear();
    components_.clear();
    variants_ranges_.clear();
    variants_.clear();
}

auto Emissions::add(lispb::KernelProfile const profile,
                    lispb::codegen::sexpr::SourceSpan const span) -> EmissionId {
    auto const id{EmissionId{static_cast<std::uint32_t>(profile_.size())}};
    profile_.push_back(profile);
    span_.push_back(span);
    header_.push_back({});
    source_.push_back({});
    tests_.push_back({});
    header_include_.push_back({});
    cpp_namespace_.push_back({});
    export_specifier_.push_back({});
    return id;
}
void Emissions::clear() {
    profile_.clear();
    span_.clear();
    header_.clear();
    source_.clear();
    tests_.clear();
    header_include_.clear();
    cpp_namespace_.clear();
    export_specifier_.clear();
}

auto Modules::add(lispb::StringId const name, lispb::codegen::sexpr::SourceSpan const span)
    -> ModuleId {
    auto const id{ModuleId{static_cast<std::uint32_t>(name_.size())}};
    expanded_variants_.push_back({});
    name_.push_back(name);
    span_.push_back(span);
    type_sets_ranges_.push_back({static_cast<std::uint32_t>(type_sets_.size()), 0});
    operations_ranges_.push_back({static_cast<std::uint32_t>(operations_.size()), 0});
    emissions_ranges_.push_back({static_cast<std::uint32_t>(emissions_.size()), 0});
    return id;
}
auto Modules::type_sets(ModuleId const id) const -> std::span<TypeSetId const> {
    auto const range{type_sets_ranges_[id.value]};
    return std::span{type_sets_}.subspan(range.offset, range.count);
}
void Modules::append_type_sets(ModuleId const id, TypeSetId const value) {
    auto& range{type_sets_ranges_[id.value]};
    assert(is_tail(range, type_sets_));
    type_sets_.push_back(value);
    ++range.count;
}
auto Modules::operations(ModuleId const id) const -> std::span<OperationId const> {
    auto const range{operations_ranges_[id.value]};
    return std::span{operations_}.subspan(range.offset, range.count);
}
void Modules::append_operations(ModuleId const id, OperationId const value) {
    auto& range{operations_ranges_[id.value]};
    assert(is_tail(range, operations_));
    operations_.push_back(value);
    ++range.count;
}
auto Modules::emissions(ModuleId const id) const -> std::span<EmissionId const> {
    auto const range{emissions_ranges_[id.value]};
    return std::span{emissions_}.subspan(range.offset, range.count);
}
void Modules::append_emissions(ModuleId const id, EmissionId const value) {
    auto& range{emissions_ranges_[id.value]};
    assert(is_tail(range, emissions_));
    emissions_.push_back(value);
    ++range.count;
}
void Modules::clear() {
    expanded_variants_.clear();
    name_.clear();
    span_.clear();
    type_sets_ranges_.clear();
    type_sets_.clear();
    operations_ranges_.clear();
    operations_.clear();
    emissions_ranges_.clear();
    emissions_.clear();
}

auto Document::operand_index(OperationId const operation, OperandId const operand) const
    -> std::size_t {
    auto const ids{operations.operands(operation)};
    auto const index{operand.value - ids.front().value};
    assert(index < ids.size() && ids[index] == operand);
    return index;
}
void Document::clear() {
    expanded_variants.clear();
    expressions.clear();
    operands.clear();
    variants.clear();
    type_sets.clear();
    operations.clear();
    emissions.clear();
    modules.clear();
}
}
