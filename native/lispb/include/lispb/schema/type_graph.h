#pragma once

#include <codegen/schema/manifest.h>
#include <codegen/schema/type_ref.h>
#include <lispb/schema/type_id.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_node.h>
#include <lispb/schema/type_use.h>

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace lispb::schema {
class TypeGraphBuilder;

class TypeGraph {
  public:
    auto types() const -> std::span<TypeNode const>;
    auto type(TypeId id) const -> TypeNode const&;
    auto find(TypeIdentity const& identity) const -> std::optional<TypeId>;
    auto find_declared(std::string const& module_name, std::string const& name) const
        -> std::optional<TypeId>;
    auto find_registered(std::string const& name) const -> std::optional<TypeId>;
    // Uses the resolver's registered/local/qualified lookup without creating a raw external type.
    auto find_reference(codegen::TypeRef const& reference, std::string const& module_name) const
        -> std::optional<TypeId>;
    auto dependencies_of(TypeId id) const -> std::span<TypeId const>;
    auto users_of(TypeId id) const -> std::span<TypeId const>;
    auto type_uses() const -> std::span<TypeUse const>;
    auto types_for_declaration(TypeIdentity const& identity) const -> std::vector<TypeId>;
  private:
    friend class TypeGraphBuilder;
    friend auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

    std::vector<TypeNode> types_;
    std::vector<TypeUse> type_uses_;
    std::map<TypeIdentity, TypeId> identities_;
    std::map<std::string, TypeId, std::less<>> registered_types_;
    std::map<std::pair<std::string, std::string>, TypeId> declarations_by_module_name_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_spelling_;
};

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;

} // namespace lispb::schema
