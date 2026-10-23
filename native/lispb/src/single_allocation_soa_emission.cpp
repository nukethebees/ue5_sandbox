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

void describe_compact_scope(LoweringContext& context, RecordId record, DeclarationHandle schema) {
    if (!context.declarations.soas.using_declarations[context.declarations.soas.index(schema)]
             .empty()) {
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
                            "get_view",
                            "get_const_view",
                            "slice",
                            "left",
                            "right"}) {
        names.push_back(context.ir.strings.intern(name));
    }
    context.ir.set_scope_bindings(record, names);
}

auto adjacent(LoweringContext& context, Nodes const& nodes) -> Nodes {
    auto& ir{context.ir};
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

auto section_header(LoweringContext& context, std::string_view title) -> Nodes {
    auto& ir{context.ir};
    return adjacent(context,
                    {ir.block_comment("****************************************"),
                     ir.line_comment(title),
                     ir.block_comment("****************************************")});
}

auto inline_function(LoweringContext& context, FunctionId spec) -> NodeId {
    auto& ir{context.ir};
    context.ir.function_flags(spec).set_is_inline(true);
    context.ir.function_flags(spec).set_define_in_header(true);
    return ir.header_function(spec);
}

auto compact_function(LoweringContext& context, FunctionId spec) -> NodeId {
    context.ir.function_formatting(spec).body_layout = FunctionBodyLayout::compact;
    return inline_function(context, spec);
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

auto column_access(LoweringContext& context,
                   LoweringPathId path,
                   std::string receiver,
                   std::map<LoweringPathId, StringId>& groups,
                   NodeListBuilder& body) -> Expr {
    auto& ir{context.ir};
    LoweringPathId group_path;
    auto const group_count{ir.lowering.paths.items(path).size() - 1};
    for (std::size_t index{}; index < group_count; ++index) {
        auto const component{ir.lowering.paths.items(path)[index]};
        group_path = ir.lowering.paths.append(group_path, component);
        auto found{groups.find(group_path)};
        if (found == groups.end()) {
            auto const name{ir.strings.intern(context.path_text(group_path, "_") + "_view")};
            found = groups.emplace(group_path, name).first;
            auto const accessor{receiver.empty() ? "view_" + context.text(component)
                                                 : receiver + ".view_" + context.text(component)};
            body.add(
                ir.variable(ir.types.intern("auto const&"), name, ir.call(ir.named(accessor))));
        }
        receiver = context.text(found->second);
    }
    auto const last{ir.lowering.paths.items(path).back()};
    return ir.call(
        ir.named(receiver.empty() ? context.text(last) : receiver + "." + context.text(last)));
}

auto storage_lifetime_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const& soas{context.declarations.soas};
    auto const owner_name{soas.name[soas.index(model_store.schema[model_store.index(model)])]};
    auto& ir{context.ir};
    auto const copy_parameter{ir.parameter(context.text(owner_name) + " const&", "")};
    auto const move_parameter{ir.parameter(context.text(owner_name) + "&&", "other")};
    auto const free_data{ir.call(ir.named("Operations::release_storage"), {ir.named("*this")})};
    auto const default_constructor{ir.constructor(owner_name)};
    ir.set_function_noexcept(default_constructor);
    ir.set_function_body(
        default_constructor,
        {},
        {ir.member_initializer(owner_name, ir.store_text("*std::pmr::get_default_resource()"))});
    auto const resource_constructor{ir.constructor(owner_name, true)};
    ir.set_function_parameters(resource_constructor,
                               {ir.parameter("std::pmr::memory_resource&", "resource")});
    ir.set_function_noexcept(resource_constructor);
    ir.set_function_body(
        resource_constructor,
        {},
        {ir.member_initializer(ir.strings.intern("resource_"), ir.store_text("&resource"))});
    auto const resource_accessor{ir.function("get_memory_resource", "auto")};
    ir.set_function_body(resource_accessor, {ir.return_stmt(ir.named("resource_"))});
    ir.set_function_return_type(
        resource_accessor, ir.types.intern("std::pmr::memory_resource*"), ReturnSyntax::trailing);
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
        copy_assignment, ir.types.intern(context.text(owner_name) + "&"), ReturnSyntax::trailing);
    ir.set_function_deleted(copy_assignment);
    auto const move_constructor{ir.constructor(owner_name)};
    ir.set_function_parameters(move_constructor, {move_parameter});
    ir.set_function_noexcept(move_constructor);
    ir.set_function_body(
        move_constructor,
        {ir.expr_stmt(
            ir.call(ir.named("Operations::take_storage"),
                    {ir.unary(UnaryOperator::dereference, ir.named("this")), ir.named("other")}))},
        {ir.member_initializer(ir.strings.intern("resource_"), ir.store_text("other.resource_"))});
    auto const move_assignment{ir.function("operator=", "auto")};
    ir.set_function_parameters(move_assignment, {move_parameter});
    ir.set_function_body(
        move_assignment,
        {ir.return_stmt(
            ir.call(ir.named("Operations::move_assign"),
                    {ir.unary(UnaryOperator::dereference, ir.named("this")), ir.named("other")}))});
    ir.set_function_return_type(
        move_assignment, ir.types.intern(context.text(owner_name) + "&"), ReturnSyntax::trailing);

    NodeListBuilder result{ir};
    result.append(section_header(context, "Lifetime"))
        .new_lines(1)
        .append(adjacent(context,
                         {inline_function(context, default_constructor),
                          inline_function(context, resource_constructor),
                          compact_function(context, resource_accessor),
                          compact_function(context, destructor),
                          ir.declaration(copy_constructor),
                          ir.declaration(copy_assignment),
                          inline_function(context, move_constructor),
                          inline_function(context, move_assignment)}));
    return result.build();
}

