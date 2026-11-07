#pragma once

#include <codegen/schema/setting_control_kind.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct SettingControl {
    SettingControlKind kind;
    std::optional<StringId> options_provider;
    std::optional<StringId> availability_provider;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    std::optional<StringId> custom_row;
};

}
