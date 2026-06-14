#pragma once

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotators.h>

#include <HAL/Platform.h>
#include <Math/UnrealMathUtility.h>

#include <concepts>
#include <initializer_list>

namespace ml {
template <typename T>
concept is_rot3f = requires(T const& vecs) {
    { vecs.pitches[0] } -> std::convertible_to<float>;
    { vecs.yaws[0] } -> std::convertible_to<float>;
    { vecs.rolls[0] } -> std::convertible_to<float>;
    { vecs.num() } -> std::convertible_to<int32>;
};

// Comparison
[[nodiscard]]
inline auto SANDBOXCORE_API almost_equal(FRotatorsf const& a,
                                         FRotatorsf const& b,
                                         float const tolerance = KINDA_SMALL_NUMBER) -> bool {
    auto const n{a.num()};
    if (n != b.num()) {
        return false;
    }

    if (n == 0) {
        return true;
    }

    return ml::kernel::almost_equal(a.pitches.GetData(), b.pitches.GetData(), n, tolerance) &&
           ml::kernel::almost_equal(a.yaws.GetData(), b.yaws.GetData(), n, tolerance) &&
           ml::kernel::almost_equal(a.rolls.GetData(), b.rolls.GetData(), n, tolerance);
}

// Construction
[[nodiscard]]
inline auto SANDBOXCORE_API make_rotatorsf(std::initializer_list<float> const pitches,
                                           std::initializer_list<float> const yaws,
                                           std::initializer_list<float> const rolls) -> FRotatorsf {
    auto const n{pitches.size()};
    check(n == yaws.size());
    check(n == rolls.size());

    return {
        .pitches = pitches,
        .yaws = yaws,
        .rolls = rolls,
    };
}
[[nodiscard]]
inline auto SANDBOXCORE_API make_rotatorsf(TArray<float> pitches,
                                           TArray<float> yaws,
                                           TArray<float> rolls) -> FRotatorsf {
    auto const n{pitches.Num()};
    check(n == yaws.Num());
    check(n == rolls.Num());

    return {
        .pitches = MoveTemp(pitches),
        .yaws = MoveTemp(yaws),
        .rolls = MoveTemp(rolls),
    };
}

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
inline void append(FRotatorsf& rotators, FVector3f const& direction_vector) {
    append(rotators, direction_vector.Rotation());
}

// Extension
template <is_rot3f Rot3f>
inline void append_from(FRotatorsf& rotators, Rot3f const& to_append) {
    auto const n_base{rotators.num()};
    auto const n_to_append{to_append.num()};

    ml::add_uninitialised(rotators, n_to_append);
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
inline auto get_vector3f(FRotatorsf::ConstView const rotators, int32 const i) -> FVector3f {
    return FRotator3f{rotators.pitches[i], rotators.yaws[i], rotators.rolls[i]}.Vector();
}
}
