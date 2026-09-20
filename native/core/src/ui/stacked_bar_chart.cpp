#include "sandbox/core/ui/stacked_bar_chart.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ml::ui::stacked_bar_chart {
namespace {
auto positive_value(float const value) noexcept -> float {
    return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}
}

auto bar_total(std::span<float const> const values) noexcept -> float {
    float total{};
    for (auto const value : values) {
        total += positive_value(value);
    }
    return total;
}

auto maximum_total(std::span<Bar const> const bars) noexcept -> float {
    float result{};
    for (auto const& bar : bars) {
        result = std::max(result, bar_total(bar));
    }
    return result;
}

auto build_geometry(std::span<Bar const> const bars, Vector2f const plot_size, float const bar_gap)
    -> Geometry {
    Geometry result;
    result.maximum_total = maximum_total(bars);
    auto const width{std::max(plot_size.x, 0.0f)};
    auto const height{std::max(plot_size.y, 0.0f)};
    if (bars.empty() || width <= 0.0f || height <= 0.0f) {
        return result;
    }
    result.slot_width = width / static_cast<float>(bars.size());
    result.bar_width = std::max(result.slot_width - std::max(bar_gap, 0.0f), 0.0f);
    if (result.maximum_total <= 0.0f || result.bar_width <= 0.0f) {
        return result;
    }

    auto const pixels_per_value{height / result.maximum_total};
    for (std::size_t bar_index{}; bar_index < bars.size(); ++bar_index) {
        auto const x{static_cast<float>(bar_index) * result.slot_width +
                     (result.slot_width - result.bar_width) * 0.5f};
        float segment_bottom{height};
        auto const& bar{bars[bar_index]};
        for (std::size_t segment_index{}; segment_index < bar.size(); ++segment_index) {
            auto const value{positive_value(bar[segment_index])};
            if (value <= 0.0f) {
                continue;
            }
            auto const segment_top{std::max(segment_bottom - value * pixels_per_value, 0.0f)};
            result.segments.push_back({.bar_index = static_cast<std::int32_t>(bar_index),
                                       .segment_index = static_cast<std::int32_t>(segment_index),
                                       .position = {x, segment_top},
                                       .size = {result.bar_width, segment_bottom - segment_top}});
            segment_bottom = segment_top;
        }
    }
    return result;
}

void Data::set_bars(std::vector<Bar> bars) {
    bars_ = std::move(bars);
}

void Data::clear_bars() {
    bars_.clear();
}
}
