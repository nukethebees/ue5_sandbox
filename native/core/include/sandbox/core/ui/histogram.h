#pragma once

#include "sandbox/core/ui/types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ml::ui::histogram {
struct BarGeometry {
    std::int32_t bin_index{-1};
    std::int32_t count{};
    Vector2f position{};
    Vector2f size{};
};

struct Geometry {
    std::vector<BarGeometry> bars;
    std::int32_t maximum_count{};
    float slot_width{};
    float bar_width{};
};

struct BinRange {
    float minimum{};
    float maximum{};
};

[[nodiscard]] auto is_valid_configuration(float domain_minimum,
                                          float domain_maximum,
                                          std::int32_t bin_count) noexcept -> bool;
[[nodiscard]] auto build_bins(std::span<float const> samples,
                              float domain_minimum,
                              float domain_maximum,
                              std::int32_t bin_count) -> std::vector<std::int32_t>;
[[nodiscard]] auto maximum_bin_count(std::span<std::int32_t const> bins) noexcept -> std::int32_t;
[[nodiscard]] auto build_geometry(std::span<std::int32_t const> bins,
                                  Vector2f plot_size,
                                  float bar_gap) -> Geometry;
[[nodiscard]] auto hit_test_bin(Vector2f point,
                                Vector2f plot_origin,
                                Vector2f plot_size,
                                std::int32_t bin_count) noexcept -> std::optional<std::int32_t>;
[[nodiscard]] auto bin_range(float domain_minimum,
                             float domain_maximum,
                             std::int32_t bin_count,
                             std::int32_t bin_index) noexcept -> std::optional<BinRange>;

class Data {
  public:
    void set_samples(std::vector<float> samples);
    void clear_samples();
    [[nodiscard]] auto set_configuration(float domain_minimum,
                                         float domain_maximum,
                                         std::int32_t bin_count) -> bool;

    [[nodiscard]] auto samples() const noexcept -> std::span<float const> { return samples_; }
    [[nodiscard]] auto bins() const noexcept -> std::span<std::int32_t const> { return bins_; }
    [[nodiscard]] auto domain_minimum() const noexcept -> float { return domain_minimum_; }
    [[nodiscard]] auto domain_maximum() const noexcept -> float { return domain_maximum_; }
    [[nodiscard]] auto bin_count() const noexcept -> std::int32_t { return bin_count_; }
  private:
    void rebuild_bins();

    std::vector<float> samples_;
    std::vector<std::int32_t> bins_;
    float domain_minimum_{};
    float domain_maximum_{1.0f};
    std::int32_t bin_count_{10};
};
}
