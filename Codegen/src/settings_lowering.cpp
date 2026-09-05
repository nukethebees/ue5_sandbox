#include "lowering.h"
#include "lowering_utils.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <utility>

namespace codegen::detail {
namespace {

auto title_case(std::string const& value) -> std::string {
    std::ostringstream result;
    bool capitalize{true};
    std::string token;
    auto flush_token = [&]() {
        if (token.empty()) {
            return;
        }
        if (token == "aa") {
            result << "AA";
        } else if (token == "vsync") {
            result << "VSync";
        } else if (token == "ui") {
            result << "UI";
        } else {
            token.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(token.front())));
            result << token;
        }
        token.clear();
        capitalize = true;
    };
    for (auto const character : value) {
        if (character == '_' || character == '-' || character == ':') {
            flush_token();
        } else {
            token += capitalize
                         ? static_cast<char>(std::tolower(static_cast<unsigned char>(character)))
                         : character;
            capitalize = false;
        }
    }
    flush_token();
    return result.str();
}

auto escaped(std::string const& value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (auto const character : value) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        if (character == '\n') {
            result += "\\n";
        } else {
            result += character;
        }
    }
    return result;
}

auto enum_name(SettingApplyMode const value) -> std::string_view {
    switch (value) {
    case SettingApplyMode::immediate:
        return "Immediate";
    case SettingApplyMode::deferred:
        return "Deferred";
    case SettingApplyMode::confirm:
        return "Confirm";
    }
    return "Deferred";
}

auto enum_name(SettingControlKind const value) -> std::string_view {
    switch (value) {
    case SettingControlKind::toggle:
        return "Toggle";
    case SettingControlKind::choice:
        return "Choice";
    case SettingControlKind::float_range:
        return "FloatRange";
    case SettingControlKind::integer_range:
        return "IntegerRange";
    case SettingControlKind::custom:
        return "Custom";
    }
    return "Custom";
}

auto export_prefix(SettingsModuleSchema const& module) -> std::string {
    return module.export_specifier.has_value() ? *module.export_specifier + " " : std::string{};
}

auto namespace_open(ModuleSettings const& settings) -> std::string {
    return settings.namespace_name.has_value() ? "namespace " + *settings.namespace_name + " {\n\n"
                                               : std::string{};
}

auto namespace_close(ModuleSettings const& settings) -> std::string {
    return settings.namespace_name.has_value() ? "\n} // namespace " + *settings.namespace_name + "\n"
                                               : std::string{};
}

