#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
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
#include <codegen/schema/indices.h>
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
#include <format>
#include <initializer_list>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto title_case(std::string const& value) -> std::string {
    std::string result{};
    bool capitalize{true};
    std::string token;
    auto flush_token = [&]() {
        if (token.empty()) {
            return;
        }
        if (token == "aa") {
            result += "AA";
        } else if (token == "vsync") {
            result += "VSync";
        } else if (token == "ui") {
            result += "UI";
        } else {
            token.front() =
                static_cast<char>(std::toupper(static_cast<unsigned char>(token.front())));
            result += token;
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

auto setting_type_name(LoweringContext& context, SettingId const setting) -> std::string {
    auto const& declarations{context.declarations};
    auto const type{declarations.settings.value_type[declarations.settings.index(setting)]};
    auto const name{context.text(declarations.type_refs.name[declarations.type_refs.index(type)])};
    return title_case(name.starts_with('@') ? name.substr(1) : name);
}

auto text_literal(CppIR& ir, std::string_view const value) -> Expr {
    return ir.call(ir.named("TEXT", {ir.types.dependency("TEXT", "CoreMinimal.h")}),
                   {ir.string_literal(value)});
}

auto text_value(CppIR& ir, std::string_view const value) -> Expr {
    return ir.call(ir.named("FText::FromString"), {text_literal(ir, value)});
}

auto build_functions(LoweringContext& context,
                     ModuleHandle const module,
                     std::span<CppTypeId const> const setting_types) -> std::vector<FunctionId> {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const module_row{declarations.settings_modules.index(module)};
    auto const state_name{context.text(declarations.settings_modules.state_name[module_row])};
    auto const settings{
        declarations.items(declarations.settings_modules.settings_list[module_row])};
    auto const state{ir.named("state")};
    auto const id{ir.named("id")};
    auto const value{ir.named("value")};
    auto const id_parameter{ir.parameter("EGameSetting const", "id")};
    auto create = [&](std::string_view const name, std::string_view const return_type) {
        auto const type{ir.types.intern(return_type)};
        auto const function{ir.function(name, type)};
        ir.set_function_return_type(function, type, ReturnSyntax::trailing);
        ir.set_function_export_specifier(
            function, declarations.settings_modules.export_specifier[module_row]);
        return function;
    };

    auto const categories{create("game_setting_category_descriptors",
                                 "TConstArrayView<FGameSettingCategoryDescriptor>")};
    ir.set_function_body(categories, {ir.return_stmt(ir.named("category_descriptors"))});

    auto const descriptors{
        create("game_setting_descriptors", "TConstArrayView<FGameSettingDescriptor>")};
    ir.set_function_body(descriptors, {ir.return_stmt(ir.named("descriptors"))});

    auto const descriptor{create("game_setting_descriptor", "FGameSettingDescriptor const&")};
    ir.set_function_parameters(descriptor, {id_parameter});
    ir.set_function_body(
        descriptor,
        {ir.return_stmt(ir.subscript(ir.named("descriptors"),
                                     ir.static_cast_expr(ir.types.intern("int32"), id)))});

    auto const getter{create("game_setting_value", "FGameSettingValue")};
    ir.set_function_parameters(getter,
                               {ir.parameter(state_name + " const&", "state"), id_parameter});
    std::vector<SwitchCaseId> getter_cases;
    for (auto const setting : settings) {
        auto const name{declarations.settings.name[declarations.settings.index(setting)]};
        getter_cases.push_back(ir.switch_case(
            ir.named("EGameSetting::" + title_case(context.text(name))),
            ir.block({ir.return_stmt(ir.init_list({ir.member_access(state, name)},
                                                  ir.types.intern("FGameSettingValue")))})));
    }
    auto const first_name{
        declarations.settings.name[declarations.settings.index(settings.front())]};
    ir.set_function_body(getter,
                         {ir.switch_stmt(id, getter_cases),
                          ir.expr_stmt(ir.call(ir.named("checkNoEntry"))),
                          ir.return_stmt(ir.init_list({ir.member_access(state, first_name)},
                                                      ir.types.intern("FGameSettingValue")))});

    auto const setter{create("set_game_setting_value", "bool")};
    ir.set_function_parameters(setter,
                               {ir.parameter(state_name + "&", "state"),
                                id_parameter,
                                ir.parameter("FGameSettingValue const&", "value")});
    std::vector<SwitchCaseId> setter_cases;
    auto const setting_count{settings.size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const name{declarations.settings.name[declarations.settings.index(settings[index])]};
        auto const typed_value{ir.named("typed_value")};
        setter_cases.push_back(ir.switch_case(
            ir.named("EGameSetting::" + title_case(context.text(name))),
            ir.block({
                ir.variable("auto const*",
                            "typed_value",
                            ir.call(ir.template_id(ir.named("std::get_if"),
                                                   {ir.type_reference(setting_types[index])}),
                                    {ir.unary(UnaryOperator::address_of, value)})),
                ir.if_stmt(ir.binary(BinaryOperator::equal, typed_value, ir.literal("nullptr")),
                           ir.block({ir.return_stmt(ir.literal("false"))})),
                ir.assignment(ir.member_access(state, name),
                              ir.unary(UnaryOperator::dereference, typed_value)),
                ir.return_stmt(ir.literal("true")),
            })));
    }
    ir.set_function_body(setter,
                         {ir.switch_stmt(id, setter_cases), ir.return_stmt(ir.literal("false"))});

    return {categories, descriptors, descriptor, getter, setter};
}

auto settings_api(LoweringContext& context,
                  ModuleHandle module,
                  std::span<CppTypeId const> setting_types) -> NodeId {
    auto& ir{context.ir};
    auto const& modules{context.declarations.settings_modules};
    auto const module_row{modules.index(module)};
    NodeListBuilder members{ir};
    members.add(ir.access(Access::public_), 1);
    auto const settings{context.declarations.items(modules.settings_list[module_row])};
    auto const setting_count{settings.size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{settings[index]};
        auto const name{
            context.declarations.settings.name[context.declarations.settings.index(setting)]};
        auto const getter{ir.function(name, ir.types.intern("auto"))};
        ir.set_function_return_type(getter, setting_types[index], ReturnSyntax::trailing);
        ir.set_function_receiver(getter, FunctionReceiver::const_);
        ir.function_flags(getter).set_define_in_header(true);
        ir.set_function_body(
            getter,
            {ir.return_stmt(ir.member_access(
                ir.call(ir.member_access(ir.call(ir.named("derived")), "settings_state")), name))});
        members.add(ir.header_function(getter), 2);

        auto const setter{ir.function("set_" + context.text(name), "void")};
        ir.function_flags(setter).set_define_in_header(true);
        ir.set_function_parameters(
            setter, {ir.parameter(qualify(ir, setting_types[index], " const"), "value")});
        ir.set_function_body(
            setter,
            {ir.expr_stmt(ir.call(
                ir.member_access(ir.call(ir.named("derived")), "set_setting"),
                {ir.named("EGameSetting::" + title_case(context.text(name))),
                 ir.init_list({ir.named("value")}, ir.types.intern("FGameSettingValue"))}))});
        members.add(ir.header_function(setter), 2);
    }
    members.add(ir.access(Access::private_), 1);
    for (auto const is_const : {false, true}) {
        auto const derived_type{ir.types.intern(is_const ? "Derived const&" : "Derived&")};
        auto const function{ir.function("derived", "auto")};
        ir.set_function_return_type(function, derived_type, ReturnSyntax::trailing);
        ir.set_function_receiver(
            function, is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        ir.function_flags(function).set_define_in_header(true);
        ir.function_formatting(function).body_layout = FunctionBodyLayout::compact;
        ir.set_function_body(
            function,
            {ir.return_stmt(ir.static_cast_expr(
                derived_type, ir.unary(UnaryOperator::dereference, ir.named("this"))))});
        members.add(ir.header_function(function), 1);
    }
    auto const result{ir.record(modules.api_name[module_row], members.build(), RecordKind::class_)};
    ir.set_record_template(
        result,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Derived"))}));
    return result;
}

auto build_header(LoweringContext& context,
                  ModuleHandle const module,
                  std::span<CppTypeId const> const setting_types,
                  std::span<FunctionId const> const functions,
                  std::vector<TypeDependencyId>& dependencies) -> Nodes {
    auto& ir{context.ir};
    auto const& modules{context.declarations.settings_modules};
    auto const& setting_schemas{context.declarations.settings};
    auto const& controls{context.declarations.setting_controls};
    auto const& declarations{context.declarations};
    auto const module_row{modules.index(module)};
    auto const settings{modules.settings[module_row]};
    auto const setting_list{declarations.items(modules.settings_list[module_row])};
    std::vector<std::string> setting_names;
    std::vector<std::string> category_names;
    std::vector<std::string> value_types;
    std::vector<std::string> type_names;
    std::set<StringId> unique_types;
    std::set<std::string> unique_type_names;
    std::set<std::string> backends;
    std::set<std::string> option_providers;
    std::set<std::string> availability_providers;
    auto const setting_count{setting_list.size()};
    for (std::size_t index{}; index < setting_count; ++index) {
        auto const setting{setting_list[index]};
        auto const row{setting_schemas.index(setting)};
        auto const control_row{controls.index(setting_schemas.control[row])};
        auto const type{setting_types[index]};
        auto const type_dependencies{ir.types.dependencies(type)};
        dependencies.insert(dependencies.end(), type_dependencies.begin(), type_dependencies.end());
        if (unique_types.insert(ir.types.spelling_id(type)).second) {
            value_types.emplace_back(ir.types.spelling(type));
        }
        auto const type_name{setting_type_name(context, setting)};
        if (unique_type_names.insert(type_name).second) {
            type_names.push_back(type_name);
        }
        setting_names.push_back(title_case(context.text(setting_schemas.name[row])));
        backends.insert(context.text(setting_schemas.backend[row]));
        if (auto const provider{controls.options_provider[control_row]}) {
            option_providers.insert(context.text(*provider));
        }
        if (auto const provider{controls.availability_provider[control_row]}) {
            availability_providers.insert(context.text(*provider));
        }
    }
    for (auto const category : declarations.items(modules.categories[module_row])) {
        category_names.push_back(
            title_case(context.text(declarations.setting_categories
                                        .name[declarations.setting_categories.index(category)])));
    }

    NodeListBuilder header{ir};
    auto add_enum = [&](std::string_view const name, std::vector<std::string> const& names) {
        std::vector<EnumValueId> values;
        for (auto const& value : names) {
            values.push_back(ir.enumerator(ir.strings.intern(value)));
        }
        header.add(ir.enumeration(ir.strings.intern(name), ir.types.intern("uint8"), values), 2);
    };
    add_enum("EGameSetting", setting_names);
    add_enum("EGameSettingCategory", category_names);
    add_enum("ESettingApplyMode", {"Immediate", "Deferred", "Confirm"});
    add_enum("ESettingControlKind", {"Toggle", "Choice", "FloatRange", "IntegerRange", "Custom"});
    add_enum("EGameSettingDevice", {"Shared", "KeyboardMouse", "Controller"});
    add_enum("EGameSettingValueType", type_names);
    std::vector<std::string> backend_names;
    for (auto const& backend : backends) {
        backend_names.push_back(title_case(backend));
    }
    add_enum("EGameSettingBackend", backend_names);
    std::vector<std::string> option_names{"None"};
    for (auto const& provider : option_providers) {
        option_names.push_back(title_case(provider));
    }
    add_enum("EGameSettingOptionProvider", option_names);
    std::vector<std::string> availability_names{"Always"};
    for (auto const& provider : availability_providers) {
        availability_names.push_back(title_case(provider));
    }
    add_enum("EGameSettingAvailabilityProvider", availability_names);

    auto add_record =
        [&](std::string_view const name,
            std::initializer_list<std::pair<std::string_view, std::string_view>> fields) {
            NodeListBuilder members{ir};
            for (auto const& [type, member] : fields) {
                members.add(ir.instance_member(ir.types.intern(type), ir.strings.intern(member)),
                            1);
            }
            auto const record{ir.record(ir.strings.intern(name), members.build())};
            ir.set_record_export_specifier(record, modules.export_specifier[module_row]);
            ir.set_record_dependencies(record, dependencies);
            header.add(record, 2);
        };
    add_record("FGameSettingCategoryDescriptor",
               {{"EGameSettingCategory", "id"}, {"FText", "label"}});
    add_record("FGameSettingDescriptor",
               {{"EGameSetting", "id"},
                {"EGameSettingCategory", "category"},
                {"EGameSettingValueType", "value_type"},
                {"EGameSettingBackend", "backend"},
                {"ESettingApplyMode", "apply_mode"},
                {"EGameSettingDevice", "device"},
                {"ESettingControlKind", "control_kind"},
                {"EGameSettingOptionProvider", "options_provider"},
                {"EGameSettingAvailabilityProvider", "availability_provider"},
                {"TCHAR const*", "name"},
                {"FText", "label"},
                {"FText", "tooltip"},
                {"double", "minimum"},
                {"double", "maximum"},
                {"double", "step"},
                {"TCHAR const*", "custom_row"}});

    NodeListBuilder state_members{ir};
    for (std::size_t index{}; index < setting_count; ++index) {
        state_members.add(
            ir.instance_member(setting_types[index],
                               setting_schemas.name[setting_schemas.index(setting_list[index])],
                               ir.init_list({})),
            1);
    }
    auto const state_name{modules.state_name[module_row]};
    auto const equality{ir.function("operator==", "bool")};
    ir.set_function_return_type(equality, ir.types.intern("bool"), ReturnSyntax::trailing);
    ir.set_function_parameters(
        equality, {ir.parameter(ir.types.intern(context.text(state_name) + " const&"))});
    ir.set_function_receiver(equality, FunctionReceiver::const_);
    ir.set_function_defaulted(equality);
    state_members.new_lines(2).add(ir.header_function(equality), 1);
    auto const state{ir.record(state_name, state_members.build())};
    ir.set_record_export_specifier(state, modules.export_specifier[module_row]);
    ir.set_record_dependencies(state, dependencies);
    header.add(state, 2).add(
        ir.using_declaration(
            ir.strings.intern("FGameSettingValue"),
            ir.types.intern("std::variant<" + join(value_types, ", ") + ">", dependencies)),
        2);
    for (auto const function : functions) {
        header.add(ir.declaration(function), 1);
    }
    header.new_lines(2).add(settings_api(context, module, setting_types));

    auto nodes{header.build()};
    if (auto const namespace_name{
            declarations.module_settings_store
                .namespace_name[declarations.module_settings_store.index(settings)]}) {
        return {ir.namespace_node(*namespace_name, nodes)};
    }
    return nodes;
}

auto build_source(LoweringContext& context,
                  ModuleHandle const module,
                  std::span<FunctionId const> const functions) -> Nodes {
    auto& ir{context.ir};
    auto const& modules{context.declarations.settings_modules};
    auto const& setting_schemas{context.declarations.settings};
    auto const& controls{context.declarations.setting_controls};
    auto const& category_schemas{context.declarations.setting_categories};
    auto const& declarations{context.declarations};
    auto const module_row{modules.index(module)};
    auto const settings{modules.settings[module_row]};
    auto enum_value = [&](std::string_view const type, std::string_view const name) {
        return ir.named(std::format("{}::{}", type, name));
    };

    std::vector<Expr> categories;
    for (auto const category : declarations.items(modules.categories[module_row])) {
        auto const row{category_schemas.index(category)};
        categories.push_back(ir.init_list({
            enum_value("EGameSettingCategory",
                       title_case(context.text(category_schemas.name[row]))),
            text_value(ir, context.text(category_schemas.label[row])),
        }));
    }
    std::vector<Expr> descriptors;
    for (auto const setting : declarations.items(modules.settings_list[module_row])) {
        auto const row{setting_schemas.index(setting)};
        auto const control_row{controls.index(setting_schemas.control[row])};
        auto const options{controls.options_provider[control_row]};
        auto const availability{controls.availability_provider[control_row]};
        descriptors.push_back(ir.init_list({
            enum_value("EGameSetting", title_case(context.text(setting_schemas.name[row]))),
            enum_value("EGameSettingCategory",
                       title_case(context.text(setting_schemas.category[row]))),
            enum_value("EGameSettingValueType", setting_type_name(context, setting)),
            enum_value("EGameSettingBackend",
                       title_case(context.text(setting_schemas.backend[row]))),
            enum_value("ESettingApplyMode", enum_name(setting_schemas.apply_mode[row])),
            enum_value("EGameSettingDevice", enum_name(setting_schemas.device[row])),
            enum_value("ESettingControlKind", enum_name(controls.kind[control_row])),
            enum_value("EGameSettingOptionProvider",
                       options ? title_case(context.text(*options)) : "None"),
            enum_value("EGameSettingAvailabilityProvider",
                       availability ? title_case(context.text(*availability)) : "Always"),
            text_literal(ir, context.text(setting_schemas.name[row])),
            text_value(ir, context.text(setting_schemas.label[row])),
            text_value(ir, context.optional_text(setting_schemas.tooltip[row]).value_or("")),
            ir.literal(std::format("{:.6g}", controls.minimum[control_row].value_or(0.0))),
            ir.literal(std::format("{:.6g}", controls.maximum[control_row].value_or(0.0))),
            ir.literal(std::format("{:.6g}", controls.step[control_row].value_or(0.0))),
            text_literal(ir, context.optional_text(controls.custom_row[control_row]).value_or("")),
        }));
    }

    NodeListBuilder globals{ir};
    globals
        .add(ir.static_member(ir.types.intern("TArray<FGameSettingCategoryDescriptor> const"),
                              ir.strings.intern("category_descriptors"),
                              ir.init_list(categories)),
             2)
        .add(ir.static_member(ir.types.intern("TArray<FGameSettingDescriptor> const"),
                              ir.strings.intern("descriptors"),
                              ir.init_list(descriptors)));
    NodeListBuilder source{ir};
    source.add(ir.namespace_node(ir.strings.intern(""), globals.build()), 2);
    for (auto const function : functions) {
        source.add(ir.definition(function), 2);
    }

    auto nodes{source.build()};
    if (auto const namespace_name{
            declarations.module_settings_store
                .namespace_name[declarations.module_settings_store.index(settings)]}) {
        return {ir.namespace_node(*namespace_name, nodes)};
    }
    return nodes;
}

} // namespace

auto lower_settings_module(LoweringContext& context, ModuleHandle module) -> ModuleId {
    auto& ir{context.ir};
    auto const& modules{context.declarations.settings_modules};
    auto const& setting_schemas{context.declarations.settings};
    auto const& settings_store{context.declarations.module_settings_store};
    auto const module_row{modules.index(module)};
    auto const settings{modules.settings[module_row]};
    auto const settings_row{settings_store.index(settings)};
    auto const setting_list{context.declarations.items(modules.settings_list[module_row])};
    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("uint8", "CoreMinimal.h"),
        ir.types.dependency("FText", "CoreMinimal.h"),
        ir.types.dependency("TConstArrayView", "Containers/ArrayView.h"),
        ir.types.dependency("std::variant", "variant"),
    };
    std::vector<CppTypeId> setting_types;
    setting_types.reserve(setting_list.size());
    for (auto const setting : setting_list) {
        setting_types.push_back(
            context.resolve(setting_schemas.value_type[setting_schemas.index(setting)]));
    }
    auto const functions{build_functions(context, module, setting_types)};
    auto header_declarations{build_header(context, module, setting_types, functions, dependencies)};

    NodeListBuilder header_nodes{ir};
    header_nodes.add(ir.include_dependencies(), 2);
    auto const prelude{settings_store.prelude_lines[settings_row]};
    if (!prelude.empty()) {
        header_nodes.add(ir.raw(join_lines(context, prelude)), 2);
    }
    header_nodes.append(std::move(header_declarations));

    auto const include_order{
        context.declarations.items(settings_store.include_order[settings_row])};
    auto const header_file{
        ir.files.append(settings_store.header[settings_row], header_nodes.build())};
    ir.files.flags[ir.files.index(header_file)].set_format_generated(true);
    ir.files.set_include_order(header_file, include_order);
    CppFileId source_file;
    if (auto const path{settings_store.source[settings_row]}) {
        NodeListBuilder source_nodes{ir};
        source_nodes
            .add(ir.include(ir.store_text(source_include(context, settings)), IncludeStyle::quoted),
                 2)
            .append(build_source(context, module, functions));
        source_file = ir.files.append(*path, source_nodes.build());
        auto const source_row{ir.files.index(source_file)};
        ir.files.flags[source_row].set_pragma_once(false);
        ir.files.flags[source_row].set_format_generated(true);
        ir.files.set_include_order(source_file, include_order);
    }
    return ir.modules.append(settings_store.name[settings_row], header_file, source_file);
}

} // namespace lispb::codegen::detail
