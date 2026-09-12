#pragma once

#include "native_soa/storage.h"
#include "sandbox/core/soa_permutation.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/vector_types.h"
#include "sandbox/simulation/vectors3f.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

namespace ml::simulation {
struct TraceHitsView;

struct TraceHitsConstView {
    using View = TraceHitsView;
    using ConstView = TraceHitsConstView;
    using size_type = std::int32_t;

    Vectors3fConstView locations;
    std::span<FRegistryEntityHandle const> entities;
    std::span<std::int32_t const> static_geometry_indices;
    std::span<std::uint8_t const> hits;

    [[nodiscard]] auto num() const noexcept -> size_type { return locations.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const {
        locations.validate_array_sizes();
        auto const count{static_cast<std::size_t>(num())};
        ml::native_soa::require(entities.size() == count);
        ml::native_soa::require(static_geometry_indices.size() == count);
        ml::native_soa::require(hits.size() == count);
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) const
        -> TraceHitsConstView {
        ml::native_soa::require(offset >= 0 && count >= 0 && offset <= num() &&
                                count <= num() - offset);
        auto const span_offset{static_cast<std::size_t>(offset)};
        auto const span_count{static_cast<std::size_t>(count)};
        return {locations.slice(offset, count),
                entities.subspan(span_offset, span_count),
                static_geometry_indices.subspan(span_offset, span_count),
                hits.subspan(span_offset, span_count)};
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

struct TraceHitsView {
    using View = TraceHitsView;
    using ConstView = TraceHitsConstView;
    using size_type = std::int32_t;

    Vectors3fView locations;
    std::span<FRegistryEntityHandle> entities;
    std::span<std::int32_t> static_geometry_indices;
    std::span<std::uint8_t> hits;

    [[nodiscard]] auto num() const noexcept -> size_type { return locations.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const { get_const_view().validate_array_sizes(); }
    void set(size_type const index,
             Vector3f const location,
             FRegistryEntityHandle const entity,
             std::int32_t const static_geometry_index,
             std::uint8_t const hit) const {
        ml::native_soa::require(index >= 0 && index < num());
        auto const array_index{static_cast<std::size_t>(index)};
        locations.set(index, location);
        entities[array_index] = entity;
        static_geometry_indices[array_index] = static_geometry_index;
        hits[array_index] = hit;
    }
    [[nodiscard]] auto slice(size_type const offset, size_type const count) const -> View {
        ml::native_soa::require(offset >= 0 && count >= 0 && offset <= num() &&
                                count <= num() - offset);
        auto const span_offset{static_cast<std::size_t>(offset)};
        auto const span_count{static_cast<std::size_t>(count)};
        return {locations.slice(offset, count),
                entities.subspan(span_offset, span_count),
                static_geometry_indices.subspan(span_offset, span_count),
                hits.subspan(span_offset, span_count)};
    }
    [[nodiscard]] auto get_view() const -> View { return *this; }
    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const -> View {
        return slice(offset, count);
    }
    [[nodiscard]] auto get_const_view() const -> ConstView {
        return {locations.get_const_view(), entities, static_geometry_indices, hits};
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

struct TraceHits {
    using View = TraceHitsView;
    using ConstView = TraceHitsConstView;
    using size_type = std::int32_t;

    Vectors3f locations;
    ml::native_soa::Vector<FRegistryEntityHandle> entities;
    ml::native_soa::Vector<std::int32_t> static_geometry_indices;
    ml::native_soa::Vector<std::uint8_t> hits;

    [[nodiscard]] auto num() const noexcept -> size_type { return locations.num(); }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return num() == 0; }
    void validate_array_sizes() const { get_const_view().validate_array_sizes(); }
    void reserve(size_type const count) {
        ml::native_soa::require(count >= 0);
        auto const size{static_cast<std::size_t>(count)};
        locations.reserve(count);
        entities.reserve(size);
        static_geometry_indices.reserve(size);
        hits.reserve(size);
    }
    void reset() noexcept {
        locations.reset();
        entities.clear();
        static_geometry_indices.clear();
        hits.clear();
    }
    void set_num(size_type const count) {
        ml::native_soa::require(count >= 0);
        auto const size{static_cast<std::size_t>(count)};
        locations.set_num(count);
        entities.resize(size);
        static_geometry_indices.resize(size);
        hits.resize(size);
    }
    void add_uninitialised(size_type const count) {
        auto const old_num{num()};
        ml::native_soa::require(count >= 0 &&
                                count <= std::numeric_limits<size_type>::max() - old_num);
        set_num(old_num + count);
    }
    void add_defaulted(size_type const count) { add_uninitialised(count); }
    void remove_at_swap(size_type const index, size_type const count) {
        validate_array_sizes();
        auto const old_num{num()};
        ml::native_soa::require(index >= 0 && index <= old_num && count >= 0 &&
                                count <= old_num - index);
        auto const moved{std::min(count, old_num - index - count)};
        auto const source{old_num - moved};
        locations.remove_at_swap(index, count);
        for (size_type i{}; i < moved; ++i) {
            entities[index + i] = entities[source + i];
            static_geometry_indices[index + i] = static_geometry_indices[source + i];
            hits[index + i] = hits[source + i];
        }
        auto const new_size{static_cast<std::size_t>(old_num - count)};
        entities.resize(new_size);
        static_geometry_indices.resize(new_size);
        hits.resize(new_size);
    }
    void set(size_type const index,
             Vector3f const location,
             FRegistryEntityHandle const entity,
             std::int32_t const static_geometry_index,
             std::uint8_t const hit) {
        get_view().set(index, location, entity, static_geometry_index, hit);
    }
    auto add(Vector3f const location,
             FRegistryEntityHandle const entity,
             std::int32_t const static_geometry_index,
             std::uint8_t const hit) -> size_type {
        auto const index{num()};
        add_defaulted(1);
        set(index, location, entity, static_geometry_index, hit);
        return index;
    }
    template <typename Other>
    void copy_element(size_type const dst_index, Other const& other, size_type const src_index) {
        auto const dst{static_cast<std::size_t>(dst_index)};
        auto const src{static_cast<std::size_t>(src_index)};
        locations.copy_element(dst_index, other.locations, src_index);
        entities[dst] = other.entities[src];
        static_geometry_indices[dst] = other.static_geometry_indices[src];
        hits[dst] = other.hits[src];
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
        locations.apply_permutation(indices);
        ml::apply_permutation(std::span{entities}, indices);
        ml::apply_permutation(std::span{static_geometry_indices}, indices);
        ml::apply_permutation(std::span{hits}, indices);
    }
    [[nodiscard]] auto get_view() -> View {
        return {locations.get_view(), entities, static_geometry_indices, hits};
    }
    [[nodiscard]] auto get_view() const -> ConstView {
        return {locations.get_const_view(), entities, static_geometry_indices, hits};
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
