#include "lowering_utils.h"

#include "lowering_context.h"
#include <ioj/ascii.h>

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_trailing_return_placement.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/type_registry.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
auto snake_case_type_name(std::string_view const name) -> std::string {
    auto const begin{name.size() > 1 && name.front() == 'E' && ioj::isupper(name[1])
                         ? std::size_t{1}
                         : std::size_t{0}};
    std::string result;
    for (auto index{begin}; index < name.size(); ++index) {
        auto const character{name[index]};
        auto const upper{ioj::isupper(character)};
        auto const previous_lower{index > begin && ioj::islower(name[index - 1])};
        auto const next_lower{index + 1 < name.size() && ioj::islower(name[index + 1])};
        if (upper && !result.empty() && (previous_lower || next_lower)) {
            result += '_';
        }
        result += ioj::tolower(character);
    }
    return result;
}

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string {
    std::string output;
    auto const count{values.size()};
    for (std::size_t index{0}; index < count; ++index) {
        if (index != 0) {
            output.append(separator);
        }
        output.append(values[index]);
    }
    return output;
}

auto join_lines(std::vector<std::string> const& values) -> std::string {
    return join(values, "\n");
}

auto join(LoweringContext const& ctx, SchemaRange<StringId> values, std::string_view separator)
    -> std::string {
    std::string result;
    bool first{true};
    for (auto const value : ctx.declarations.items(values)) {
        if (!first) {
            result += separator;
        }
        result += ctx.declarations.strings().view(value);
        first = false;
    }
    return result;
}

auto join_lines(LoweringContext const& ctx, SchemaRange<StringId> values) -> std::string {
    return join(ctx, values, "\n");
}

auto source_include(LoweringContext const& ctx, ModuleSettingsId settings) -> std::string {
    if (ctx.declarations.module_settings_store.header_include[settings].has_value()) {
        return ctx.text(*ctx.declarations.module_settings_store.header_include[settings]);
    }
    return std::filesystem::path{ctx.text(ctx.declarations.module_settings_store.header[settings])}
        .filename()
        .generic_string();
}

void append_type_dependencies(std::vector<TypeDependencyId>& result,
                              std::string const& key,
                              TypeRegistry const& types) {
    auto const found{types.find(key)};
    if (!found.has_value() ||
        types.cpp_types.dependencies(types.registered.cpp_type[types.registered.index(*found)])
            .empty()) {
        throw std::invalid_argument{"Unknown dependency type: " + key};
    }
    auto const dependencies{
        types.cpp_types.dependencies(types.registered.cpp_type[types.registered.index(*found)])};
    result.insert(result.end(), dependencies.begin(), dependencies.end());
}

auto title_case_identifier(std::string_view const identifier) -> std::string {
    std::string result;
    result.reserve(identifier.size());
    bool capitalize{true};
    for (auto const character : identifier) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        result.push_back(
            capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                       : character);
        capitalize = false;
    }
    return result;
}

