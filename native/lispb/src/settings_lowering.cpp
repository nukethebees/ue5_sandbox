#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/access.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_category_schemas.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_device.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cctype>
#include <cstddef>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
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
            token.front() =
                static_cast<char>(std::toupper(static_cast<unsigned char>(token.front())));
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

auto enum_name(SettingDevice const value) -> std::string_view {
    switch (value) {
        case SettingDevice::shared:
            return "Shared";
        case SettingDevice::keyboard_mouse:
            return "KeyboardMouse";
        case SettingDevice::controller:
            return "Controller";
    }
    return "Shared";
}

auto export_prefix(LoweringContext& context, ModuleHandle module) -> std::string {
    auto const export_specifier{
        context.declarations.settings_modules
            .export_specifier[context.declarations.settings_modules.index(module)]};
    return export_specifier.value != 0 ? context.text(export_specifier) + " " : std::string{};
}

auto namespace_open(LoweringContext& context, ModuleSettingsId settings) -> std::string {
    return context.declarations.module_settings_store
                   .namespace_name[context.declarations.module_settings_store.index(settings)]
                   .has_value()
             ? "namespace " +
                   context.text(*context.declarations.module_settings_store.namespace_name
                                     [context.declarations.module_settings_store.index(settings)]) +
                   " {\n\n"
             : std::string{};
}

auto namespace_close(LoweringContext& context, ModuleSettingsId settings) -> std::string {
    return context.declarations.module_settings_store
                   .namespace_name[context.declarations.module_settings_store.index(settings)]
                   .has_value()
             ? "\n} // namespace " +
                   context.text(*context.declarations.module_settings_store.namespace_name
                                     [context.declarations.module_settings_store.index(settings)]) +
                   "\n"
             : std::string{};
}

