#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/setting_control_kind.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct SettingControlSchemas {
    [[nodiscard]] auto append() -> SettingControlId;
    [[nodiscard]] auto clone(SettingControlId id) -> SettingControlId;
    [[nodiscard]] auto index(SettingControlId id) const -> std::uint32_t;
    void clear();

    std::vector<SettingControlKind> kind;
    std::vector<std::optional<StringId>> options_provider;
    std::vector<std::optional<StringId>> availability_provider;
    std::vector<std::optional<double>> minimum;
    std::vector<std::optional<double>> maximum;
    std::vector<std::optional<double>> step;
    std::vector<std::optional<StringId>> custom_row;
};

}
