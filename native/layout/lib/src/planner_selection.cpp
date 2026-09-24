#include <ioj/layout/planner_selection.hpp>

#include <algorithm>

namespace ioj::layout {
namespace {

auto declaration_kind(lispb::schema::EditableSchemaDocument const& document,
                      lispb::schema::DeclarationInfo const& info) -> DeclarationKind {
    auto const& module{
        std::get<codegen::NormalModuleSchema>(document.manifest().modules[info.module_index])};
    return declaration_capabilities(module.declarations[info.declaration_index]).kind;
}

} // namespace

auto PlannerSelection::select_type(lispb::schema::TypeGraph const& types,
                                   std::optional<lispb::schema::TypeId> next) -> bool {
    if (next.has_value() && (!next->valid() || next->value >= types.types().size())) {
        next.reset();
    }
    auto const next_identity{next.has_value() ? std::optional{types.type(*next).identity}
                                              : std::optional<lispb::schema::TypeIdentity>{}};
    auto const next_kind{next.has_value()
                             ? std::optional{declaration_capabilities(types.type(*next)).kind}
                             : std::nullopt};
    if (type == next && !declaration.has_value() && identity_ == next_identity &&
        kind_ == next_kind) {
        return false;
    }
    type = next;
    declaration.reset();
    identity_ = next_identity;
    kind_ = next_kind;
    clear_type_local_state();
    return true;
}

auto PlannerSelection::select_declaration(lispb::schema::EditableSchemaDocument const& document,
                                          lispb::schema::DeclarationId const next) -> bool {
    auto const* info{document.declaration(next)};
    if (info == nullptr) {
        return false;
    }
    if (auto const semantic{document.types().find(info->identity)}) {
        return select_type(document.types(), *semantic);
    }
    auto const next_kind{declaration_kind(document, *info)};
    if (declaration == next && !type.has_value() && identity_ == info->identity &&
        kind_ == next_kind) {
        return false;
    }

    type.reset();
    declaration = next;
    identity_ = info->identity;
    kind_ = next_kind;
    clear_type_local_state();
    return true;
}

auto PlannerSelection::identity() const -> std::optional<lispb::schema::TypeIdentity> const& {
    return identity_;
}

void
    PlannerSelection::reconcile(lispb::schema::TypeGraph const& types,
                                std::optional<lispb::schema::TypeIdentity> const& selected_identity,
                                lispb::schema::EditableSchemaDocument const* const document) {
    auto const target_identity{selected_identity.has_value() ? selected_identity
                               : declaration.has_value()     ? identity_
                                                             : std::nullopt};
    auto const next_type{target_identity.has_value() ? types.find(*target_identity)
                                                     : std::optional<lispb::schema::TypeId>{}};
    auto const next_declaration{target_identity.has_value() && !next_type.has_value() &&
                                        document != nullptr
                                    ? document->find_declaration(*target_identity)
                                    : std::optional<lispb::schema::DeclarationId>{}};
    auto const next_kind{
        next_type.has_value() ? std::optional{declaration_capabilities(types.type(*next_type)).kind}
        : next_declaration.has_value()
            ? std::optional{declaration_kind(*document, *document->declaration(*next_declaration))}
            : std::nullopt};
    if (target_identity != identity_ || next_kind != kind_ ||
        type.has_value() != next_type.has_value() ||
        declaration.has_value() != next_declaration.has_value()) {
        clear_type_local_state();
    }

    type = next_type;
    declaration = next_declaration;
    identity_ =
        next_type.has_value() || next_declaration.has_value() ? target_identity : std::nullopt;
    kind_ = next_kind;
    if (type.has_value()) {
        prune_members(types);
    }
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
