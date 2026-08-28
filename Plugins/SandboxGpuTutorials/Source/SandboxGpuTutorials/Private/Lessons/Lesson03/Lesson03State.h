#pragma once

#include "Math/Color.h"

struct FLesson03State {
    static constexpr float minimum_ring_thickness{0.002f};
    static constexpr float maximum_ring_thickness{0.080f};
    static constexpr float maximum_animation_speed{3.0f};
    static constexpr float maximum_pulse_amount{0.25f};

    float elapsed_time{0.0f};
    float ring_thickness{0.018f};
    float animation_speed{1.0f};
    float pulse_amount{0.08f};
    FLinearColor primary_color{0.10f, 0.85f, 1.0f, 1.0f};
    bool animation_enabled{true};

    void advance(float delta_time);
    void set_ring_thickness_from_slider(float value);
    void set_animation_speed_from_slider(float value);
    void set_pulse_amount_from_slider(float value);

    [[nodiscard]] auto ring_thickness_slider_value() const -> float;
    [[nodiscard]] auto animation_speed_slider_value() const -> float;
    [[nodiscard]] auto pulse_amount_slider_value() const -> float;
};