auto settings_api(LoweringContext& context,
                  ModuleHandle module,
                  std::span<CppTypeId const> setting_types) -> NodeId {
    auto& ir{context.ir};
    NodeListBuilder members{ir};
    members.add(ir.access(Access::public_), 1);
    auto const settings{context.declarations.settings_modules
                            .settings_list[context.declarations.settings_modules.index(module)]};
    auto const setting_count{settings.size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{context.declarations.items(settings)[index]};
        auto const name{
            context.declarations.settings.name[context.declarations.settings.index(setting)]};
        auto const getter{ir.function(name, ir.types.intern("auto"))};
        ir.set_function_return_type(getter, setting_types[index], ReturnSyntax::trailing);
        ir.set_function_receiver(getter, FunctionReceiver::const_);
        ir.function_flags(getter).set_define_in_header(true);
        ir.set_function_body(
            getter,
            {ir.return_statement(ir.member_access(
                ir.call(ir.member_access(ir.call(ir.named("derived")), "settings_state")), name))});
        members.add(ir.header_function(getter), 2);

        auto const setter{
            ir.function(ir.strings.intern("set_" + context.text(name)), ir.types.intern("void"))};
        ir.function_flags(setter).set_define_in_header(true);
        ir.set_function_parameters(setter,
                                   {ir.parameter(qualify(ir, setting_types[index], " const"),
                                                 ir.strings.intern("value"))});
        ir.set_function_body(
            setter,
            {ir.expression_statement(ir.call(
                ir.member_access(ir.call(ir.named("derived")), "set_setting"),
                {ir.named("EGameSetting::" + title_case(context.text(name))),
                 ir.init_list({ir.named("value")}, ir.types.intern("FGameSettingValue"))}))});
        members.add(ir.header_function(setter), 2);
    }
    members.add(ir.access(Access::private_), 1);
    for (auto const is_const : {false, true}) {
        auto const derived_type{ir.types.intern(is_const ? "Derived const&" : "Derived&")};
        auto const function{ir.function(ir.strings.intern("derived"), ir.types.intern("auto"))};
        ir.set_function_return_type(function, derived_type, ReturnSyntax::trailing);
        ir.set_function_receiver(
            function, is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        ir.function_flags(function).set_define_in_header(true);
        ir.function_formatting(function).body_layout = FunctionBodyLayout::compact;
        ir.set_function_body(
            function,
            {ir.return_statement(ir.static_cast_expr(
                derived_type, ir.unary(UnaryOperator::dereference, ir.named("this"))))});
        members.add(ir.header_function(function), 1);
    }
    auto const result{ir.record(context.declarations.settings_modules
                                    .api_name[context.declarations.settings_modules.index(module)],
                                members.build(),
                                RecordKind::class_)};
    ir.set_record_template(
        result,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Derived"))}));
    return result;
}

auto build_header(LoweringContext& context,
                  ModuleHandle module,
                  std::span<CppTypeId const> const setting_types,
                  std::vector<TypeDependencyId>& dependencies) -> Nodes {
    auto& ir{context.ir};
    auto const settings{context.declarations.settings_modules
                            .settings[context.declarations.settings_modules.index(module)]};
    std::vector<CppTypeId> value_types;
    std::set<StringId> unique_types;
    std::vector<std::string> type_names;
    std::set<std::string> unique_type_names;
    std::set<std::string> backends;
    std::set<std::string> option_providers;
    std::set<std::string> availability_providers;
    auto const setting_count{context.declarations.settings_modules
                                 .settings_list[context.declarations.settings_modules.index(module)]
                                 .size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{context.declarations.items(
            context.declarations.settings_modules
                .settings_list[context.declarations.settings_modules.index(module)])[index]};
        auto const control{
            context.declarations.settings.control[context.declarations.settings.index(setting)]};
        auto const type{setting_types[index]};
        auto const type_dependencies{ir.types.dependencies(type)};
        dependencies.insert(dependencies.end(), type_dependencies.begin(), type_dependencies.end());
        if (unique_types.insert(ir.types.spelling_id(type)).second) {
            value_types.push_back(type);
        }
        auto const type_name{title_case(
            context.text(context.declarations.type_refs.name[context.declarations.type_refs.index(
                             context.declarations.settings
                                 .value_type[context.declarations.settings.index(setting)])])
                    .starts_with('@')
                ? context
                      .text(
                          context.declarations.type_refs.name[context.declarations.type_refs.index(
                              context.declarations.settings
                                  .value_type[context.declarations.settings.index(setting)])])
                      .substr(1)
                : context.text(
                      context.declarations.type_refs.name[context.declarations.type_refs.index(
                          context.declarations.settings
                              .value_type[context.declarations.settings.index(setting)])]))};
        if (unique_type_names.insert(type_name).second) {
            type_names.push_back(type_name);
        }
        backends.insert(context.text(
            context.declarations.settings.backend[context.declarations.settings.index(setting)]));
        if (context.declarations.setting_controls
                .options_provider[context.declarations.setting_controls.index(control)]
                .has_value()) {
            option_providers.insert(context.text(
                *context.declarations.setting_controls
                     .options_provider[context.declarations.setting_controls.index(control)]));
        }
        if (context.declarations.setting_controls
                .availability_provider[context.declarations.setting_controls.index(control)]
                .has_value()) {
            availability_providers.insert(context.text(
                *context.declarations.setting_controls
                     .availability_provider[context.declarations.setting_controls.index(control)]));
        }
    }

    std::ostringstream output;
    output << "enum class EGameSetting : uint8 {\n";
    for (auto const setting_id : context.declarations.items(
             context.declarations.settings_modules
                 .settings_list[context.declarations.settings_modules.index(module)])) {
        auto const setting{setting_id};

        output << "    "
               << title_case(context.text(context.declarations.settings
                                              .name[context.declarations.settings.index(setting)]))
               << ",\n";
    }
    output << "};\n\n";
    output << "enum class EGameSettingCategory : uint8 {\n";
    for (auto const category_id : context.declarations.items(
             context.declarations.settings_modules
                 .categories[context.declarations.settings_modules.index(module)])) {
        auto const category{category_id};
        output << "    "
               << title_case(context.text(
                      context.declarations.setting_categories
                          .name[context.declarations.setting_categories.index(category)]))
               << ",\n";
    }
    output << "};\n\n";
    output << "enum class ESettingApplyMode : uint8 { Immediate, Deferred, Confirm };\n";
    output << "enum class ESettingControlKind : uint8 { Toggle, Choice, FloatRange, IntegerRange, "
              "Custom };\n";
    output << "enum class EGameSettingDevice : uint8 { Shared, KeyboardMouse, Controller };\n";
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

    output << "struct " << export_prefix(context, module) << "FGameSettingCategoryDescriptor {\n"
           << "    EGameSettingCategory id;\n"
           << "    FText label;\n"
           << "};\n\n";

    output << "struct " << export_prefix(context, module) << "FGameSettingDescriptor {\n"
           << "    EGameSetting id;\n"
           << "    EGameSettingCategory category;\n"
           << "    EGameSettingValueType value_type;\n"
           << "    EGameSettingBackend backend;\n"
           << "    ESettingApplyMode apply_mode;\n"
           << "    EGameSettingDevice device;\n"
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

    output << "struct " << export_prefix(context, module)
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << " {\n";
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{context.declarations.items(
            context.declarations.settings_modules
                .settings_list[context.declarations.settings_modules.index(module)])[index]};

        output << "    " << ir.types.spelling(setting_types[index]) << " "
               << context.text(context.declarations.settings
                                   .name[context.declarations.settings.index(setting)])
               << "{};\n";
    }
    output << "\n    auto operator==("
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << " const&) const -> bool = default;\n";
    output << "};\n\n";

    std::vector<std::string> variant_types;
    for (auto const type : value_types) {
        variant_types.emplace_back(ir.types.spelling(type));
    }
    output << "using FGameSettingValue = std::variant<" << join(variant_types, ", ") << ">;\n\n";
    output << export_prefix(context, module)
           << "auto game_setting_category_descriptors() -> "
              "TConstArrayView<FGameSettingCategoryDescriptor>;\n";
    output << export_prefix(context, module)
           << "auto game_setting_descriptors() -> TConstArrayView<FGameSettingDescriptor>;\n";
    output << export_prefix(context, module)
           << "auto game_setting_descriptor(EGameSetting id) -> FGameSettingDescriptor const&;\n";
    output << export_prefix(context, module) << "auto game_setting_value("
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << " const& state, EGameSetting id) -> FGameSettingValue;\n";
    output << export_prefix(context, module) << "auto set_game_setting_value("
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << "& state, EGameSetting id, FGameSettingValue const& value) -> bool;\n\n";

    NodeListBuilder result{ir};
    result.add(ir.raw(output.str(), dependencies), 2)
        .add(settings_api(context, module, setting_types));
    auto nodes{result.build()};
    if (auto const namespace_name{
            context.declarations.module_settings_store
                .namespace_name[context.declarations.module_settings_store.index(settings)]}) {
        return {ir.namespace_node(*namespace_name, nodes)};
    }
    return nodes;
}

auto build_source(LoweringContext& context,
                  ModuleHandle module,
                  std::span<CppTypeId const> const setting_types) -> std::string {
    auto& ir{context.ir};
    auto const settings{context.declarations.settings_modules
                            .settings[context.declarations.settings_modules.index(module)]};
    std::ostringstream output;
    output << namespace_open(context, settings);
    output << "namespace {\n\nstatic TArray<FGameSettingCategoryDescriptor> const "
              "category_descriptors{\n";
    for (auto const category_id : context.declarations.items(
             context.declarations.settings_modules
                 .categories[context.declarations.settings_modules.index(module)])) {
        auto const category{category_id};
        output << "    {EGameSettingCategory::"
               << title_case(context.text(
                      context.declarations.setting_categories
                          .name[context.declarations.setting_categories.index(category)]))
               << ", FText::FromString(TEXT(\""
               << escaped(context.text(
                      context.declarations.setting_categories
                          .label[context.declarations.setting_categories.index(category)]))
               << "\"))},\n";
    }
    output << "};\n\nstatic TArray<FGameSettingDescriptor> const descriptors{\n";
    for (auto const setting_id : context.declarations.items(
             context.declarations.settings_modules
                 .settings_list[context.declarations.settings_modules.index(module)])) {
        auto const setting{setting_id};
        auto const control{
            context.declarations.settings.control[context.declarations.settings.index(setting)]};
        auto const type_name{title_case(
            context.text(context.declarations.type_refs.name[context.declarations.type_refs.index(
                             context.declarations.settings
                                 .value_type[context.declarations.settings.index(setting)])])
                    .starts_with('@')
                ? context
                      .text(
                          context.declarations.type_refs.name[context.declarations.type_refs.index(
                              context.declarations.settings
                                  .value_type[context.declarations.settings.index(setting)])])
                      .substr(1)
                : context.text(
                      context.declarations.type_refs.name[context.declarations.type_refs.index(
                          context.declarations.settings
                              .value_type[context.declarations.settings.index(setting)])]))};
        output
            << "    {EGameSetting::"
            << title_case(context.text(context.declarations.settings
                                           .name[context.declarations.settings.index(setting)]))
            << ", EGameSettingCategory::"
            << title_case(context.text(context.declarations.settings
                                           .category[context.declarations.settings.index(setting)]))
            << ", EGameSettingValueType::" << type_name << ", EGameSettingBackend::"
            << title_case(context.text(context.declarations.settings
                                           .backend[context.declarations.settings.index(setting)]))
            << ", ESettingApplyMode::"
            << enum_name(context.declarations.settings
                             .apply_mode[context.declarations.settings.index(setting)])
            << ", EGameSettingDevice::"
            << enum_name(context.declarations.settings
                             .device[context.declarations.settings.index(setting)])
            << ", ESettingControlKind::"
            << enum_name(context.declarations.setting_controls
                             .kind[context.declarations.setting_controls.index(control)])
            << ", EGameSettingOptionProvider::"
            << (context.declarations.setting_controls
                        .options_provider[context.declarations.setting_controls.index(control)]
                        .has_value()
                    ? title_case(
                          context.text(*context.declarations.setting_controls.options_provider
                                            [context.declarations.setting_controls.index(control)]))
                    : "None")
            << ", EGameSettingAvailabilityProvider::"
            << (context.declarations.setting_controls
                        .availability_provider[context.declarations.setting_controls.index(control)]
                        .has_value()
                    ? title_case(
                          context.text(*context.declarations.setting_controls.availability_provider
                                            [context.declarations.setting_controls.index(control)]))
                    : "Always")
            << ", TEXT(\""
            << escaped(context.text(context.declarations.settings
                                        .name[context.declarations.settings.index(setting)]))
            << "\"), FText::FromString(TEXT(\""
            << escaped(context.text(context.declarations.settings
                                        .label[context.declarations.settings.index(setting)]))
            << "\")), FText::FromString(TEXT(\""
            << escaped(
                   context
                       .optional_text(context.declarations.settings
                                          .tooltip[context.declarations.settings.index(setting)])
                       .value_or(""))
            << "\")), "
            << context.declarations.setting_controls
                   .minimum[context.declarations.setting_controls.index(control)]
                   .value_or(0.0)
            << ", "
            << context.declarations.setting_controls
                   .maximum[context.declarations.setting_controls.index(control)]
                   .value_or(0.0)
            << ", "
            << context.declarations.setting_controls
                   .step[context.declarations.setting_controls.index(control)]
                   .value_or(0.0)
            << ", TEXT(\""
            << escaped(
                   context
                       .optional_text(
                           context.declarations.setting_controls
                               .custom_row[context.declarations.setting_controls.index(control)])
                       .value_or(""))
            << "\")},\n";
    }
    output << "};\n\n} // namespace\n\n";
    output << "auto game_setting_category_descriptors() -> "
              "TConstArrayView<FGameSettingCategoryDescriptor> {\n"
           << "    return category_descriptors;\n}\n\n";
    output << "auto game_setting_descriptors() -> TConstArrayView<FGameSettingDescriptor> {\n"
           << "    return descriptors;\n}\n\n"
           << "auto game_setting_descriptor(EGameSetting const id) -> FGameSettingDescriptor "
              "const& {\n"
           << "    return descriptors[static_cast<int32>(id)];\n}\n\n";

    output << "auto game_setting_value("
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << " const& state, EGameSetting const id) -> FGameSettingValue {\n"
           << "    switch (id) {\n";
    for (auto const setting_id : context.declarations.items(
             context.declarations.settings_modules
                 .settings_list[context.declarations.settings_modules.index(module)])) {
        auto const setting{setting_id};

        output << "    case EGameSetting::"
               << title_case(context.text(context.declarations.settings
                                              .name[context.declarations.settings.index(setting)]))
               << ": return FGameSettingValue{state."
               << context.text(context.declarations.settings
                                   .name[context.declarations.settings.index(setting)])
               << "};\n";
    }
    output
        << "    }\n    checkNoEntry();\n    return FGameSettingValue{state."
        << context.text(context.declarations.settings.name[context.declarations.settings.index(
               context.declarations
                   .items(context.declarations.settings_modules
                              .settings_list[context.declarations.settings_modules.index(module)])
                   .front())])
        << "};\n}\n\n";

    output << "auto set_game_setting_value("
           << context.text(context.declarations.settings_modules
                               .state_name[context.declarations.settings_modules.index(module)])
           << "& state, EGameSetting const id, FGameSettingValue const& value) -> bool {\n"
           << "    switch (id) {\n";
    auto const setting_count{context.declarations.settings_modules
                                 .settings_list[context.declarations.settings_modules.index(module)]
                                 .size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{context.declarations.items(
            context.declarations.settings_modules
                .settings_list[context.declarations.settings_modules.index(module)])[index]};

        auto const type{ir.types.spelling(setting_types[index])};
        output << "    case EGameSetting::"
               << title_case(context.text(context.declarations.settings
                                              .name[context.declarations.settings.index(setting)]))
               << ": {\n"
               << "        auto const* typed_value{std::get_if<" << type << ">(&value)};\n"
               << "        if (typed_value == nullptr) { return false; }\n"
               << "        state."
               << context.text(context.declarations.settings
                                   .name[context.declarations.settings.index(setting)])
               << " = *typed_value;\n"
               << "        return true;\n"
               << "    }\n";
    }
    output << "    }\n    return false;\n}\n";
    output << namespace_close(context, settings);
    return output.str();
}

} // namespace

