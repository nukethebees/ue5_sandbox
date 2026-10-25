#pragma once

#include <ioj/structural_diff.h>
#include <ioj/transform3d.h>

namespace ioj {
template <typename T, std::size_t N>
void diff_members(Vector<T, N> const& expected, Vector<T, N> const& actual, DiffContext& context) {
    diff_value(expected.elements, actual.elements, context);
}

template <std::floating_point T>
void
    diff_members(Quaternion<T> const& expected, Quaternion<T> const& actual, DiffContext& context) {
    diff_value(expected.components, actual.components, context);
}

template <std::floating_point T>
void diff_members(Rotator<T> const& expected, Rotator<T> const& actual, DiffContext& context) {
    context.diff("pitch", expected.pitch, actual.pitch);
    context.diff("yaw", expected.yaw, actual.yaw);
    context.diff("roll", expected.roll, actual.roll);
}

inline void
    diff_members(Transform3d const& expected, Transform3d const& actual, DiffContext& context) {
    context.diff("rotation", expected.rotation, actual.rotation);
    context.diff("location", expected.location, actual.location);
    context.diff("scale", expected.scale, actual.scale);
}
}
