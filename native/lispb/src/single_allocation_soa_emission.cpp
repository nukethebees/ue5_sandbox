#include "single_allocation_soa_emission.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_api.h"
#include "soa_receiver.h"

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
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

void describe_compact_scope(LoweringContext& ctx, RecordId record, DeclarationHandle schema) {
    if (!ctx.declarations.soas.using_declarations[ctx.declarations.soas.index(schema)].empty()) {
        return;
    }
    // Describe inherited runtime members and declarations retained as generated raw C++.
    std::vector<StringId> names;
    for (auto const name : {"StorageState",
                            "StorageOperations",
                            "CompactViewBase",
                            "StridedViewBase",
                            "ResolvedViewBase",
                            "Element",
                            "DataPointers",
                            "apply_permutation",
                            "Operations",
                            "Layout",
                            "size_type",
                            "byte_size_type",
                            "capacity_granularity",
                            "allocation_alignment",
                            "capacity_block_bound",
                            "max_capacity",
                            "layout_bytes",
                            "data_",
                            "num_",
                            "capacity_",
                            "release_storage",
                            "take_storage",
                            "move_assign",
                            "allocate_storage",
                            "reallocate",
                            "num",
                            "capacity",
                            "is_empty",
                            "allocated_bytes",
                            "reserve",
                            "reset",
                            "set_num",
                            "add_defaulted",
                            "add_uninitialised",
                            "remove_at_swap",
                            "copy_element",
                            "copy_elements",
                            "append_from",
                            "validate",
                            "view",
                            "const_view",
                            "slice",
                            "left",
                            "right"}) {
        names.push_back(ctx.ir.strings.intern(name));
    }
    ctx.ir.set_scope_bindings(record, names);
}

auto adjacent(LoweringContext& ctx, Nodes const& nodes) -> Nodes {
    auto& ir{ctx.ir};
    NodeListBuilder result{ir};
    auto const count{nodes.size()};
    for (std::size_t index{}; index < count; ++index) {
        result.add(nodes[index]);
        if (index + 1 < count) {
            result.new_lines(1);
        }
    }
    return result.build();
}

auto section_header(LoweringContext& ctx, std::string_view title) -> Nodes {
    auto& ir{ctx.ir};
    return adjacent(ctx,
                    {ir.block_comment("****************************************"),
                     ir.line_comment(title),
                     ir.block_comment("****************************************")});
}

auto inline_function(LoweringContext& ctx, FunctionId spec) -> NodeId {
    auto& ir{ctx.ir};
    auto& flags{ir.function_flags(spec)};
    flags.set_is_inline(true);
    flags.set_define_in_header(true);
    return ir.header_function(spec);
}

auto compact_function(LoweringContext& ctx, FunctionId spec) -> NodeId {
    ctx.ir.function_formatting(spec).body_layout = FunctionBodyLayout::compact;
    return inline_function(ctx, spec);
}

// Accept braced bodies and existing node ranges without copying them.
template <typename Body = std::initializer_list<NodeId>>
auto make_function(CppIR& ir,
                   std::string_view name,
                   std::string_view return_type,
                   std::initializer_list<FunctionParameterId> parameters,
                   Body const& body,
                   FunctionReceiver receiver = FunctionReceiver::unqualified,
                   ReturnSyntax syntax = ReturnSyntax::direct) -> FunctionId {
    auto const function{ir.function(name, return_type)};
    ir.set_function_parameters(function, parameters);
    ir.set_function_body(function, body);
    ir.set_function_receiver(function, receiver);
    if (syntax == ReturnSyntax::trailing) {
        ir.set_function_return_type(function, ir.function_return_type(function), syntax);
    }
    return function;
}

auto column_access(LoweringContext& ctx,
                   LoweringPathId path,
                   std::string receiver,
                   std::map<LoweringPathId, StringId>& groups,
                   NodeListBuilder& body) -> Expr {
    auto& ir{ctx.ir};
    LoweringPathId group_path;
    auto const group_count{ir.lowering.paths.items(path).size() - 1};
    for (std::size_t index{}; index < group_count; ++index) {
        auto const component{ir.lowering.paths.items(path)[index]};
        group_path = ir.lowering.paths.append(group_path, component);
        auto found{groups.find(group_path)};
        if (found == groups.end()) {
            auto const name{ir.strings.intern(ctx.path_text(group_path, "_") + "_view")};
            found = groups.emplace(group_path, name).first;
            auto const member{"view_" + ctx.text(component)};
            auto const accessor{receiver.empty() ? ir.named(member)
                                                 : ir.member_access(ir.named(receiver), member)};
            body.add(ir.variable(ir.types.intern("auto const&"), name, ir.call(accessor)));
        }
        receiver = ctx.text(found->second);
    }
    auto const last{ir.lowering.paths.items(path).back()};
    return ir.call(receiver.empty() ? ir.named(last) : ir.member_access(ir.named(receiver), last));
}

