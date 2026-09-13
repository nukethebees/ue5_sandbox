#include "sandbox/simulation/frame_vectors3f.h"

namespace ml::simulation {
FrameVectors3f::FrameVectors3f(std::pmr::memory_resource* const resource)
    : xs{resource}
    , ys{resource}
    , zs{resource} {}

void FrameVectors3f::reserve(size_type const count) {
    xs.reserve(count);
    ys.reserve(count);
    zs.reserve(count);
}
void FrameVectors3f::set_num(size_type const count) {
    xs.set_num(count);
    ys.set_num(count);
    zs.set_num(count);
}
void FrameVectors3f::clear() noexcept {
    xs.clear();
    ys.clear();
    zs.clear();
}
void FrameVectors3f::add(Vector3f const value) {
    xs.add(value.X);
    ys.add(value.Y);
    zs.add(value.Z);
}
void FrameVectors3f::set(size_type const index, Vector3f const value) {
    xs[index] = value.X;
    ys[index] = value.Y;
    zs[index] = value.Z;
}

auto FrameVectors3f::get_view() noexcept -> Vectors3fView {
    return {xs, ys, zs};
}
auto FrameVectors3f::get_view() const noexcept -> Vectors3fConstView {
    return {xs, ys, zs};
}
auto FrameVectors3f::get_const_view() const noexcept -> Vectors3fConstView {
    return get_view();
}
auto FrameVectors3f::num() const noexcept -> size_type {
    return xs.num();
}
auto FrameVectors3f::is_empty() const noexcept -> bool {
    return xs.is_empty();
}
void FrameVectors3f::validate_array_sizes() const {
    get_const_view().validate_array_sizes();
}
} // namespace ml::simulation
