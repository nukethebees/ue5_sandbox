#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <lispb/schema/type_graph.h>

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ioj::layout_planner::detail {

void pane_section_menu();
enum class ExpansionDomain { sections, modules };
void request_expansion(ExpansionDomain domain, bool open);
void prepare_expansion(char const* label, ExpansionDomain domain, bool reveal = false);
[[nodiscard]] auto section(char const* label, bool default_open = false, bool module_tree = false)
    -> bool;

class WrappingButtonRow {
  public:
    explicit WrappingButtonRow(bool follow_previous_item = false);
    auto button(char const* label) -> bool;
  private:
    float right_edge_{};
    float available_width_{};
    float last_right_{};
    bool has_previous_{};
};

auto begin_editable_table(char const* id, int columns, std::size_t rows) -> bool;
void editable_table_column(char const* label,
                           ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None);
void editable_table_content_hint(std::string_view text, float trailing_width = 0.0F);
auto prepare_editable_type_input() -> bool;
void prepare_property_input(char const* label);
auto editable_table_row_handle(bool selected) -> bool;
auto semantic_type_navigation_button() -> bool;

auto format_bytes(std::optional<std::uint64_t> bytes) -> std::string;
auto format_number(std::optional<std::uint64_t> value) -> std::string;
auto format_code_count(layout::ExactCodeCount value) -> std::string;
auto format_fit(std::optional<bool> fits) -> std::string;
auto access_operation_name(layout::AccessOperation operation) -> char const*;
auto access_operation_summary(std::span<layout::AccessIntent const> accesses) -> char const*;
auto format_delta_bytes(std::optional<layout::NumericDelta> delta) -> std::string;
auto format_delta_number(std::optional<layout::NumericDelta> delta) -> std::string;
auto relationship_extent_term(codegen::SemanticRelationKind kind) -> std::string_view;
auto relationship_extent_unit(codegen::SemanticRelationKind kind,
                              std::optional<codegen::SemanticRelationUnit> unit)
    -> std::string_view;
auto parse_unsigned(std::string_view text) -> std::optional<std::uint64_t>;
auto parse_packed_integer(std::string_view text) -> std::optional<codegen::PackedIntegerValue>;
auto diagnostic_color(layout::DiagnosticSeverity severity) -> ImVec4;
auto packed_field(lispb::schema::PackedType const& packed, std::string const& name)
    -> lispb::schema::PackedField const*;
auto soa_column(lispb::schema::SoaType const& soa, std::string const& name)
    -> lispb::schema::SoaColumn const*;
auto has_error(std::vector<layout::Diagnostic> const& diagnostics) -> bool;
auto override_count(layout::VariantOverrides const& overrides) -> std::size_t;
void draw_labeled_gap(ImDrawList* draw_list,
                      ImVec2 minimum,
                      ImVec2 maximum,
                      std::string_view label,
                      std::string_view compact_label);

} // namespace ioj::layout_planner::detail
