#pragma once

#include "sandbox/core/ui/types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ml::ui::stacked_bar_chart {
struct SegmentGeometry {
    std::int32_t bar_index{-1};
    std::int32_t segment_index{-1};
    Vector2f position{};
    Vector2f size{};
};

struct Geometry {
    std::vector<SegmentGeometry> segments;
    float maximum_total{};
    float slot_width{};
    float bar_width{};
};

using Bar = std::vector<float>;

[[nodiscard]] auto bar_total(std::span<float const> values) noexcept -> float;
[[nodiscard]] auto maximum_total(std::span<Bar const> bars) noexcept -> float;
[[nodiscard]] auto build_geometry(std::span<Bar const> bars, Vector2f plot_size, float bar_gap)
    -> Geometry;

class Data {
  public:
    void set_bars(std::vector<Bar> bars);
    void clear_bars();
    [[nodiscard]] auto bars() const noexcept -> std::span<Bar const> { return bars_; }
  private:
    std::vector<Bar> bars_;
};
}