auto storage_lifetime_nodes(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
    auto& model_store{ctx.ir.lowering.single_allocation_models};
    auto const& soas{ctx.declarations.soas};
    auto const owner_name{soas.name[soas.index(model_store.schema[model_store.index(model)])]};
    auto& ir{ctx.ir};
    auto const owner_type{ir.types.named(ctx.text(owner_name))};
    auto const copy_parameter{ir.parameter(ir.types.decorate(owner_type, "", " const&"))};
    auto const move_parameter{ir.parameter(ir.types.decorate(owner_type, "", "&&"), "other")};
    auto const free_data{ir.call(
        ir.type_reference(ir.types.decorate(ir.types.named("Operations"), "", "::release_storage")),
        {ir.unary(UnaryOperator::dereference, ir.named("this"))})};
    auto const default_constructor{ir.constructor(owner_name)};
    ir.set_function_noexcept(default_constructor);
    ir.set_function_body(
        default_constructor,
        {},
        {ir.member_initializer(
            owner_name,
            ir.unary(UnaryOperator::dereference,
                     ir.call(ir.type_reference(
                         ir.types.named("std::pmr::get_default_resource", NameOrigin::global)))))});
    auto const resource_constructor{ir.constructor(owner_name, true)};
    ir.set_function_parameters(
        resource_constructor,
        {ir.parameter(ir.types.decorate(ir.types.std.pmr.memory_resource, "", "&"), "resource")});
    ir.set_function_noexcept(resource_constructor);
    ir.set_function_body(
        resource_constructor,
        {},
        {ir.member_initializer(ir.strings.intern("resource_"),
                               ir.unary(UnaryOperator::address_of, ir.named("resource")))});
    auto const resource_accessor{ir.function("memory_resource", "auto")};
    ir.set_function_body(resource_accessor, {ir.return_stmt(ir.named("resource_"))});
    ir.set_function_return_type(resource_accessor,
                                ir.types.decorate(ir.types.std.pmr.memory_resource, "", "*"),
                                ReturnSyntax::trailing);
    ir.set_function_receiver(resource_accessor, FunctionReceiver::const_);
    ir.set_function_noexcept(resource_accessor);
    auto const destructor{ir.destructor(owner_name)};
    ir.set_function_body(destructor, {ir.expr_stmt(free_data)});
    auto const copy_constructor{ir.constructor(owner_name)};
    ir.set_function_parameters(copy_constructor, {copy_parameter});
    ir.set_function_deleted(copy_constructor);
    auto const copy_assignment{ir.function("operator=", "auto")};
    ir.set_function_parameters(copy_assignment, {copy_parameter});
    ir.set_function_return_type(
        copy_assignment, ir.types.decorate(owner_type, "", "&"), ReturnSyntax::trailing);
    ir.set_function_deleted(copy_assignment);
    auto const move_constructor{ir.constructor(owner_name)};
    ir.set_function_parameters(move_constructor, {move_parameter});
    ir.set_function_noexcept(move_constructor);
    ir.set_function_body(
        move_constructor,
        {ir.expr_stmt(
            ir.call(ir.type_reference(
                        ir.types.decorate(ir.types.named("Operations"), "", "::take_storage")),
                    {ir.unary(UnaryOperator::dereference, ir.named("this")), ir.named("other")}))},
        {ir.member_initializer(ir.strings.intern("resource_"),
                               ir.member_access(ir.named("other"), "resource_"))});
    auto const move_assignment{ir.function("operator=", "auto")};
    ir.set_function_parameters(move_assignment, {move_parameter});
    ir.set_function_body(
        move_assignment,
        {ir.return_stmt(ir.call(
            ir.type_reference(ir.types.decorate(ir.types.named("Operations"), "", "::move_assign")),
            {ir.unary(UnaryOperator::dereference, ir.named("this")), ir.named("other")}))});
    ir.set_function_return_type(
        move_assignment, ir.types.decorate(owner_type, "", "&"), ReturnSyntax::trailing);

    NodeListBuilder result{ir};
    result.append(section_header(ctx, "Lifetime"))
        .new_lines(1)
        .append(adjacent(ctx,
                         {inline_function(ctx, default_constructor),
                          inline_function(ctx, resource_constructor),
                          compact_function(ctx, resource_accessor),
                          compact_function(ctx, destructor),
                          ir.declaration(copy_constructor),
                          ir.declaration(copy_assignment),
                          inline_function(ctx, move_constructor),
                          inline_function(ctx, move_assignment)}));
    return result.build();
}

