#pragma once

#include "native_soa/storage.h"
#include "sandbox/simulation/vector_types.h"
#include "sandbox/simulation/vectors3f.h"

#include <cstdint>
#include <span>

namespace ml::simulation {
struct LineTracesView;

struct LineTracesConstView {
    using View = LineTracesView;
    using ConstView = LineTracesConstView;
    using size_type = std::int32_t;

    Vectors3fConstView starts;
    Vectors3fConstView ends;

    [[nodiscard]] auto num() const noexcept -> size_type { return starts.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const {
        starts.validate_array_sizes();
        ends.validate_array_sizes();
        ml::native_soa::require(starts.num() == ends.num());
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) const
        -> LineTracesConstView {
        return {starts.slice(offset, count), ends.slice(offset, count)};
    }
    [[nodiscard]] auto get_view() const -> ConstView { return *this; }
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const -> ConstView {
        return slice(offset, count);
    }
    [[nodiscard]] auto get_const_view() const -> ConstView { return *this; }
    [[nodiscard]] auto get_const_view(size_type const offset, size_type const count) const
        -> ConstView {
        return slice(offset, count);
    }
    [[nodiscard]] auto left(size_type const count) const -> ConstView { return slice(0, count); }
    [[nodiscard]] auto right(size_type const count) const -> ConstView {
        return slice(num() - count, count);
    }
};

struct LineTracesView {
    using View = LineTracesView;
    using ConstView = LineTracesConstView;
    using size_type = std::int32_t;

    Vectors3fView starts;
    Vectors3fView ends;

    [[nodiscard]] auto num() const noexcept -> size_type { return starts.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const {
        starts.validate_array_sizes();
        ends.validate_array_sizes();
        ml::native_soa::require(starts.num() == ends.num());
    }
    void set(size_type const index, Vector3f const start, Vector3f const end) const {
        starts.set(index, start);
        ends.set(index, end);
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) const -> View {
        return {starts.slice(offset, count), ends.slice(offset, count)};
    }
    [[nodiscard]] auto get_view() const -> View { return *this; }
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const -> View {
        return slice(offset, count);
    }
    [[nodiscard]] auto get_const_view() const -> ConstView {
        return {starts.get_const_view(), ends.get_const_view()};
    }
    [[nodiscard]] auto get_const_view(size_type const offset, size_type const count) const
        -> ConstView {
        return get_const_view().slice(offset, count);
    }
    [[nodiscard]] auto left(size_type const count) const -> View { return slice(0, count); }
    [[nodiscard]] auto right(size_type const count) const -> View {
        return slice(num() - count, count);
    }
};

struct LineTraces {
    using View = LineTracesView;
    using ConstView = LineTracesConstView;
    using size_type = std::int32_t;

    Vectors3f starts;
    Vectors3f ends;

    [[nodiscard]] auto num() const noexcept -> size_type { return starts.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const { get_const_view().validate_array_sizes(); }
    void reserve(size_type const count) {
        starts.reserve(count);
        ends.reserve(count);
    }
    void reset() noexcept {
        starts.reset();
        ends.reset();
    }
    void set_num(size_type const count) {
        starts.set_num(count);
        ends.set_num(count);
    }
    void add_uninitialised(size_type const count) {
        starts.add_uninitialised(count);
        ends.add_uninitialised(count);
    }
    void add_defaulted(size_type const count) {
        starts.add_defaulted(count);
        ends.add_defaulted(count);
    }
    void remove_at_swap(size_type const index, size_type const count) {
        validate_array_sizes();
        starts.remove_at_swap(index, count);
        ends.remove_at_swap(index, count);
    }
    void set(size_type const index, Vector3f const start, Vector3f const end) {
        get_view().set(index, start, end);
    }
    auto add(Vector3f const start, Vector3f const end) -> size_type {
        auto const index{num()};
        add_defaulted(1);
        set(index, start, end);
        return index;
    }
    template <typename Other>
    void copy_element(size_type const dst_index, Other const& other, size_type const src_index) {
        starts.copy_element(dst_index, other.starts, src_index);
        ends.copy_element(dst_index, other.ends, src_index);
    }
    template <typename Other>
    void copy_elements(size_type const dst_index,
                       Other const& other,
                       size_type const src_index,
                       size_type const count) {
        for (size_type i{}; i < count; ++i) {
            copy_element(dst_index + i, other, src_index + i);
        }
    }
    void apply_permutation(std::span<std::int32_t> const indices) {
        validate_array_sizes();
        starts.apply_permutation(indices);
        ends.apply_permutation(indices);
    }
    [[nodiscard]] auto get_view() -> View { return {starts.get_view(), ends.get_view()}; }
    [[nodiscard]] auto get_view() const -> ConstView {
        return {starts.get_const_view(), ends.get_const_view()};
    }
    [[nodiscard]] auto get_const_view() const -> ConstView { return get_view(); }
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) -> View {
        return get_view().slice(offset, count);
    }
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const -> ConstView {
        return get_view().slice(offset, count);
    }
    [[nodiscard]] auto get_const_view(size_type const offset, size_type const count) const
        -> ConstView {
        return get_const_view().slice(offset, count);
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) -> View {
        return get_view(offset, count);
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) const -> ConstView {
        return get_const_view(offset, count);
    }
    [[nodiscard]] auto left(size_type const count) -> View { return slice(0, count); }
    [[nodiscard]] auto right(size_type const count) -> View { return slice(num() - count, count); }
    [[nodiscard]] auto left(size_type const count) const -> ConstView { return slice(0, count); }
    [[nodiscard]] auto right(size_type const count) const -> ConstView {
        return slice(num() - count, count);
    }
};
}
