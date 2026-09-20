#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <lispb/schema/type_graph.h>

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout_planner::detail {

auto format_bytes(std::optional<std::uint64_t> bytes) -> std::string;
auto format_number(std::optional<std::uint64_t> value) -> std::string;
auto format_code_count(layout::ExactCodeCount value) -> std::string;
auto format_fit(std::optional<bool> fits) -> std::string;
auto format_delta_bytes(std::optional<layout::NumericDelta> delta) -> std::string;
auto format_delta_number(std::optional<layout::NumericDelta> delta) -> std::string;
auto parse_unsigned(std::string_view text) -> std::optional<std::uint64_t>;
auto parse_packed_integer(std::string_view text) -> std::optional<codegen::PackedIntegerValue>;
auto diagnostic_color(layout::DiagnosticSeverity severity) -> ImVec4;
auto packed_field(lispb::schema::PackedType const& packed, std::string const& name)
    -> lispb::schema::PackedField const*;
auto soa_column(lispb::schema::SoaType const& soa, std::string const& name)
    -> lispb::schema::SoaColumn const*;
auto has_error(std::vector<layout::Diagnostic> const& diagnostics) -> bool;
auto override_count(layout::VariantOverrides const& overrides) -> std::size_t;

} // namespace ioj::layout_planner::detail
