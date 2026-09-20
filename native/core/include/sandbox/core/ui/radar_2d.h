#pragma once

#include "sandbox/core/ui/types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ml::ui::radar_2d {
struct Layout {
    Vector2f origin{};
    Vector2f size{};
    Vector2f centre{};
    float pixels_per_unit{};
};

struct Positions {
    std::vector<float> xs;
    std::vector<float> ys;

    void add(float x, float y);
    void clear();
    [[nodiscard]] auto is_valid() const noexcept -> bool { return xs.size() == ys.size(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return xs.empty(); }
    [[nodiscard]] auto size() const noexcept -> std::size_t { return xs.size(); }
};

[[nodiscard]] auto make_layout(Vector2f widget_size, float range) noexcept -> Layout;
[[nodiscard]] auto to_local(Vector2f radar_position, Layout const& layout) noexcept -> Vector2f;
[[nodiscard]] auto is_valid_extent(Vector2f extent) noexcept -> bool;

class Data {
  public:
    [[nodiscard]] auto set_range(float range) noexcept -> bool;
    [[nodiscard]] auto set_buckets(std::vector<Positions> buckets) -> bool;
    [[nodiscard]] auto add_bucket() -> std::int32_t;
    [[nodiscard]] auto set_positions(std::int32_t index, Positions positions) -> bool;
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
