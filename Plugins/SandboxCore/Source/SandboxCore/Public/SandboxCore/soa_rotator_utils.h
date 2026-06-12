#pragma once

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotators.h>

#include <HAL/Platform.h>

#include <concepts>

namespace ml {
template <typename T>
concept is_rot3f = requires(T const& vecs) {
    { vecs.pitches[0] } -> std::convertible_to<float>;
    { vecs.yaws[0] } -> std::convertible_to<float>;
    { vecs.rolls[0] } -> std::convertible_to<float>;
    { vecs.num() } -> std::convertible_to<int32>;
};

template <is_rot3f Rot3f>
inline void append_from(FRotatorsf& rotators, Rot3f const& to_append) {
    auto const n_base{rotators.num()};
    auto const n_to_append{to_append.num()};

    rotators.add_uninitialized(n_to_append);
    ml::assign_from(rotators.xs.GetData() + n_base,
                    rotators.ys.GetData() + n_base,
                    rotators.zs.GetData() + n_base,
                    to_append.xs.GetData(),
                    to_append.ys.GetData(),
                    to_append.zs.GetData(),
                    n_to_append);
}

inline void append(FRotatorsf& rotators, float const pitch, float const yaw, float const roll) {
    rotators.pitches.Add(pitch);
    rotators.yaws.Add(yaw);
    rotators.rolls.Add(roll);
}

inline void fill(FRotatorsf& vector, float const value) {
    ml::kernel::fill(vector.pitches.GetData(),
                     vector.yaws.GetData(),
                     vector.rolls.GetData(),
                     value,
                     vector.num());
}
inline void fill(FRotatorsf::View vector, float const value) {
    ml::kernel::fill(vector.pitches.GetData(),
                     vector.yaws.GetData(),
                     vector.rolls.GetData(),
                     value,
                     vector.num());
}
}