auto lower_settings_module(LoweringContext& context, ModuleHandle module) -> ModuleId {
    auto& ir{context.ir};
    auto const settings{context.declarations.settings_modules
                            .settings[context.declarations.settings_modules.index(module)]};
    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("uint8", "CoreMinimal.h"),
        ir.types.dependency("FText", "CoreMinimal.h"),
        ir.types.dependency("TConstArrayView", "Containers/ArrayView.h"),
        ir.types.dependency("std::variant", "variant"),
    };
    std::vector<CppTypeId> setting_types;
    setting_types.reserve(context.declarations.settings_modules
                              .settings_list[context.declarations.settings_modules.index(module)]
                              .size());
    for (auto const setting_id : context.declarations.items(
             context.declarations.settings_modules
                 .settings_list[context.declarations.settings_modules.index(module)])) {
        auto const setting{setting_id};

        setting_types.push_back(
            context.resolve(context.declarations.settings
                                .value_type[context.declarations.settings.index(setting)]));
    }
    auto header_declarations{build_header(context, module, setting_types, dependencies)};

    NodeListBuilder header_nodes{ir};
    header_nodes.add(ir.include_dependencies(), 2);
    if (!context.declarations.module_settings_store
             .prelude_lines[context.declarations.module_settings_store.index(settings)]
             .empty()) {
        header_nodes.add(
            ir.raw(join_lines(
                context,
                context.declarations.module_settings_store
                    .prelude_lines[context.declarations.module_settings_store.index(settings)])),
            2);
    }
    header_nodes.append(std::move(header_declarations));

    auto const& settings_store{context.declarations.module_settings_store};
    auto const settings_row{settings_store.index(settings)};
    auto const include_order{
        context.declarations.items(settings_store.include_order[settings_row])};
    auto const header_file{
        ir.files.append(settings_store.header[settings_row], header_nodes.build())};
    ir.files.flags[ir.files.index(header_file)].set_clang_format_off(true);
    ir.files.set_include_order(header_file, include_order);
    CppFileId source_file;
    if (auto const path{settings_store.source[settings_row]}) {
        NodeListBuilder source_nodes{ir};
        source_nodes
            .add(ir.include(ir.store_text(source_include(context, settings)), IncludeStyle::quoted),
                 2)
            .add(ir.raw(build_source(context, module, setting_types)), 2);
        source_file = ir.files.append(*path, source_nodes.build());
        auto const source_row{ir.files.index(source_file)};
        ir.files.flags[source_row].set_pragma_once(false);
        ir.files.flags[source_row].set_clang_format_off(true);
        ir.files.set_include_order(source_file, include_order);
    }
    return ir.modules.append(settings_store.name[settings_row], header_file, source_file);
}

} // namespace lispb::codegen::detail