auto data_pointers_node(LoweringContext& ctx, SingleAllocationModelId model) -> NodeId {
    auto& model_store{ctx.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{ctx.ir.lowering.single_allocation_columns};
    auto& ir{ctx.ir};
    NodeListBuilder children{ir};
    children.add(ir.using_declaration(
        ir.strings.intern("Element"),
        ir.types.template_type(
            ir.types.std.conditional_t,
            {ir.types.template_type(ir.types.std.is_const_v, {ir.types.named("Byte")}),
             ir.types.decorate(ir.types.named("T"), "", " const"),
             ir.types.named("T")}),
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})));
    for (auto const column : ctx.ir.lowering.items(model_store.columns[model_store_index])) {
        children.new_lines(1).add(ir.instance_member(
            ir.types.decorate(
                ir.types.template_type(ir.types.named("Element"),
                                       {column_store.type[column_store.index(column)]}),
                "",
                "*"),
            column_store.flattened_identifier[column_store.index(column)],
            ir.init_list({})));
    }

    std::vector<Expr> arguments;
    for (auto const column : ctx.ir.lowering.items(model_store.columns[model_store_index])) {
        arguments.push_back(ir.member_access(
            ir.named("self"), column_store.flattened_identifier[column_store.index(column)]));
    }
    auto const visit{make_function(ir,
                                   "with_columns",
                                   "decltype(auto)",
                                   {ir.parameter("Self&", "self"), ir.parameter("Func&&", "func")},
                                   {ir.return_stmt(ir.call(ir.named("func"), arguments))},
                                   FunctionReceiver::static_)};
    ir.set_function_template(
        visit,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Self")),
                                 ir.type_template_parameter(ir.strings.intern("Func"))}));
    children.new_lines(1).add(inline_function(ctx, visit));

    auto const created_record{
        ir.record(ir.strings.intern("DataPointers"), children.build(), RecordKind::struct_)};
    ir.set_record_template(
        created_record,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))}));
    return created_record;
}

auto layout_column(LoweringContext& ctx, SingleAllocationColumnId column) -> Expr {
    auto& ir{ctx.ir};
    auto const& columns{ir.lowering.single_allocation_columns};
    return ir.type_reference(ir.types.decorate(
        ir.types.named("Layout"),
        "",
        "::" + ctx.text(columns.flattened_identifier[columns.index(column)]) + "_column"));
}

auto member_columns(LoweringContext& ctx, SingleAllocationModelId model, StringId member)
    -> std::vector<SingleAllocationColumnId> {
    auto const& storage{ctx.ir.lowering};
    auto const& models{storage.single_allocation_models};
    auto const& columns{storage.single_allocation_columns};
    std::vector<SingleAllocationColumnId> result;
    for (auto const column : storage.items(models.columns[models.index(model)])) {
        if (storage.paths.items(columns.member_path[columns.index(column)]).front() == member) {
            result.push_back(column);
        }
    }
    return result;
}

auto schema_node(LoweringContext& ctx, SingleAllocationModelId model) -> NodeId {
    auto& ir{ctx.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    NodeListBuilder body{ir};
    std::map<LoweringPathId, StringId> groups;
    std::vector<Expr> arguments;
    for (auto const column : ir.lowering.items(models.columns[models.index(model)])) {
        arguments.push_back(
            column_access(ctx, columns.member_path[columns.index(column)], "source", groups, body));
    }
    body.add(ir.return_stmt(ir.call(ir.named("func"), arguments)));
    auto const function{
        make_function(ir,
                      "with_columns",
                      "auto",
                      {ir.parameter("Columns const&", "source"), ir.parameter("Func&&", "func")},
                      body.build(),
                      FunctionReceiver::static_)};
    ir.set_function_template(
        function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Columns")),
                                 ir.type_template_parameter(ir.strings.intern("Func"))}));
    return ir.record(
        ir.strings.intern("Schema"), {inline_function(ctx, function)}, RecordKind::struct_);
}

