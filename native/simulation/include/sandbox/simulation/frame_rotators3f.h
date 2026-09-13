#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/rotator_types.h"
#include "sandbox/simulation/rotators3f.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation {
struct FrameRotators3f {
    explicit FrameRotators3f(std::pmr::memory_resource* resource);

    FrameRotators3f(FrameRotators3f const&) = delete;
    FrameRotators3f(FrameRotators3f&&) = delete;
    auto operator=(FrameRotators3f const&) -> FrameRotators3f& = delete;
    auto operator=(FrameRotators3f&&) -> FrameRotators3f& = delete;
    ~FrameRotators3f() = default;

    void reserve(std::int32_t count);
    void set_num(std::int32_t count);
    void clear() noexcept;
    void add(Rotator3f value);
    void set(std::int32_t index, Rotator3f value);

    [[nodiscard]] auto get_view() noexcept -> Rotators3fView;
    [[nodiscard]] auto get_const_view() const noexcept -> Rotators3fConstView;
    [[nodiscard]] auto num() const noexcept -> std::int32_t;

    FrameArray<float> pitches;
    FrameArray<float> yaws;
    FrameArray<float> rolls;
};
} // namespace ml::simulation
