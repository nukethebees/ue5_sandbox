#include "ioj/sim/frame_vectors3f.h"

namespace ioj::sim {
FrameVectors3f::FrameVectors3f(ml::FrameScratch& scratch)
    : xs_{&scratch}
    , ys_{&scratch}
    , zs_{&scratch} {}

void FrameVectors3f::reserve(size_type const count) {
    xs_.reserve(count);
    ys_.reserve(count);
    zs_.reserve(count);
}
void FrameVectors3f::set_num(size_type const count) {
    xs_.set_num(count);
    ys_.set_num(count);
    zs_.set_num(count);
}
void FrameVectors3f::clear() noexcept {
    xs_.clear();
    ys_.clear();
    zs_.clear();
}
void FrameVectors3f::add(Vector3f const value) {
    xs_.add(value.X);
    ys_.add(value.Y);
    zs_.add(value.Z);
}
void FrameVectors3f::set(size_type const index, Vector3f const value) {
    xs_[index] = value.X;
    ys_[index] = value.Y;
    zs_[index] = value.Z;
}

auto FrameVectors3f::get_view() noexcept -> Vectors3fView {
    return {xs_.data(), ys_.data(), zs_.data(), num()};
}
auto FrameVectors3f::get_view() const noexcept -> Vectors3fConstView {
    return {xs_.data(), ys_.data(), zs_.data(), num()};
}
auto FrameVectors3f::get_const_view() const noexcept -> Vectors3fConstView {
    return get_view();
}
auto FrameVectors3f::num() const noexcept -> size_type {
    return xs_.num();
}
auto FrameVectors3f::is_empty() const noexcept -> bool {
    return xs_.is_empty();
}
} // namespace ioj::sim
