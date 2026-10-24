#pragma once

#include "ioj/ui/types.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace ioj::ui::radar_2d {
using PositionList = std::vector<Vector2f>;

// Borrow until the next bucket mutation; the Data must outlive this view.
struct PositionView {
    std::span<float const> xs;
    std::span<float const> ys;
};

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
    Data();
    ~Data();
    Data(Data const&) = delete;
    auto operator=(Data const&) -> Data& = delete;
    [[nodiscard]] auto set_range(float range) noexcept -> bool;
    void set_buckets(std::span<PositionList const> buckets);
    [[nodiscard]] auto add_bucket() -> std::int32_t;
    [[nodiscard]] auto set_positions(std::int32_t index, std::span<Vector2f const> positions)
        -> bool;
    [[nodiscard]] auto clear_positions(std::int32_t index) -> bool;
    auto clear_positions() -> bool;
    void clear_buckets();

    [[nodiscard]] auto range() const noexcept -> float { return range_; }
    [[nodiscard]] auto bucket_count() const noexcept -> std::size_t;
    // Require index < bucket_count().
    [[nodiscard]] auto positions(std::size_t index) const noexcept -> PositionView;
    [[nodiscard]] auto copy_positions(std::size_t index) const -> PositionList;
  private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
    float range_{1.0f};
};
}
