#pragma once

#include <SandboxCore/frame_array.h>
#include <SandboxCore/soa_vectors_3f.h>

#include <memory_resource>

struct FFrameVectors3f {
    using value_type = float;
    using equivalent_type = FVector3f;
    using size_type = int32;
    using View = FVectors3f::View;
    using ConstView = FVectors3f::ConstView;

    explicit FFrameVectors3f(std::pmr::memory_resource* const resource)
        : xs{resource}
        , ys{resource}
        , zs{resource} {}

    FFrameVectors3f(FFrameVectors3f const&) = delete;
    FFrameVectors3f(FFrameVectors3f&&) = delete;
    auto operator=(FFrameVectors3f const&) -> FFrameVectors3f& = delete;
    auto operator=(FFrameVectors3f&&) -> FFrameVectors3f& = delete;
    ~FFrameVectors3f() = default;

    void reserve(int32 const count) {
        xs.reserve(count);
        ys.reserve(count);
        zs.reserve(count);
    }
    void clear() {
        xs.clear();
        ys.clear();
        zs.clear();
    }
    void add(float const x, float const y, float const z) {
        xs.add(x);
        ys.add(y);
        zs.add(z);
    }
    void add(FVector3f const value) { add(value.X, value.Y, value.Z); }

    auto get_view() -> View { return {xs, ys, zs}; }
    auto get_view() const -> ConstView { return {xs, ys, zs}; }
    auto get_const_view() const -> ConstView { return {xs, ys, zs}; }
    auto num() const -> int32 { return xs.num(); }
    auto is_empty() const -> bool { return xs.is_empty(); }
    void validate_array_sizes() const {
        check(ys.num() == xs.num());
        check(zs.num() == xs.num());
    }

    ml::TFrameArray<float> xs;
    ml::TFrameArray<float> ys;
    ml::TFrameArray<float> zs;
};
