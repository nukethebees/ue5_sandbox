#include "memory_regions.hpp"

#include "analysis_limits.hpp"
#include "checked_arithmetic.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout::analysis_detail {

auto cache_capacity_analysis(std::optional<std::uint64_t> const working_set_bytes,
                             MemoryFacts const& memory) -> CacheCapacityAnalysis {
    auto fits =
        [working_set_bytes](std::optional<std::uint64_t> const capacity) -> std::optional<bool> {
        if (!working_set_bytes.has_value() || !capacity.has_value()) {
            return std::nullopt;
        }
        return *working_set_bytes <= *capacity;
    };
    return {.working_set_bytes = working_set_bytes,
            .l1_data_capacity_bytes = memory.l1_data_cache_bytes,
            .l2_capacity_bytes = memory.l2_cache_bytes,
            .l3_capacity_bytes = memory.l3_cache_bytes,
            .fits_l1_data = fits(memory.l1_data_cache_bytes),
            .fits_l2 = fits(memory.l2_cache_bytes),
            .fits_l3 = fits(memory.l3_cache_bytes)};
}

auto minimum_regions(std::uint64_t const bytes, std::uint64_t const region_bytes) -> std::uint64_t {
    return bytes / region_bytes + (bytes % region_bytes == 0 ? 0 : 1);
}

auto straddling_elements(std::uint64_t const element_bytes,
                         std::uint64_t const element_count,
                         std::uint64_t const region_bytes,
                         std::optional<std::uint64_t> const total_bytes)
    -> std::optional<std::uint64_t> {
    if (element_count == 0) {
        return 0;
    }
    if (element_bytes == 0) {
        return std::nullopt;
    }
    if (element_bytes > region_bytes) {
        return element_count;
    }
    if (!total_bytes.has_value()) {
        return std::nullopt;
    }

    auto const boundaries_before_end{(*total_bytes - 1) / region_bytes};
    auto const coincident_boundary_period{region_bytes / std::gcd(element_bytes, region_bytes)};
    auto const coincident_boundaries{(element_count - 1) / coincident_boundary_period};
    return boundaries_before_end - coincident_boundaries;
}

auto straddling_elements_at_offset(std::uint64_t const element_bytes,
                                   std::uint64_t const element_count,
                                   std::uint64_t const region_bytes,
                                   std::optional<std::uint64_t> const total_bytes,
                                   std::uint64_t const allocation_offset)
    -> std::optional<std::uint64_t> {
    if (element_count == 0) {
        return 0;
    }
    if (element_bytes == 0 || !total_bytes.has_value()) {
        return std::nullopt;
    }
    if (element_bytes > region_bytes) {
        return element_count;
    }

    auto const period{region_bytes / std::gcd(element_bytes, region_bytes)};
    auto const elements_to_analyze{std::min(element_count, period)};
    if (elements_to_analyze > maximum_exact_region_period) {
        return std::nullopt;
    }

    auto offset_in_region{allocation_offset % region_bytes};
    std::uint64_t straddles_in_analyzed_elements{};
    std::uint64_t straddles_in_remainder{};
    auto const remainder{element_count % period};
    for (std::uint64_t index{}; index < elements_to_analyze; ++index) {
        auto const straddles{offset_in_region > region_bytes - element_bytes};
        straddles_in_analyzed_elements += straddles ? 1U : 0U;
        if (index < remainder) {
            straddles_in_remainder += straddles ? 1U : 0U;
        }
        if (offset_in_region >= region_bytes - element_bytes) {
            offset_in_region -= region_bytes - element_bytes;
        } else {
            offset_in_region += element_bytes;
        }
    }
    if (element_count <= period) {
        return straddles_in_analyzed_elements;
    }

    auto const full_periods{element_count / period};
    auto const full_period_straddles{
        checked_multiply(straddles_in_analyzed_elements, full_periods)};
    return full_period_straddles.has_value()
             ? checked_add(*full_period_straddles, straddles_in_remainder)
             : std::nullopt;
}

