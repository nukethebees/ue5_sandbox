#pragma once

#include <codegen/ast/access.h>
#include <codegen/schema/declarations.h>
#include <codegen/sexpr/form_builder.h>
#include <lispb/strings.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace lispb::schema::detail {

using codegen::sexpr::Expr;

class CanonicalRenderer {
  public:
    explicit CanonicalRenderer(codegen::Declarations const& declarations);
    auto render_declaration_schema(codegen::DeclarationHandle declaration) -> std::string;
    auto render_module(codegen::ModuleHandle module) -> std::string;
  private:
    auto text(StringId value) const -> std::string_view;

    void add_export_specifier(codegen::sexpr::FormBuilder& result, StringId specifier);
    template <typename Id>
    auto items(codegen::SchemaRange<Id> range) const {
        return store_.items(range);
    }
    auto string_list(auto const& values) -> Expr {
        return ir_.list(values, [this](auto value) { return ir_.string(value); });
    }
    auto build_type_ref(codegen::TypeRefId id) -> Expr;
    auto build_enum_value(codegen::EnumeratorId value) -> Expr;
    auto build_enum_unreal_projection(codegen::EnumUnrealProjectionId projection) -> Expr;
    auto build_enum(codegen::DeclarationHandle schema) -> Expr;
    auto build_parameter(codegen::ParameterId parameter) -> Expr;
    auto build_function(codegen::FunctionSchemaId function, std::string_view kind = "function")
        -> Expr;
    auto build_semantic_relation(codegen::SemanticRelationId relationship) -> Expr;
    auto build_child(codegen::ChildOccurrenceId child) -> Expr;
    void add_projections(codegen::sexpr::FormBuilder& owner,
                         std::optional<codegen::CppProjection> const& cpp,
                         std::optional<codegen::EnumUnrealProjectionId> unreal = {});
    auto build_named_code(codegen::PackedNamedCodeId code) -> Expr;
    auto build_integer_scalar(codegen::DeclarationHandle schema) -> Expr;
    auto build_linear_quantized(codegen::DeclarationHandle schema) -> Expr;
    auto build_integer_varint(codegen::DeclarationHandle schema) -> Expr;
    auto build_fixed_point(codegen::DeclarationHandle schema) -> Expr;
    auto build_mini_float(codegen::DeclarationHandle schema) -> Expr;
    auto build_optional_sentinel(codegen::DeclarationHandle schema) -> Expr;
    auto build_optional_presence_bit(codegen::DeclarationHandle schema) -> Expr;
    auto build_record(codegen::DeclarationHandle schema) -> Expr;
    auto build_union(codegen::DeclarationHandle schema) -> Expr;
    auto build_soa_member(codegen::SoaMemberId member) -> Expr;
    auto build_soa(codegen::DeclarationHandle schema) -> Expr;
    auto build_static_table(codegen::DeclarationHandle schema) -> Expr;
    auto build_facade(codegen::DeclarationHandle schema) -> Expr;
    auto build_declaration_schema(codegen::DeclarationHandle declaration) -> Expr;
    auto build_module(codegen::ModuleHandle module) -> Expr;
    void build_settings_module(codegen::sexpr::FormBuilder& result, codegen::ModuleHandle module);
  private:
    codegen::Declarations const& store_;
    codegen::sexpr::SExprIR ir_;
};

} // namespace lispb::schema::detail
