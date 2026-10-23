#include "ioj/ui/radar_2d.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ioj::ui::radar_2d {
auto make_layout(Vector2f const widget_size, float const range) noexcept -> Layout {
    auto const width{std::max(widget_size.x(), 0.0f)};
    auto const height{std::max(widget_size.y(), 0.0f)};
    auto const side{std::min(width, height)};
    auto const origin{Vector2f{(width - side) * 0.5f, (height - side) * 0.5f}};
    auto const size{Vector2f{side, side}};
    auto const centre{Vector2f{origin.x() + size.x() * 0.5f, origin.y() + size.y() * 0.5f}};
    auto const pixels_per_unit{std::isfinite(range) && range > 0.0f ? side / (range * 2.0f) : 0.0f};
    return {.origin = origin, .size = size, .centre = centre, .pixels_per_unit = pixels_per_unit};
}

auto to_local(Vector2f const radar_position, Layout const& layout) noexcept -> Vector2f {
    return {layout.centre.x() + radar_position.x() * layout.pixels_per_unit,
            layout.centre.y() - radar_position.y() * layout.pixels_per_unit};
}

auto is_valid_extent(Vector2f const extent) noexcept -> bool {
    return std::isfinite(extent.x()) && std::isfinite(extent.y()) && extent.x() > 0.0f &&
           extent.y() > 0.0f;
}

auto Data::set_range(float const range) noexcept -> bool {
    if (!std::isfinite(range) || range <= 0.0f || range_ == range) {
        return false;
    }
    range_ = range;
    return true;
}

void Data::set_buckets(std::vector<Positions> buckets) {
    buckets_ = std::move(buckets);
}

auto Data::add_bucket() -> std::int32_t {
    buckets_.emplace_back();
    return static_cast<std::int32_t>(buckets_.size() - 1);
}

auto Data::set_positions(std::int32_t const index, PositionsConstView const positions) -> bool {
    if (index < 0 || static_cast<std::size_t>(index) >= buckets_.size()) {
        return false;
    }

    Positions snapshot;
    snapshot.append_from(positions);
    buckets_[static_cast<std::size_t>(index)] = std::move(snapshot);
    return true;
}

auto Data::clear_positions(std::int32_t const index) -> bool {
    if (index < 0 || static_cast<std::size_t>(index) >= buckets_.size() ||
        buckets_[static_cast<std::size_t>(index)].is_empty()) {
        return false;
    }
    buckets_[static_cast<std::size_t>(index)].reset();
    return true;
}

void Data::clear_positions() {
    for (auto& bucket : buckets_) {
        bucket.reset();
    }
}

void Data::clear_buckets() {
    buckets_.clear();
}
}
