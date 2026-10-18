#include <ioj/layout/workspace.hpp>

#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/union_alternatives.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

auto valid_type(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type) -> bool {
    return lispb::schema::is_valid(type) && type.value < types.types().size();
}

auto packed_type(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<lispb::schema::TypeDefinitionId> {
    return valid_type(types, type)
             ? ((types.definition(type)).kind() == lispb::schema::TypeDefinitionKind::packed
                    ? std::optional{types.definition(type)}
                    : std::nullopt)
             : std::nullopt;
}

auto overrideable_packed_field(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId const type,
                               std::string const& name) -> bool {
    auto const packed{packed_type(types, type)};
    if (!packed.has_value()) {
        return false;
    }
    return std::ranges::any_of(
        types.definitions().packed.segments_of(*packed), [&](auto const& segment) {
            auto const field{((segment).kind() == lispb::schema::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)};
            return field &&
                   types.strings().view(
                       types.definitions()
                           .packed_fields.name[types.definitions().packed_fields.index(*field)]) ==
                       name &&
                   types.definitions()
                           .packed_fields.kind[types.definitions().packed_fields.index(*field)] !=
                       lispb::codegen::PackedFieldKind::linear_quantized &&
                   types.definitions()
                           .packed_fields.kind[types.definitions().packed_fields.index(*field)] !=
                       lispb::codegen::PackedFieldKind::fixed_point &&
                   types.definitions()
                           .packed_fields.kind[types.definitions().packed_fields.index(*field)] !=
                       lispb::codegen::PackedFieldKind::mini_float;
        });
}

auto supported_soa(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<lispb::schema::TypeDefinitionId> {
    if (!valid_type(types, type)) {
        return std::nullopt;
    }
    auto const soa{((types.definition(type)).kind() == lispb::schema::TypeDefinitionKind::soa
                        ? std::optional{types.definition(type)}
                        : std::nullopt)};
    return static_cast<bool>(soa) &&
                   types.definitions().soas.backend[types.definitions().soas.index(*soa)] ==
                       lispb::codegen::SoaBackend::standard_library
             ? soa
             : std::nullopt;
}

auto supported_soa_column(lispb::schema::TypeGraph const& types,
                          lispb::schema::TypeId const type,
                          std::string const& name) -> bool {
    auto const soa{supported_soa(types, type)};
    return static_cast<bool>(soa) &&
           std::ranges::any_of(types.definitions().soas.columns_of(*soa), [&](auto const& column) {
               return types.strings().view(
                          types.definitions()
                              .soa_columns.name[types.definitions().soa_columns.index(column)]) ==
                      name;
           });
}

} // namespace

LayoutWorkspace::LayoutWorkspace()
    : LayoutWorkspace{[]() -> lispb::schema::TypeGraph const& {
        static lispb::schema::TypeGraph const empty;
        return empty;
    }()} {}

LayoutWorkspace::LayoutWorkspace(lispb::schema::TypeGraph const& types,
                                 std::uint64_t const default_capacity)
    : types_{&types}
    , default_capacity_{default_capacity} {
    retain_type_identities();
    variants_.push_back(
        Variant{.id = baseline_variant_id, .name = "Baseline", .overrides = {}, .revision = 0});
}

auto LayoutWorkspace::types() const -> lispb::schema::TypeGraph const& {
    return *types_;
}

auto LayoutWorkspace::default_capacity() const -> std::uint64_t {
    return default_capacity_;
}

auto LayoutWorkspace::element_count() const -> std::uint64_t {
    return element_count_;
}

auto LayoutWorkspace::variants() const -> std::vector<Variant> const& {
    return variants_;
}

auto LayoutWorkspace::variant(std::uint64_t const id) const -> Variant const* {
    auto const found{std::ranges::find(variants_, id, &Variant::id)};
    return found == variants_.end() ? nullptr : &*found;
}

auto LayoutWorkspace::active_variant() const -> Variant const& {
    return *variant(active_variant_id_);
}

auto LayoutWorkspace::active_variant_id() const -> std::uint64_t {
    return active_variant_id_;
}

auto LayoutWorkspace::revision() const -> std::uint64_t {
    return revision_;
}

auto LayoutWorkspace::graph_revision() const -> std::uint64_t {
    return graph_revision_;
}

void LayoutWorkspace::replace_types(lispb::schema::TypeGraph const& types) {
    auto remap_type{
        [&](lispb::schema::TypeId const old_type) -> std::optional<lispb::schema::TypeId> {
            if (!lispb::schema::is_valid(old_type) || old_type.value >= previous_types_.size()) {
                return std::nullopt;
            }
            auto identity{previous_types_[old_type.value].identity};
            for (auto* part : {&identity.module_name, &identity.namespace_name, &identity.name}) {
                auto const spelling{identity_strings_.view(*part)};
                *part = types.strings().find(spelling);
                if (!spelling.empty() && part->value == 0) {
                    return std::nullopt;
                }
            }
            return types.find(identity);
        }};
    for (auto& variant : variants_) {
        VariantOverrides remapped;
        for (auto const& [type, spelling] : variant.overrides.packed_storage_types) {
            if (auto const replacement{remap_type(type)};
                replacement.has_value() && packed_type(types, *replacement).has_value()) {
                remapped.packed_storage_types.emplace(*replacement, spelling);
            }
        }
        for (auto const& [field, width] : variant.overrides.packed_field_widths) {
            if (auto const replacement{remap_type(field.type)};
                replacement.has_value() &&
                overrideable_packed_field(types, *replacement, field.field_name)) {
                remapped.packed_field_widths.emplace(
                    FieldOverrideId{.type = *replacement, .field_name = field.field_name}, width);
            }
        }
        for (auto const& [field, spelling] : variant.overrides.soa_column_types) {
            if (auto const replacement{remap_type(field.type)};
                replacement.has_value() &&
                supported_soa_column(types, *replacement, field.field_name)) {
                remapped.soa_column_types.emplace(
                    FieldOverrideId{.type = *replacement, .field_name = field.field_name},
                    spelling);
            }
        }
        for (auto const& [type, capacity] : variant.overrides.capacities) {
            if (auto const replacement{remap_type(type)};
                replacement.has_value() && supported_soa(types, *replacement).has_value()) {
                remapped.capacities.emplace(*replacement, capacity);
            }
        }
        variant.overrides = std::move(remapped);
    }
    types_ = &types;
    retain_type_identities();
    ++revision_;
    ++graph_revision_;
}

void LayoutWorkspace::retain_type_identities() {
    previous_types_.clear();
    previous_types_.reserve(types().types().size());
    for (auto const& node : types().types()) {
        TypeSnapshot snapshot{.identity = types().identity(node), .alternative_keys = {}};
        for (auto* part : {&snapshot.identity.module_name,
                           &snapshot.identity.namespace_name,
                           &snapshot.identity.name}) {
            *part = identity_strings_.intern(types().strings().view(*part));
        }
        if (auto const value{
                ((types().definition(node)).kind() == lispb::schema::TypeDefinitionKind::union_type
                     ? std::optional{types().definition(node)}
                     : std::nullopt)}) {
            for (auto const& alternative : types().definitions().unions.alternatives_of(*value)) {
                snapshot.alternative_keys.emplace_back(types().strings().view(
                    types()
                        .definitions()
                        .union_alternatives
                        .name[types().definitions().union_alternatives.index(alternative)]));
            }
        } else if (auto const value{((types().definition(node)).kind() ==
                                             lispb::schema::TypeDefinitionKind::tagged_union
                                         ? std::optional{types().definition(node)}
                                         : std::nullopt)}) {
            for (auto const& alternative :
                 types().definitions().tagged_unions.alternatives_of(*value)) {
                snapshot.alternative_keys.emplace_back(types().strings().view(
                    types()
                        .definitions()
                        .tagged_union_alternatives
                        .tag[types().definitions().tagged_union_alternatives.index(alternative)]));
            }
        }
        previous_types_.push_back(std::move(snapshot));
    }
}

auto LayoutWorkspace::previous_alternative_keys(lispb::Strings const& strings,
                                                lispb::schema::TypeIdentity const& identity) const
    -> std::span<std::string const> {
    auto retained{identity};
    for (auto* part : {&retained.module_name, &retained.namespace_name, &retained.name}) {
        auto const spelling{strings.view(*part)};
        *part = identity_strings_.find(spelling);
        if (!spelling.empty() && part->value == 0) {
            return {};
        }
    }
    auto const found{std::ranges::find(previous_types_, retained, &TypeSnapshot::identity)};
    return found == previous_types_.end() ? std::span<std::string const>{}
                                          : found->alternative_keys;
}

auto LayoutWorkspace::select_variant(std::uint64_t const id) -> bool {
    if (variant(id) == nullptr || active_variant_id_ == id) {
        return false;
    }
    active_variant_id_ = id;
    ++revision_;
    return true;
}

auto LayoutWorkspace::create_variant(std::string name) -> std::uint64_t {
    auto const id{next_variant_id_++};
    variants_.push_back(Variant{.id = id, .name = std::move(name), .overrides = {}, .revision = 0});
    active_variant_id_ = id;
    ++revision_;
    return id;
}

auto LayoutWorkspace::duplicate_variant(std::uint64_t const source_id, std::string name)
    -> std::optional<std::uint64_t> {
    auto const* source{variant(source_id)};
    if (source == nullptr) {
        return std::nullopt;
    }
    auto const id{next_variant_id_++};
    variants_.push_back(
        Variant{.id = id, .name = std::move(name), .overrides = source->overrides, .revision = 0});
    active_variant_id_ = id;
    ++revision_;
    return id;
}

auto LayoutWorkspace::rename_variant(std::uint64_t const id, std::string name) -> bool {
    if (id == baseline_variant_id || name.empty()) {
        return false;
    }
    auto found{std::ranges::find(variants_, id, &Variant::id)};
    if (found == variants_.end() || found->name == name) {
        return false;
    }
    found->name = std::move(name);
    note_change(*found);
    return true;
}

auto LayoutWorkspace::reset_variant(std::uint64_t const id) -> bool {
    if (id == baseline_variant_id) {
        return false;
    }
    auto found{std::ranges::find(variants_, id, &Variant::id)};
    if (found == variants_.end() || found->overrides == VariantOverrides{}) {
        return false;
    }
    found->overrides = {};
    note_change(*found);
    return true;
}

auto LayoutWorkspace::delete_variant(std::uint64_t const id) -> bool {
    if (id == baseline_variant_id) {
        return false;
    }
    auto const found{std::ranges::find(variants_, id, &Variant::id)};
    if (found == variants_.end()) {
        return false;
    }
    variants_.erase(found);
    if (active_variant_id_ == id) {
        active_variant_id_ = baseline_variant_id;
    }
    ++revision_;
    return true;
}

auto LayoutWorkspace::set_packed_storage_type(lispb::schema::TypeId const type,
                                              std::optional<std::string> spelling) -> bool {
    auto* selected{editable_active_variant()};
    if (selected == nullptr) {
        return false;
    }
    auto& values{selected->overrides.packed_storage_types};
    if (spelling.has_value()) {
        if (!packed_type(types(), type).has_value()) {
            return false;
        }
        auto const found{values.find(type)};
        if (found != values.end() && found->second == *spelling) {
            return false;
        }
        values.insert_or_assign(type, std::move(*spelling));
    } else if (values.erase(type) == 0) {
        return false;
    }
    note_change(*selected);
    return true;
}

auto LayoutWorkspace::set_packed_field_width(lispb::schema::TypeId const type,
                                             std::string field_name,
                                             std::optional<std::uint32_t> width) -> bool {
    auto* selected{editable_active_variant()};
    if (selected == nullptr) {
        return false;
    }
    auto& values{selected->overrides.packed_field_widths};
    FieldOverrideId const key{.type = type, .field_name = std::move(field_name)};
    if (width.has_value()) {
        if (!overrideable_packed_field(types(), type, key.field_name)) {
            return false;
        }
        auto const found{values.find(key)};
        if (found != values.end() && found->second == *width) {
            return false;
        }
        values.insert_or_assign(key, *width);
    } else if (values.erase(key) == 0) {
        return false;
    }
    note_change(*selected);
    return true;
}

auto LayoutWorkspace::set_soa_column_type(lispb::schema::TypeId const type,
                                          std::string column_name,
                                          std::optional<std::string> spelling) -> bool {
    auto* selected{editable_active_variant()};
    if (selected == nullptr) {
        return false;
    }
    auto& values{selected->overrides.soa_column_types};
    FieldOverrideId const key{.type = type, .field_name = std::move(column_name)};
    if (spelling.has_value()) {
        if (!supported_soa_column(types(), type, key.field_name)) {
            return false;
        }
        auto const found{values.find(key)};
        if (found != values.end() && found->second == *spelling) {
            return false;
        }
        values.insert_or_assign(key, std::move(*spelling));
    } else if (values.erase(key) == 0) {
        return false;
    }
    note_change(*selected);
    return true;
}

auto LayoutWorkspace::set_capacity(lispb::schema::TypeId const type,
                                   std::optional<std::uint64_t> const capacity) -> bool {
    auto* selected{editable_active_variant()};
    if (selected == nullptr) {
        return false;
    }
    auto& values{selected->overrides.capacities};
    if (capacity.has_value()) {
        if (!supported_soa(types(), type).has_value()) {
            return false;
        }
        auto const found{values.find(type)};
        if (found != values.end() && found->second == *capacity) {
            return false;
        }
        values.insert_or_assign(type, *capacity);
    } else if (values.erase(type) == 0) {
        return false;
    }
    note_change(*selected);
    return true;
}

auto LayoutWorkspace::set_element_count(std::uint64_t const count) -> bool {
    if (count == 0 || element_count_ == count) {
        return false;
    }
    element_count_ = count;
    ++revision_;
    return true;
}

auto LayoutWorkspace::editable_active_variant() -> Variant* {
    if (active_variant_id_ == baseline_variant_id) {
        return nullptr;
    }
    auto found{std::ranges::find(variants_, active_variant_id_, &Variant::id)};
    return found == variants_.end() ? nullptr : &*found;
}

void LayoutWorkspace::note_change(Variant& variant) {
    ++variant.revision;
    ++revision_;
}

} // namespace ioj::layout
