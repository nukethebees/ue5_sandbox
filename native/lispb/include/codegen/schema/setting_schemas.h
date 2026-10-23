#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_device.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct SettingSchemas {
    [[nodiscard]] auto append() -> SettingId;
    [[nodiscard]] auto clone(SettingId id) -> SettingId;
    [[nodiscard]] auto index(SettingId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<StringId> label;
    std::vector<std::optional<StringId>> tooltip;
    std::vector<StringId> category;
    std::vector<TypeRefId> value_type;
    std::vector<StringId> backend;
    std::vector<SettingApplyMode> apply_mode;
    std::vector<SettingDevice> device;
    std::vector<SettingControlId> control;
};

}