auto build_header(SettingsModuleSchema const& module,
                  std::map<std::string, CppType> const& types,
                  std::vector<TypeDependency>& dependencies) -> std::string {
    std::vector<CppType> value_types;
    std::set<std::string> unique_types;
    std::vector<std::string> type_names;
    std::set<std::string> unique_type_names;
    std::set<std::string> backends;
    std::set<std::string> option_providers;
    std::set<std::string> availability_providers;
    for (auto const& setting : module.settings_list) {
        auto const type{resolve_type(setting.value_type, types)};
        if (unique_types.insert(type.spelling).second) {
            value_types.push_back(type);
            dependencies.insert(dependencies.end(), type.dependencies.begin(), type.dependencies.end());
        }
        auto const type_name{title_case(setting.value_type.name.starts_with('@')
                                            ? setting.value_type.name.substr(1)
                                            : setting.value_type.name)};
        if (unique_type_names.insert(type_name).second) {
            type_names.push_back(type_name);
        }
        backends.insert(setting.backend);
        if (setting.control.options_provider.has_value()) {
            option_providers.insert(*setting.control.options_provider);
        }
        if (setting.control.availability_provider.has_value()) {
            availability_providers.insert(*setting.control.availability_provider);
        }
    }

    std::ostringstream output;
    output << namespace_open(module.settings);
    output << "enum class EGameSetting : uint8 {\n";
    for (auto const& setting : module.settings_list) {
        output << "    " << title_case(setting.name) << ",\n";
    }
    output << "};\n\n";
    output << "enum class EGameSettingCategory : uint8 {\n";
    for (auto const& category : module.categories) {
        output << "    " << title_case(category.name) << ",\n";
    }
    output << "};\n\n";
    output << "enum class ESettingApplyMode : uint8 { Immediate, Deferred, Confirm };\n";
    output << "enum class ESettingControlKind : uint8 { Toggle, Choice, FloatRange, IntegerRange, Custom };\n";
    output << "enum class EGameSettingValueType : uint8 { " << join(type_names, ", ") << " };\n";
    output << "enum class EGameSettingBackend : uint8 { ";
    {
        std::vector<std::string> names;
        for (auto const& backend : backends) {
            names.push_back(title_case(backend));
        }
        output << join(names, ", ");
    }
    output << " };\n";
    output << "enum class EGameSettingOptionProvider : uint8 { None";
    for (auto const& provider : option_providers) {
        output << ", " << title_case(provider);
    }
    output << " };\n";
    output << "enum class EGameSettingAvailabilityProvider : uint8 { Always";
    for (auto const& provider : availability_providers) {
        output << ", " << title_case(provider);
    }
    output << " };\n\n";

    output << "struct " << export_prefix(module) << "FGameSettingCategoryDescriptor {\n"
           << "    EGameSettingCategory id;\n"
           << "    FText label;\n"
           << "};\n\n";

    output << "struct " << export_prefix(module) << "FGameSettingDescriptor {\n"
           << "    EGameSetting id;\n"
           << "    EGameSettingCategory category;\n"
           << "    EGameSettingValueType value_type;\n"
           << "    EGameSettingBackend backend;\n"
           << "    ESettingApplyMode apply_mode;\n"
           << "    ESettingControlKind control_kind;\n"
           << "    EGameSettingOptionProvider options_provider;\n"
           << "    EGameSettingAvailabilityProvider availability_provider;\n"
           << "    TCHAR const* name;\n"
           << "    FText label;\n"
           << "    FText tooltip;\n"
           << "    double minimum;\n"
           << "    double maximum;\n"
           << "    double step;\n"
           << "    TCHAR const* custom_row;\n"
           << "};\n\n";

    output << "struct " << export_prefix(module) << module.state_name << " {\n";
    for (auto const& setting : module.settings_list) {
        output << "    " << resolve_type(setting.value_type, types).spelling << " " << setting.name
               << "{};\n";
    }
    output << "\n    auto operator==("
           << module.state_name << " const&) const -> bool = default;\n";
    output << "};\n\n";

    std::vector<std::string> variant_types;
    for (auto const& type : value_types) {
        variant_types.push_back(type.spelling);
    }
    output << "using FGameSettingValue = std::variant<" << join(variant_types, ", ") << ">;\n\n";
    output << export_prefix(module)
           << "auto game_setting_category_descriptors() -> "
              "TConstArrayView<FGameSettingCategoryDescriptor>;\n";
    output << export_prefix(module)
           << "auto game_setting_descriptors() -> TConstArrayView<FGameSettingDescriptor>;\n";
    output << export_prefix(module)
           << "auto game_setting_descriptor(EGameSetting id) -> FGameSettingDescriptor const&;\n";
    output << export_prefix(module) << "auto game_setting_value(" << module.state_name
           << " const& state, EGameSetting id) -> FGameSettingValue;\n";
    output << export_prefix(module) << "auto set_game_setting_value(" << module.state_name
           << "& state, EGameSetting id, FGameSettingValue const& value) -> bool;\n\n";

    output << "template <typename Derived>\nclass " << module.api_name << " {\npublic:\n";
    for (auto const& setting : module.settings_list) {
        auto const type{resolve_type(setting.value_type, types).spelling};
        output << "    auto " << setting.name << "() const -> " << type << " {\n"
               << "        return derived().settings_state()." << setting.name << ";\n"
               << "    }\n\n"
               << "    void set_" << setting.name << "(" << type << " const value) {\n"
               << "        derived().set_setting(EGameSetting::" << title_case(setting.name)
               << ", FGameSettingValue{value});\n"
               << "    }\n\n";
    }
    output << "private:\n"
           << "    auto derived() -> Derived& { return static_cast<Derived&>(*this); }\n"
           << "    auto derived() const -> Derived const& { return static_cast<Derived const&>(*this); }\n"
           << "};\n";
    output << namespace_close(module.settings);
    return output.str();
}

