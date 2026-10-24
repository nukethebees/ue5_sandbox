#include "ioj/ui/radar_2d.h"

#include "radar_positions.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ioj::ui::radar_2d {
struct Data::Storage {
    std::vector<Positions> buckets;
};

Data::Data()
    : storage_{std::make_unique<Storage>()} {}
Data::~Data() = default;

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

void Data::set_buckets(std::span<PositionList const> buckets) {
    std::vector<Positions> storage(buckets.size());
    auto const count{buckets.size()};
    for (std::size_t i{}; i < count; ++i) {
        storage[i].reserve(static_cast<std::uint32_t>(buckets[i].size()));
        for (auto const position : buckets[i]) {
            storage[i].add(position.x(), position.y());
        }
    }
    storage_->buckets = std::move(storage);
}

auto Data::add_bucket() -> std::int32_t {
    storage_->buckets.emplace_back();
    return static_cast<std::int32_t>(storage_->buckets.size() - 1);
}

auto Data::set_positions(std::int32_t const index, std::span<Vector2f const> positions) -> bool {
    if (index < 0 || static_cast<std::size_t>(index) >= storage_->buckets.size()) {
        return false;
    }

    Positions snapshot;
    snapshot.reserve(static_cast<std::uint32_t>(positions.size()));
    for (auto const position : positions) {
        snapshot.add(position.x(), position.y());
    }
    storage_->buckets[static_cast<std::size_t>(index)] = std::move(snapshot);
    return true;
}

auto Data::clear_positions(std::int32_t const index) -> bool {
    if (index < 0 || static_cast<std::size_t>(index) >= storage_->buckets.size() ||
        storage_->buckets[static_cast<std::size_t>(index)].is_empty()) {
        return false;
    }
    storage_->buckets[static_cast<std::size_t>(index)].reset();
    return true;
}

auto Data::clear_positions() -> bool {
    bool changed{};
    for (auto& bucket : storage_->buckets) {
        changed |= !bucket.is_empty();
        bucket.reset();
    }
    return changed;
}

void Data::clear_buckets() {
    storage_->buckets.clear();
}

auto Data::bucket_count() const noexcept -> std::size_t {
    return storage_->buckets.size();
}
auto Data::positions(std::size_t index) const noexcept -> PositionView {
    assert(index < bucket_count());
    auto const view{storage_->buckets[index].get_const_view()};
    return {view.xs(), view.ys()};
}
auto Data::copy_positions(std::size_t index) const -> PositionList {
    auto const view{positions(index)};
    PositionList result;
    auto const count{view.xs.size()};
    result.reserve(count);
    for (std::size_t i{}; i < count; ++i) {
        result.push_back({view.xs[i], view.ys[i]});
    }
    return result;
}
}
