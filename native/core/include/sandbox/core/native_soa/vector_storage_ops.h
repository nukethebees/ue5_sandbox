#pragma once

#include <sandbox/core/container_ops.h>
#include <sandbox/core/native_soa/storage.h>
#include <sandbox/core/soa_permutation.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace ml::native_soa::vector_storage_ops {
namespace detail {
struct ColumnVisitor {
    template <typename Column>
    void operator()(Column&) const {}
};
}

template <typename Soa>
concept VectorStorageSoa = requires(Soa& soa, Soa const& const_soa) {
    { const_soa.num() } -> std::convertible_to<std::int32_t>;
    { const_soa.validate_array_sizes() } -> std::same_as<void>;
    { soa.each_column(detail::ColumnVisitor{}) } -> std::same_as<void>;
};

template <typename View>
void validate_array_sizes(View const& view) {
    auto const count{view.num()};
    view.each_column(
        [count](auto const& column) { require(column.size() == static_cast<std::size_t>(count)); });
}

template <VectorStorageSoa Soa>
void reserve(Soa& soa, std::int32_t const count) {
    require(count >= 0);
    soa.each_column([count](auto& column) { column.reserve(static_cast<std::size_t>(count)); });
}

template <VectorStorageSoa Soa>
void reset(Soa& soa) noexcept {
    soa.each_column([](auto& column) { column.clear(); });
}

template <VectorStorageSoa Soa>
void set_num(Soa& soa, std::int32_t const count) {
    require(count >= 0);
    auto const size{static_cast<std::size_t>(count)};
    soa.each_column([size](auto& column) { column.resize(size); });
}

template <VectorStorageSoa Soa>
void add_uninitialised(Soa& soa, std::int32_t const count) {
    // The API name matches Unreal SoAs; std::vector::resize value-initializes new elements.
    auto const old_num{soa.num()};
    require(count >= 0 && count <= std::numeric_limits<std::int32_t>::max() - old_num);
    set_num(soa, old_num + count);
}

template <VectorStorageSoa Soa>
void add_defaulted(Soa& soa, std::int32_t const count) {
    add_uninitialised(soa, count);
}

template <VectorStorageSoa Soa>
void remove_at_swap(Soa& soa, std::int32_t const index, std::int32_t const count) {
    soa.validate_array_sizes();
    auto const old_num{soa.num()};
    require(index >= 0 && index <= old_num && count >= 0 && count <= old_num - index);
    soa.each_column([index, count](auto& column) { ml::remove_at_swap(column, index, count); });
}

template <VectorStorageSoa Soa>
void apply_permutation(Soa& soa, std::span<std::int32_t> const indices) {
    soa.validate_array_sizes();
    require(indices.size() == static_cast<std::size_t>(soa.num()));
    soa.each_column([indices](auto& column) { ml::apply_permutation(std::span{column}, indices); });
}

template <VectorStorageSoa Soa, typename Compare>
void sort(Soa& soa, Compare&& compare, std::span<std::int32_t> const scratch_indices) {
    soa.validate_array_sizes();
    auto const count{soa.num()};
    require(scratch_indices.size() == static_cast<std::size_t>(count));
    for (std::int32_t i{}; i < count; ++i) {
        scratch_indices[static_cast<std::size_t>(i)] = i;
    }
    std::sort(scratch_indices.begin(),
              scratch_indices.end(),
              [&soa, &compare](std::int32_t const lhs, std::int32_t const rhs) {
                  return compare(soa, lhs, rhs);
              });
    apply_permutation(soa, scratch_indices);
}

} // namespace ml::native_soa::vector_storage_ops
