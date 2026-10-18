#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_type.hpp>

#include <lispb/schema/editable_document.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/strings.h>

#include <map>
#include <optional>
#include <string>

namespace ioj::layout {

struct PlannerSelection {
    std::optional<lispb::schema::TypeId> type;
    std::optional<lispb::schema::DeclarationId> declaration;
    std::string field;
    std::map<std::string, AccessOperation, std::less<>> packed_access_fields;
    std::map<std::string, AccessOperation, std::less<>> record_access_members;
    std::map<std::string, AccessOperation, std::less<>> soa_access_columns;
    bool packed_access_set_explicit{};
    bool record_access_set_explicit{};
    bool soa_access_set_explicit{};

    auto select_type(lispb::schema::TypeGraph const& types,
                     std::optional<lispb::schema::TypeId> next) -> bool;
    auto select_declaration(lispb::schema::EditableSchemaDocument const& document,
                            lispb::schema::DeclarationId next) -> bool;
    auto identity() const -> std::optional<lispb::schema::TypeIdentity> const&;
    // An explicit identity belongs to the incoming graph; nullopt retains the previous
    // selection by spelling, even after replacing its owning document.
    void reconcile(lispb::schema::TypeGraph const& types,
                   std::optional<lispb::schema::TypeIdentity> selected_identity,
                   lispb::schema::EditableSchemaDocument const* document = nullptr);
    void clear_type_local_state();
  private:
    std::optional<lispb::schema::TypeIdentity> identity_;
    lispb::Strings identity_strings_;
    std::optional<lispb::schema::TypeIdentity> retained_identity_;
    std::optional<DeclarationKind> kind_;
    auto retain_identity(lispb::Strings const& strings,
                         std::optional<lispb::schema::TypeIdentity> identity)
        -> std::optional<lispb::schema::TypeIdentity>;
    auto restore_identity(lispb::Strings const& strings) const
        -> std::optional<lispb::schema::TypeIdentity>;
    void prune_members(lispb::schema::TypeGraph const& types);
};

} // namespace ioj::layout
