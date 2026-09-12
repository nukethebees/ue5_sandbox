#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/vector_types.h"
#include "sandbox/simulation/vectors3f.h"

#include <memory_resource>

namespace ml::simulation {
struct FrameVectors3f {
    using size_type = std::int32_t;

    explicit FrameVectors3f(std::pmr::memory_resource* resource);

    FrameVectors3f(FrameVectors3f const&) = delete;
    FrameVectors3f(FrameVectors3f&&) = delete;
    auto operator=(FrameVectors3f const&) -> FrameVectors3f& = delete;
    auto operator=(FrameVectors3f&&) -> FrameVectors3f& = delete;
    ~FrameVectors3f() = default;

    void reserve(size_type count);
    void set_num(size_type count);
    void clear() noexcept;
    void add(Vector3f value);
    void set(size_type index, Vector3f value);

    [[nodiscard]] auto get_view() noexcept -> Vectors3fView;
    [[nodiscard]] auto get_view() const noexcept -> Vectors3fConstView;
    [[nodiscard]] auto get_const_view() const noexcept -> Vectors3fConstView;
    [[nodiscard]] auto num() const noexcept -> size_type;
    [[nodiscard]] auto is_empty() const noexcept -> bool;
    void validate_array_sizes() const;

    ml::FrameArray<float> xs;
    ml::FrameArray<float> ys;
    ml::FrameArray<float> zs;
};
} // namespace ml::simulation
