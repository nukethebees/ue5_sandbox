#pragma once

#include <SandboxCore/frame_array.h>
#include <SandboxCore/soa_rotators.h>

#include <memory_resource>

struct FFrameRotatorsf {
    using value_type = float;
    using equivalent_type = FRotator3f;
    using size_type = int32;
    using View = FRotatorsf::View;
    using ConstView = FRotatorsf::ConstView;

    explicit FFrameRotatorsf(std::pmr::memory_resource* const resource)
        : pitches{resource}
        , yaws{resource}
        , rolls{resource} {}

    FFrameRotatorsf(FFrameRotatorsf const&) = delete;
    FFrameRotatorsf(FFrameRotatorsf&&) = delete;
    auto operator=(FFrameRotatorsf const&) -> FFrameRotatorsf& = delete;
    auto operator=(FFrameRotatorsf&&) -> FFrameRotatorsf& = delete;
    ~FFrameRotatorsf() = default;

    void reserve(int32 const count) {
        pitches.reserve(count);
        yaws.reserve(count);
        rolls.reserve(count);
    }
    void clear() {
        pitches.clear();
        yaws.clear();
        rolls.clear();
    }
    void add(float const pitch, float const yaw, float const roll) {
        pitches.add(pitch);
        yaws.add(yaw);
        rolls.add(roll);
    }
    void add(FRotator3f const value) { add(value.Pitch, value.Yaw, value.Roll); }

    auto get_view() -> View { return {pitches, yaws, rolls}; }
    auto get_view() const -> ConstView { return {pitches, yaws, rolls}; }
    auto get_const_view() const -> ConstView { return {pitches, yaws, rolls}; }
    auto num() const -> int32 { return pitches.num(); }
    auto is_empty() const -> bool { return pitches.is_empty(); }
    void validate_array_sizes() const {
        check(yaws.num() == pitches.num());
        check(rolls.num() == pitches.num());
    }

    ml::TFrameArray<float> pitches;
    ml::TFrameArray<float> yaws;
    ml::TFrameArray<float> rolls;
};
