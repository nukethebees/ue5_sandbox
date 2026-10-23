#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_role.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/relationships.h>
#include <lispb/schema/resolved_occurrences.h>
#include <lispb/schema/type_definition_id.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_edge_range.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>

#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace lispb {
class Strings;
namespace codegen {
class Declarations;
class CppTypes;
struct Manifest;
}
}

namespace lispb::schema {
class TypeGraphBuilder;

class TypeGraph {
  public:
    TypeGraph() = default;
    TypeGraph(codegen::Declarations& declarations, codegen::CppTypes& cpp_types);

    auto definitions() const -> TypeDefinitions const& { return definitions_; }
    auto enum_domains() const -> EnumDomains const& { return enum_domains_; }
    auto occurrences() const -> ResolvedOccurrences const& { return occurrences_; }
    auto relationships() const -> Relationships const& { return relationships_; }
    auto declarations() const -> codegen::Declarations& { return *declarations_; }
    auto strings() const -> Strings&;
    auto cpp_types() const -> codegen::CppTypes& { return *cpp_types_; }
    auto identity(TypeId id) const -> TypeIdentity;
    auto origin(TypeId id) const -> TypeOrigin;
    auto module_name(TypeId id) const -> StringId;
    auto namespace_name(TypeId id) const -> StringId;
    auto name(TypeId id) const -> StringId;
    auto cpp_spelling(TypeId id) const -> StringId;
    auto definition(TypeId id) const -> TypeDefinitionId;
    auto module(TypeId id) const -> codegen::ModuleHandle;
    auto source_declaration(TypeId id) const -> codegen::DeclarationHandle;
    auto types() const {
        return std::views::iota(std::uint32_t{},
                                static_cast<std::uint32_t>(definitions_ids_.size())) |
               std::views::transform([](std::uint32_t index) { return TypeId{index}; });
    }
    auto find(TypeIdentity const& identity) const -> std::optional<TypeId>;
    auto find_declared(codegen::ModuleHandle module, StringId name) const -> std::optional<TypeId>;
    auto find_registered(StringId name) const -> std::optional<TypeId>;
    auto find_named(StringId identifier, codegen::ModuleHandle module) const
        -> std::optional<TypeId>;
    auto find_reference(codegen::TypeRefId reference, codegen::ModuleHandle module) const
        -> std::optional<TypeId>;
    // Every authored occurrence is bound before semantic definitions and lowering.
    auto resolved(codegen::ModuleHandle module, codegen::TypeRefId reference) const
        -> ResolvedOccurrenceId;
    auto dependencies_of(TypeId id) const -> std::span<TypeId const>;
    auto users_of(TypeId id) const -> std::span<TypeId const>;
    auto module(TypeUseId id) const -> codegen::ModuleHandle;
    auto declaration(TypeUseId id) const -> codegen::DeclarationHandle;
    auto owner(TypeUseId id) const -> std::optional<TypeId>;
    auto reference(TypeUseId id) const -> codegen::TypeRefId;
    auto role(TypeUseId id) const -> codegen::TypeReferenceRole;
    auto target(TypeUseId id) const -> ResolvedOccurrenceId;
    auto kind(TypeUseId id) const -> codegen::TypeReferenceKind;
    auto type_uses() const {
        return std::views::iota(std::uint32_t{}, static_cast<std::uint32_t>(use_modules_.size())) |
               std::views::transform([](std::uint32_t index) { return TypeUseId{index}; });
    }
    auto types_for_declaration(codegen::DeclarationHandle declaration) const -> std::vector<TypeId>;
  private:
    friend class TypeGraphBuilder;

    auto index(TypeId id) const -> std::uint32_t;
    auto index(TypeUseId id) const -> std::uint32_t;

    TypeDefinitions definitions_;
    EnumDomains enum_domains_;
    ResolvedOccurrences occurrences_;
    Relationships relationships_;
    codegen::Declarations* declarations_{};
    codegen::CppTypes* cpp_types_{};
    std::vector<TypeOrigin> origins_;
    std::vector<StringId> module_names_;
    std::vector<StringId> namespace_names_;
    std::vector<StringId> names_;
    std::vector<StringId> cpp_spellings_;
    std::vector<TypeDefinitionId> definitions_ids_;
    std::vector<codegen::ModuleHandle> modules_;
    std::vector<codegen::DeclarationHandle> source_declarations_;

    std::vector<TypeEdgeRange> dependency_ranges_;
    std::vector<TypeEdgeRange> user_ranges_;
    std::vector<TypeId> dependencies_;
    std::vector<TypeId> users_;
    std::vector<codegen::ModuleHandle> use_modules_;
    std::vector<codegen::DeclarationHandle> use_declarations_;
    std::vector<std::optional<TypeId>> use_owners_;
    std::vector<codegen::TypeRefId> use_references_;
    std::vector<codegen::TypeReferenceRole> use_roles_;
    std::vector<ResolvedOccurrenceId> use_targets_;
    std::vector<codegen::TypeReferenceKind> use_kinds_;
    std::map<std::pair<codegen::ModuleHandle, codegen::TypeRefId>, ResolvedOccurrenceId> bindings_;
    std::map<TypeIdentity, TypeId> identities_;
    std::map<StringId, TypeId> registered_types_;
    std::map<std::pair<codegen::ModuleHandle, StringId>, TypeId> declarations_by_module_name_;
    std::multimap<StringId, TypeId> declarations_by_spelling_;
};

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph;
auto forward_declaration_kind(TypeId node, TypeGraph const& graph)
    -> std::optional<codegen::RecordKind>;
auto
    uses_forward_declaration(ResolvedOccurrenceId reference,
                             TypeGraph const& graph,
                             codegen::TypeReferenceKind kind = codegen::TypeReferenceKind::ordinary)
        -> bool;
auto uses_cross_module_signature_forward_declaration(ResolvedOccurrenceId reference,
                                                     codegen::TypeReferenceKind kind,
                                                     TypeGraph const& graph) -> bool;
}
