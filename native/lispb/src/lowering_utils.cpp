#include "lowering_utils.h"

#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_trailing_return_placement.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/type_registry.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <array>
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
auto join(LoweringContext const& context,
          SchemaRange<IdentifierId> values,
          std::string_view separator) -> std::string {
    std::string result;
    bool first{true};
    for (auto const value : context.declarations.items(values)) {
        if (!first) {
            result += separator;
        }
        result += context.declarations.strings().view(value);
        first = false;
    }
    return result;
}
auto join_lines(LoweringContext const& context, SchemaRange<StringId> values) -> std::string {
    std::string result;
    bool first{true};
    for (auto const value : context.declarations.items(values)) {
        if (!first) {
            result += '\n';
        }
        result += context.declarations.strings().view(value);
        first = false;
    }
    return result;
}

auto source_include(LoweringContext const& context, ModuleSettingsId settings) -> std::string {
    if (context.declarations.module_settings_store
            .header_include[context.declarations.module_settings_store.index(settings)]
            .has_value()) {
        return context.text(
            *context.declarations.module_settings_store
                 .header_include[context.declarations.module_settings_store.index(settings)]);
    }
    return std::filesystem::path{
        context.text(context.declarations.module_settings_store
                         .header[context.declarations.module_settings_store.index(settings)])}
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

auto qualify(CppIR& ir, CppTypeId type, std::string const& suffix) -> CppTypeId {
    return ir.types.derive(type, std::string{ir.types.spelling(type)} + suffix);
}

auto native_spelling(std::string const& spelling) -> std::string {
    constexpr std::array integer_types{
        std::pair{"int8", "std::int8_t"},
        std::pair{"uint8", "std::uint8_t"},
        std::pair{"int16", "std::int16_t"},
        std::pair{"uint16", "std::uint16_t"},
        std::pair{"int32", "std::int32_t"},
        std::pair{"uint32", "std::uint32_t"},
        std::pair{"int64", "std::int64_t"},
        std::pair{"uint64", "std::uint64_t"},
    };
    for (auto const& [source, destination] : integer_types) {
        if (spelling == source) {
            return destination;
        }
    }
    return spelling;
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
auto column_apply_arrays_function(CppIR& ir, std::vector<std::string> const& columns) -> NodeId {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{columns.size()};
    for (std::size_t index{0}; index < count; ++index) {
        body.push_back("    self." + columns[index] + (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    auto const created_function{
        ir.function(ir.strings.intern("apply_arrays"), ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern("this auto&&"), ir.strings.intern("self")),
         ir.parameter(ir.types.intern("TFunc&&"), ir.strings.intern("func"))});
    ir.set_function_body(
        created_function,
        {ir.raw(join_lines(body), {ir.types.dependency("std::forward", "utility")})});
    ir.set_function_return_type(
        created_function, ir.types.intern("decltype(auto)"), ReturnSyntax::trailing);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    ir.set_function_template(
        created_function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    return ir.header_function(created_function);
}

auto column_apply_array_pairs_function(CppIR& ir, std::vector<std::string> const& columns)
    -> NodeId {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{columns.size()};
    for (std::size_t index{0}; index < count; ++index) {
        auto const& name{columns[index]};
        body.push_back("    self." + name + ", other." + name + (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    auto const created_function{
        ir.function(ir.strings.intern("apply_array_pairs"), ir.types.intern("auto"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(ir.types.intern("this Self&&"), ir.strings.intern("self")),
         ir.parameter(ir.types.intern("Other&&"), ir.strings.intern("other")),
         ir.parameter(ir.types.intern("TFunc&&"), ir.strings.intern("func"))});
    ir.set_function_body(
        created_function,
        {ir.raw(join_lines(body), {ir.types.dependency("std::forward", "utility")})});
    ir.set_function_return_type(
        created_function, ir.types.intern("decltype(auto)"), ReturnSyntax::trailing);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    ir.set_function_template(
        created_function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Self")),
                                 ir.type_template_parameter(ir.strings.intern("Other")),
                                 ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    ir.function_formatting(created_function) = {
        .trailing_return_placement = FunctionTrailingReturnPlacement::next_line,
    };
    return ir.header_function(created_function);
}

auto lower_schema_function(LoweringContext& context,
                           FunctionSchemaId schema,
                           std::optional<std::string_view> body) -> FunctionId {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const row{declarations.functions.index(schema)};
    std::vector<FunctionParameterId> parameters;
    for (auto const id :
         declarations.items(context.declarations.functions
                                .parameters[context.declarations.functions.index(schema)])) {
        auto const parameter{id};
        auto resolved{context.resolve(
            declarations.parameters.type[declarations.parameters.index(parameter)])};
        if (declarations.parameters.default_value[declarations.parameters.index(parameter)]) {
            parameters.push_back(ir.parameter(
                resolved,
                declarations.parameters.name[declarations.parameters.index(parameter)],
                *declarations.parameters.default_value[declarations.parameters.index(parameter)]));
        } else {
            parameters.push_back(ir.parameter(
                resolved, declarations.parameters.name[declarations.parameters.index(parameter)]));
        }
    }
    std::vector<TypeDependencyId> dependencies;
    for (auto const key :
         declarations.items(context.declarations.functions
                                .dependencies[context.declarations.functions.index(schema)])) {
        append_type_dependencies(dependencies, context.text(key), context.types);
    }
    auto const trailing{declarations.functions.trailing_return_type[row]};
    auto const return_type{
        context.resolve(trailing.value_or(declarations.functions.return_type[row]))};
    auto const created_function{ir.function(declarations.functions.name[row], return_type)};
    ir.set_function_parameters(created_function, std::move(parameters));
    ir.set_function_body(
        created_function,
        {body ? ir.raw(*body, dependencies)
              : ir.raw_lines(declarations.items(
                                 context.declarations.functions
                                     .body_lines[context.declarations.functions.index(schema)]),
                             dependencies)});
    if (trailing) {
        ir.set_function_return_type(created_function, return_type, ReturnSyntax::trailing);
    }
    ir.set_function_receiver(created_function,
                             declarations.functions.is_static[row] ? FunctionReceiver::static_
                             : declarations.functions.is_const[row]
                                 ? FunctionReceiver::const_
                                 : FunctionReceiver::unqualified);
    if (declarations.functions.is_noexcept[row]) {
        ir.set_function_noexcept(created_function);
    }
    ir.function_flags(created_function)
        .set_is_constexpr(context.declarations.functions
                              .is_constexpr[context.declarations.functions.index(schema)]);
    ir.function_flags(created_function)
        .set_is_inline(
            context.declarations.functions.is_inline[context.declarations.functions.index(schema)]);
    ir.function_flags(created_function)
        .set_is_nodiscard(context.declarations.functions
                              .is_nodiscard[context.declarations.functions.index(schema)]);
    ir.function_flags(created_function)
        .set_define_in_header(!declarations.functions.definition_in_source[row] &&
                              (declarations.functions.is_inline[row] ||
                               !declarations.functions.body_lines[row].empty()));
    if (auto const parameters{declarations.functions.template_parameters[row]}) {
        ir.set_function_template(created_function, ir.raw_template_declaration(*parameters));
    }
    if (auto const constraint{declarations.functions.requires_clause[row]}) {
        ir.set_function_requires_clause(created_function, ir.raw_expression(*constraint));
    }
    return created_function;
}

} // namespace lispb::codegen::detail
