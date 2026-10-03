#pragma once

#include "ioj/sim/rotator_types.h"
#include "ioj/sim/rotators3f.h"

#include "sandbox/core/frame_array.h"

#include <cassert>
#include <cstdint>
#include <memory_resource>

namespace ioj::sim {
struct FrameRotators3f {
    explicit FrameRotators3f(std::pmr::memory_resource* const scratch_resource);

    FrameRotators3f(FrameRotators3f const&) = delete;
    FrameRotators3f(FrameRotators3f&&) = delete;
    auto operator=(FrameRotators3f const&) -> FrameRotators3f& = delete;
    auto operator=(FrameRotators3f&&) -> FrameRotators3f& = delete;
    ~FrameRotators3f() = default;

    void reserve(std::uint32_t count);
    void set_num(std::uint32_t count);
    void clear() noexcept;
    void add(Rotator3f value);
    void set(std::uint32_t index, Rotator3f value);

    [[nodiscard]] auto get_view() noexcept -> Rotators3fView;
    [[nodiscard]] auto get_const_view() const noexcept -> Rotators3fConstView;
    [[nodiscard]] auto num() const noexcept -> std::uint32_t;

    auto pitches() -> std::span<float> { return pitches_.view(); }
    auto pitches() const -> std::span<float const> { return pitches_.view(); }
    auto yaws() -> std::span<float> { return yaws_.view(); }
    auto yaws() const -> std::span<float const> { return yaws_.view(); }
    auto rolls() -> std::span<float> { return rolls_.view(); }
    auto rolls() const -> std::span<float const> { return rolls_.view(); }
    void validate() const {
        assert(pitches_.num() == num());
        assert(yaws_.num() == num());
        assert(rolls_.num() == num());
    }
  private:
    ml::FrameArray<float> pitches_;
    ml::FrameArray<float> yaws_;
    ml::FrameArray<float> rolls_;
};
} // namespace ioj::sim
