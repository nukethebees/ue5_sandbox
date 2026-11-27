#include "single_allocation_soa_emission.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_api.h"
#include "soa_receiver.h"

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {
namespace {

class SingleAllocationEmitter : public CppIRBuilder {
  public:
    explicit SingleAllocationEmitter(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    auto emit_single_allocation_layout(SingleAllocationModel const& model) -> Nodes {
        auto const& selected{model.columns};
        auto const base{template_type(
            named_type("Pointers::apply"),
            {named_type("ioj::single_allocation_layout::ColumnLayouts",
                        NameOrigin::global,
                        {dependency("ColumnLayouts", "ioj/single_allocation/layout.h")})})};
        auto children{node_list()};
        for (std::size_t index{}; index < selected.size(); ++index) {
            if (index != 0) {
                children.new_lines(1);
            }
            children.add(constexpr_member(
                std_types().size_t, store_text(selected[index].name + "_column"), literal(index)));
        }
        auto const layout{add_record({.name = this->name(store_text("Layout")),
                                      .children = children.build(),
                                      .bases = {base}})};
        auto result{node_list()};
        result.append(section_header("Layout")).new_lines(1).add(layout);
        return result.build();
    }
    auto emit_resolved_views(LoweringContext& ctx,
                             SingleAllocationModel const& model,
                             NodeListBuilder& source,
                             NodeListBuilder& declarations) -> Nodes {
        auto result{node_list()};
        result
            .add(using_import(
                named_type("ioj::soa_storage_detail::ColumnReceiver", NameOrigin::global)))
            .new_lines(1)
            .add(using_import(named_type("ioj::soa_storage_detail::column_at", NameOrigin::global)))
            .new_lines(2)
            .append(section_header("Columns"))
            .new_lines(1)
            .add(schema_columns(ctx, model.schema))
            .new_lines(1)
            .add(using_declaration(store_text("Pointers"), named_type("Columns::pointers")))
            .new_lines(2)
            .add(schema_node(ctx, model))
            .new_lines(2)
            .append(emit_pointer_views(ctx, model, source, declarations));
        return result.build();
    }
    auto emit_compact_views(LoweringContext& ctx,
                            SingleAllocationModel const& model,
                            NodeListBuilder& source) -> Nodes {
        auto result{node_list()};
        auto const schema{model.schema};
        result.append(section_header("Compact views")).new_lines(1);
        for (bool const is_const : {true, false}) {
            std::string const name{is_const ? "CompactConstView" : "CompactView"};
            auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
            auto const base{view_base(ctx, schema, receiver, name, false)};
            auto children{node_list()};
            children.add(using_import(named_type("implementation_type::implementation_type")))
                .new_lines(1);
            auto api{lower_soa_api(ctx, schema, receiver, name)};
            children.append(api.header);
            if (!api.source.empty()) {
                source.append(api.source).new_lines(2);
            }
            auto const record_id{add_record(
                {.name = this->name(store_text(name)),
                 .children = children.build(),
                 .bases = {base},
                 .export_specifier =
                     ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(schema)],
                 .dependencies = child_dependencies(ctx, schema, true)})};
            result.add(record_id).new_lines(1).append(view_validation_nodes(name)).new_lines(1);
        }
        return result.build();
    }
    auto emit_single_allocation_container(LoweringContext& ctx,
                                          SingleAllocationModel const& model,
                                          NodeListBuilder& source) -> NodeId {
        auto const schema{model.schema};
        auto const& soas{ctx.declarations.soas};
        auto const owner_name{soas.name[soas.index(schema)]};
        auto const scope{schema_namespace(ctx, schema)};
        auto children{node_list()};
        auto const owner{
            template_type(named_type("ioj::native_soa::Owner",
                                     NameOrigin::global,
                                     {dependency("Owner", "ioj/native_soa/storage.h")}),
                          {named_type(scope + "::CompactView", NameOrigin::global),
                           named_type(scope + "::CompactConstView", NameOrigin::global)})};
        auto const base{
            soa_row_operations(ctx, schema, SoaReceiver::owner, ctx.text(owner_name), owner)};
        children.add(using_import(named_type("implementation_type::implementation_type")))
            .new_lines(1);
        auto api{lower_soa_api(ctx, schema, SoaReceiver::owner, ctx.text(owner_name))};
        children.append(api.header);
        source.append(api.source).new_lines(2);
        auto const created_record{
            add_record({.name = this->name(owner_name),
                        .children = children.build(),
                        .bases = {base},
                        .export_specifier = soas.export_specifier[soas.index(schema)]})};
        return created_record;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto adjacent(Nodes const& nodes) -> Nodes {
        auto result{node_list()};
        auto const count{nodes.size()};
        for (std::size_t index{}; index < count; ++index) {
            result.add(nodes[index]);
            if (index + 1 < count) {
                result.new_lines(1);
            }
        }
        return result.build();
    }
    auto section_header(std::string_view title) -> Nodes {
        return adjacent({block_comment("****************************************"),
                         line_comment(title),
                         block_comment("****************************************")});
    }
    auto schema_columns(LoweringContext& ctx, DeclarationHandle schema) -> NodeId {
        auto const& members{ctx.declarations.soa_members};
        std::vector<CppTypeId> types;
        for (auto member : ctx.declarations.items(ctx.declarations.soas.members[schema])) {
            if (members.kind[member] == SoaMemberKind::nested) {
                auto const child{ctx.for_declaration(schema).nested_declaration(member)};
                types.push_back(template_type(
                    named_type("ioj::soa_storage_detail::NestedColumn", NameOrigin::global),
                    {named_type(schema_namespace(ctx, child) + "::Schema", NameOrigin::global)}));
            } else {
                types.push_back(ctx.for_declaration(schema).resolve(members.type[member]));
            }
        }
        return using_declaration(
            store_text("Columns"),
            template_type(
                named_type("ioj::soa_storage_detail::SchemaColumns",
                           NameOrigin::global,
                           {dependency("SchemaColumns", "ioj/single_allocation/column_types.h")}),
                types));
    }
    auto schema_node(LoweringContext& ctx, SingleAllocationModel const& model) -> NodeId {
        auto body{node_list()};
        std::vector<Syntax> arguments;
        auto collect = [&](auto&& self,
                           DeclarationHandle current,
                           std::string const& prefix,
                           Expr receiver) -> void {
            auto const& members{ctx.declarations.soa_members};
            for (auto member : ctx.declarations.items(ctx.declarations.soas.members[current])) {
                auto const name{ctx.text(members.name[member])};
                if (members.kind[member] == SoaMemberKind::nested) {
                    auto const group{prefix + name + "_view"};
                    body.add(variable("auto const&", group, member_call(receiver, "view_" + name)));
                    self(self,
                         ctx.for_declaration(current).nested_declaration(member),
                         prefix + name + "_",
                         named(group));
                } else {
                    arguments.push_back(member_call(receiver, name));
                }
            }
        };
        collect(collect, model.schema, "", named("source"));
        body.add(return_stmt(call(this->name("func"), arguments)));
        FunctionDefinition const function{
            .name = this->name("with_columns"),
            .return_type = intern_type("auto"),
            .parameters = {parameter("Columns const&", "source"), parameter("Func&&", "func")},
            .implementation = {.kind = FunctionImplementationKind::body, .body = body.build()},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_inline = true, .define_in_header = true},
            .template_declaration =
                template_declaration({type_template_parameter(store_text("Columns")),
                                      type_template_parameter(store_text("Func"))})};
        auto const schema{model.schema};
        auto const& members{ctx.declarations.soa_members};
        auto children{node_list()};
        children.add(header_function(add_function(function))).new_lines(1);
        std::size_t index{};
        for (auto member : ctx.declarations.items(ctx.declarations.soas.members[schema])) {
            auto const name{(members.kind[member] == SoaMemberKind::nested ? "view_" : "") +
                            ctx.text(members.name[member])};
            auto const access{template_call(this->name("column_at"),
                                            {literal(std_types().size_t, std::to_string(index))},
                                            {named("self")})};
            FunctionDefinition const accessor{
                .name = this->name(name),
                .return_type = intern_type("auto"),
                .parameters = {parameter(
                    decorate_type(named_type("ColumnReceiver"), "this ", " auto&&"), "self")},
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {return_stmt(access)}},
                .flags = {.define_in_header = true},
                .formatting = {.body_layout = FunctionBodyLayout::compact}};
            children.add(header_function(add_function(accessor))).new_lines(1);
            ++index;
        }
        children.append(schema_rows(ctx, schema));
        auto const base{
            template_type(named_type("ioj::soa_storage_detail::ViewTypes",
                                     NameOrigin::global,
                                     {dependency("ViewTypes", "ioj/single_allocation/view.h")}),
                          {named_type("View"),
                           named_type("ConstView"),
                           named_type("CompactView"),
                           named_type("CompactConstView"),
                           named_type("Columns")})};
        return add_record({.name = this->name(store_text("Schema")),
                           .children = children.build(),
                           .bases = {base},
                           .dependencies = child_dependencies(ctx, schema, false)});
    }
    auto child_dependencies(LoweringContext& ctx, DeclarationHandle schema, bool compact)
        -> std::vector<TypeDependencyId> {
        std::vector<TypeDependencyId> deps;
        for (auto member : ctx.declarations.items(ctx.declarations.soas.members[schema])) {
            if (ctx.declarations.soa_members.kind[member] != SoaMemberKind::nested) {
                continue;
            }
            auto const child{ctx.for_declaration(schema).nested_declaration(member)};
            auto const target{ctx.for_declaration(child).module};
            if (target != ctx.module) {
                deps.push_back(dependency(
                    schema_namespace(ctx, child),
                    soa_header_path(source_include(ctx, ctx.declarations.module_settings(target)),
                                    compact ? "_compact_view" : "_view")));
            }
        }
        return deps;
    }
    auto view_base(LoweringContext& ctx,
                   DeclarationHandle schema,
                   SoaReceiver receiver,
                   std::string const& name,
                   bool resolved) -> CppTypeId {
        auto const scope{schema_namespace(ctx, schema)};
        std::vector<CppTypeId> args{named_type(scope + "::Schema", NameOrigin::global)};
        if (!resolved) {
            args.push_back(named_type(scope + "::Layout", NameOrigin::global));
        }
        args.push_back(intern_type(receiver == SoaReceiver::const_view ? "true" : "false"));
        auto const base{template_type(named_type(resolved ? "ioj::soa_storage_detail::ResolvedView"
                                                          : "ioj::soa_storage_detail::CompactView",
                                                 NameOrigin::global),
                                      args)};
        return soa_row_operations(ctx, schema, receiver, name, base);
    }
    auto emit_pointer_views(LoweringContext& ctx,
                            SingleAllocationModel const& model,
                            NodeListBuilder& source,
                            NodeListBuilder& declarations) -> Nodes {
        auto const schema{model.schema};
        for (auto name : {"View", "ConstView", "CompactView", "CompactConstView"}) {
            declarations.add(forward_declaration(store_text(name))).new_lines(1);
        }
        auto result{node_list()};
        result.append(section_header("Views")).new_lines(1);
        for (bool const is_const : {true, false}) {
            std::string const name{is_const ? "ConstView" : "View"};
            auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
            auto const base{view_base(ctx, schema, receiver, name, true)};
            auto wrapper{node_list()};
            wrapper.add(using_import(named_type("implementation_type::implementation_type")))
                .new_lines(1);
            auto api{lower_soa_api(ctx, schema, receiver, name)};
            wrapper.append(api.header);
            if (!api.source.empty()) {
                source.append(api.source).new_lines(1);
            }
            auto const type{add_record({.name = this->name(store_text(name)),
                                        .children = wrapper.build(),
                                        .bases = {base}})};
            result.add(type).new_lines(1);
        }
        return result.build();
    }
    auto view_validation_nodes(std::string const& name) -> Nodes {
        auto const validation{
            template_id(type_reference(named_type("ioj::soa_storage_detail::validate_compact_view",
                                                  NameOrigin::global)),
                        {type_reference(named_type(name))})};
        return {static_assertion(call(validation))};
    }
};

} // namespace

auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModel const& model)
    -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_layout(model);
}
auto emit_resolved_views(LoweringContext& ctx,
                         SingleAllocationModel const& model,
                         NodeListBuilder& source,
                         NodeListBuilder& declarations) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_resolved_views(ctx, model, source, declarations);
}
auto emit_compact_views(LoweringContext& ctx,
                        SingleAllocationModel const& model,
                        NodeListBuilder& source) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_compact_views(ctx, model, source);
}
auto emit_single_allocation_container(LoweringContext& ctx,
                                      SingleAllocationModel const& model,
                                      NodeListBuilder& source) -> NodeId {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_container(ctx, model, source);
}

} // namespace lispb::codegen::detail
