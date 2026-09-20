#pragma once

namespace ml::ui::settings_slider {
[[nodiscard]] auto normalize(float value, float minimum, float maximum) noexcept -> float;
[[nodiscard]] auto normalized_step(float step, float minimum, float maximum) noexcept -> float;
[[nodiscard]] auto denormalize(float value, float minimum, float maximum, float step) noexcept
    -> float;
}