auto shared_view_type(LoweringContext& ctx, DeclarationHandle schema, bool resolved) -> CppTypeId {
    auto const scope{schema_namespace(ctx, schema)};
    auto& types{ctx.ir.types};
    return types.template_type(
        types.std.conditional_t,
        {types.named("Const"),
         types.named(scope + (resolved ? "::ResolvedConstView" : "::ConstView"),
                     NameOrigin::global),
         types.named(scope + (resolved ? "::ResolvedView" : "::View"), NameOrigin::global)});
}

auto view_accessor_nodes(LoweringContext& ctx, SingleAllocationModelId model, bool resolved)
    -> Nodes {
    auto& ir{ctx.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const& members{ctx.declarations.soa_members};
    auto const model_row{models.index(model)};
    auto const schema{models.schema[model_row]};
    auto const schema_row{ctx.declarations.soas.index(schema)};
    auto const selected{ir.lowering.items(models.columns[model_row])};
    auto const strided{is_strided_schema(ctx, schema)};
    NodeListBuilder children{ir};
    auto accessor = [&](std::string_view name, Expr value, CppTypeId type = {}) {
        auto const function{
            make_function(ir, name, "auto", {}, {ir.return_stmt(value)}, FunctionReceiver::const_)};
        if (type.value != 0) {
            ir.set_function_return_type(function, type, ReturnSyntax::trailing);
        }
        children.new_lines(1).add(compact_function(ctx, function));
    };
    auto column_span = [&](SingleAllocationColumnId column) {
        auto const index{std::find(selected.begin(), selected.end(), column) - selected.begin()};
        if (resolved) {
            return ir.call(
                ir.this_member("column"),
                {ir.member_access(ir.this_member("columns_"),
                                  ctx.text(columns.flattened_identifier[columns.index(column)]))});
        }
        if (strided) {
            return ir.call(ir.this_member("column"), {ir.literal(std::to_string(index))});
        }
        return ir.call(ir.template_id(ir.this_member("template column"),
                                      {ir.type_reference(columns.type[columns.index(column)])}),
                       {layout_column(ctx, column)});
    };
    for (auto const member : ctx.declarations.items(ctx.declarations.soas.members[schema_row])) {
        auto const row{members.index(member)};
        auto const name{ctx.text(members.name[row])};
        if (members.kind[row] == SoaMemberKind::nested) {
            auto const nested{ctx.for_declaration(schema).nested_declaration(member)};
            auto const nested_columns{member_columns(ctx, model, members.name[row])};
            auto const nested_strided{is_strided_schema(ctx, nested)};
            auto const type{shared_view_type(ctx, nested, resolved || !nested_strided)};
            if (!resolved && nested_strided && strided) {
                accessor("view_" + name,
                         ir.init_list({ir.call(ir.member_access(column_span(nested_columns.front()),
                                                                "data")),
                                       ir.call(ir.this_member("byte_stride")),
                                       ir.call(ir.this_member("num"))}),
                         type);
            } else if (!resolved && nested_strided) {
                auto const factory{ir.template_id(
                    ir.type_reference(ir.types.named("ioj::soa_storage_detail::make_strided_view",
                                                     NameOrigin::global)),
                    {ir.type_reference(type), ir.type_reference(ir.types.intern("Layout"))})};
                accessor("view_" + name,
                         ir.call(factory,
                                 {ir.this_member("state_"),
                                  ir.this_member("offset_"),
                                  ir.call(ir.this_member("num")),
                                  layout_column(ctx, nested_columns[0]),
                                  layout_column(ctx, nested_columns[1])}),
                         type);
            } else {
                std::vector<Expr> pointers;
                for (auto const column : nested_columns) {
                    pointers.push_back(
                        resolved
                            ? ir.member_access(
                                  ir.this_member("columns_"),
                                  ctx.text(columns.flattened_identifier[columns.index(column)]))
                            : ir.call(ir.member_access(column_span(column), "data")));
                }
                pointers.push_back(ir.call(ir.this_member("num")));
                accessor("view_" + name, ir.init_list(pointers), type);
            }
        } else {
            auto const column{
                column_for(ctx, model, ir.lowering.paths.append({}, members.name[row]))};
            accessor(name, column_span(column));
        }
    }
    return children.build();
}

auto emit_resolved_views(LoweringContext& ctx,
                         SingleAllocationModelId model,
                         NodeListBuilder& source,
                         NodeListBuilder& declarations) -> Nodes {
    auto& ir{ctx.ir};
    auto& storage{ir.lowering};
    auto const& models{storage.single_allocation_models};
    auto const schema{models.schema[models.index(model)]};
    std::string const mutable_name{"ResolvedView"};
    std::string const const_name{"ResolvedConstView"};
    std::string const implementation{"ResolvedViewImpl"};
    NodeListBuilder result{ir};
    declarations.add(ir.forward_declaration(ir.strings.intern(mutable_name)))
        .new_lines(1)
        .add(ir.forward_declaration(ir.strings.intern(const_name)))
        .new_lines(1);
    result.append(section_header(ctx, "Resolved views")).new_lines(1);

    auto const scope{schema_namespace(ctx, schema)};
    auto const base{ir.types.template_type(
        ir.types.named("ioj::soa_storage_detail::ResolvedViewBase", NameOrigin::global),
        {ir.types.decorate(ir.types.named("Layout"), "", "::column_types"),
         ir.types.named("DataPointers"),
         ir.types.named("Const")})};
    NodeListBuilder children{ir};
    children.append(adjacent(
        ctx,
        {ir.using_declaration(ir.strings.intern("Base"), base),
         ir.using_import(ir.types.named("Base::Base")),
         ir.using_declaration(ir.strings.intern("Layout"),
                              ir.types.named(scope + "::Layout", NameOrigin::global)),
         ir.using_declaration(ir.strings.intern("view_type"),
                              ir.types.named(scope + "::" + mutable_name, NameOrigin::global)),
         ir.using_declaration(ir.strings.intern("const_view_type"),
                              ir.types.named(scope + "::" + const_name, NameOrigin::global)),
         ir.using_declaration(ir.strings.intern("soa_schema"),
                              ir.types.named(scope + "::Schema", NameOrigin::global))}));

    auto const conversion{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_parameters(
        conversion,
        {ir.parameter(ir.types.decorate(ir.types.template_type(ir.types.named(implementation),
                                                               {ir.types.intern("false")}),
                                        "",
                                        " const&"),
                      "other")});
    ir.set_function_template(
        conversion,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.std.bool_, ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion, ir.named("Enabled"));
    ir.set_function_body(
        conversion, {}, {ir.member_initializer(ir.strings.intern("Base"), ir.named("other"))});
    children.new_lines(1).add(inline_function(ctx, conversion));

    children.append(view_accessor_nodes(ctx, model, true));
    auto const record{
        ir.record(ir.strings.intern(implementation), children.build(), RecordKind::struct_)};
    describe_compact_scope(ctx, record, schema);
    ir.set_record_bases(record, {base});
    ir.set_record_template(record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.std.bool_, ir.strings.intern("Const"))}));
    result.add(record).new_lines(1);
    for (bool const is_const : {true, false}) {
        auto const name{is_const ? const_name : mutable_name};
        auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
        auto const base{soa_row_operations(
            ctx,
            schema,
            receiver,
            name,
            ir.types.template_type(ir.types.named(implementation),
                                   {ir.types.intern(is_const ? "true" : "false")}))};
        NodeListBuilder wrapper{ir};
        wrapper.add(ir.using_declaration(ir.strings.intern("Base"), base))
            .new_lines(1)
            .add(ir.using_import(ir.types.named("Base::Base")))
            .new_lines(1);
        auto api{lower_soa_api(ctx, schema, receiver, name, base)};
        wrapper.append(api.header);
        if (!api.source.empty()) {
            source.append(api.source).new_lines(1);
        }
        auto const type{ir.record(ir.strings.intern(name), wrapper.build(), RecordKind::struct_)};
        ir.set_record_bases(type, {base});
        result.add(type).new_lines(1);
    }
    return result.build();
}

