#include "CoreMinimal.h"
#include "benchmark_cli_args.h"
#include "TestHarness.h"

#include <catch2/benchmark/catch_benchmark.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace ml::ray_aabb_benchmark {

constexpr int32 axis_count{3};
constexpr int32 octant_count{8};
constexpr float no_hit{std::numeric_limits<float>::infinity()};

struct FAABBs {
    std::array<TArray<float>, axis_count> mins;
    std::array<TArray<float>, axis_count> maxes;

    auto num() const -> int32 { return mins[0].Num(); }
};

struct FRay {
    std::array<float, axis_count> start;
    std::array<float, axis_count> delta;
};

struct FRayData {
    std::array<float, axis_count> start;
    std::array<float, axis_count> delta;
    std::array<float, axis_count> inverse_delta;
};

struct FOrderedAABBs {
    std::array<float const*, axis_count> near_bounds;
    std::array<float const*, axis_count> far_bounds;
    std::array<float, axis_count> near_expansion;
    std::array<float, axis_count> far_expansion;
};

// SpaceGame uses fast floating-point semantics in optimized builds.
#pragma float_control(precise, off, push)

auto make_ray_data(FRay const& ray) -> FRayData {
    FRayData result{.start = ray.start, .delta = ray.delta};
    for (int32 axis{}; axis < axis_count; ++axis) {
        if (ray.delta[axis] != 0.0f) {
            result.inverse_delta[axis] = 1.0f / ray.delta[axis];
        }
    }
    return result;
}

auto make_ordered_aabbs(FAABBs const& aabbs, FRayData const& ray, float const expansion) -> FOrderedAABBs {
    FOrderedAABBs result;
    for (int32 axis{}; axis < axis_count; ++axis) {
        auto const negative{ray.delta[axis] < 0.0f};
        result.near_bounds[axis] = negative ? aabbs.maxes[axis].GetData() : aabbs.mins[axis].GetData();
        result.far_bounds[axis] = negative ? aabbs.mins[axis].GetData() : aabbs.maxes[axis].GetData();
        result.near_expansion[axis] = negative ? expansion : -expansion;
        result.far_expansion[axis] = -result.near_expansion[axis];
    }
    return result;
}

template <int32 SignMask>
auto make_specialized_ordered_aabbs(FAABBs const& aabbs, float const expansion) -> FOrderedAABBs {
    FOrderedAABBs result;
    for (int32 axis{}; axis < axis_count; ++axis) {
        auto const negative{(SignMask & (1 << axis)) != 0};
        result.near_bounds[axis] = negative ? aabbs.maxes[axis].GetData() : aabbs.mins[axis].GetData();
        result.far_bounds[axis] = negative ? aabbs.mins[axis].GetData() : aabbs.maxes[axis].GetData();
        result.near_expansion[axis] = negative ? expansion : -expansion;
        result.far_expansion[axis] = -result.near_expansion[axis];
    }
    return result;
}

auto get_sign_mask(FRayData const& ray) -> int32 {
    int32 result{};
    for (int32 axis{}; axis < axis_count; ++axis) {
        if (ray.delta[axis] < 0.0f) {
            result |= 1 << axis;
        }
    }
    return result;
}

FORCEINLINE auto trace_current(FAABBs const& aabbs, int32 const index, FRayData const& ray, float const expansion) -> float {
    float t_min{};
    float t_max{1.0f};
    for (int32 axis{}; axis < axis_count; ++axis) {
        auto const slab_min{aabbs.mins[axis][index] - expansion};
        auto const slab_max{aabbs.maxes[axis][index] + expansion};
        auto const start{ray.start[axis]};

        if (ray.delta[axis] == 0.0f) {
            if (start < slab_min || start > slab_max) {
                return no_hit;
            }
            continue;
        }

        auto t1{(slab_min - start) * ray.inverse_delta[axis]};
        auto t2{(slab_max - start) * ray.inverse_delta[axis]};
        if (t1 > t2) {
            Swap(t1, t2);
        }

        t_min = std::max(t_min, t1);
        t_max = std::min(t_max, t2);
        if (t_min > t_max) {
            return no_hit;
        }
    }
    return t_min;
}

