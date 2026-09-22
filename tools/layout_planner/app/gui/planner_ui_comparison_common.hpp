#pragma once

#include "planner_ui.hpp"

#include "planner_ui_support.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout_planner {
namespace {

using namespace layout;
using namespace lispb::schema;

inline constexpr ImVec4 changed_color{0.4F, 0.75F, 0.95F, 1.0F};

inline void comparison_row(char const* const label,
                           std::string const& baseline,
                           std::string const& variant,
                           std::string const& difference = {}) {
    auto const changed{baseline != variant};
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(baseline.c_str());
    ImGui::TableNextColumn();
    if (changed) {
        ImGui::PushStyleColor(ImGuiCol_Text, changed_color);
    }
    ImGui::TextUnformatted(variant.c_str());
    if (changed) {
        ImGui::PopStyleColor();
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(difference.empty() ? (changed ? "Changed" : "—") : difference.c_str());
}

inline auto field_by_name(layout::PackedAnalysis const& analysis, std::string const& name)
    -> layout::PackedFieldAnalysis const* {
    auto const found{std::ranges::find(analysis.fields, name, &layout::PackedFieldAnalysis::name)};
    return found == analysis.fields.end() ? nullptr : &*found;
}

inline auto column_by_name(layout::SoaAnalysis const& analysis, std::string const& name)
    -> layout::SoaColumnAnalysis const* {
    auto const found{std::ranges::find(analysis.columns, name, &layout::SoaColumnAnalysis::name)};
    return found == analysis.columns.end() ? nullptr : &*found;
}

inline auto format_decimal(long double const value) -> std::string {
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.12g", static_cast<double>(value));
    return buffer.data();
}

inline auto format_decimal_delta(std::optional<long double> const value) -> std::string {
    if (!value.has_value()) {
        return "Unknown";
    }
    if (*value == 0.0L) {
        return "0";
    }
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%+.12g", static_cast<double>(*value));
    return buffer.data();
}

inline auto format_optional_decimal(std::optional<long double> const value) -> std::string {
    return value.has_value() ? format_decimal(*value) : "Unknown";
}

inline auto type_label(lispb::schema::TypeNode const& node) -> std::string {
    return node.identity.module_name.empty()
             ? node.identity.name
             : node.identity.module_name + "::" + node.identity.name;
}

inline auto optional_source(TypeDefinition const& definition) -> std::optional<TypeId> {
    if (auto const* sentinel{std::get_if<OptionalSentinelType>(&definition)}) {
        return sentinel->source.type;
    }
    if (auto const* presence{std::get_if<OptionalPresenceBitType>(&definition)}) {
        return presence->source.type;
    }
    return std::nullopt;
}

inline auto optional_encoding_label(OptionalEncodingKind const kind) -> char const* {
    return kind == OptionalEncodingKind::sentinel ? "sentinel" : "presence bit";
}

inline auto format_enum_code(std::optional<EnumCodeValue> const& code) -> std::string {
    return code.has_value() ? lispb::schema::format_enum_code(*code) : "Unknown";
}

inline auto format_signedness(std::optional<bool> const signedness) -> std::string {
    if (!signedness.has_value()) {
        return "Unknown";
    }
    return *signedness ? "Signed" : "Unsigned";
}

inline auto as_uint64(std::optional<std::uint32_t> const value) -> std::optional<std::uint64_t> {
    return value.transform(
        [](std::uint32_t const known) { return static_cast<std::uint64_t>(known); });
}

} // namespace

} // namespace ioj::layout_planner
