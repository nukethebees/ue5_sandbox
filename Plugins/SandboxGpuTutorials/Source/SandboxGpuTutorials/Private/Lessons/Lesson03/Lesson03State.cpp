#include "Lessons/Lesson03/Lesson03State.h"

void FLesson03State::advance(float const delta_time) {
    if (animation_enabled) {
        elapsed_time += delta_time;
    }
}

void FLesson03State::set_ring_thickness_from_slider(float const value) {
    ring_thickness =
        minimum_ring_thickness + value * (maximum_ring_thickness - minimum_ring_thickness);
}

void FLesson03State::set_animation_speed_from_slider(float const value) {
    animation_speed = value * maximum_animation_speed;
}

void FLesson03State::set_pulse_amount_from_slider(float const value) {
    pulse_amount = value * maximum_pulse_amount;
}

auto FLesson03State::ring_thickness_slider_value() const -> float {
    return (ring_thickness - minimum_ring_thickness) /
           (maximum_ring_thickness - minimum_ring_thickness);
}

auto FLesson03State::animation_speed_slider_value() const -> float {
    return animation_speed / maximum_animation_speed;
}

auto FLesson03State::pulse_amount_slider_value() const -> float {
    return pulse_amount / maximum_pulse_amount;
}