auto compact_view_node(LoweringContext& ctx, SingleAllocationModelId model) -> NodeId {
    auto& ir{ctx.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const model_row{models.index(model)};
    auto const schema{models.schema[model_row]};
    auto const schema_row{ctx.declarations.soas.index(schema)};
    auto const scope{schema_namespace(ctx, schema)};
    auto const selected{ir.lowering.items(models.columns[model_row])};
    auto const strided{is_strided_schema(ctx, schema)};
    auto const base{
        strided
            ? ir.types.template_type(
                  ir.types.named("ioj::soa_storage_detail::StridedViewBase", NameOrigin::global),
                  {columns.type[columns.index(selected.front())], ir.types.named("Const")})
            : ir.types.template_type(
                  ir.types.named("ioj::soa_storage_detail::CompactViewBase", NameOrigin::global),
                  {ir.types.named("Const")})};
    NodeListBuilder children{ir};
    children.append(
        adjacent(ctx,
                 {ir.using_declaration(ir.strings.intern("soa_schema"),
                                       ir.types.named(schema_namespace(ctx, schema) + "::Schema",
                                                      NameOrigin::global)),
                  ir.using_declaration(ir.strings.intern("Base"), base),
                  ir.using_import(ir.types.named("Base::Base")),
                  ir.using_declaration(ir.strings.intern("Layout"),
                                       ir.types.named(scope + "::Layout", NameOrigin::global)),
                  ir.using_declaration(ir.strings.intern("view_type"),
                                       ir.types.named(scope + "::View", NameOrigin::global)),
                  ir.using_declaration(ir.strings.intern("const_view_type"),
                                       ir.types.named(scope + "::ConstView", NameOrigin::global)),
                  ir.using_declaration(ir.strings.intern("resolved_view_type"),
                                       shared_view_type(ctx, schema, true))}));
    if (strided && !ctx.declarations.soas.layout_only[schema_row]) {
        auto const state{
            ir.types.named("ioj::soa_storage_detail::StorageState", NameOrigin::global)};
        children.new_lines(1).add(ir.using_declaration(
            ir.store_text("State"),
            ir.types.template_type(
                ir.types.std.conditional_t,
                {ir.types.named("Const"), ir.types.decorate(state, "", " const"), state})));
        auto const constructor{ir.constructor(ir.strings.intern("ViewImpl"))};
        ir.set_function_parameters(constructor,
                                   {ir.parameter("State*", "state"),
                                    ir.parameter(ir.types.std.u32, "offset"),
                                    ir.parameter(ir.types.std.u32, "count")});
        std::vector<Expr> arguments{ir.named("state"), ir.named("offset"), ir.named("count")};
        for (auto const column : selected.first(2)) {
            arguments.push_back(layout_column(ctx, column));
        }
        ir.set_function_body(
            constructor,
            {},
            {ir.member_initializer(
                ir.strings.intern("Base"),
                ir.call(ir.template_id(
                            ir.type_reference(ir.types.named(
                                "ioj::soa_storage_detail::make_strided_view", NameOrigin::global)),
                            {ir.type_reference(ir.types.named("Base")),
                             ir.type_reference(ir.types.named("Layout"))}),
                        arguments))});
        children.new_lines(1).add(inline_function(ctx, constructor));
    }
    auto const conversion{ir.constructor(ir.strings.intern("ViewImpl"))};
    ir.set_function_parameters(
        conversion,
        {ir.parameter(ir.types.decorate(ir.types.template_type(ir.types.named("ViewImpl"),
                                                               {ir.types.intern("false")}),
                                        "",
                                        " const&"),
                      "other")});
    ir.set_function_body(
        conversion, {}, {ir.member_initializer(ir.strings.intern("Base"), ir.named("other"))});
    ir.set_function_template(
        conversion,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.std.bool_, ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion, ir.named("Enabled"));
    children.new_lines(1).add(inline_function(ctx, conversion));
    children.append(view_accessor_nodes(ctx, model, false));
    auto const record{
        ir.record(ir.strings.intern("ViewImpl"), children.build(), RecordKind::struct_)};
    describe_compact_scope(ctx, record, schema);
    ir.set_record_bases(record, {base});
    ir.set_record_template(record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.std.bool_, ir.strings.intern("Const"))}));
    return record;
}