namespace {

class FunctionLowering : public CppIRBuilder {
  public:
    explicit FunctionLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    auto lower_member_type(LoweringContext& ctx, ChildOccurrenceId member) -> CppTypeId {
        auto const& members{ctx.declarations.child_occurrences};
        auto const row{members.index(member)};
        auto const element{ctx.resolve(members.type[row])};
        auto const count{members.count[row]};
        if (!count) {
            return element;
        }
        return template_type(std_types().array, {element, intern_type(std::to_string(*count))});
    }
    auto column_apply_arrays_function(std::vector<std::string> const& columns) -> NodeId {
        std::vector<Expr> arguments;
        arguments.reserve(columns.size());
        for (auto const& column : columns) {
            arguments.push_back(member_access(named("self"), column));
        }
        FunctionDefinition created_function{.name = named("apply_arrays"),
                                            .return_type = intern_type("auto")};
        created_function.parameters = {parameter("this auto&&", "self"),
                                       parameter("TFunc&&", "func")};
        created_function.implementation = {.kind = FunctionImplementationKind::body,
                                           .body = {return_stmt(call_array_callback(arguments))}};
        created_function.return_type = intern_type("decltype(auto)");
        created_function.return_syntax = ReturnSyntax::trailing;
        created_function.flags.set_is_inline(true);
        created_function.flags.set_define_in_header(true);
        created_function.template_declaration =
            template_declaration({type_template_parameter(store_text("TFunc"))});
        return header_function(add_function(created_function));
    }
    auto column_apply_array_pairs_function(std::vector<std::string> const& columns) -> NodeId {
        std::vector<Expr> arguments;
        arguments.reserve(columns.size() * 2);
        for (auto const& column : columns) {
            arguments.push_back(member_access(named("self"), column));
            arguments.push_back(member_access(named("other"), column));
        }
        FunctionDefinition created_function{.name = named("apply_array_pairs"),
                                            .return_type = intern_type("auto")};
        created_function.parameters = {parameter("this Self&&", "self"),
                                       parameter("Other&&", "other"),
                                       parameter("TFunc&&", "func")};
        created_function.implementation = {.kind = FunctionImplementationKind::body,
                                           .body = {return_stmt(call_array_callback(arguments))}};
        created_function.return_type = intern_type("decltype(auto)");
        created_function.return_syntax = ReturnSyntax::trailing;
        created_function.flags.set_is_inline(true);
        created_function.flags.set_define_in_header(true);
        created_function.template_declaration =
            template_declaration({type_template_parameter(store_text("Self")),
                                  type_template_parameter(store_text("Other")),
                                  type_template_parameter(store_text("TFunc"))});
        created_function.formatting = {
            .trailing_return_placement = FunctionTrailingReturnPlacement::next_line,
        };
        return header_function(add_function(created_function));
    }
    auto lower_schema_function(LoweringContext& ctx,
                               FunctionSchemaId schema,
                               std::optional<std::span<Expr const>> body) -> FunctionDefinition {
        auto const& decls{ctx.declarations};
        auto const& functions{decls.functions};
        auto const& parameter_schemas{decls.parameters};
        auto const row{functions.index(schema)};
        std::vector<FunctionParameterId> parameters;
        for (auto const parameter_id : decls.items(functions.parameters[schema])) {
            auto const default_value{parameter_schemas.default_value[parameter_id]};
            auto type{ctx.resolve(parameter_schemas.type[parameter_id])};
            if (parameter_schemas.is_pack[parameter_id]) {
                type = decorate_type(type, "", "...");
            }
            parameters.push_back(parameter(type,
                                           parameter_schemas.name[parameter_id],
                                           default_value ? raw_expr(*default_value) : Expr{}));
        }
        std::vector<TypeDependencyId> dependencies;
        for (auto const key : decls.items(functions.dependencies[schema])) {
            append_type_dependencies(dependencies, ctx.text(key), ctx.types);
        }
        auto const trailing{functions.trailing_return_type[row]};
        auto const return_type{ctx.resolve(trailing.value_or(functions.return_type[row]))};
        FunctionDefinition created_function{.name = named(functions.name[row]),
                                            .return_type = return_type};
        created_function.parameters = std::move(parameters);
        created_function.implementation = {
            .kind = FunctionImplementationKind::body,
            .body = {body ? raw_parts(*body, dependencies)
                          : raw_lines(decls.items(functions.body_lines[schema]), dependencies)}};
        if (trailing) {
            created_function.return_type = return_type;
            created_function.return_syntax = ReturnSyntax::trailing;
        }
        created_function.receiver = functions.is_static[row] ? FunctionReceiver::static_
                                  : functions.is_const[row]  ? FunctionReceiver::const_
                                                             : FunctionReceiver::unqualified;
        if (functions.is_noexcept[row]) {
            created_function.exception_specification = {.kind = FunctionExceptionKind::noexcept_};
        }
        auto& flags{created_function.flags};
        flags.set_is_constexpr(functions.is_constexpr[schema]);
        flags.set_is_inline(functions.is_inline[schema]);
        flags.set_is_nodiscard(functions.is_nodiscard[schema]);
        flags.set_define_in_header(
            !functions.definition_in_source[row] &&
            (functions.is_inline[row] || !functions.body_lines[row].empty()));
        if (auto const parameters{functions.template_parameters[row]}) {
            created_function.template_declaration = raw_template_declaration(*parameters);
        }
        if (auto const constraint{functions.requires_clause[row]}) {
            created_function.requires_clause = raw_expr(*constraint);
        }
        return created_function;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto call_array_callback(std::span<Expr const> const arguments) -> Expr {
        auto const forwarded{call(template_id(type_reference(std_types().forward),
                                              {type_reference(intern_type("TFunc"))}),
                                  {named("func")})};
        return call(forwarded, arguments);
    }
};

} // namespace

auto column_apply_arrays_function(LoweringContext& ctx, std::vector<std::string> const& columns)
    -> NodeId {
    return FunctionLowering{ctx.ir}.column_apply_arrays_function(columns);
}
auto column_apply_array_pairs_function(LoweringContext& ctx,
                                       std::vector<std::string> const& columns) -> NodeId {
    return FunctionLowering{ctx.ir}.column_apply_array_pairs_function(columns);
}
auto lower_member_type(LoweringContext& ctx, ChildOccurrenceId member) -> CppTypeId {
    return FunctionLowering{ctx.ir}.lower_member_type(ctx, member);
}

auto lower_schema_function(LoweringContext& ctx,
                           FunctionSchemaId schema,
                           std::optional<std::span<Expr const>> body) -> FunctionDefinition {
    return FunctionLowering{ctx.ir}.lower_schema_function(ctx, schema, body);
}

} // namespace lispb::codegen::detail
