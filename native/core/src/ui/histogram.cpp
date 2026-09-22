#include "sandbox/core/ui/histogram.h"

#include <algorithm>
#include <cmath>

namespace ml::ui::histogram {
auto is_valid_configuration(float const domain_minimum,
                            float const domain_maximum,
                            std::int32_t const bin_count) noexcept -> bool {
    return std::isfinite(domain_minimum) && std::isfinite(domain_maximum) &&
           domain_maximum > domain_minimum && bin_count > 0;
}

auto build_bins(std::span<float const> const samples,
                float const domain_minimum,
                float const domain_maximum,
                std::int32_t const bin_count) -> std::vector<std::int32_t> {
    if (!is_valid_configuration(domain_minimum, domain_maximum, bin_count)) {
        return {};
    }

    std::vector<std::int32_t> bins(static_cast<std::size_t>(bin_count));
    auto const domain_size{domain_maximum - domain_minimum};
    for (auto const sample : samples) {
        if (!std::isfinite(sample) || sample < domain_minimum || sample > domain_maximum) {
            continue;
        }

        auto const bin_index{sample == domain_maximum
                                 ? bin_count - 1
                                 : std::clamp(static_cast<std::int32_t>(std::floor(
                                                  (sample - domain_minimum) / domain_size *
                                                  static_cast<float>(bin_count))),
                                              std::int32_t{},
                                              bin_count - 1)};
        ++bins[static_cast<std::size_t>(bin_index)];
    }
    return bins;
}

auto maximum_bin_count(std::span<std::int32_t const> const bins) noexcept -> std::int32_t {
    std::int32_t maximum{};
    for (auto const count : bins) {
        maximum = std::max(maximum, count);
    }
    return maximum;
}

auto build_geometry(std::span<std::int32_t const> const bins,
                    Vector2f const plot_size,
                    float const bar_gap) -> Geometry {
    Geometry geometry;
    geometry.maximum_count = maximum_bin_count(bins);

    auto const bin_count{bins.size()};
    auto const width{std::max(plot_size.x, 0.0f)};
    auto const height{std::max(plot_size.y, 0.0f)};
    if (bin_count == 0 || width <= 0.0f || height <= 0.0f) {
        return geometry;
    }

    geometry.slot_width = width / static_cast<float>(bin_count);
    geometry.bar_width = std::max(geometry.slot_width - std::max(bar_gap, 0.0f), 0.0f);
    if (geometry.maximum_count <= 0 || geometry.bar_width <= 0.0f) {
        return geometry;
    }

    auto const pixels_per_sample{height / static_cast<float>(geometry.maximum_count)};
    geometry.bars.reserve(bin_count);
    for (std::size_t index{}; index < bin_count; ++index) {
        auto const count{bins[index]};
        if (count <= 0) {
            continue;
        }

        auto const bar_height{static_cast<float>(count) * pixels_per_sample};
        auto const x{static_cast<float>(index) * geometry.slot_width +
                     (geometry.slot_width - geometry.bar_width) * 0.5f};
        geometry.bars.push_back({.bin_index = static_cast<std::int32_t>(index),
                                 .count = count,
                                 .position = {x, height - bar_height},
                                 .size = {geometry.bar_width, bar_height}});
    }
    return geometry;
}

auto hit_test_bin(Vector2f const point,
                  Vector2f const plot_origin,
                  Vector2f const plot_size,
                  std::int32_t const bin_count) noexcept -> std::optional<std::int32_t> {
    if (bin_count <= 0 || plot_size.x <= 0.0f || plot_size.y <= 0.0f || point.x < plot_origin.x ||
        point.y < plot_origin.y || point.x >= plot_origin.x + plot_size.x ||
        point.y >= plot_origin.y + plot_size.y) {
        return std::nullopt;
    }
    return std::clamp(static_cast<std::int32_t>(std::floor((point.x - plot_origin.x) / plot_size.x *
                                                           static_cast<float>(bin_count))),
                      std::int32_t{},
                      bin_count - 1);
}

auto bin_range(float const domain_minimum,
               float const domain_maximum,
               std::int32_t const bin_count,
               std::int32_t const bin_index) noexcept -> std::optional<BinRange> {
    if (!is_valid_configuration(domain_minimum, domain_maximum, bin_count) || bin_index < 0 ||
        bin_index >= bin_count) {
        return std::nullopt;
    }
    auto const width{(domain_maximum - domain_minimum) / static_cast<float>(bin_count)};
    auto const minimum{domain_minimum + width * static_cast<float>(bin_index)};
    return BinRange{minimum, minimum + width};
}

void Data::set_samples(std::vector<float> samples) {
    samples_ = std::move(samples);
    rebuild_bins();
}

void Data::clear_samples() {
    samples_.clear();
    rebuild_bins();
}

auto Data::set_configuration(float const domain_minimum,
                             float const domain_maximum,
                             std::int32_t const bin_count) -> bool {
    if (!is_valid_configuration(domain_minimum, domain_maximum, bin_count)) {
        return false;
    }
    domain_minimum_ = domain_minimum;
    domain_maximum_ = domain_maximum;
    bin_count_ = bin_count;
    rebuild_bins();
    return true;
}

void Data::rebuild_bins() {
    bins_ = build_bins(samples_, domain_minimum_, domain_maximum_, bin_count_);
}
}
