#pragma once

#include <ioj/sim/rotator_types.h>
#include <ioj/sim/vector_types.h>

#include <concepts>
#include <cstdint>

namespace ioj::sim {
template <typename View>
concept VectorColumns = requires(View const& view) {
    view.xs();
    view.ys();
    view.zs();
    view.num();
};

template <VectorColumns View>
auto vector_at(View const& view, std::int32_t const index) -> Vector3f {
    return HMM_V3(view.xs()[index], view.ys()[index], view.zs()[index]);
}

template <VectorColumns View>
void
    set_vector(View&& view, std::int32_t const index, float const x, float const y, float const z) {
    view.xs()[index] = x;
    view.ys()[index] = y;
    view.zs()[index] = z;
}

template <VectorColumns View>
void set_vector(View&& view, std::int32_t const index, Vector3f const value) {
    set_vector(view, index, value.X, value.Y, value.Z);
}

template <typename View>
    requires requires(View const& view) {
        view.pitches();
        view.yaws();
        view.rolls();
    }
auto rotation_at(View const& view, std::int32_t const index) -> Rotator3f {
    return {view.pitches()[index], view.yaws()[index], view.rolls()[index]};
}

template <typename View>
    requires requires(View const& view) {
        view.pitches();
        view.yaws();
        view.rolls();
    }
void set_rotation(View&& view, std::int32_t const index, Rotator3f const value) {
    view.pitches()[index] = value.pitch;
    view.yaws()[index] = value.yaw;
    view.rolls()[index] = value.roll;
}
}
