#include "single_allocation_soa_emission.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_api.h"
#include "soa_receiver.h"

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
    auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModelId model)
        -> Nodes {
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        auto const selected{lowering_storage().items(models.columns[models.index(model)])};
        auto const base{physical_types("ioj::single_allocation_layout::ColumnLayouts",
                                       "ioj/single_allocation/layout.h")};
        auto children{node_list()};
        for (std::size_t index{}; index < selected.size(); ++index) {
            auto const row{columns.index(selected[index])};
            if (index != 0) {
                children.new_lines(1);
            }
            children.add(constexpr_member(
                std_types().size_t,
                store_text(ctx.text(columns.flattened_identifier[row]) + "_column"),
                literal(index)));
        }
        std::vector<StringId> inherited;
        for (auto const name : {"ColumnLayouts",
                                "column_types",
                                "metadata",
                                "num_columns",
                                "size_type",
                                "byte_size_type",
                                "capacity_granularity",
                                "column_gap",
                                "allocation_alignment",
                                "capacity_block_bound",
                                "max_capacity",
                                "offset",
                                "make_data",
                                "make_resolved_view",
                                "layout_bytes"}) {
            inherited.push_back(store_text(name));
        }
        auto const layout{add_record({.name = this->name(store_text("Layout")),
                                      .children = children.build(),
                                      .bases = {base},
                                      .scope_bindings = inherited})};
        auto result{node_list()};
        result.append(section_header("Layout")).new_lines(1).add(layout);
        return result.build();
    }
    auto emit_resolved_views(LoweringContext& ctx,
                             SingleAllocationModelId model,
                             NodeListBuilder& source,
                             NodeListBuilder& declarations) -> Nodes {
        auto result{node_list()};
        result.append(section_header("Column pointers"))
            .new_lines(1)
            .add(column_pointers_alias(model))
            .new_lines(2)
            .add(using_declaration(store_text("ColumnTypes"),
                                   physical_types("ioj::soa_storage_detail::ColumnTypes",
                                                  "ioj/single_allocation/column_types.h")))
            .new_lines(2)
            .add(schema_node(ctx, model))
            .new_lines(2)
            .append(emit_pointer_views(ctx, model, source, declarations));
        return result.build();
    }
    auto emit_compact_views(LoweringContext& ctx,
                            SingleAllocationModelId model,
                            NodeListBuilder& source) -> Nodes {
        auto result{node_list()};
        auto const& models{lowering_storage().single_allocation_models};
        auto const schema{models.schema[models.index(model)]};
        auto const strided{is_strided_schema(ctx, schema)};
        result.append(section_header(strided ? "Strided views" : "Compact views")).new_lines(1);
        for (bool const is_const : {true, false}) {
            std::string const name{is_const ? "CompactConstView" : "CompactView"};
            auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
            auto const base{view_base(ctx, schema, receiver, name, false)};
            auto children{node_list()};
            children.add(using_import(named_type("implementation_type::implementation_type")))
                .new_lines(1);
            auto api{lower_soa_api(ctx, schema, receiver, name, base)};
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
            result.add(record_id)
                .new_lines(1)
                .append(view_validation_nodes(name, strided))
                .new_lines(1);
        }
        return result.build();
    }
    auto emit_single_allocation_container(LoweringContext& ctx,
                                          SingleAllocationModelId model,
                                          NodeListBuilder& source) -> NodeId {
        auto& model_store{lowering_storage().single_allocation_models};
        auto const model_store_index{model_store.index(model)};
        auto const schema{model_store.schema[model_store_index]};
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
        auto api{lower_soa_api(ctx, schema, SoaReceiver::owner, ctx.text(owner_name), base)};
        children.append(api.header);
        source.append(api.source).new_lines(2);
        auto const created_record{add_record(
            {.name = this->name(owner_name),
             .children = children.build(),
             .bases = {base},
             .export_specifier = ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(
                 model_store.schema[model_store_index])],
             .dependencies = {
                 std::from_range,
                 lowering_storage().items(model_store.dependencies[model_store_index])}})};
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
    auto column_access(LoweringContext& ctx,
                       LoweringPathId path,
                       std::string receiver,
                       std::map<LoweringPathId, StringId>& groups,
                       NodeListBuilder& body) -> Expr {
        LoweringPathId group_path;
        auto const group_count{lowering_storage().paths.items(path).size() - 1};
        for (std::size_t index{}; index < group_count; ++index) {
            auto const component{lowering_storage().paths.items(path)[index]};
            group_path = lowering_storage().paths.append(group_path, component);
            auto found{groups.find(group_path)};
            if (found == groups.end()) {
                auto const name{store_text(ctx.path_text(group_path, "_") + "_view")};
                found = groups.emplace(group_path, name).first;
                auto const member{"view_" + ctx.text(component)};
                auto const accessor{receiver.empty() ? named(member)
                                                     : member_access(named(receiver), member)};
                body.add(variable(intern_type("auto const&"), name, call(accessor)));
            }
            receiver = ctx.text(found->second);
        }
        auto const last{lowering_storage().paths.items(path).back()};
        return call(receiver.empty() ? named(last) : member_access(named(receiver), last));
    }
    auto physical_types(std::string_view target, std::string_view header) -> CppTypeId {
        return template_type(
            named_type("ioj::soa_storage_detail::FromPointers",
                       NameOrigin::global,
                       {dependency("FromPointers", "ioj/single_allocation/column_types.h")}),
            {named_type(target, NameOrigin::global, {dependency(target, header)}),
             named_type("Pointers")});
    }
    auto column_pointers_alias(SingleAllocationModelId model) -> NodeId {
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        std::vector<CppTypeId> types;
        for (auto const column : lowering_storage().items(models.columns[models.index(model)])) {
            types.push_back(columns.type[columns.index(column)]);
        }
        return using_declaration(
            store_text("Pointers"),
            template_type(
                named_type("ioj::soa_storage_detail::ColumnPointers",
                           NameOrigin::global,
                           {dependency("ColumnPointers", "ioj/single_allocation/column_types.h")}),
                types));
    }
    auto schema_node(LoweringContext& ctx, SingleAllocationModelId model) -> NodeId {
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        auto body{node_list()};
        std::map<LoweringPathId, StringId> groups;
        std::vector<Syntax> arguments;
        for (auto const column : lowering_storage().items(models.columns[models.index(model)])) {
            arguments.push_back(column_access(
                ctx, columns.member_path[columns.index(column)], "source", groups, body));
        }
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
        auto const schema{models.schema[models.index(model)]};
        auto const& members{ctx.declarations.soa_members};
        auto const selected{lowering_storage().items(models.columns[models.index(model)])};
        auto children{node_list()};
        children.add(header_function(add_function(function))).new_lines(1);
        std::size_t index{};
        for (auto member : ctx.declarations.items(ctx.declarations.soas.members[schema])) {
            auto name{ctx.text(members.name[member])};
            auto const nested{members.kind[member] == SoaMemberKind::nested};
            std::vector<Syntax> args;
            if (nested) {
                auto const child{ctx.for_declaration(schema).nested_declaration(member)};
                args.push_back(type_reference(
                    named_type(schema_namespace(ctx, child) + "::Schema", NameOrigin::global)));
                name = "view_" + name;
            }
            args.push_back(literal(index));
            auto const self{static_cast_expr(intern_type("decltype(self)&&"), named("self"))};
            auto const access{template_call(
                member_access(self, nested ? "template nested_at" : "template column_at"),
                args,
                {})};
            FunctionDefinition const accessor{
                .name = this->name(name),
                .return_type = intern_type("auto"),
                .parameters = {parameter(decorate_type(intern_type("auto"), "this ", "&&"),
                                         "self")},
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {return_stmt(access)}},
                .flags = {.define_in_header = true},
                .formatting = {.body_layout = FunctionBodyLayout::compact}};
            children.add(header_function(add_function(accessor))).new_lines(1);
            do {
                ++index;
            } while (index < selected.size() &&
                     lowering_storage()
                             .paths.items(columns.member_path[columns.index(selected[index])])
                             .front() == members.name[member]);
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
                           named_type("ColumnTypes"),
                           intern_type(is_strided_schema(ctx, schema) ? "true" : "false")})};
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
            if (target != ctx.module && (!compact || is_strided_schema(ctx, child))) {
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
                            SingleAllocationModelId model,
                            NodeListBuilder& source,
                            NodeListBuilder& declarations) -> Nodes {
        auto const& models{lowering_storage().single_allocation_models};
        auto const schema{models.schema[models.index(model)]};
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
            auto api{lower_soa_api(ctx, schema, receiver, name, base)};
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
    auto view_validation_nodes(std::string const& name, bool strided) -> Nodes {
        auto const validation{template_id(
            type_reference(strided ? std_types().is_trivially_copyable_v
                                   : named_type("ioj::soa_storage_detail::validate_compact_view",
                                                NameOrigin::global)),
            {type_reference(named_type(name))})};
        return {static_assertion(strided ? reference(validation, std_types().bool_)
                                         : call(validation))};
    }
};

} // namespace

auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_layout(ctx, model);
}
auto emit_resolved_views(LoweringContext& ctx,
                         SingleAllocationModelId model,
                         NodeListBuilder& source,
                         NodeListBuilder& declarations) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_resolved_views(ctx, model, source, declarations);
}
auto emit_compact_views(LoweringContext& ctx,
                        SingleAllocationModelId model,
                        NodeListBuilder& source) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_compact_views(ctx, model, source);
}
auto emit_single_allocation_container(LoweringContext& ctx,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source) -> NodeId {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_container(ctx, model, source);
}

} // namespace lispb::codegen::detail