auto data_pointers_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder children{ir};
    children.add(ir.using_declaration(
        ir.strings.intern("Element"),
        ir.types.intern("std::conditional_t<std::is_const_v<Byte>, T const, T>"),
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})));
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        children.new_lines(1).add(ir.instance_member(
            ir.types.decorate(column_store.type[column_store.index(column)], "Element<", ">*"),
            column_store.flattened_identifier[column_store.index(column)],
            ir.init_list({})));
    }

    auto const created_record{
        ir.record(ir.strings.intern("DataPointers"), children.build(), RecordKind::struct_)};
    ir.set_record_template(
        created_record,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))}));
    return created_record;
}

auto layout_column(LoweringContext& context, SingleAllocationColumnId column) -> Expr {
    auto& ir{context.ir};
    auto const& columns{ir.lowering.single_allocation_columns};
    return ir.named("Layout::" + context.text(columns.flattened_identifier[columns.index(column)]) +
                    "_column");
}

auto member_columns(LoweringContext& context, SingleAllocationModelId model, StringId member)
    -> std::vector<SingleAllocationColumnId> {
    auto const& storage{context.ir.lowering};
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

auto schema_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    NodeListBuilder body{ir};
    std::map<LoweringPathId, StringId> groups;
    std::vector<Expr> arguments;
    for (auto const column : ir.lowering.items(models.columns[models.index(model)])) {
        arguments.push_back(column_access(
            context, columns.member_path[columns.index(column)], "source", groups, body));
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
        ir.strings.intern("Schema"), {inline_function(context, function)}, RecordKind::struct_);
}

auto shared_view_type(LoweringContext& context, DeclarationHandle schema, bool resolved)
    -> std::string {
    auto const scope{schema_namespace(context, schema)};
    return "std::conditional_t<Const, " + scope +
           (resolved ? "::ResolvedConstView, " : "::ConstView, ") + scope +
           (resolved ? "::ResolvedView>" : "::View>");
}

