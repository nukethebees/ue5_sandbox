#pragma once

#include "ioj/ui/radar_positions.hpp"
#include "ioj/ui/types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::ui::radar_2d {
struct Layout {
    Vector2f origin{};
    Vector2f size{};
    Vector2f centre{};
    float pixels_per_unit{};
};

[[nodiscard]] auto make_layout(Vector2f widget_size, float range) noexcept -> Layout;
[[nodiscard]] auto to_local(Vector2f radar_position, Layout const& layout) noexcept -> Vector2f;
[[nodiscard]] auto is_valid_extent(Vector2f extent) noexcept -> bool;

class Data {
  public:
    [[nodiscard]] auto set_range(float range) noexcept -> bool;
    void set_buckets(std::vector<Positions> buckets);
    [[nodiscard]] auto add_bucket() -> std::int32_t;
    [[nodiscard]] auto set_positions(std::int32_t index, positions::ConstView positions) -> bool;
    [[nodiscard]] auto clear_positions(std::int32_t index) -> bool;
    void clear_positions();
    void clear_buckets();

    [[nodiscard]] auto range() const noexcept -> float { return range_; }
    [[nodiscard]] auto buckets() const noexcept -> std::span<Positions const> { return buckets_; }
  private:
    std::vector<Positions> buckets_;
    float range_{1.0f};
};
}
