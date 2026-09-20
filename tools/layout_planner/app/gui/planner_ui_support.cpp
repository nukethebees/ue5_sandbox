#include "planner_ui_support.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <ranges>

namespace ioj::layout_planner::detail {

auto format_bytes(std::optional<std::uint64_t> const bytes) -> std::string {
    if (!bytes.has_value()) {
        return "Unknown";
    }
    constexpr double kibibyte{1024.0};
    constexpr double mebibyte{1024.0 * 1024.0};
    constexpr double gibibyte{1024.0 * 1024.0 * 1024.0};
    std::array<char, 64> buffer{};
    auto const value{static_cast<double>(*bytes)};
    if (value >= gibibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f GiB", value / gibibyte);
    } else if (value >= mebibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f MiB", value / mebibyte);
    } else if (value >= kibibyte) {
        std::snprintf(buffer.data(), buffer.size(), "%.2f KiB", value / kibibyte);
    } else {
        std::snprintf(
            buffer.data(), buffer.size(), "%llu B", static_cast<unsigned long long>(*bytes));
    }
    return buffer.data();
}

auto format_number(std::optional<std::uint64_t> const value) -> std::string {
    return value.has_value() ? std::to_string(*value) : "Unknown";
}

auto format_code_count(layout::ExactCodeCount const value) -> std::string {
    return value.two_to_64 ? "18446744073709551616 (2^64)" : std::to_string(value.value);
}

auto format_fit(std::optional<bool> const fits) -> std::string {
    return fits.has_value() ? (*fits ? "Yes" : "No") : "Unknown";
}

auto format_delta(std::optional<layout::NumericDelta> const delta, auto const& format_magnitude)
    -> std::string {
    if (!delta.has_value()) {
        return "Unknown";
    }
    auto const prefix{delta->direction == layout::NumericDeltaDirection::increased   ? "+"
                      : delta->direction == layout::NumericDeltaDirection::decreased ? "-"
                                                                                     : ""};
    auto result{std::string{prefix} + format_magnitude(delta->magnitude)};
    if (delta->percentage.has_value()) {
        std::array<char, 32> percentage{};
        std::snprintf(percentage.data(), percentage.size(), " (%.1f%%)", *delta->percentage);
        result += percentage.data();
    }
    return result;
}

auto format_delta_bytes(std::optional<layout::NumericDelta> const delta) -> std::string {
    return format_delta(delta, [](std::uint64_t const magnitude) {
        return format_bytes(std::optional<std::uint64_t>{magnitude});
    });
}

auto format_delta_number(std::optional<layout::NumericDelta> const delta) -> std::string {
    return format_delta(delta,
                        [](std::uint64_t const magnitude) { return std::to_string(magnitude); });
}

auto parse_unsigned(std::string_view text) -> std::optional<std::uint64_t> {
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value{};
    auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value, base)};
    return error == std::errc{} && end == text.data() + text.size() ? std::optional{value}
                                                                    : std::nullopt;
}

auto parse_packed_integer(std::string_view text) -> std::optional<codegen::PackedIntegerValue> {
    auto negative{false};
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto const magnitude{parse_unsigned(text)};
    return magnitude.has_value()
             ? std::optional{codegen::PackedIntegerValue::from_parts(negative, *magnitude)}
             : std::nullopt;
}

auto diagnostic_color(layout::DiagnosticSeverity const severity) -> ImVec4 {
    switch (severity) {
        case layout::DiagnosticSeverity::info:
            return {0.65F, 0.75F, 0.9F, 1.0F};
        case layout::DiagnosticSeverity::warning:
            return {0.95F, 0.72F, 0.25F, 1.0F};
        case layout::DiagnosticSeverity::error:
            return {0.95F, 0.35F, 0.3F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

auto packed_field(lispb::schema::PackedType const& packed, std::string const& name)
    -> lispb::schema::PackedField const* {
    for (auto const& segment : packed.segments) {
        if (auto const* field{std::get_if<lispb::schema::PackedField>(&segment)};
            field != nullptr && field->name == name) {
            return field;
        }
    }
    return nullptr;
}

auto soa_column(lispb::schema::SoaType const& soa, std::string const& name)
    -> lispb::schema::SoaColumn const* {
    auto const found{std::ranges::find(soa.columns, name, &lispb::schema::SoaColumn::name)};
    return found == soa.columns.end() ? nullptr : &*found;
}

auto has_error(std::vector<layout::Diagnostic> const& diagnostics) -> bool {
    return std::ranges::any_of(diagnostics, [](layout::Diagnostic const& diagnostic) {
        return diagnostic.severity == layout::DiagnosticSeverity::error;
    });
}

auto override_count(layout::VariantOverrides const& overrides) -> std::size_t {
    return overrides.packed_storage_types.size() + overrides.packed_field_widths.size() +
           overrides.soa_column_types.size() + overrides.capacities.size();
}

} // namespace ioj::layout_planner::detail