auto view_accessor_nodes(LoweringContext& context, SingleAllocationModelId model, bool resolved)
    -> Nodes {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const& members{context.declarations.soa_members};
    auto const model_row{models.index(model)};
    auto const schema{models.schema[model_row]};
    auto const schema_row{context.declarations.soas.index(schema)};
    auto const selected{ir.lowering.items(models.columns[model_row])};
    auto const strided{is_strided_schema(context, schema)};
    NodeListBuilder children{ir};
    auto accessor = [&](std::string_view name, Expr value, CppTypeId type = {}) {
        auto const function{
            make_function(ir, name, "auto", {}, {ir.return_stmt(value)}, FunctionReceiver::const_)};
        if (type.value != 0) {
            ir.set_function_return_type(function, type, ReturnSyntax::trailing);
        }
        children.new_lines(1).add(compact_function(context, function));
    };
    auto column_span = [&](SingleAllocationColumnId column) {
        auto const index{std::find(selected.begin(), selected.end(), column) - selected.begin()};
        if (resolved) {
            return ir.call(ir.template_id(ir.named("this->template column"),
                                          {layout_column(context, column)}));
        }
        if (strided) {
            return ir.call(ir.named("this->column"), {ir.literal(std::to_string(index))});
        }
        return ir.call(ir.template_id(ir.named("this->template column"),
                                      {ir.type_reference(columns.type[columns.index(column)])}),
                       {layout_column(context, column)});
    };
    for (auto const member :
         context.declarations.items(context.declarations.soas.members[schema_row])) {
        auto const row{members.index(member)};
        auto const name{context.text(members.name[row])};
        if (members.kind[row] == SoaMemberKind::nested) {
            auto const nested{context.for_declaration(schema).nested_declaration(member)};
            auto const nested_columns{member_columns(context, model, members.name[row])};
            auto const nested_strided{is_strided_schema(context, nested)};
            auto const type{
                ir.types.intern(shared_view_type(context, nested, resolved || !nested_strided))};
            if (!resolved && nested_strided && strided) {
                accessor("view_" + name,
                         ir.init_list({ir.call(ir.member_access(column_span(nested_columns.front()),
                                                                "data")),
                                       ir.call(ir.named("this->byte_stride")),
                                       ir.call(ir.named("this->num"))}),
                         type);
            } else if (!resolved && nested_strided) {
                auto const factory{ir.template_id(
                    ir.named("ioj::soa_storage_detail::make_strided_view"),
                    {ir.type_reference(type), ir.type_reference(ir.types.intern("Layout"))})};
                accessor("view_" + name,
                         ir.call(factory,
                                 {ir.named("this->state_"),
                                  ir.named("this->offset_"),
                                  ir.call(ir.named("this->num")),
                                  layout_column(context, nested_columns[0]),
                                  layout_column(context, nested_columns[1])}),
                         type);
            } else {
                std::vector<Expr> pointers;
                for (auto const column : nested_columns) {
                    pointers.push_back(
                        resolved
                            ? ir.member_access(
                                  ir.named("this->columns_"),
                                  context.text(columns.flattened_identifier[columns.index(column)]))
                            : ir.call(ir.member_access(column_span(column), "data")));
                }
                pointers.push_back(ir.call(ir.named("this->num")));
                accessor("view_" + name, ir.init_list(pointers), type);
            }
        } else {
            auto const column{
                column_for(context, model, ir.lowering.paths.append({}, members.name[row]))};
            accessor(name, column_span(column));
        }
    }
    return children.build();
}