FORCEINLINE auto
    trace_ordered(FOrderedAABBs const& aabbs, FAABBs const& original_aabbs, int32 const index, FRayData const& ray, float const expansion)
        -> float {
    float t_min{};
    float t_max{1.0f};
    for (int32 axis{}; axis < axis_count; ++axis) {
        auto const start{ray.start[axis]};
        if (ray.delta[axis] == 0.0f) {
            auto const slab_min{original_aabbs.mins[axis][index] - expansion};
            auto const slab_max{original_aabbs.maxes[axis][index] + expansion};
            if (start < slab_min || start > slab_max) {
                return no_hit;
            }
            continue;
        }

        auto const near_t{(aabbs.near_bounds[axis][index] + aabbs.near_expansion[axis] - start) * ray.inverse_delta[axis]};
        auto const far_t{(aabbs.far_bounds[axis][index] + aabbs.far_expansion[axis] - start) * ray.inverse_delta[axis]};
        t_min = std::max(t_min, near_t);
        t_max = std::min(t_max, far_t);
        if (t_min > t_max) {
            return no_hit;
        }
    }
    return t_min;
}

FORCENOINLINE auto closest_current(FAABBs const& aabbs, FRay const& ray, float const expansion) -> float {
    auto const ray_data{make_ray_data(ray)};
    auto nearest_t{no_hit};
    auto const count{aabbs.num()};
    for (int32 index{}; index < count; ++index) {
        nearest_t = std::min(nearest_t, trace_current(aabbs, index, ray_data, expansion));
    }
    return nearest_t;
}

FORCENOINLINE auto closest_ordered(FAABBs const& aabbs, FRay const& ray, float const expansion) -> float {
    auto const ray_data{make_ray_data(ray)};
    auto const ordered_aabbs{make_ordered_aabbs(aabbs, ray_data, expansion)};
    auto nearest_t{no_hit};
    auto const count{aabbs.num()};
    for (int32 index{}; index < count; ++index) {
        nearest_t = std::min(nearest_t, trace_ordered(ordered_aabbs, aabbs, index, ray_data, expansion));
    }
    return nearest_t;
}

template <int32 SignMask>
FORCENOINLINE auto closest_specialized(FAABBs const& aabbs, FRayData const& ray, float const expansion) -> float {
    auto const ordered_aabbs{make_specialized_ordered_aabbs<SignMask>(aabbs, expansion)};
    auto nearest_t{no_hit};
    auto const count{aabbs.num()};
    for (int32 index{}; index < count; ++index) {
        nearest_t = std::min(nearest_t, trace_ordered(ordered_aabbs, aabbs, index, ray, expansion));
    }
    return nearest_t;
}

FORCENOINLINE auto closest_specialized_dispatch(FAABBs const& aabbs, FRay const& ray, float const expansion) -> float {
    auto const ray_data{make_ray_data(ray)};
    switch (get_sign_mask(ray_data)) {
        case 0:
            return closest_specialized<0>(aabbs, ray_data, expansion);
        case 1:
            return closest_specialized<1>(aabbs, ray_data, expansion);
        case 2:
            return closest_specialized<2>(aabbs, ray_data, expansion);
        case 3:
            return closest_specialized<3>(aabbs, ray_data, expansion);
        case 4:
            return closest_specialized<4>(aabbs, ray_data, expansion);
        case 5:
            return closest_specialized<5>(aabbs, ray_data, expansion);
        case 6:
            return closest_specialized<6>(aabbs, ray_data, expansion);
        case 7:
            return closest_specialized<7>(aabbs, ray_data, expansion);
        default:
            checkNoEntry();
            return no_hit;
    }
}

