#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir_builder.h>
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
#include <filesystem>
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

class SettingsLowering : public CppIRBuilder {
  public:
    explicit SettingsLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    auto lower_settings_module(LoweringContext& ctx, ModuleHandle module) -> std::vector<ModuleId> {
        auto const& modules{ctx.declarations.settings_modules};
        auto const& setting_schemas{ctx.declarations.settings};
        auto const& settings_store{ctx.declarations.module_settings_store};
        auto const module_row{modules.index(module)};
        auto const settings{modules.settings[module_row]};
        auto const settings_row{settings_store.index(settings)};
        auto const setting_list{ctx.declarations.items(modules.settings_list[module_row])};
        std::vector<CppTypeId> setting_types;
        setting_types.reserve(setting_list.size());
        for (auto const setting : setting_list) {
            setting_types.push_back(
                ctx.resolve(setting_schemas.value_type[setting_schemas.index(setting)]));
        }
        auto const functions{build_functions(ctx, module, setting_types)};
        auto result{build_type_headers(ctx, module, setting_types)};
        auto const include_order{
            ctx.declarations.items(settings_store.include_order[settings_row])};
        auto const include_directory{
            std::filesystem::path{source_include(ctx, settings)}.parent_path()};
        auto header_nodes{node_list()};
        for (auto const type :
             {"GameSettingCategoryDescriptor", "GameSettingDescriptor", "GameSettingValue"}) {
            auto const include_value{
                (include_directory / (snake_case_type_name(type) + ".h")).generic_string()};
            header_nodes.add(include(store_text(include_value), IncludeStyle::quoted), 1);
        }
        auto const state_include{
            (include_directory /
             (snake_case_type_name(ctx.text(modules.state_name[module_row])) + ".h"))
                .generic_string()};
        header_nodes.add(include(store_text(state_include), IncludeStyle::quoted), 1)
            .add(include_dependencies(), 2);
        auto const prelude{settings_store.prelude_lines[settings_row]};
        if (!prelude.empty()) {
            header_nodes.add(raw(join_lines(ctx, prelude)), 2);
        }
        auto declarations{node_list()};
        for (auto function_id : functions) {
            declarations.add(declaration(function_id), 1);
        }
        if (auto const scope{settings_store.namespace_name[settings_row]}) {
            header_nodes.add(namespace_node(*scope, declarations.build()));
        } else {
            header_nodes.append(declarations.build());
        }
        auto const header_file{
            append_file(settings_store.header[settings_row], header_nodes.build())};
        file_flags(header_file).set_format_generated(true);
        set_file_include_order(header_file, include_order);
        CppFileId source_file;
        if (auto const path{settings_store.source[settings_row]}) {
            auto source_nodes{node_list()};
            source_nodes
                .add(include(store_text(source_include(ctx, settings)), IncludeStyle::quoted), 1)
                .add(include_dependencies(), 2)
                .append(build_source(ctx, module, functions));
            source_file = append_file(*path, source_nodes.build());
            file_flags(source_file).set_pragma_once(false);
            file_flags(source_file).set_format_generated(true);
            set_file_include_order(source_file, include_order);
        }
        result.push_back(
            append_module(settings_store.name[settings_row], header_file, source_file));
        return result;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
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
    auto setting_type_name(LoweringContext& ctx, SettingId const setting) -> std::string {
        auto const& decls{ctx.declarations};
        auto const type{decls.settings.value_type[setting]};
        auto const name{ctx.text(decls.type_refs.name[type])};
        return title_case(name.starts_with('@') ? name.substr(1) : name);
    }
    auto text_literal(std::string_view const value) -> Expr { return string_literal(value); }
    auto text_value(std::string_view const value) -> Expr { return string_literal(value); }
    auto build_functions(LoweringContext& ctx,
                         ModuleHandle const module,
                         std::span<CppTypeId const> const setting_types)
        -> std::vector<FunctionId> {
        auto const& decls{ctx.declarations};
        auto const module_row{decls.settings_modules.index(module)};
        auto const state_name{ctx.text(decls.settings_modules.state_name[module_row])};
        auto const settings{decls.items(decls.settings_modules.settings_list[module_row])};
        auto const state{named("state")};
        auto const id{named("id")};
        auto const value{named("value")};
        auto const id_parameter{parameter("GameSetting const", "id")};
        auto create = [&](std::string_view const name, CppTypeId const type) {
            FunctionDefinition function_id{.name = named(name), .return_type = type};
            function_id.return_type = type;
            function_id.return_syntax = ReturnSyntax::trailing;
            function_id.export_specifier = decls.settings_modules.export_specifier[module_row];
            return function_id;
        };

        auto span_type = [&](std::string_view const element) {
            return template_type(std_types().span,
                                 {decorate_type(named_type(element), "", " const")});
        };
        auto categories{create("game_setting_category_descriptors",
                               span_type("GameSettingCategoryDescriptor"))};
        categories.implementation = {.kind = FunctionImplementationKind::body,
                                     .body = {return_stmt(named("category_descriptors"))}};

        auto descriptors{create("game_setting_descriptors", span_type("GameSettingDescriptor"))};
        descriptors.implementation = {.kind = FunctionImplementationKind::body,
                                      .body = {return_stmt(named("descriptors"))}};

        auto descriptor{create("game_setting_descriptor",
                               decorate_type(named_type("GameSettingDescriptor"), "", " const&"))};
        descriptor.parameters = {id_parameter};
        descriptor.implementation = {
            .kind = FunctionImplementationKind::body,
            .body = {return_stmt(
                subscript(named("descriptors"), static_cast_expr(std_types().size_t, id)))}};

        auto getter{create("game_setting_value", named_type("GameSettingValue"))};
        getter.parameters = {parameter(state_name + " const&", "state"), id_parameter};
        std::vector<SwitchCaseId> getter_cases;
        for (auto const setting : settings) {
            auto const name{decls.settings.name[setting]};
            getter_cases.push_back(
                switch_case(type_reference(decorate_type(
                                named_type("GameSetting"), "", "::" + title_case(ctx.text(name)))),
                            block({return_stmt(init_list({member_access(state, name)},
                                                         intern_type("GameSettingValue")))})));
        }
        auto const first_name{decls.settings.name[settings.front()]};
        getter.implementation = {.kind = FunctionImplementationKind::body,
                                 .body = {switch_stmt(id, getter_cases),
                                          assertion(literal("false")),
                                          return_stmt(init_list({member_access(state, first_name)},
                                                                intern_type("GameSettingValue")))}};

        auto setter{create("set_game_setting_value", std_types().bool_)};
        setter.parameters = {parameter(state_name + "&", "state"),
                             id_parameter,
                             parameter("GameSettingValue const&", "value")};
        std::vector<SwitchCaseId> setter_cases;
        auto const setting_count{settings.size()};
        for (std::size_t index{}; index < setting_count; ++index) {
            auto const name{decls.settings.name[settings[index]]};
            auto const typed_value{named("typed_value")};
            setter_cases.push_back(switch_case(
                type_reference(decorate_type(
                    named_type("GameSetting"), "", "::" + title_case(ctx.text(name)))),
                block({
                    variable("auto const*",
                             "typed_value",
                             call(template_id(type_reference(std_types().get_if),
                                              {type_reference(setting_types[index])}),
                                  {unary(UnaryOperator::address_of, value)})),
                    if_stmt(binary(BinaryOperator::equal, typed_value, literal("nullptr")),
                            block({return_stmt(literal("false"))})),
                    assignment(member_access(state, name),
                               unary(UnaryOperator::dereference, typed_value)),
                    return_stmt(literal("true")),
                })));
        }
        setter.implementation = {
            .kind = FunctionImplementationKind::body,
            .body = {switch_stmt(id, setter_cases), return_stmt(literal("false"))}};

        return {add_function(categories),
                add_function(descriptors),
                add_function(descriptor),
                add_function(getter),
                add_function(setter)};
    }
    auto settings_api(LoweringContext& ctx,
                      ModuleHandle module,
                      std::span<CppTypeId const> setting_types) -> NodeId {
        auto const& modules{ctx.declarations.settings_modules};
        auto const module_row{modules.index(module)};
        auto members{node_list()};
        members.add(access(Access::public_), 1);
        auto const settings{ctx.declarations.items(modules.settings_list[module_row])};
        auto const setting_count{settings.size()};
        for (std::size_t index{}; index < setting_count; ++index) {
            auto const setting{settings[index]};
            auto const name{ctx.declarations.settings.name[setting]};
            FunctionDefinition getter{.name = named(name), .return_type = intern_type("auto")};
            getter.return_type = setting_types[index];
            getter.return_syntax = ReturnSyntax::trailing;
            getter.receiver = FunctionReceiver::const_;
            getter.flags.set_define_in_header(true);
            getter.implementation = {
                .kind = FunctionImplementationKind::body,
                .body = {return_stmt(member_access(
                    call(member_access(call(named("derived")), "settings_state")), name))}};
            members.add(header_function(add_function(getter)), 2);

            FunctionDefinition setter{.name = named("set_" + ctx.text(name)),
                                      .return_type = std_types().void_};
            setter.flags.set_define_in_header(true);
            setter.parameters = {
                parameter(decorate_type(setting_types[index], "", " const"), "value")};
            setter.implementation = {
                .kind = FunctionImplementationKind::body,
                .body = {expr_stmt(
                    call(member_access(call(named("derived")), "set_setting"),
                         {type_reference(decorate_type(
                              named_type("GameSetting"), "", "::" + title_case(ctx.text(name)))),
                          init_list({named("value")}, intern_type("GameSettingValue"))}))}};
            members.add(header_function(add_function(setter)), 2);
        }
        members.add(access(Access::private_), 1);
        for (auto const is_const : {false, true}) {
            auto const derived_type{intern_type(is_const ? "Derived const&" : "Derived&")};
            FunctionDefinition function_id{.name = named("derived"),
                                           .return_type = intern_type("auto")};
            function_id.return_type = derived_type;
            function_id.return_syntax = ReturnSyntax::trailing;
            function_id.receiver =
                is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified;
            function_id.flags.set_define_in_header(true);
            function_id.formatting.body_layout = FunctionBodyLayout::compact;
            function_id.implementation = {
                .kind = FunctionImplementationKind::body,
                .body = {return_stmt(static_cast_expr(
                    derived_type, unary(UnaryOperator::dereference, named("this"))))}};
            members.add(header_function(add_function(function_id)), 1);
        }
        auto const result{
            record(modules.api_name[module_row], members.build(), RecordKind::class_)};
        set_record_template(result,
                            template_declaration({type_template_parameter(store_text("Derived"))}));
        return result;
    }
    auto build_type_headers(LoweringContext& ctx,
                            ModuleHandle const module,
                            std::span<CppTypeId const> const setting_types)
        -> std::vector<ModuleId> {
        auto const& modules{ctx.declarations.settings_modules};
        auto const& setting_schemas{ctx.declarations.settings};
        auto const& controls{ctx.declarations.setting_controls};
        auto const& decls{ctx.declarations};
        auto const module_row{modules.index(module)};
        auto const settings{modules.settings[module_row]};
        auto const setting_list{decls.items(modules.settings_list[module_row])};
        std::vector<std::string> setting_names;
        std::vector<std::string> category_names;
        std::vector<CppTypeId> value_types;
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
            if (unique_types.insert(type_spelling_id(type)).second) {
                value_types.push_back(type);
            }
            auto const type_name{setting_type_name(ctx, setting)};
            if (unique_type_names.insert(type_name).second) {
                type_names.push_back(type_name);
            }
            setting_names.push_back(title_case(ctx.text(setting_schemas.name[row])));
            backends.insert(ctx.text(setting_schemas.backend[row]));
            if (auto const provider{controls.options_provider[control_row]}) {
                option_providers.insert(ctx.text(*provider));
            }
            if (auto const provider{controls.availability_provider[control_row]}) {
                availability_providers.insert(ctx.text(*provider));
            }
        }
        for (auto const category : decls.items(modules.categories[module_row])) {
            category_names.push_back(title_case(ctx.text(decls.setting_categories.name[category])));
        }

        auto const& settings_store{decls.module_settings_store};
        auto const settings_row{settings_store.index(settings)};
        auto const output_directory{
            std::filesystem::path{ctx.text(settings_store.header[settings_row])}.parent_path()};
        auto const include_directory{
            std::filesystem::path{source_include(ctx, settings)}.parent_path()};
        auto const include_order{decls.items(settings_store.include_order[settings_row])};
        auto const prelude{settings_store.prelude_lines[settings_row]};
        std::vector<ModuleId> result;
        auto emit_header = [&](std::string_view const name,
                               NodeId const declaration_id,
                               std::initializer_list<std::string_view> const type_headers) {
            auto header{node_list()};
            header.add(include_dependencies(), 1);
            for (auto const type : type_headers) {
                auto const include_value{
                    (include_directory / (snake_case_type_name(type) + ".h")).generic_string()};
                header.add(include(store_text(include_value), IncludeStyle::quoted), 1);
            }
            header.new_lines(2);
            if (!prelude.empty()) {
                header.add(raw(join_lines(ctx, prelude)), 2);
            }
            if (auto const scope{settings_store.namespace_name[settings_row]}) {
                header.add(namespace_node(*scope, {declaration_id}));
            } else {
                header.add(declaration_id);
            }
            auto const path{output_directory / (snake_case_type_name(name) + ".h")};
            auto const file{append_file(store_text(path.generic_string()), header.build())};
            file_flags(file).set_format_generated(true);
            set_file_include_order(file, include_order);
            result.push_back(append_module(store_text(name), file));
        };
        auto add_enum = [&](std::string_view const name, std::vector<std::string> const& names) {
            std::vector<EnumValueId> values;
            for (auto const& value : names) {
                values.push_back(enumerator(store_text(value)));
            }
            emit_header(name, enumeration(store_text(name), std_types().u8, values), {});
        };
        add_enum("GameSetting", setting_names);
        add_enum("GameSettingCategory", category_names);
        add_enum("SettingApplyMode", {"Immediate", "Deferred", "Confirm"});
        add_enum("SettingControlKind",
                 {"Toggle", "Choice", "FloatRange", "IntegerRange", "Custom"});
        add_enum("GameSettingDevice", {"Shared", "KeyboardMouse", "Controller"});
        add_enum("GameSettingValueType", type_names);
        std::vector<std::string> backend_names;
        for (auto const& backend : backends) {
            backend_names.push_back(title_case(backend));
        }
        add_enum("GameSettingBackend", backend_names);
        std::vector<std::string> option_names{"None"};
        for (auto const& provider : option_providers) {
            option_names.push_back(title_case(provider));
        }
        add_enum("GameSettingOptionProvider", option_names);
        std::vector<std::string> availability_names{"Always"};
        for (auto const& provider : availability_providers) {
            availability_names.push_back(title_case(provider));
        }
        add_enum("GameSettingAvailabilityProvider", availability_names);

        auto add_record = [&](std::string_view const name,
                              std::initializer_list<std::pair<CppTypeId, std::string_view>> fields,
                              std::initializer_list<std::string_view> const includes) {
            auto members{node_list()};
            for (auto const& [type, member] : fields) {
                members.add(instance_member(type, store_text(member)), 1);
            }
            auto const record_id{record(store_text(name), members.build())};
            set_record_export_specifier(record_id, modules.export_specifier[module_row]);
            emit_header(name, record_id, includes);
        };
        add_record("GameSettingCategoryDescriptor",
                   {{named_type("GameSettingCategory"), "id"}, {std_types().string_view, "label"}},
                   {"GameSettingCategory"});
        add_record("GameSettingDescriptor",
                   {{named_type("GameSetting"), "id"},
                    {named_type("GameSettingCategory"), "category"},
                    {named_type("GameSettingValueType"), "value_type"},
                    {named_type("GameSettingBackend"), "backend"},
                    {named_type("SettingApplyMode"), "apply_mode"},
                    {named_type("GameSettingDevice"), "device"},
                    {named_type("SettingControlKind"), "control_kind"},
                    {named_type("GameSettingOptionProvider"), "options_provider"},
                    {named_type("GameSettingAvailabilityProvider"), "availability_provider"},
                    {decorate_type(std_types().char_, "", " const*"), "name"},
                    {std_types().string_view, "label"},
                    {std_types().string_view, "tooltip"},
                    {std_types().f64, "minimum"},
                    {std_types().f64, "maximum"},
                    {std_types().f64, "step"},
                    {decorate_type(std_types().char_, "", " const*"), "custom_row"}},
                   {"GameSetting",
                    "GameSettingCategory",
                    "GameSettingValueType",
                    "GameSettingBackend",
                    "SettingApplyMode",
                    "GameSettingDevice",
                    "SettingControlKind",
                    "GameSettingOptionProvider",
                    "GameSettingAvailabilityProvider"});

        auto state_members{node_list()};
        for (std::size_t index{}; index < setting_count; ++index) {
            state_members.add(
                instance_member(setting_types[index],
                                setting_schemas.name[setting_schemas.index(setting_list[index])],
                                init_list({})),
                1);
        }
        auto const state_name{modules.state_name[module_row]};
        FunctionDefinition equality{.name = named("operator=="), .return_type = std_types().bool_};
        equality.return_type = std_types().bool_;
        equality.return_syntax = ReturnSyntax::trailing;
        equality.parameters = {parameter(intern_type(ctx.text(state_name) + " const&"))};
        equality.receiver = FunctionReceiver::const_;
        equality.implementation = {.kind = FunctionImplementationKind::defaulted};
        state_members.new_lines(2).add(header_function(add_function(equality)), 1);
        auto const state{record(state_name, state_members.build())};
        set_record_export_specifier(state, modules.export_specifier[module_row]);
        emit_header(ctx.text(state_name), state, {});
        emit_header("GameSettingValue",
                    using_declaration(store_text("GameSettingValue"),
                                      template_type(std_types().variant, value_types)),
                    {});
        emit_header(ctx.text(modules.api_name[module_row]),
                    settings_api(ctx, module, setting_types),
                    {"GameSetting", ctx.text(state_name), "GameSettingValue"});
        return result;
    }
    auto build_source(LoweringContext& ctx,
                      ModuleHandle const module,
                      std::span<FunctionId const> const functions) -> Nodes {
        auto const& modules{ctx.declarations.settings_modules};
        auto const& setting_schemas{ctx.declarations.settings};
        auto const& controls{ctx.declarations.setting_controls};
        auto const& category_schemas{ctx.declarations.setting_categories};
        auto const& decls{ctx.declarations};
        auto const module_row{modules.index(module)};
        auto const settings{modules.settings[module_row]};
        auto enum_value = [&](std::string_view const type, std::string_view const name) {
            return type_reference(decorate_type(named_type(type), "", "::" + std::string{name}));
        };

        std::vector<Expr> categories;
        for (auto const category : decls.items(modules.categories[module_row])) {
            auto const row{category_schemas.index(category)};
            categories.push_back(init_list({
                enum_value("GameSettingCategory", title_case(ctx.text(category_schemas.name[row]))),
                text_value(ctx.text(category_schemas.label[row])),
            }));
        }
        std::vector<Expr> descriptors;
        for (auto const setting : decls.items(modules.settings_list[module_row])) {
            auto const row{setting_schemas.index(setting)};
            auto const control_row{controls.index(setting_schemas.control[row])};
            auto const options{controls.options_provider[control_row]};
            auto const availability{controls.availability_provider[control_row]};
            descriptors.push_back(init_list({
                enum_value("GameSetting", title_case(ctx.text(setting_schemas.name[row]))),
                enum_value("GameSettingCategory",
                           title_case(ctx.text(setting_schemas.category[row]))),
                enum_value("GameSettingValueType", setting_type_name(ctx, setting)),
                enum_value("GameSettingBackend",
                           title_case(ctx.text(setting_schemas.backend[row]))),
                enum_value("SettingApplyMode", enum_name(setting_schemas.apply_mode[row])),
                enum_value("GameSettingDevice", enum_name(setting_schemas.device[row])),
                enum_value("SettingControlKind", enum_name(controls.kind[control_row])),
                enum_value("GameSettingOptionProvider",
                           options ? title_case(ctx.text(*options)) : "None"),
                enum_value("GameSettingAvailabilityProvider",
                           availability ? title_case(ctx.text(*availability)) : "Always"),
                text_literal(ctx.text(setting_schemas.name[row])),
                text_value(ctx.text(setting_schemas.label[row])),
                text_value(ctx.optional_text(setting_schemas.tooltip[row]).value_or("")),
                literal(std::format("{:.6g}", controls.minimum[control_row].value_or(0.0))),
                literal(std::format("{:.6g}", controls.maximum[control_row].value_or(0.0))),
                literal(std::format("{:.6g}", controls.step[control_row].value_or(0.0))),
                text_literal(ctx.optional_text(controls.custom_row[control_row]).value_or("")),
            }));
        }

        auto globals{node_list()};
        auto array_type = [&](std::string_view const element, std::size_t const count) {
            return decorate_type(
                template_type(std_types().array,
                              {named_type(element), intern_type(std::to_string(count))}),
                "",
                " const");
        };
        globals
            .add(static_member(array_type("GameSettingCategoryDescriptor", categories.size()),
                               store_text("category_descriptors"),
                               init_list({init_list(categories)})),
                 2)
            .add(static_member(array_type("GameSettingDescriptor", descriptors.size()),
                               store_text("descriptors"),
                               init_list({init_list(descriptors)})));
        auto source{node_list()};
        source.add(namespace_node(store_text(""), globals.build()), 2);
        for (auto function_id : functions) {
            source.add(definition(function_id), 2);
        }

        auto nodes{source.build()};
        if (auto const namespace_name{decls.module_settings_store.namespace_name[settings]}) {
            return {namespace_node(*namespace_name, nodes)};
        }
        return nodes;
    }
};

} // namespace

auto lower_settings_module(LoweringContext& ctx, ModuleHandle module) -> std::vector<ModuleId> {
    return SettingsLowering{ctx.ir}.lower_settings_module(ctx, module);
}

} // namespace lispb::codegen::detail