auto emit_resolved_views(LoweringContext& context,
                         SingleAllocationModelId model,
                         NodeListBuilder& source,
                         NodeListBuilder& declarations) -> Nodes {
    auto& ir{context.ir};
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
    result.append(section_header(context, "Resolved views")).new_lines(1);

    auto const scope{schema_namespace(context, schema)};
    std::string const base{
        "ioj::soa_storage_detail::ResolvedViewBase<Layout::column_types, DataPointers, Const>"};
    NodeListBuilder children{ir};
    children.append(adjacent(
        context,
        {ir.using_declaration(ir.strings.intern("Base"), ir.types.intern(base)),
         ir.raw("using Base::Base;"),
         ir.using_declaration(ir.strings.intern("Layout"), ir.types.intern(scope + "::Layout")),
         ir.using_declaration(ir.strings.intern("view_type"),
                              ir.types.intern(scope + "::" + mutable_name)),
         ir.using_declaration(ir.strings.intern("const_view_type"),
                              ir.types.intern(scope + "::" + const_name)),
         ir.using_declaration(ir.strings.intern("soa_schema"),
                              ir.types.intern(scope + "::Schema"))}));

    auto const conversion{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_parameters(conversion,
                               {ir.parameter(implementation + "<false> const&", "other")});
    ir.set_function_template(
        conversion,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.intern("bool"), ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion, ir.named("Enabled"));
    ir.set_function_body(
        conversion, {}, {ir.member_initializer(ir.strings.intern("Base"), ir.store_text("other"))});
    children.new_lines(1).add(inline_function(context, conversion));

    children.append(view_accessor_nodes(context, model, true));
    auto const record{
        ir.record(ir.strings.intern(implementation), children.build(), RecordKind::struct_)};
    describe_compact_scope(context, record, schema);
    ir.set_record_bases(record, {ir.types.intern(base)});
    ir.set_record_template(record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("bool"), ir.strings.intern("Const"))}));
    result.add(record).new_lines(1);
    for (bool const is_const : {true, false}) {
        auto const name{is_const ? const_name : mutable_name};
        auto const base{implementation + (is_const ? "<true>" : "<false>")};
        NodeListBuilder wrapper{ir};
        wrapper.add(ir.raw("using Base = " + base + ";\nusing Base::Base;")).new_lines(1);
        auto api{lower_soa_api(
            context, schema, is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view, name)};
        wrapper.append(api.header);
        if (!api.source.empty()) {
            source.append(api.source).new_lines(1);
        }
        auto const type{ir.record(ir.strings.intern(name), wrapper.build(), RecordKind::struct_)};
        ir.set_record_bases(type, {ir.types.intern(base)});
        result.add(type).new_lines(1);
    }
    return result.build();
}

auto compact_view_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const model_row{models.index(model)};
    auto const schema{models.schema[model_row]};
    auto const schema_row{context.declarations.soas.index(schema)};
    auto const scope{schema_namespace(context, schema)};
    auto const selected{ir.lowering.items(models.columns[model_row])};
    auto const strided{is_strided_schema(context, schema)};
    auto const base{strided ? "ioj::soa_storage_detail::StridedViewBase<" +
                                  std::string{ir.types.spelling(
                                      columns.type[columns.index(selected.front())])} +
                                  ", Const>"
                            : "ioj::soa_storage_detail::CompactViewBase<Const>"};
    NodeListBuilder children{ir};
    children.append(adjacent(
        context,
        {ir.using_declaration(ir.strings.intern("soa_schema"),
                              ir.types.intern(schema_namespace(context, schema) + "::Schema")),
         ir.using_declaration(ir.strings.intern("Base"), ir.types.intern(base)),
         ir.raw("using Base::Base;"),
         ir.using_declaration(ir.strings.intern("Layout"), ir.types.intern(scope + "::Layout")),
         ir.using_declaration(ir.strings.intern("view_type"), ir.types.intern(scope + "::View")),
         ir.using_declaration(ir.strings.intern("const_view_type"),
                              ir.types.intern(scope + "::ConstView")),
         ir.using_declaration(ir.strings.intern("resolved_view_type"),
                              ir.types.intern(shared_view_type(context, schema, true)))}));
    if (strided && !context.declarations.soas.layout_only[schema_row]) {
        children.new_lines(1).add(
            ir.raw("using State = std::conditional_t<Const, ioj::soa_storage_detail::StorageState "
                   "const, ioj::soa_storage_detail::StorageState>;"));
        auto const constructor{ir.constructor(ir.strings.intern("ViewImpl"))};
        ir.set_function_parameters(constructor,
                                   {ir.parameter("State*", "state"),
                                    ir.parameter("std::uint32_t", "offset"),
                                    ir.parameter("std::uint32_t", "count")});
        std::vector<std::string> arguments{"state", "offset", "count"};
        for (auto const column : selected.first(2)) {
            arguments.push_back(
                "Layout::" + context.text(columns.flattened_identifier[columns.index(column)]) +
                "_column");
        }
        ir.set_function_body(
            constructor,
            {},
            {ir.member_initializer(
                ir.strings.intern("Base"),
                ir.store_text("ioj::soa_storage_detail::make_strided_view<Base, Layout>(" +
                              join(arguments, ", ") + ")"))});
        children.new_lines(1).add(inline_function(context, constructor));
    }
    auto const conversion{ir.constructor(ir.strings.intern("ViewImpl"))};
    ir.set_function_parameters(conversion, {ir.parameter("ViewImpl<false> const&", "other")});
    ir.set_function_body(
        conversion, {}, {ir.member_initializer(ir.strings.intern("Base"), ir.store_text("other"))});
    ir.set_function_template(
        conversion,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.intern("bool"), ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion, ir.named("Enabled"));
    children.new_lines(1).add(inline_function(context, conversion));
    children.append(view_accessor_nodes(context, model, false));
    auto const record{
        ir.record(ir.strings.intern("ViewImpl"), children.build(), RecordKind::struct_)};
    describe_compact_scope(context, record, schema);
    ir.set_record_bases(record, {ir.types.intern(base)});
    ir.set_record_template(record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("bool"), ir.strings.intern("Const"))}));
    return record;
}