FORCENOINLINE void trace_batch_current(FAABBs const& aabbs, FRay const& ray, TArrayView<float> const out_t_values) {
    auto const ray_data{make_ray_data(ray)};
    auto const count{aabbs.num()};
    check(out_t_values.Num() == count);

    auto const* RESTRICT min_x{aabbs.mins[0].GetData()};
    auto const* RESTRICT min_y{aabbs.mins[1].GetData()};
    auto const* RESTRICT min_z{aabbs.mins[2].GetData()};
    auto const* RESTRICT max_x{aabbs.maxes[0].GetData()};
    auto const* RESTRICT max_y{aabbs.maxes[1].GetData()};
    auto const* RESTRICT max_z{aabbs.maxes[2].GetData()};
    auto* RESTRICT out{out_t_values.GetData()};

    for (int32 index{}; index < count; ++index) {
        auto const tx1{(min_x[index] - ray_data.start[0]) * ray_data.inverse_delta[0]};
        auto const tx2{(max_x[index] - ray_data.start[0]) * ray_data.inverse_delta[0]};
        auto const ty1{(min_y[index] - ray_data.start[1]) * ray_data.inverse_delta[1]};
        auto const ty2{(max_y[index] - ray_data.start[1]) * ray_data.inverse_delta[1]};
        auto const tz1{(min_z[index] - ray_data.start[2]) * ray_data.inverse_delta[2]};
        auto const tz2{(max_z[index] - ray_data.start[2]) * ray_data.inverse_delta[2]};

        auto const t_min{std::max(0.0f, std::max(std::min(tx1, tx2), std::max(std::min(ty1, ty2), std::min(tz1, tz2))))};
        auto const t_max{std::min(1.0f, std::min(std::max(tx1, tx2), std::min(std::max(ty1, ty2), std::max(tz1, tz2))))};
        out[index] = t_min <= t_max ? t_min : no_hit;
    }
}

FORCEINLINE void trace_batch_ordered_impl(FOrderedAABBs const& aabbs, FRayData const& ray, int32 const count, float* RESTRICT const out) {
    auto const* RESTRICT near_x{aabbs.near_bounds[0]};
    auto const* RESTRICT near_y{aabbs.near_bounds[1]};
    auto const* RESTRICT near_z{aabbs.near_bounds[2]};
    auto const* RESTRICT far_x{aabbs.far_bounds[0]};
    auto const* RESTRICT far_y{aabbs.far_bounds[1]};
    auto const* RESTRICT far_z{aabbs.far_bounds[2]};

    for (int32 index{}; index < count; ++index) {
        auto const tx_near{(near_x[index] - ray.start[0]) * ray.inverse_delta[0]};
        auto const tx_far{(far_x[index] - ray.start[0]) * ray.inverse_delta[0]};
        auto const ty_near{(near_y[index] - ray.start[1]) * ray.inverse_delta[1]};
        auto const ty_far{(far_y[index] - ray.start[1]) * ray.inverse_delta[1]};
        auto const tz_near{(near_z[index] - ray.start[2]) * ray.inverse_delta[2]};
        auto const tz_far{(far_z[index] - ray.start[2]) * ray.inverse_delta[2]};

        auto const t_min{std::max(0.0f, std::max(tx_near, std::max(ty_near, tz_near)))};
        auto const t_max{std::min(1.0f, std::min(tx_far, std::min(ty_far, tz_far)))};
        out[index] = t_min <= t_max ? t_min : no_hit;
    }
}

FORCENOINLINE void trace_batch_ordered(FAABBs const& aabbs, FRay const& ray, TArrayView<float> const out_t_values) {
    auto const ray_data{make_ray_data(ray)};
    auto const ordered_aabbs{make_ordered_aabbs(aabbs, ray_data, 0.0f)};
    auto const count{aabbs.num()};
    check(out_t_values.Num() == count);
    trace_batch_ordered_impl(ordered_aabbs, ray_data, count, out_t_values.GetData());
}

