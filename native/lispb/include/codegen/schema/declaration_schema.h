#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_role.h>
#include <lispb/indices.h>

#include <functional>
#include <string_view>
#include <vector>

namespace lispb::codegen {

class Declarations;

auto declaration_head(DeclarationHandle declaration) -> std::string_view;
auto declaration_name(Declarations const& declarations, DeclarationHandle declaration) -> StringId;
auto has_primary_semantic_type(DeclarationHandle declaration) -> bool;
void visit_type_references(
    Declarations const& declarations,
    DeclarationHandle declaration,
    std::function<void(TypeReferenceRole const&, TypeRefId, TypeReferenceKind)> const& visit);
auto rewrite_type_references(
    Declarations& declarations,
    DeclarationHandle declaration,
    std::function<TypeRefId(TypeReferenceRole const&, TypeRefId, TypeReferenceKind)> const& rewrite)
    -> DeclarationHandle;
auto generated_cpp_names(Declarations const& declarations, DeclarationHandle declaration)
    -> std::vector<StringId>;

} // namespace lispb::codegen
