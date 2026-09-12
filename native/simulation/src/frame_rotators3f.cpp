#include "sandbox/simulation/frame_rotators3f.h"

namespace ml::simulation {
FrameRotators3f::FrameRotators3f(std::pmr::memory_resource* const resource)
    : pitches{resource}
    , yaws{resource}
    , rolls{resource} {}

void FrameRotators3f::reserve(std::int32_t const count) {
    pitches.reserve(count);
    yaws.reserve(count);
    rolls.reserve(count);
}
void FrameRotators3f::set_num(std::int32_t const count) {
    pitches.set_num(count);
    yaws.set_num(count);
    rolls.set_num(count);
}
void FrameRotators3f::clear() noexcept {
    pitches.clear();
    yaws.clear();
    rolls.clear();
}
void FrameRotators3f::add(Rotator3f const value) {
    pitches.add(value.pitch);
    yaws.add(value.yaw);
    rolls.add(value.roll);
}
void FrameRotators3f::set(std::int32_t const index, Rotator3f const value) {
    pitches[index] = value.pitch;
    yaws[index] = value.yaw;
    rolls[index] = value.roll;
}
auto FrameRotators3f::get_view() noexcept -> Rotators3fView {
    return {pitches, yaws, rolls};
}
auto FrameRotators3f::get_const_view() const noexcept -> Rotators3fConstView {
    return {pitches, yaws, rolls};
}
auto FrameRotators3f::num() const noexcept -> std::int32_t {
    return pitches.num();
}
} // namespace ml::simulation