auto touched_regions(std::uint64_t const stride_bytes,
                     std::span<std::pair<std::uint64_t, std::uint64_t> const> const members,
                     std::uint64_t const element_count,
                     std::uint64_t const region_bytes,
                     std::string& error) -> std::optional<std::uint64_t> {
    if (element_count == 0 || members.empty()) {
        return 0;
    }
    auto intervals = [&](std::uint64_t const index)
        -> std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> {
        auto const base{checked_multiply(index, stride_bytes)};
        if (!base.has_value()) {
            return std::nullopt;
        }
        std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
        result.reserve(members.size());
        for (auto const& [offset, extent] : members) {
            auto const begin{checked_add(*base, offset)};
            auto const end{begin.has_value() ? checked_add(*begin, extent) : std::nullopt};
            if (!end.has_value() || *end == 0) {
                return std::nullopt;
            }
            result.emplace_back(*begin / region_bytes, (*end - 1) / region_bytes);
        }
        return result;
    };
    auto contribution =
        [&](std::uint64_t const index,
            std::optional<std::uint64_t>& previous_end) -> std::optional<std::uint64_t> {
        auto const current{intervals(index)};
        if (!current.has_value()) {
            return std::nullopt;
        }
        std::uint64_t added{};
        for (auto const& [begin, end] : *current) {
            auto interval_added{std::uint64_t{}};
            if (!previous_end.has_value() || begin > *previous_end) {
                interval_added = end - begin + 1;
            } else if (end > *previous_end) {
                interval_added = end - *previous_end;
            }
            auto const next{checked_add(added, interval_added)};
            if (!next.has_value()) {
                return std::nullopt;
            }
            added = *next;
            previous_end = std::max(previous_end.value_or(0), end);
        }
        return added;
    };
    std::optional<std::uint64_t> previous_end;
    auto total{contribution(0, previous_end)};
    if (!total.has_value()) {
        error = "Member access address arithmetic overflows uint64.";
        return std::nullopt;
    }
    if (element_count == 1) {
        return total;
    }

    auto const period{region_bytes / std::gcd(stride_bytes, region_bytes)};
    if (period > maximum_exact_region_period) {
        error = "Target region/stride period is too large for exact access analysis.";
        return std::nullopt;
    }
    auto const transition_count{element_count - 1};
    auto const complete_periods{transition_count / period};
    auto const remaining_transitions{transition_count % period};
    auto const transitions_to_analyze{complete_periods == 0 ? remaining_transitions : period};
    std::uint64_t period_contribution{};
    std::uint64_t remainder_contribution{};
    for (std::uint64_t index{1}; index <= transitions_to_analyze; ++index) {
        auto const added{contribution(index, previous_end)};
        if (!added.has_value()) {
            error = "Member access address arithmetic overflows uint64.";
            return std::nullopt;
        }
        auto const next_period{checked_add(period_contribution, *added)};
        if (!next_period.has_value()) {
            error = "Member access region count overflows uint64.";
            return std::nullopt;
        }
        period_contribution = *next_period;
        if (index <= remaining_transitions) {
            auto const next_remainder{checked_add(remainder_contribution, *added)};
            if (!next_remainder.has_value()) {
                error = "Member access region count overflows uint64.";
                return std::nullopt;
            }
            remainder_contribution = *next_remainder;
        }
    }

    auto const periods_contribution{checked_multiply(complete_periods, period_contribution)};
    if (!periods_contribution.has_value()) {
        error = "Member access region count overflows uint64.";
        return std::nullopt;
    }
    total = checked_add(*total, *periods_contribution);
    total = total.has_value() ? checked_add(*total, remainder_contribution) : std::nullopt;
    if (!total.has_value()) {
        error = "Member access region count overflows uint64.";
    }
    return total;
}

auto cache_line_tiling(std::uint64_t const element_bytes, std::uint64_t const cache_line_bytes)
    -> CacheLineTiling {
    auto const complete_elements{
        element_bytes <= cache_line_bytes ? cache_line_bytes / element_bytes : std::uint64_t{0}};
    return {.cache_line_bytes = cache_line_bytes,
            .element_bytes = element_bytes,
            .exact_elements_per_cache_line =
                element_bytes <= cache_line_bytes && cache_line_bytes % element_bytes == 0
                    ? std::optional<std::uint64_t>{complete_elements}
                    : std::nullopt,
            .complete_elements_from_line_start = complete_elements,
            .boundary_fragment_bytes =
                element_bytes <= cache_line_bytes ? cache_line_bytes % element_bytes : 0,
            .minimum_cache_lines_per_element = minimum_regions(element_bytes, cache_line_bytes)};
}

}
