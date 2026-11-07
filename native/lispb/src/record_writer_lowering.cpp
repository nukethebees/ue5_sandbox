#include "record_writer_lowering.h"

#include "declaration_emission.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <filesystem>

namespace lispb::codegen::detail {
auto writer_header_path(std::string_view const header) -> std::string {
    auto const path{std::filesystem::path{header}};
    return (path.parent_path() / (path.stem().string() + "_writer" + path.extension().string()))
        .generic_string();
}

namespace {
class RecordWriterLowering : public CppIRBuilder {
  public:
    explicit RecordWriterLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    void
        lower(LoweringContext& ctx, DeclarationHandle const record, DeclarationEmission& emission) {
        auto const& records{ctx.declarations.records};
        auto const& members{ctx.declarations.child_occurrences};
        auto const type{ctx.semantic.find_declared(ctx.module, records.name[record]).value()};
        auto const value_type{named_type(ctx.text(ctx.semantic.cpp_spelling(type)) +
                                             (records.constraint_policy[record] ? "Value" : ""),
                                         NameOrigin::global)};
        auto const positional{records.s7_positional[record] != 0};
        auto body{node_list()};
        body.add(variable("auto",
                          "form",
                          member_call(named("ir"),
                                      "form",
                                      {string_literal(ctx.text(*records.s7_tag[record]))})),
                 2);
        for (auto const field : ctx.declarations.items(records.members[record])) {
            auto const& options{members.s7[field]};
            if (options.exclude) {
                continue;
            }
            auto const member{members.name[field]};
            auto key{ctx.text(options.name.value_or(member))};
            if (!options.name) {
                std::ranges::replace(key, '_', '-');
            }
            auto const value{member_access(named("value"), member)};
            if (positional) {
                body.add(
                    expr_stmt(member_call(named("form"),
                                          "add",
                                          {call(named_type("ioj::s7::write", NameOrigin::global),
                                                {named("ir"), value})})));
            } else {
                auto const base{conditional(
                    named("base"),
                    unary(UnaryOperator::address_of,
                          member_access(unary(UnaryOperator::dereference, named("base")), member)),
                    named("nullptr"))};
                body.add(expr_stmt(
                    call(named_type("ioj::s7::write_property", NameOrigin::global),
                         {named("ir"), named("form"), string_literal(key), value, base})));
            }
        }
        body.add(return_stmt(member_call(named("form"), "build")), 2);
        FunctionDefinition const function{
            .name = name("write"),
            .return_type = named_type("lispb::codegen::sexpr::Expr", NameOrigin::global),
            .parameters = {parameter(decorate_type(named_type("lispb::codegen::sexpr::SExprIR",
                                                              NameOrigin::global),
                                                   "",
                                                   "&"),
                                     "ir"),
                           parameter(decorate_type(value_type, "", " const&"), "value"),
                           parameter(decorate_type(value_type, "", " const*"),
                                     positional ? "" : "base")},
            .implementation = {.kind = FunctionImplementationKind::body, .body = body.build()},
            .receiver = FunctionReceiver::static_,
            .flags = {.define_in_header = true}};
        emission.writer_declarations.push_back(
            add_record({.name = template_id(name("TypeWriter"), {type_reference(value_type)}),
                        .children = {header_function(add_function(function))},
                        .template_declaration = template_specialization()}));
        auto const includes{ctx.declarations.items(records.s7_writer_includes[record])};
        emission.writer_includes.insert(
            emission.writer_includes.end(), includes.begin(), includes.end());
        for (auto const dependency : ctx.semantic.dependencies_of(type)) {
            auto const declaration{ctx.semantic.source_declaration(dependency)};
            auto const module{ctx.semantic.module(dependency)};
            if (declaration.value != 0 && declaration.kind() == DeclarationKind::record &&
                records.generate_s7_writer[declaration] && module != ctx.module) {
                emission.writer_includes.push_back(store_text(writer_header_path(
                    source_include(ctx, ctx.declarations.module_settings(module)))));
            }
        }
    }
};
}

void lower_record_writer(LoweringContext& ctx,
                         DeclarationHandle const record,
                         DeclarationEmission& emission) {
    RecordWriterLowering{ctx.ir}.lower(ctx, record, emission);
}
}
