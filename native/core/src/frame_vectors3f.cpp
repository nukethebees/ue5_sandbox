#include "ioj/frame_vectors3f.h"

namespace ioj {
FrameVectors3f::FrameVectors3f(std::pmr::memory_resource* const scratch_resource)
    : xs_{scratch_resource}
    , ys_{scratch_resource}
    , zs_{scratch_resource} {}

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
    xs_.add(value.x());
    ys_.add(value.y());
    zs_.add(value.z());
}
void FrameVectors3f::add(Vectors3fConstView const source,
                         std::span<size_type const> const indices) {
    xs_.add(source.xs(), indices);
    ys_.add(source.ys(), indices);
    zs_.add(source.zs(), indices);
}
void FrameVectors3f::set(size_type const index, Vector3f const value) {
    xs_[index] = value.x();
    ys_[index] = value.y();
    zs_[index] = value.z();
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
} // namespace ioj
