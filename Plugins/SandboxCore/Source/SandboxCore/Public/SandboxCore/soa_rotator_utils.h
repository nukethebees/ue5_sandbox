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

// Assignment
inline void
    assign(FRotatorsf& r, int32 const i, float const pitch, float const yaw, float const roll) {
    r.pitches[i] = pitch;
    r.yaws[i] = yaw;
    r.rolls[i] = roll;
}
inline void assign(FRotatorsf& r, int32 const i, FRotator3f const& rot) {
    r.pitches[i] = rot.Pitch;
    r.yaws[i] = rot.Yaw;
    r.rolls[i] = rot.Roll;
}
inline void assign(FRotatorsf& r, int32 const i, FRotator const& rot) {
    r.pitches[i] = static_cast<float>(rot.Pitch);
    r.yaws[i] = static_cast<float>(rot.Yaw);
    r.rolls[i] = static_cast<float>(rot.Roll);
}

inline void append(FRotatorsf& rotators, float const pitch, float const yaw, float const roll) {
    rotators.pitches.Add(pitch);
    rotators.yaws.Add(yaw);
    rotators.rolls.Add(roll);
}
inline void append(FRotatorsf& rotators, FRotator3f const& to_append) {
    rotators.pitches.Add(to_append.Pitch);
    rotators.yaws.Add(to_append.Yaw);
    rotators.rolls.Add(to_append.Roll);
}
inline void append(FRotatorsf& rotators, FRotator const& to_append) {
    rotators.pitches.Add(static_cast<float>(to_append.Pitch));
    rotators.yaws.Add(static_cast<float>(to_append.Yaw));
    rotators.rolls.Add(static_cast<float>(to_append.Roll));
}
inline void append(FRotatorsf& rotators, FVector const& direction_vector) {
    append(rotators, direction_vector.Rotation());
}

// Extension
template <is_rot3f Rot3f>
inline void append_from(FRotatorsf& rotators, Rot3f const& to_append) {
    auto const n_base{rotators.num()};
    auto const n_to_append{to_append.num()};

    rotators.add_uninitialized(n_to_append);
    ml::kernel::assign_from(rotators.pitches.GetData() + n_base,
                            rotators.yaws.GetData() + n_base,
                            rotators.rolls.GetData() + n_base,
                            to_append.pitches.GetData(),
                            to_append.yaws.GetData(),
                            to_append.rolls.GetData(),
                            n_to_append);
}

// Filling
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

// Conversion
inline auto get_rotator3f(FRotatorsf const& rotator, int32 const i) -> FRotator3f {
    return {rotator.pitches[i], rotator.yaws[i], rotator.rolls[i]};
}
inline auto get_rotator3d(FRotatorsf const& rotator, int32 const i) -> FRotator3d {
    return {rotator.pitches[i], rotator.yaws[i], rotator.rolls[i]};
}
inline auto get_rotator3d(FRotatorsf::ConstView const& rotator, int32 const i) -> FRotator3d {
    return {rotator.pitches[i], rotator.yaws[i], rotator.rolls[i]};
}
inline auto get_vector3f(FRotatorsf const& rotator, int32 const i) -> FVector3f {
    return get_rotator3f(rotator, i).Vector();
}
}