auto view_validation_nodes(LoweringContext& context, std::string const& name, bool strided)
    -> Nodes {
    auto& ir{context.ir};
    return {ir.static_assertion(
        ir.store_text(strided ? "std::is_trivially_copyable_v<" + name + ">"
                              : "ioj::soa_storage_detail::validate_compact_view<" + name + ">()"),
        {})};
}

} // namespace

auto emit_single_allocation_layout(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const selected{ir.lowering.items(models.columns[models.index(model)])};
    std::vector<CppTypeId> types;
    for (auto const column : selected) {
        types.push_back(columns.type[columns.index(column)]);
    }
    auto const base{ir.types.template_type(
        ir.types.intern(
            "ioj::single_allocation_layout::ColumnLayouts",
            {ir.types.dependency("single_allocation_layout", "ioj/single_allocation/layout.h")}),
        types)};
    NodeListBuilder children{ir};
    for (std::size_t index{}; index < selected.size(); ++index) {
        auto const row{columns.index(selected[index])};
        if (index != 0) {
            children.new_lines(1);
        }
        children.add(ir.constexpr_member(
            ir.types.intern("std::size_t"),
            ir.strings.intern(context.text(columns.flattened_identifier[row]) + "_column"),
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
    result.append(section_header(context, "Layout")).new_lines(1).add(layout);
    return result.build();
}

auto storage_implementation_nodes(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const scope{schema_namespace(context, model_store.schema[model_store.index(model)])};
    auto& ir{context.ir};
    NodeListBuilder children{ir};
    children
        .append(adjacent(
            context,
            {ir.using_declaration(ir.strings.intern("View"), ir.types.intern(scope + "::View")),
             ir.using_declaration(ir.strings.intern("ConstView"),
                                  ir.types.intern(scope + "::ConstView"))}))
        .new_lines(2)
        .append(storage_lifetime_nodes(context, model))
        .new_lines(1)
        .add(ir.access(Access::protected_))
        .new_lines(1)
        .add(ir.using_declaration(
            ir.strings.intern("DataPointers"),
            ir.types.intern(scope + "::DataPointers<Byte>"),
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))})));

    return children.build();
}

