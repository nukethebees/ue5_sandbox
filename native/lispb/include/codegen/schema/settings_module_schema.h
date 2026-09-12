#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

enum class SettingApplyMode { immediate, deferred, confirm };
enum class SettingControlKind { toggle, choice, float_range, integer_range, custom };

struct SettingCategorySchema {
    std::string name;
    std::string label;
};

struct SettingControlSchema {
    SettingControlKind kind;
    std::optional<std::string> options_provider;
    std::optional<std::string> availability_provider;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    std::optional<std::string> custom_row;
};

struct SettingSchema {
    std::string name;
    std::string label;
    std::optional<std::string> tooltip;
    std::string category;
    TypeRef value_type;
    std::string backend;
    SettingApplyMode apply_mode;
    SettingControlSchema control;
};

struct SettingsModuleSchema {
    ModuleSettings settings;
    std::string api_name;
    std::string state_name;
    std::optional<std::string> export_specifier;
    std::vector<SettingCategorySchema> categories;
    std::vector<SettingSchema> settings_list;
};

} // namespace codegen
