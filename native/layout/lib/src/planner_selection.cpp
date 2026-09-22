#include <ioj/layout/planner_selection.hpp>

#include <algorithm>

namespace ioj::layout {

auto PlannerSelection::select_type(lispb::schema::TypeGraph const& types,
                                   std::optional<lispb::schema::TypeId> next) -> bool {
    if (next.has_value() && (!next->valid() || next->value >= types.types().size())) {
        next.reset();
    }
    if (type == next) {
        return false;
    }
    type = next;
    identity_ = next.has_value()
                  ? std::optional<lispb::schema::TypeIdentity>{types.type(*next).identity}
                  : std::nullopt;
    kind_ = next.has_value() ? std::optional{declaration_capabilities(types.type(*next)).kind}
                             : std::nullopt;
    clear_type_local_state();
    return true;
}

void PlannerSelection::reconcile(lispb::schema::TypeGraph const& types,
                                 std::optional<lispb::schema::TypeIdentity> selected_identity) {
    auto const next{selected_identity.has_value() ? types.find(*selected_identity)
                                                  : std::optional<lispb::schema::TypeId>{}};
    auto const next_kind{next.has_value()
                             ? std::optional{declaration_capabilities(types.type(*next)).kind}
                             : std::nullopt};
    if (selected_identity != identity_ || next_kind != kind_) {
        clear_type_local_state();
    }
    type = next;
    identity_ = next.has_value() ? std::move(selected_identity) : std::nullopt;
    kind_ = next_kind;
    prune_members(types);
}

void PlannerSelection::clear_type_local_state() {
    field.clear();
    packed_access_fields.clear();
    record_access_members.clear();
    soa_access_columns.clear();
    packed_access_set_explicit = false;
    record_access_set_explicit = false;
    soa_access_set_explicit = false;
}

void PlannerSelection::prune_members(lispb::schema::TypeGraph const& types) {
    if (!type.has_value()) {
        clear_type_local_state();
        return;
    }
    auto const& definition{types.type(*type).definition};
    auto valid_name = [&](std::string const& name) {
        if (auto const* packed{std::get_if<lispb::schema::PackedType>(&definition)}) {
            return std::ranges::any_of(packed->segments, [&](auto const& segment) {
                return std::visit([&](auto const& value) { return value.name == name; }, segment);
            });
        }
        if (auto const* record{std::get_if<lispb::schema::RecordType>(&definition)}) {
            return std::ranges::any_of(record->members,
                                       [&](auto const& member) { return member.name == name; });
        }
        if (auto const* union_type{std::get_if<lispb::schema::UnionType>(&definition)}) {
            return std::ranges::any_of(union_type->alternatives, [&](auto const& alternative) {
                return alternative.name == name;
            });
        }
        if (auto const* tagged{std::get_if<lispb::schema::TaggedUnionType>(&definition)}) {
            return std::ranges::any_of(tagged->alternatives, [&](auto const& alternative) {
                return alternative.name == name;
            });
        }
        if (auto const* soa{std::get_if<lispb::schema::SoaType>(&definition)}) {
            return soa->backend == codegen::SoaBackend::standard_library &&
                   std::ranges::any_of(soa->columns,
                                       [&](auto const& column) { return column.name == name; });
        }
        return false;
    };
    if (!field.empty() && !valid_name(field)) {
        field.clear();
    }

    auto prune = [&](auto& accesses, bool& explicit_selection, auto const& valid_access) {
        auto const had_entries{!accesses.empty()};
        std::erase_if(accesses, [&](auto const& entry) { return !valid_access(entry.first); });
        if (had_entries && accesses.empty()) {
            explicit_selection = false;
        }
    };
    auto const* packed{std::get_if<lispb::schema::PackedType>(&definition)};
    prune(packed_access_fields, packed_access_set_explicit, [&](auto const& name) {
        return packed != nullptr && std::ranges::any_of(packed->segments, [&](auto const& segment) {
                   auto const* value{std::get_if<lispb::schema::PackedField>(&segment)};
                   return value != nullptr && value->name == name;
               });
    });
    auto const* record{std::get_if<lispb::schema::RecordType>(&definition)};
    prune(record_access_members, record_access_set_explicit, [&](auto const& name) {
        return record != nullptr && std::ranges::any_of(record->members, [&](auto const& member) {
                   return member.name == name;
               });
    });
    auto const* soa{std::get_if<lispb::schema::SoaType>(&definition)};
    prune(soa_access_columns, soa_access_set_explicit, [&](auto const& name) {
        return soa != nullptr && soa->backend == codegen::SoaBackend::standard_library &&
               std::ranges::any_of(soa->columns,
                                   [&](auto const& column) { return column.name == name; });
    });
}

} // namespace ioj::layout
