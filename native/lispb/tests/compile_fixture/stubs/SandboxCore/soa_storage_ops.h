#pragma once

#include "SandboxCore/array_checks.h"
#include "SandboxCore/array_utils.h"
#include "SandboxCore/container_ops.h"
#include "SandboxCore/soa_permutation.h"

#include "Containers/AllowShrinking.h"
#include "Containers/ArrayView.h"

namespace ml::soa_ops {

template <typename View>
void validate_array_sizes(View const& view) {
    view.apply_arrays(
        [](auto const&... columns) { ml::fatal_if_nums_not_equal({ml::num(columns)...}); });
}

template <typename Soa>
void reset(Soa& value) {
    value.apply_arrays([](auto&... columns) { (ml::reset(columns), ...); });
}

template <typename Soa>
void reserve(Soa& value, int32 const count) {
    value.apply_arrays([count](auto&... columns) { (ml::reserve(columns, count), ...); });
}

template <typename Soa>
void add_uninitialised(Soa& value, int32 const count) {
    value.apply_arrays([count](auto&... columns) { (ml::add_uninitialised(columns, count), ...); });
}

template <typename Soa>
void add_defaulted(Soa& value, int32 const count) {
    value.apply_arrays([count](auto&... columns) { (ml::add_defaulted(columns, count), ...); });
}

template <typename Soa>
void remove_at_swap(Soa& value,
                    int32 const index,
                    int32 const count,
                    EAllowShrinking const allow_shrinking) {
    value.apply_arrays([=](auto&... columns) {
        (ml::remove_at_swap(columns, index, count, allow_shrinking), ...);
    });
}

template <typename Soa>
void set_num(Soa& value, int32 const count, EAllowShrinking const allow_shrinking) {
    value.apply_arrays(
        [=](auto&... columns) { (ml::set_num(columns, count, allow_shrinking), ...); });
}

template <typename Soa>
void apply_permutation(Soa& value, TArrayView<int32> const indices) {
    value.validate_array_sizes();
    check(indices.Num() == value.num());
    value.apply_arrays([&](auto&... columns) { (ml::apply_permutation(columns, indices), ...); });
}

template <typename Soa, typename Compare>
void sort(Soa& value, Compare&& compare, TArrayView<int32> scratch_indices) {
    value.validate_array_sizes();
    auto const n{value.num()};
    check(scratch_indices.Num() == n);
    ml::fill_indices(scratch_indices);
    scratch_indices.Sort(
        [&value, &compare](int32 const lhs, int32 const rhs) { return compare(value, lhs, rhs); });
    apply_permutation(value, scratch_indices);
}

template <auto Compare, typename Soa>
void sort(Soa& value, TArrayView<int32> scratch_indices) {
    value.validate_array_sizes();
    auto const n{value.num()};
    check(scratch_indices.Num() == n);
    ml::fill_indices(scratch_indices);
    scratch_indices.Sort(
        [&value](int32 const lhs, int32 const rhs) { return Compare(value, lhs, rhs); });
    apply_permutation(value, scratch_indices);
}

} // namespace ml::soa_ops