auto view_validation_nodes(LoweringContext& ctx, std::string const& name, bool strided) -> Nodes {
    auto& ir{ctx.ir};
    auto const validation{ir.template_id(
        ir.type_reference(ir.types.named(strided ? "std::is_trivially_copyable_v"
                                                 : "ioj::soa_storage_detail::validate_compact_view",
                                         NameOrigin::global)),
        {ir.type_reference(ir.types.named(name))})};
    return {ir.static_assertion(strided ? validation : ir.call(validation))};
}

} // namespace

auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
    auto& ir{ctx.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const selected{ir.lowering.items(models.columns[models.index(model)])};
    std::vector<CppTypeId> types;
    for (auto const column : selected) {
        types.push_back(columns.type[columns.index(column)]);
    }
    auto const base{ir.types.template_type(
        ir.types.named(
            "ioj::single_allocation_layout::ColumnLayouts",
            NameOrigin::global,
            {ir.types.dependency("single_allocation_layout", "ioj/single_allocation/layout.h")}),
        types)};
    NodeListBuilder children{ir};
    for (std::size_t index{}; index < selected.size(); ++index) {
        auto const row{columns.index(selected[index])};
        if (index != 0) {
            children.new_lines(1);
        }
        children.add(ir.constexpr_member(
            ir.types.std.size_t,
            ir.strings.intern(ctx.text(columns.flattened_identifier[row]) + "_column"),
            ir.literal(std::to_string(index))));
    }
    auto const layout{
        ir.record(ir.strings.intern("Layout"), children.build(), RecordKind::struct_)};
    ir.set_record_bases(layout, {base});
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
        inherited.push_back(ir.strings.intern(name));
    }
    ir.set_scope_bindings(layout, inherited);
    NodeListBuilder result{ir};
    result.append(section_header(ctx, "Layout")).new_lines(1).add(layout);
    return result.build();
}

