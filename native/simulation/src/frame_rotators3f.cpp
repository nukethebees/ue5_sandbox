#include "ioj/sim/frame_rotators3f.h"

namespace ioj::sim {
FrameRotators3f::FrameRotators3f(ml::FrameScratch& scratch)
    : pitches_{&scratch}
    , yaws_{&scratch}
    , rolls_{&scratch} {}

void FrameRotators3f::reserve(std::int32_t const count) {
    pitches_.reserve(count);
    yaws_.reserve(count);
    rolls_.reserve(count);
}
void FrameRotators3f::set_num(std::int32_t const count) {
    pitches_.set_num(count);
    yaws_.set_num(count);
    rolls_.set_num(count);
}
void FrameRotators3f::clear() noexcept {
    pitches_.clear();
    yaws_.clear();
    rolls_.clear();
}
void FrameRotators3f::add(Rotator3f const value) {
    pitches_.add(value.pitch);
    yaws_.add(value.yaw);
    rolls_.add(value.roll);
}
void FrameRotators3f::set(std::int32_t const index, Rotator3f const value) {
    pitches_[index] = value.pitch;
    yaws_[index] = value.yaw;
    rolls_[index] = value.roll;
}
auto FrameRotators3f::get_view() noexcept -> Rotators3fView {
    return {pitches_, yaws_, rolls_};
}
auto FrameRotators3f::get_const_view() const noexcept -> Rotators3fConstView {
    return {pitches_, yaws_, rolls_};
}
auto FrameRotators3f::num() const noexcept -> std::int32_t {
    return pitches_.num();
}
} // namespace ioj::sim
