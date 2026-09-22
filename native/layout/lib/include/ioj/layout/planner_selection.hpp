#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_type.hpp>

#include <lispb/schema/type_graph.h>

#include <map>
#include <optional>
#include <string>

namespace ioj::layout {

struct PlannerSelection {
    std::optional<lispb::schema::TypeId> type;
    std::string field;
    std::map<std::string, AccessOperation, std::less<>> packed_access_fields;
    std::map<std::string, AccessOperation, std::less<>> record_access_members;
    std::map<std::string, AccessOperation, std::less<>> soa_access_columns;
    bool packed_access_set_explicit{};
    bool record_access_set_explicit{};
    bool soa_access_set_explicit{};

    auto select_type(lispb::schema::TypeGraph const& types,
                     std::optional<lispb::schema::TypeId> next) -> bool;
    void reconcile(lispb::schema::TypeGraph const& types,
                   std::optional<lispb::schema::TypeIdentity> selected_identity);
    void clear_type_local_state();
  private:
    std::optional<lispb::schema::TypeIdentity> identity_;
    std::optional<DeclarationKind> kind_;
    void prune_members(lispb::schema::TypeGraph const& types);
};

} // namespace ioj::layout