auto storage_implementation_nodes(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
    auto& model_store{ctx.ir.lowering.single_allocation_models};
    auto const scope{schema_namespace(ctx, model_store.schema[model_store.index(model)])};
    auto& ir{ctx.ir};
    NodeListBuilder children{ir};
    children
        .append(adjacent(
            ctx,
            {ir.using_declaration(ir.strings.intern("View"),
                                  ir.types.named(scope + "::View", NameOrigin::global)),
             ir.using_declaration(ir.strings.intern("ConstView"),
                                  ir.types.named(scope + "::ConstView", NameOrigin::global))}))
        .new_lines(2)
        .append(storage_lifetime_nodes(ctx, model))
        .new_lines(1)
        .add(ir.access(Access::protected_))
        .new_lines(1)
        .add(ir.using_declaration(
            ir.strings.intern("DataPointers"),
            ir.types.template_type(ir.types.named(scope + "::DataPointers", NameOrigin::global),
                                   {ir.types.named("Byte")}),
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))})));

    return children.build();
}

auto emit_single_allocation_views(LoweringContext& ctx,
                                  SingleAllocationModelId model,
                                  NodeListBuilder& source,
                                  NodeListBuilder& declarations) -> Nodes {
    auto& ir{ctx.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const schema{models.schema[models.index(model)]};
    auto const strided{is_strided_schema(ctx, schema)};
    declarations.add(ir.forward_declaration(ir.strings.intern("View")))
        .new_lines(1)
        .add(ir.forward_declaration(ir.strings.intern("ConstView")))
        .new_lines(1);
    NodeListBuilder result{ir};
    result.append(section_header(ctx, "Column pointers"))
        .new_lines(1)
        .add(data_pointers_node(ctx, model))
        .new_lines(2)
        .add(schema_node(ctx, model))
        .new_lines(2)
        .append(emit_resolved_views(ctx, model, source, declarations))
        .new_lines(2)
        .append(section_header(ctx, strided ? "Strided views" : "Compact views"))
        .new_lines(1)
        .add(compact_view_node(ctx, model))
        .new_lines(1);
    for (bool const is_const : {true, false}) {
        std::string const name{is_const ? "ConstView" : "View"};
        auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
        auto const base{soa_row_operations(
            ctx,
            schema,
            receiver,
            name,
            ir.types.template_type(ir.types.named("ViewImpl"),
                                   {ir.types.intern(is_const ? "true" : "false")}))};
        NodeListBuilder children{ir};
        children.add(ir.using_declaration(ir.strings.intern("Base"), base))
            .new_lines(1)
            .add(ir.using_import(ir.types.named("Base::Base")))
            .new_lines(1);
        auto api{lower_soa_api(ctx, schema, receiver, name, base)};
        children.append(api.header);
        if (!api.source.empty()) {
            source.append(api.source).new_lines(2);
        }
        auto const record{
            ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
        ir.set_record_bases(record, {base});
        ir.set_record_export_specifier(
            record, ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(schema)]);
        result.add(record)
            .new_lines(1)
            .append(view_validation_nodes(ctx, name, strided))
            .new_lines(1);
    }
    return result.build();
}

auto emit_single_allocation_container(LoweringContext& ctx,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source) -> NodeId {
    auto& model_store{ctx.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto const schema{model_store.schema[model_store_index]};
    auto const& soas{ctx.declarations.soas};
    auto const owner_name{soas.name[soas.index(schema)]};
    auto const scope{schema_namespace(ctx, schema)};
    auto& ir{ctx.ir};
    NodeListBuilder children{ir};
    auto const operations{std::string{"ioj::native_soa::StorageOperations"}};
    auto const row_operations{soa_row_operations(ctx,
                                                 schema,
                                                 SoaReceiver::owner,
                                                 ctx.text(owner_name),
                                                 ir.types.named(operations, NameOrigin::global))};
    children
        .add(ir.using_declaration(
            ir.strings.intern("soa_schema"),
            ir.types.named(schema_namespace(ctx, model_store.schema[model_store_index]) +
                               "::Schema",
                           NameOrigin::global)))
        .new_lines(1);
    children
        .add(ir.using_declaration(ir.strings.intern("Operations"),
                                  ir.types.named(operations, NameOrigin::global)))
        .new_lines(1);
    children
        .add(ir.using_declaration(ir.store_text("Layout"),
                                  ir.types.named(scope + "::Layout", NameOrigin::global)))
        .new_lines(1);
    for (auto const name : {"size_type", "byte_size_type"}) {
        children
            .add(ir.using_declaration(
                ir.store_text(name),
                ir.types.decorate(ir.types.named("Layout"), "", "::" + std::string{name})))
            .new_lines(1);
    }
    children.new_lines(1);
    for (auto const name :
         {"capacity_granularity", "allocation_alignment", "capacity_block_bound", "max_capacity"}) {
        children
            .add(ir.constexpr_member(ir.types.intern("auto"),
                                     ir.store_text(name),
                                     ir.type_reference(ir.types.decorate(
                                         ir.types.named("Layout"), "", "::" + std::string{name}))))
            .new_lines(1);
    }
    auto const bytes{make_function(
        ir,
        "layout_bytes",
        "byte_size_type",
        {ir.parameter("byte_size_type", "blocks")},
        {ir.return_stmt(ir.call(
            ir.type_reference(ir.types.decorate(ir.types.named("Layout"), "", "::layout_bytes")),
            {ir.named("blocks")}))},
        FunctionReceiver::static_,
        ReturnSyntax::trailing)};
    ir.function_flags(bytes).set_is_constexpr(true);
    ir.function_flags(bytes).set_define_in_header(true);
    ir.set_function_noexcept(bytes);
    children.new_lines(1)
        .add(ir.header_function(bytes))
        .new_lines(2)
        .append(storage_implementation_nodes(ctx, model))
        .new_lines(1)
        .add(ir.access(Access::public_))
        .new_lines(1);

    auto api{lower_soa_api(ctx,
                           model_store.schema[model_store_index],
                           SoaReceiver::owner,
                           ctx.text(owner_name),
                           row_operations)};
    children.new_lines(1).append(std::move(api.header));
    source.append(std::move(api.source)).new_lines(2);
    children.new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .add(ir.friend_type(ir.types.intern("Operations")))
        .new_lines(1)
        .add(ir.friend_type(
            ir.types.named("ioj::soa_storage_detail::StorageRequirements", NameOrigin::global)))
        .new_lines(1)
        .add(ir.instance_member(ir.types.decorate(ir.types.std.pmr.memory_resource, "", "*"),
                                ir.strings.intern("resource_"),
                                ir.init_list({})));
    auto const created_record{ir.record(owner_name, children.build(), RecordKind::struct_)};
    describe_compact_scope(ctx, created_record, model_store.schema[model_store_index]);
    ir.set_record_bases(
        created_record,
        {ir.types.decorate(
             ir.types.named("ioj::native_soa::StorageState", NameOrigin::global), "protected ", ""),
         row_operations});
    ir.set_record_export_specifier(
        created_record,
        ctx.declarations.soas
            .export_specifier[ctx.declarations.soas.index(model_store.schema[model_store_index])]);
    ir.set_record_dependencies(created_record,
                               ctx.ir.lowering.items(model_store.dependencies[model_store_index]));
    return created_record;
}

} // namespace lispb::codegen::detail
