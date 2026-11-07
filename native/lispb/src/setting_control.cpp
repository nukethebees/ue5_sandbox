#include <codegen/schema/setting_control.h>

#include <cmath>
#include <memory>

namespace lispb::codegen {

auto SettingControl::try_make(SettingControlInput const& input)
    -> std::expected<SettingControl, std::string_view> {
    if (input.kind > SettingControlKind::custom) {
        return std::unexpected{"has an invalid control kind"};
    }
    if (is_range(input.kind)) {
        if (!input.minimum || !input.maximum || !input.step) {
            return std::unexpected{"range requires min, max, and step"};
        }
        if (!std::isfinite(*input.minimum) || !std::isfinite(*input.maximum) ||
            !std::isfinite(*input.step) || *input.minimum >= *input.maximum || *input.step <= 0.0) {
            return std::unexpected{"has an invalid range"};
        }
    } else if (input.minimum || input.maximum || input.step) {
        return std::unexpected{"non-range control has range values"};
    }
    if (input.kind == SettingControlKind::choice && !input.options_provider) {
        return std::unexpected{"choice requires an options provider"};
    }
    if (input.kind == SettingControlKind::custom) {
        if (!input.custom_row) {
            return std::unexpected{"custom control requires custom_row"};
        }
    } else if (input.custom_row) {
        return std::unexpected{"non-custom control has custom_row"};
    }

    SettingControl result{input};
    if (is_range(input.kind)) {
        result.range_ = {*input.minimum, *input.maximum, *input.step};
    } else if (input.kind == SettingControlKind::custom) {
        std::construct_at(&result.custom_row_, *input.custom_row);
    }
    return result;
}

}