template <int32 SignMask>
FORCENOINLINE void trace_batch_specialized(FAABBs const& aabbs, FRayData const& ray, TArrayView<float> const out_t_values) {
    auto const ordered_aabbs{make_specialized_ordered_aabbs<SignMask>(aabbs, 0.0f)};
    trace_batch_ordered_impl(ordered_aabbs, ray, aabbs.num(), out_t_values.GetData());
}

FORCENOINLINE void trace_batch_specialized_dispatch(FAABBs const& aabbs, FRay const& ray, TArrayView<float> const out_t_values) {
    auto const ray_data{make_ray_data(ray)};
    switch (get_sign_mask(ray_data)) {
        case 0:
            trace_batch_specialized<0>(aabbs, ray_data, out_t_values);
            return;
        case 1:
            trace_batch_specialized<1>(aabbs, ray_data, out_t_values);
            return;
        case 2:
            trace_batch_specialized<2>(aabbs, ray_data, out_t_values);
            return;
        case 3:
            trace_batch_specialized<3>(aabbs, ray_data, out_t_values);
            return;
        case 4:
            trace_batch_specialized<4>(aabbs, ray_data, out_t_values);
            return;
        case 5:
            trace_batch_specialized<5>(aabbs, ray_data, out_t_values);
            return;
        case 6:
            trace_batch_specialized<6>(aabbs, ray_data, out_t_values);
            return;
        case 7:
            trace_batch_specialized<7>(aabbs, ray_data, out_t_values);
            return;
        default:
            checkNoEntry();
    }
}

#pragma float_control(pop)

auto make_aabbs(int32 const count) -> FAABBs {
    FAABBs result;
    for (int32 axis{}; axis < axis_count; ++axis) {
        result.mins[axis].SetNumUninitialized(count);
        result.maxes[axis].SetNumUninitialized(count);
    }

    for (int32 index{}; index < count; ++index) {
        auto const position{static_cast<float>((index * 37) % 241) - 120.0f};
        FVector3f centre{position, position, position};
        switch (index % 4) {
            case 1:
                centre.X += 400.0f;
                break;
            case 2:
                centre.Y += 400.0f;
                break;
            case 3:
                centre.Z += 400.0f;
                break;
            default:
                break;
        }

        auto const half_extent{2.0f + static_cast<float>(index % 7)};
        result.mins[0][index] = centre.X - half_extent;
        result.mins[1][index] = centre.Y - half_extent;
        result.mins[2][index] = centre.Z - half_extent;
        result.maxes[0][index] = centre.X + half_extent;
        result.maxes[1][index] = centre.Y + half_extent;
        result.maxes[2][index] = centre.Z + half_extent;
    }
    return result;
}

auto make_octant_rays() -> std::array<FRay, octant_count> {
    std::array<FRay, octant_count> result;
    for (int32 sign_mask{}; sign_mask < octant_count; ++sign_mask) {
        auto& ray{result[sign_mask]};
        for (int32 axis{}; axis < axis_count; ++axis) {
            auto const direction{(sign_mask & (1 << axis)) != 0 ? -1.0f : 1.0f};
            ray.start[axis] = -200.0f * direction;
            ray.delta[axis] = 400.0f * direction;
        }
    }
    return result;
}

auto values_match(float const lhs, float const rhs) -> bool {
    return lhs == rhs || (std::isinf(lhs) && std::isinf(rhs));
}

void check_scalar_variants(FAABBs const& aabbs, std::array<FRay, octant_count> const& rays, float const expansion) {
    for (auto const& ray : rays) {
        auto const expected{closest_current(aabbs, ray, expansion)};
        CHECK(values_match(expected, closest_ordered(aabbs, ray, expansion)));
        CHECK(values_match(expected, closest_specialized_dispatch(aabbs, ray, expansion)));
    }
}

