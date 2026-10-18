#include <ioj/layout/workspace.hpp>

#include <ioj/layout/document.hpp>
#include <ioj/layout/variants.hpp>

#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cassert>

namespace ioj::layout {

LayoutWorkspace::LayoutWorkspace()
    : LayoutWorkspace{[]() -> lispb::schema::TypeGraph const& {
        static lispb::schema::TypeGraph const empty;
        return empty;
    }()} {}
LayoutWorkspace::LayoutWorkspace(lispb::schema::TypeGraph const& types,
                                 std::uint64_t const default_capacity)
    : types_{&types}
    , default_capacity_{default_capacity} {
    variants_.push_back({.id = baseline_variant_id, .name = "Baseline", .overrides = {}});
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
    auto const* selected{variant(active_variant_id_)};
    assert(selected);
    return *selected;
}
auto LayoutWorkspace::active_variant_id() const -> std::uint64_t {
    return active_variant_id_;
}
void LayoutWorkspace::bind(Document const& document) {
    types_ = &document.types();
    variants_.clear();
    auto const& authored{document.variants()};
    for (auto const id : authored.ids()) {
        auto& resolved{
            variants_.emplace_back(Variant{.id = id, .name = authored.name(id), .overrides = {}})};
        auto const& settings{authored.settings(id)};
        auto const storage_count{settings.packed_types.size()};
        for (std::size_t row{}; row < storage_count; ++row) {
            if (auto const type{document.declaration_type(settings.packed_types[row])}) {
                resolved.overrides.packed_storage_types.emplace(*type,
                                                                settings.packed_storage[row]);
            }
        }
        auto const field_count{settings.field_types.size()};
        for (std::size_t row{}; row < field_count; ++row) {
            if (auto const type{document.declaration_type(settings.field_types[row])}) {
                resolved.overrides.packed_field_widths.emplace(
                    FieldOverrideId{*type, settings.field_names[row]}, settings.field_widths[row]);
            }
        }
        auto const column_count{settings.column_types.size()};
        for (std::size_t row{}; row < column_count; ++row) {
            if (auto const type{document.declaration_type(settings.column_types[row])}) {
                resolved.overrides.soa_column_types.emplace(
                    FieldOverrideId{*type, settings.column_names[row]},
                    settings.column_spelling[row]);
            }
        }
        auto const capacity_count{settings.capacity_types.size()};
        for (std::size_t row{}; row < capacity_count; ++row) {
            if (auto const type{document.declaration_type(settings.capacity_types[row])}) {
                resolved.overrides.capacities.emplace(*type, settings.capacities[row]);
            }
        }
    }
    if (!authored.contains(active_variant_id_)) {
        active_variant_id_ = baseline_variant_id;
    }
}
auto LayoutWorkspace::select_variant(std::uint64_t const id) -> bool {
    assert(variant(id));
    if (active_variant_id_ == id) {
        return false;
    }
    active_variant_id_ = id;
    return true;
}
auto LayoutWorkspace::set_element_count(std::uint64_t const count) -> bool {
    assert(count > 0);
    if (element_count_ == count) {
        return false;
    }
    element_count_ = count;
    return true;
}

}
