#pragma once

#include <codegen/ast/access.h>
#include <codegen/schema/declarations.h>
#include <lispb/strings.h>

#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

namespace lispb::schema::detail {

class CanonicalRenderer {
  public:
    explicit CanonicalRenderer(codegen::Declarations const& declarations);
    auto render_declaration_schema(codegen::DeclarationHandle declaration) -> std::string;
    auto render_module(codegen::ModuleHandle module) -> std::string;
  private:
    auto text(StringId value) const -> std::string_view;
    auto text(IdentifierId value) const -> std::string_view;
    auto quote(StringId value) -> std::string;
    static auto access_keyword(codegen::Access access) -> std::string_view;
    template <typename Id>
    auto items(codegen::SchemaRange<Id> range) const {
        return store_.items(range);
    }
    template <typename Id>
    auto texts(codegen::SchemaRange<Id> range) const {
        return store_.items(range) | std::views::transform([this](Id id) { return text(id); });
    }
    auto quote(std::string_view const value) -> std::string;
    auto render_type_ref(codegen::TypeRefId id) -> std::string;
    auto render_enum_value(lispb::codegen::EnumeratorId const& value) -> std::string;
    auto render_enum_unreal_projection(lispb::codegen::EnumUnrealProjectionId const& projection)
        -> std::string;
    auto render_enum(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto packed_field_kind_name(codegen::PackedFieldKind const kind) -> std::string_view;
    auto soa_member_kind_name(codegen::SoaMemberKind const kind) -> std::string_view;
    auto storage_operation_name(codegen::StorageOperation const operation) -> std::string_view;
    void render_quoted_list(std::ostringstream& output, auto const& values) {
        output << '(';
        for (std::size_t index{}; index < values.size(); ++index) {
            output << (index == 0 ? "" : " ") << quote(values[index]);
        }
        output << ')';
    }
    void render_function(std::ostringstream& output,
                         lispb::codegen::FunctionSchemaId const& function,
                         std::string_view const indent,
                         std::string_view const kind = "function");
    auto render_semantic_relation(lispb::codegen::SemanticRelationId const& relationship)
        -> std::string;
    auto render_packed_segment(codegen::PackedSegmentId const segment) -> std::string;
    auto render_packed_value(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_named_code(lispb::codegen::PackedNamedCodeId const& code) -> std::string;
    auto render_integer_scalar(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_linear_quantized(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_integer_varint(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_fixed_point(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_mini_float(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_optional_sentinel(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_optional_presence_bit(lispb::codegen::DeclarationHandle const& schema)
        -> std::string;
    template <typename Child>
    auto render_aggregate_child(auto const& columns, std::string_view const head, Child const child)
        -> std::string {
        std::ostringstream output;
        output << '(' << head << ' ' << text(columns.name[columns.index(child)]) << ' '
               << render_type_ref(columns.type[columns.index(child)]);
        if (columns.count[columns.index(child)].has_value()) {
            output << " :count " << *columns.count[columns.index(child)];
        }
        if constexpr (std::is_same_v<Child, lispb::codegen::RecordMemberId>) {
            if (columns.initializer[columns.index(child)]) {
                output << " :initializer " << quote(*columns.initializer[columns.index(child)]);
            }
            if (columns.relationship[columns.index(child)].has_value()) {
                output << "\n      "
                       << render_semantic_relation(*columns.relationship[columns.index(child)]);
            }
        }
        output << ')';
        return output.str();
    }
    auto render_record(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_union(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto
        render_tagged_union_alternative(lispb::codegen::TaggedUnionAlternativeId const& alternative)
            -> std::string;
    auto render_tagged_union(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_soa_member(lispb::codegen::SoaMemberId const& member) -> std::string;
    auto render_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_vector_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_homogeneous_layout(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_static_table(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    auto render_facade(lispb::codegen::DeclarationHandle const& schema) -> std::string;
    void render_settings_module(std::ostringstream& output, codegen::ModuleHandle const module);
  private:
    codegen::Declarations const& store_;
};

} // namespace lispb::schema::detail
