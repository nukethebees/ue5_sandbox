#include "sandbox/core/ui/settings_slider.h"

#include <algorithm>
#include <cmath>
namespace ml::ui::settings_slider {
namespace {
float constexpr small_number{1.0e-8f};
}

auto normalize(float const value, float const minimum, float const maximum) noexcept -> float {
    auto const range{maximum - minimum};
    return range > small_number ? std::clamp((value - minimum) / range, 0.0f, 1.0f) : 0.0f;
}

auto normalized_step(float const step, float const minimum, float const maximum) noexcept -> float {
    auto const range{maximum - minimum};
    return range > small_number ? std::clamp(step / range, 0.0f, 1.0f) : 1.0f;
}

auto denormalize(float const value,
                 float const minimum,
                 float const maximum,
                 float const step) noexcept -> float {
    if (maximum <= minimum) {
        return minimum;
    }

    auto const unclamped{minimum + std::clamp(value, 0.0f, 1.0f) * (maximum - minimum)};
    auto const valid_step{std::max(step, small_number)};
    auto const stepped{minimum + std::round((unclamped - minimum) / valid_step) * valid_step};
    return std::clamp(stepped, minimum, maximum);
}
}
