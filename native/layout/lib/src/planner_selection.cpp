#include <ioj/layout/planner_selection.hpp>

#include <ioj/layout/declaration_capabilities.hpp>

#include <codegen/schema/soa_backend.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
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
#include <cassert>
#include <optional>
#include <ranges>
#include <string>

namespace ioj::layout {
auto PlannerSelection::select_type(lispb::schema::TypeGraph const& types,
                                   std::optional<lispb::schema::TypeId> next) -> bool {
    assert(!next || (lispb::schema::is_valid(*next) && next->value < types.types().size()));
    auto const next_identity{next.has_value() ? std::optional{types.identity(*next)}
                                              : std::optional<lispb::schema::TypeIdentity>{}};
    auto const next_kind{next.has_value()
                             ? std::optional{declaration_capabilities(types, *next).kind}
                             : std::nullopt};
    auto const retained{next_identity ? retain_identity(types.strings(), next_identity)
                                      : std::nullopt};
    if (type == next && identity_ == next_identity && retained_identity_ == retained &&
        kind_ == next_kind) {
        return false;
    }
    type = next;
    identity_ = next_identity;
    retained_identity_ = retained;
    kind_ = next_kind;
    clear_type_local_state();
    return true;
}

auto PlannerSelection::identity() const -> std::optional<lispb::schema::TypeIdentity> const& {
    return identity_;
}

void PlannerSelection::reconcile(lispb::schema::TypeGraph const& types,
                                 std::optional<lispb::schema::TypeIdentity> selected_identity) {
    if (types.types().empty()) {
        select_type(types, std::nullopt);
        return;
    }

    auto const target_identity{selected_identity.has_value() ? selected_identity
                                                             : restore_identity(types.strings())};
    auto const next_type{target_identity.has_value() ? types.find(*target_identity)
                                                     : std::optional<lispb::schema::TypeId>{}};
    auto const next_kind{next_type.has_value()
                             ? std::optional{declaration_capabilities(types, *next_type).kind}
                             : std::nullopt};
    auto const retained{retain_identity(types.strings(), target_identity)};
    if (retained != retained_identity_ || next_kind != kind_ ||
        type.has_value() != next_type.has_value()) {
        clear_type_local_state();
    }

    type = next_type;
    identity_ = next_type.has_value() ? target_identity : std::nullopt;
    retained_identity_ = identity_.has_value() ? retained : std::nullopt;
    kind_ = next_kind;
    if (type.has_value()) {
        prune_members(types);
    }
}

auto PlannerSelection::retain_identity(lispb::Strings const& strings,
                                       std::optional<lispb::schema::TypeIdentity> identity)
    -> std::optional<lispb::schema::TypeIdentity> {
    if (identity) {
        for (auto* part : {&identity->module_name, &identity->namespace_name, &identity->name}) {
            *part = identity_strings_.intern(strings.view(*part));
        }
    }
    return identity;
}

auto PlannerSelection::restore_identity(lispb::Strings const& strings) const
    -> std::optional<lispb::schema::TypeIdentity> {
    auto identity{retained_identity_};
    if (identity) {
        for (auto* part : {&identity->module_name, &identity->namespace_name, &identity->name}) {
            auto const spelling{identity_strings_.view(*part)};
            *part = strings.find(spelling);
            if (!spelling.empty() && part->value == 0) {
                return std::nullopt;
            }
        }
    }
    return identity;
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
    auto const& definition{types.definition(*type)};
    auto valid_name = [&](std::string const& name) {
        if (auto const packed{((definition).kind() == lispb::schema::TypeDefinitionKind::packed
                                   ? std::optional{definition}
                                   : std::nullopt)}) {
            return std::ranges::any_of(
                types.definitions().packed.segments_of(*packed), [&](auto const& segment) {
                    auto const value{((segment).kind() == lispb::schema::PackedSegmentKind::field
                                          ? std::optional{segment}
                                          : std::nullopt)};
                    auto const identifier{
                        value ? types.definitions()
                                    .packed_fields
                                    .name[types.definitions().packed_fields.index(*value)]
                              : types.definitions()
                                    .packed_reserved_bits
                                    .name[types.definitions().packed_reserved_bits.index(segment)]};
                    return types.strings().view(identifier) == name;
                });
        }
        if (auto const record{((definition).kind() == lispb::schema::TypeDefinitionKind::record
                                   ? std::optional{definition}
                                   : std::nullopt)}) {
            return std::ranges::any_of(
                types.definitions().records.members_of(*record), [&](auto const& member) {
                    return types.strings().view(
                               types.definitions()
                                   .record_members
                                   .name[types.definitions().record_members.index(member)]) == name;
                });
        }
        if (auto const union_type{
                ((definition).kind() == lispb::schema::TypeDefinitionKind::union_type
                     ? std::optional{definition}
                     : std::nullopt)}) {
            return std::ranges::any_of(
                types.definitions().unions.alternatives_of(*union_type),
                [&](auto const& alternative) {
                    return types.strings().view(
                               types.definitions().union_alternatives.name
                                   [types.definitions().union_alternatives.index(alternative)]) ==
                           name;
                });
        }
        if (auto const tagged{
                ((definition).kind() == lispb::schema::TypeDefinitionKind::tagged_union
                     ? std::optional{definition}
                     : std::nullopt)}) {
            return std::ranges::any_of(
                types.definitions().tagged_unions.alternatives_of(*tagged),
                [&](auto const& alternative) {
                    return types.strings().view(
                               types.definitions()
                                   .tagged_union_alternatives
                                   .name[types.definitions().tagged_union_alternatives.index(
                                       alternative)]) == name;
                });
        }
        if (auto const soa{((definition).kind() == lispb::schema::TypeDefinitionKind::soa
                                ? std::optional{definition}
                                : std::nullopt)}) {
            return types.definitions().soas.backend[types.definitions().soas.index(*soa)] ==
                       lispb::codegen::SoaBackend::standard_library &&
                   std::ranges::any_of(
                       types.definitions().soas.columns_of(*soa), [&](auto const& column) {
                           return types.strings().view(
                                      types.definitions()
                                          .soa_columns
                                          .name[types.definitions().soa_columns.index(column)]) ==
                                  name;
                       });
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
    auto const packed{((definition).kind() == lispb::schema::TypeDefinitionKind::packed
                           ? std::optional{definition}
                           : std::nullopt)};
    prune(packed_access_fields, packed_access_set_explicit, [&](auto const& name) {
        return static_cast<bool>(packed) &&
               std::ranges::any_of(
                   types.definitions().packed.segments_of(*packed), [&](auto const& segment) {
                       auto const value{((segment).kind() == lispb::schema::PackedSegmentKind::field
                                             ? std::optional{segment}
                                             : std::nullopt)};
                       return value &&
                              types.strings().view(
                                  types.definitions()
                                      .packed_fields
                                      .name[types.definitions().packed_fields.index(*value)]) ==
                                  name;
                   });
    });
    auto const record{((definition).kind() == lispb::schema::TypeDefinitionKind::record
                           ? std::optional{definition}
                           : std::nullopt)};
    prune(record_access_members, record_access_set_explicit, [&](auto const& name) {
        return static_cast<bool>(record) &&
               std::ranges::any_of(
                   types.definitions().records.members_of(*record), [&](auto const& member) {
                       return types.strings().view(
                                  types.definitions()
                                      .record_members
                                      .name[types.definitions().record_members.index(member)]) ==
                              name;
                   });
    });
    auto const soa{((definition).kind() == lispb::schema::TypeDefinitionKind::soa
                        ? std::optional{definition}
                        : std::nullopt)};
    prune(soa_access_columns, soa_access_set_explicit, [&](auto const& name) {
        return static_cast<bool>(soa) &&
               types.definitions().soas.backend[types.definitions().soas.index(*soa)] ==
                   lispb::codegen::SoaBackend::standard_library &&
               std::ranges::any_of(
                   types.definitions().soas.columns_of(*soa), [&](auto const& column) {
                       return types.strings().view(
                                  types.definitions()
                                      .soa_columns
                                      .name[types.definitions().soa_columns.index(column)]) == name;
                   });
    });
}

} // namespace ioj::layout