auto build_source(SettingsModuleSchema const& module,
                  std::map<std::string, CppType> const& types) -> std::string {
    std::ostringstream output;
    output << namespace_open(module.settings);
    output << "namespace {\n\nstatic TArray<FGameSettingCategoryDescriptor> const category_descriptors{\n";
    for (auto const& category : module.categories) {
        output << "    {EGameSettingCategory::" << title_case(category.name)
               << ", FText::FromString(TEXT(\"" << escaped(category.label) << "\"))},\n";
    }
    output << "};\n\nstatic TArray<FGameSettingDescriptor> const descriptors{\n";
    for (auto const& setting : module.settings_list) {
        auto const type_name{title_case(setting.value_type.name.starts_with('@')
                                            ? setting.value_type.name.substr(1)
                                            : setting.value_type.name)};
        output << "    {EGameSetting::" << title_case(setting.name)
               << ", EGameSettingCategory::" << title_case(setting.category)
               << ", EGameSettingValueType::" << type_name << ", EGameSettingBackend::"
               << title_case(setting.backend) << ", ESettingApplyMode::"
               << enum_name(setting.apply_mode) << ", ESettingControlKind::"
               << enum_name(setting.control.kind) << ", EGameSettingOptionProvider::"
               << (setting.control.options_provider.has_value()
                       ? title_case(*setting.control.options_provider)
                       : "None")
               << ", EGameSettingAvailabilityProvider::"
               << (setting.control.availability_provider.has_value()
                       ? title_case(*setting.control.availability_provider)
                       : "Always")
               << ", TEXT(\"" << escaped(setting.name) << "\"), FText::FromString(TEXT(\""
               << escaped(setting.label) << "\")), FText::FromString(TEXT(\""
               << escaped(setting.tooltip.value_or("")) << "\")), "
               << setting.control.minimum.value_or(0.0) << ", "
               << setting.control.maximum.value_or(0.0) << ", "
               << setting.control.step.value_or(0.0) << ", TEXT(\""
               << escaped(setting.control.custom_row.value_or("")) << "\")},\n";
    }
    output << "};\n\n} // namespace\n\n";
    output << "auto game_setting_category_descriptors() -> "
              "TConstArrayView<FGameSettingCategoryDescriptor> {\n"
           << "    return category_descriptors;\n}\n\n";
    output << "auto game_setting_descriptors() -> TConstArrayView<FGameSettingDescriptor> {\n"
           << "    return descriptors;\n}\n\n"
           << "auto game_setting_descriptor(EGameSetting const id) -> FGameSettingDescriptor const& {\n"
           << "    return descriptors[static_cast<int32>(id)];\n}\n\n";

    output << "auto game_setting_value(" << module.state_name
           << " const& state, EGameSetting const id) -> FGameSettingValue {\n"
           << "    switch (id) {\n";
    for (auto const& setting : module.settings_list) {
        output << "    case EGameSetting::" << title_case(setting.name)
               << ": return FGameSettingValue{state." << setting.name << "};\n";
    }
    output << "    }\n    checkNoEntry();\n    return FGameSettingValue{state."
           << module.settings_list.front().name << "};\n}\n\n";

    output << "auto set_game_setting_value(" << module.state_name
           << "& state, EGameSetting const id, FGameSettingValue const& value) -> bool {\n"
           << "    switch (id) {\n";
    for (auto const& setting : module.settings_list) {
        auto const type{resolve_type(setting.value_type, types).spelling};
        output << "    case EGameSetting::" << title_case(setting.name) << ": {\n"
               << "        auto const* typed_value{std::get_if<" << type << ">(&value)};\n"
               << "        if (typed_value == nullptr) { return false; }\n"
               << "        state." << setting.name << " = *typed_value;\n"
               << "        return true;\n"
               << "    }\n";
    }
    output << "    }\n    return false;\n}\n";
    output << namespace_close(module.settings);
    return output.str();
}

} // namespace

auto lower_settings_module(SettingsModuleSchema const& module,
                           std::map<std::string, CppType> const& types) -> Module {
    std::vector<TypeDependency> dependencies{
        TypeDependency{"uint8", "CoreMinimal.h", {}},
        TypeDependency{"FText", "CoreMinimal.h", {}},
        TypeDependency{"TConstArrayView", "Containers/ArrayView.h", {}},
        TypeDependency{"std::variant", "variant", {}},
    };
    auto header_text{build_header(module, types, dependencies)};

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.add(raw(std::move(header_text), std::move(dependencies)));

    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (module.settings.source.has_value()) {
        NodeListBuilder source_nodes;
        source_nodes.add(Include{source_include(module.settings), false}, 2)
            .add(raw(build_source(module, types)), 2);
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = source_nodes.build(),
            .pragma_once = false,
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        };
    }
    return result;
}

} // namespace codegen::detail