void check_batch_variants(FAABBs const& aabbs, std::array<FRay, octant_count> const& rays) {
    TArray<float> expected;
    TArray<float> actual;
    expected.SetNumUninitialized(aabbs.num());
    actual.SetNumUninitialized(aabbs.num());

    for (auto const& ray : rays) {
        trace_batch_current(aabbs, ray, expected);
        trace_batch_ordered(aabbs, ray, actual);
        for (int32 index{}; index < aabbs.num(); ++index) {
            CHECK(values_match(expected[index], actual[index]));
        }

        trace_batch_specialized_dispatch(aabbs, ray, actual);
        for (int32 index{}; index < aabbs.num(); ++index) {
            CHECK(values_match(expected[index], actual[index]));
        }
    }
}

template <typename TKernel>
FORCENOINLINE auto run_scalar_rays(FAABBs const& aabbs, std::array<FRay, octant_count> const& rays, float const expansion, TKernel&& kernel)
    -> float {
    float result{};
    for (auto const& ray : rays) {
        result += kernel(aabbs, ray, expansion);
    }
    return result;
}

template <typename TKernel>
FORCENOINLINE auto
    run_batch_rays(FAABBs const& aabbs, std::array<FRay, octant_count> const& rays, TArrayView<float> const out_t_values, TKernel&& kernel)
        -> float {
    for (auto const& ray : rays) {
        kernel(aabbs, ray, out_t_values);
    }
    return out_t_values[aabbs.num() / 2];
}

auto get_counts() -> TArray<int32> {
    auto const benchmark_cli_args{get_benchmark_cli_args()};
    if (benchmark_cli_args.benchmark_entities) {
        return {*benchmark_cli_args.benchmark_entities};
    }
    return {1, 4, 16, 64, 128, 2048};
}

TEST_CASE("SandboxCore.RayAABB.Scalar", "[benchmark]") {
    auto const rays{make_octant_rays()};
    for (auto const count : get_counts()) {
        REQUIRE(count > 0);
        auto const aabbs{make_aabbs(count)};
        check_scalar_variants(aabbs, rays, 0.0f);
        check_scalar_variants(aabbs, rays, 3.0f);

        for (auto const expansion : {0.0f, 3.0f}) {
            auto const prefix{std::string{expansion == 0.0f ? "trace/" : "sweep/"} + std::to_string(count) + "/"};
            BENCHMARK((prefix + "current").c_str()) {
                return run_scalar_rays(aabbs, rays, expansion, closest_current);
            };
            BENCHMARK((prefix + "ordered-views").c_str()) {
                return run_scalar_rays(aabbs, rays, expansion, closest_ordered);
            };
            BENCHMARK((prefix + "sign-specialized").c_str()) {
                return run_scalar_rays(aabbs, rays, expansion, closest_specialized_dispatch);
            };
        }
    }
}

TEST_CASE("SandboxCore.RayAABB.Autovec", "[benchmark]") {
    auto const rays{make_octant_rays()};
    for (auto const count : get_counts()) {
        REQUIRE(count > 0);
        auto const aabbs{make_aabbs(count)};
        check_batch_variants(aabbs, rays);

        TArray<float> out_t_values;
        out_t_values.SetNumUninitialized(count);
        auto const prefix{std::to_string(count) + "/"};
        BENCHMARK((prefix + "current").c_str()) {
            return run_batch_rays(aabbs, rays, out_t_values, trace_batch_current);
        };
        BENCHMARK((prefix + "ordered-views").c_str()) {
            return run_batch_rays(aabbs, rays, out_t_values, trace_batch_ordered);
        };
        BENCHMARK((prefix + "sign-specialized").c_str()) {
            return run_batch_rays(aabbs, rays, out_t_values, trace_batch_specialized_dispatch);
        };
    }
}

} // namespace ml::ray_aabb_benchmark
