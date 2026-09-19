#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/model.hpp>

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout_planner::detail {

auto definition_id(layout::LayoutDefinition const& definition) -> layout::SchemaId const&;
auto format_bytes(std::optional<std::uint64_t> bytes) -> std::string;
auto format_number(std::optional<std::uint64_t> value) -> std::string;
auto format_delta_bytes(std::optional<layout::NumericDelta> delta) -> std::string;
auto format_delta_number(std::optional<layout::NumericDelta> delta) -> std::string;
auto diagnostic_color(layout::DiagnosticSeverity severity) -> ImVec4;
auto packed_field(layout::PackedLayout const& layout, std::string const& name)
    -> layout::PackedField const*;
auto soa_column(layout::SoaLayout const& layout, std::string const& name)
    -> layout::SoaColumn const*;
auto has_error(std::vector<layout::Diagnostic> const& diagnostics) -> bool;
auto override_count(layout::VariantOverrides const& overrides) -> std::size_t;

} // namespace ioj::layout_planner::detail