auto emit_single_allocation_views(LoweringContext& context,
                                  SingleAllocationModelId model,
                                  NodeListBuilder& source,
                                  NodeListBuilder& declarations) -> Nodes {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const schema{models.schema[models.index(model)]};
    auto const strided{is_strided_schema(context, schema)};
    declarations.add(ir.forward_declaration(ir.strings.intern("View")))
        .new_lines(1)
        .add(ir.forward_declaration(ir.strings.intern("ConstView")))
        .new_lines(1);
    NodeListBuilder result{ir};
    result.append(section_header(context, "Column pointers"))
        .new_lines(1)
        .add(data_pointers_node(context, model))
        .new_lines(2)
        .add(schema_node(context, model))
        .new_lines(2)
        .append(emit_resolved_views(context, model, source, declarations))
        .new_lines(2)
        .append(section_header(context, strided ? "Strided views" : "Compact views"))
        .new_lines(1)
        .add(compact_view_node(context, model))
        .new_lines(1);
    for (bool const is_const : {true, false}) {
        std::string const name{is_const ? "ConstView" : "View"};
        std::string const base{is_const ? "ViewImpl<true>" : "ViewImpl<false>"};
        NodeListBuilder children{ir};
        children.add(ir.raw("using Base = " + base + ";\nusing Base::Base;")).new_lines(1);
        auto api{lower_soa_api(
            context, schema, is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view, name)};
        children.append(api.header);
        if (!api.source.empty()) {
            source.append(api.source).new_lines(2);
        }
        auto const record{
            ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
        ir.set_record_bases(record, {ir.types.intern(base)});
        ir.set_record_export_specifier(
            record,
            context.declarations.soas.export_specifier[context.declarations.soas.index(schema)]);
        result.add(record)
            .new_lines(1)
            .append(view_validation_nodes(context, name, strided))
            .new_lines(1);
    }
    return result.build();
}

auto emit_single_allocation_container(LoweringContext& context,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto const schema{model_store.schema[model_store_index]};
    auto const& soas{context.declarations.soas};
    auto const owner_name{soas.name[soas.index(schema)]};
    auto const scope{schema_namespace(context, schema)};
    auto& ir{context.ir};
    NodeListBuilder children{ir};
    auto const operations{std::string{"ioj::native_soa::StorageOperations"}};
    children
        .add(ir.using_declaration(
            ir.strings.intern("soa_schema"),
            ir.types.intern(schema_namespace(context, model_store.schema[model_store_index]) +
                            "::Schema")))
        .new_lines(1);
    children.add(ir.using_declaration(ir.strings.intern("Operations"), ir.types.intern(operations)))
        .new_lines(1);
    children
        .add(ir.raw("using Layout = " + scope + "::Layout" +
                    ";\n"
                    "using size_type = Layout::size_type;\n"
                    "using byte_size_type = Layout::byte_size_type;\n\n"
                    "static constexpr auto capacity_granularity = "
                    "Layout::capacity_granularity;\n"
                    "static constexpr auto allocation_alignment = "
                    "Layout::allocation_alignment;\n"
                    "static constexpr auto capacity_block_bound = "
                    "Layout::capacity_block_bound;\n"
                    "static constexpr auto max_capacity = Layout::max_capacity;\n\n"
                    "static constexpr auto layout_bytes(byte_size_type blocks) noexcept "
                    "-> byte_size_type { return Layout::layout_bytes(blocks); }"))
        .new_lines(2)
        .append(storage_implementation_nodes(context, model))
        .new_lines(1)
        .add(ir.access(Access::public_))
        .new_lines(1);

    auto api{lower_soa_api(context,
                           model_store.schema[model_store_index],
                           SoaReceiver::owner,
                           context.text(owner_name))};
    children.new_lines(1).append(std::move(api.header));
    source.append(std::move(api.source)).new_lines(2);
    children.new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .add(ir.friend_type(ir.types.intern("Operations")))
        .new_lines(1)
        .add(ir.friend_type(ir.types.intern("::ioj::soa_storage_detail::StorageRequirements")))
        .new_lines(1)
        .add(ir.instance_member(ir.types.intern("std::pmr::memory_resource*"),
                                ir.strings.intern("resource_"),
                                ir.init_list({})));
    auto const created_record{ir.record(owner_name, children.build(), RecordKind::struct_)};
    describe_compact_scope(context, created_record, model_store.schema[model_store_index]);
    ir.set_record_bases(
        created_record,
        {ir.types.intern("protected " + std::string{"ioj::native_soa::StorageState"}),
         ir.types.intern(operations)});
    ir.set_record_export_specifier(
        created_record,
        context.declarations.soas.export_specifier[context.declarations.soas.index(
            model_store.schema[model_store_index])]);
    ir.set_record_dependencies(
        created_record, context.ir.lowering.items(model_store.dependencies[model_store_index]));
    return created_record;
}

} // namespace lispb::codegen::detail
