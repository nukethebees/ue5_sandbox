#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "settings_fields.h"
#include "string_enum_parser.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/settings_schema_schemas.h>
#include <lispb/strings.h>

#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {
namespace settings_schema {
auto member_expression(CppIR& ir, std::string_view path) -> Expr {
    auto result{ir.named("target")};
    while (true) {
        auto const separator{path.find('.')};
        result = ir.member_access(result, path.substr(0, separator));
        if (separator == std::string_view::npos) {
            return result;
        }
        path.remove_prefix(separator + 1);
    }
}

auto parser_function(LoweringContext& context,
                     std::string const& enum_name,
                     std::string const& stem,
                     std::span<StringId const> const paths) -> NodeId {
    auto& ir{context.ir};
    std::vector<StringEnumEntry> entries;
    entries.reserve(paths.size());
    for (auto const path : paths) {
        auto const name{context.text(path)};
        entries.push_back({name, enum_name + "::" + settings_schema_field_name(name)});
    }

    return lower_string_enum_parser(ir, "try_parse_" + stem + "_field", enum_name, "path", entries);
}

auto visitor_function(LoweringContext& context,
                      std::string const& enum_name,
                      std::string const& stem,
                      StringId const target,
                      StringId const target_header,
                      std::span<StringId const> const paths,
                      std::span<StringId const> const members) -> NodeId {
    auto& ir{context.ir};
    std::vector<SwitchCaseId> cases;
    auto const count{paths.size()};
    cases.reserve(count);
    for (std::size_t index{}; index < count; ++index) {
        cases.push_back(ir.switch_case(
            ir.named(enum_name + "::" + settings_schema_field_name(context.text(paths[index]))),
            ir.block({ir.return_stmt(ir.call(
                ir.named("callback"), {member_expression(ir, context.text(members[index]))}))})));
    }

    auto const visitor{ir.function("visit_" + stem + "_field", "bool")};
    ir.set_function_template(
        visitor,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Callback"))}));
    ir.set_function_parameters(
        visitor,
        {ir.parameter(ir.types.intern(
                          context.text(target) + "&",
                          {ir.types.dependency(context.text(target), context.text(target_header))}),
                      "target"),
         ir.parameter(enum_name + " const", "field"),
         ir.parameter("Callback&&", "callback")});
    ir.function_flags(visitor).set_define_in_header(true);
    ir.function_flags(visitor).set_is_nodiscard(true);
    ir.set_function_body(
        visitor,
        {ir.switch_stmt(ir.named("field"), cases),
         ir.expr_stmt(ir.call(ir.named("assert", {ir.types.dependency("assert", "cassert")}),
                              {ir.literal("false")})),
         ir.expr_stmt(ir.call(
             ir.named("std::unreachable", {ir.types.dependency("std::unreachable", "utility")})))});
    return ir.header_function(visitor);
}
}

auto lower_settings_schema(LoweringContext& context, DeclarationHandle const schema)
    -> DeclarationEmission {
    auto& ir{context.ir};
    auto const& schemas{context.declarations.settings_schemas};
    auto const row{schemas.index(schema)};
    auto const name{context.text(schemas.name[row])};
    auto const enum_name{name + "Field"};
    auto const stem{settings_schema_function_stem(name)};
    auto const fields{settings_fields(context.semantic, context.module, schema)};
    std::vector<StringId> paths;
    std::vector<StringId> members;
    paths.reserve(fields.size());
    members.reserve(fields.size());
    for (auto const& field : fields) {
        paths.push_back(ir.strings.intern(field.path));
        members.push_back(ir.strings.intern(field.member));
    }

    std::vector<EnumValueId> values;
    values.reserve(paths.size());
    for (auto const path : paths) {
        values.push_back(
            ir.enumerator(ir.strings.intern(settings_schema_field_name(context.text(path)))));
    }

    NodeListBuilder declarations{ir};
    declarations.add(
        ir.enumeration(
            ir.strings.intern(enum_name),
            ir.types.intern("std::uint32_t", {ir.types.dependency("std::uint32_t", "cstdint")}),
            values),
        2);
    declarations.add(settings_schema::parser_function(context, enum_name, stem, paths), 2);
    declarations.add(ir.line_comment("Require a valid field returned by the schema's key parser."),
                     1);
    declarations.add(settings_schema::visitor_function(
        context, enum_name, stem, schemas.target[row], schemas.target_header[row], paths, members));
    return {.header = declarations.build(), .format_generated = true};
}
}
