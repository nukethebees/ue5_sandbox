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
#include <codegen/schema/module_settings_store.h>
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
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
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

void set_source_template(LoweringContext& context,
                         SingleAllocationModelId model,
                         FunctionId function) {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    ir.set_function_template(
        function,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Columns"))}));
    ir.set_function_requires_clause(
        function,
        ir.template_id(ir.named("ioj::soa_storage_detail::SoaSourceFor"),
                       {ir.type_reference(ir.types.intern("Columns")),
                        ir.type_reference(
                            ir.types.intern(context.text(models.owner_name[models.index(model)]))),
                        ir.type_reference(ir.types.intern("size_type"))}));
}

auto column_access(LoweringContext& context,
                   LoweringPathId path,
                   std::string receiver,
                   std::map<LoweringPathId, StringId>& groups,
                   NodeListBuilder& body,
                   std::size_t prefix_count = 0) -> Expr {
    auto& ir{context.ir};
    LoweringPathId group_path;
    auto const group_count{ir.lowering.paths.items(path).size() - prefix_count - 1};
    for (std::size_t index{}; index < group_count; ++index) {
        auto const component{ir.lowering.paths.items(path)[prefix_count + index]};
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
    auto const model_store_index{model_store.index(model)};
    auto& ir{context.ir};
    auto const copy_parameter{
        ir.parameter(context.text(model_store.owner_name[model_store_index]) + " const&", "")};
    auto const move_parameter{
        ir.parameter(context.text(model_store.owner_name[model_store_index]) + "&&", "other")};
    auto const free_data{ir.call(ir.named("Operations::release_storage"), {ir.named("*this")})};
    auto const default_constructor{ir.constructor(model_store.owner_name[model_store_index])};
    ir.set_function_noexcept(default_constructor);
    ir.set_function_body(
        default_constructor,
        {},
        {ir.member_initializer(model_store.owner_name[model_store_index],
                               ir.store_text("*std::pmr::get_default_resource()"))});
    auto const resource_constructor{
        ir.constructor(model_store.owner_name[model_store_index], true)};
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
    auto const destructor{ir.destructor(model_store.owner_name[model_store_index])};
    ir.set_function_body(destructor, {ir.expr_stmt(free_data)});
    auto const copy_constructor{ir.constructor(model_store.owner_name[model_store_index])};
    ir.set_function_parameters(copy_constructor, {copy_parameter});
    ir.set_function_deleted(copy_constructor);
    auto const copy_assignment{ir.function("operator=", "auto")};
    ir.set_function_parameters(copy_assignment, {copy_parameter});
    ir.set_function_return_type(
        copy_assignment,
        ir.types.intern(context.text(model_store.owner_name[model_store_index]) + "&"),
        ReturnSyntax::trailing);
    ir.set_function_deleted(copy_assignment);
    auto const move_constructor{ir.constructor(model_store.owner_name[model_store_index])};
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
        move_assignment,
        ir.types.intern(context.text(model_store.owner_name[model_store_index]) + "&"),
        ReturnSyntax::trailing);

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

    std::vector<Expr> shifted_columns;
    shifted_columns.reserve(
        context.ir.lowering.items(model_store.columns[model_store_index]).size());
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        shifted_columns.push_back(ir.binary(
            BinaryOperator::add,
            ir.named(context.text(column_store.flattened_identifier[column_store.index(column)])),
            ir.named("offset")));
    }
    auto const offset_operator{make_function(
        ir,
        "operator+",
        "DataPointers",
        {ir.parameter("size_type const", "offset")},
        {ir.if_stmt(
             ir.binary(
                 BinaryOperator::equal,
                 ir.named(context.text(column_store.flattened_identifier[column_store.index(
                     context.ir.lowering.items(model_store.columns[model_store_index]).front())])),
                 ir.literal("nullptr")),
             ir.block({ir.return_stmt(ir.init_list({}))})),
         ir.return_stmt(ir.init_list(std::move(shifted_columns)))},
        FunctionReceiver::const_,
        ReturnSyntax::trailing)};
    ir.set_function_noexcept(offset_operator);
    children.new_lines(1).add(inline_function(context, offset_operator));
    auto const created_record{
        ir.record(ir.strings.intern("DataPointers"), children.build(), RecordKind::struct_)};
    ir.set_record_template(
        created_record,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))}));
    return created_record;
}

auto storage_access_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& ir{context.ir};
    auto const self{ir.parameter("this Self&", "self")};
    auto const get_data{ir.function("get_data", "auto")};
    ir.set_function_parameters(get_data, {self});
    ir.set_function_body(
        get_data,
        {ir.using_declaration(
             ir.strings.intern("Byte"),
             ir.types.intern(
                 "std::conditional_t<std::is_const_v<Self>, std::byte const, std::byte>")),
         ir.if_stmt(
             ir.binary(BinaryOperator::equal,
                       ir.member_access(ir.named("self"), "data_"),
                       ir.literal("nullptr")),
             ir.block({ir.return_stmt(ir.init_list({}, ir.types.intern("DataPointers<Byte>")))})),
         ir.return_stmt(
             ir.call(ir.named("make_data_unchecked"),
                     {ir.static_cast_expr(ir.types.intern("Byte*"),
                                          ir.member_access(ir.named("self"), "data_")),
                      ir.call(ir.member_access(ir.named("self"), "capacity_blocks"))}))});
    ir.set_function_noexcept(get_data);
    ir.set_function_template(
        get_data, ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Self"))}));
    auto const get_data_offset{ir.function("get_data", "auto")};
    ir.set_function_parameters(get_data_offset, {self, ir.parameter("size_type const", "offset")});
    ir.set_function_body(
        get_data_offset,
        {ir.return_stmt(ir.binary(BinaryOperator::add,
                                  ir.call(ir.member_access(ir.named("self"), "get_data")),
                                  ir.named("offset")))});
    ir.set_function_noexcept(get_data_offset);
    ir.set_function_template(
        get_data_offset,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Self"))}));
    return adjacent(context,
                    {data_pointers_node(context, model),
                     inline_function(context, get_data),
                     inline_function(context, get_data_offset)});
}

auto layout_column(LoweringContext& context, SingleAllocationColumnId column) -> Expr {
    auto& ir{context.ir};
    auto const& columns{ir.lowering.single_allocation_columns};
    auto const path{ir.lowering.paths.items(columns.member_path[columns.index(column)])};
    auto result{ir.named("Layout::columns")};
    auto const count{path.size()};
    for (std::size_t index{}; index < count; ++index) {
        result = ir.member_access(result,
                                  title_case_identifier(context.text(path[index])) +
                                      (index + 1 == count ? "Column" : ""));
    }
    return result;
}

auto column_pointer_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& ir{context.ir};
    NodeListBuilder make_data_body{ir};
    make_data_body.add(
        ir.variable(std::string{"ioj::native_soa::LayoutCursor"}, "cursor", ir.named("blocks")));
    std::vector<Expr> pointers;
    pointers.reserve(context.ir.lowering.items(model_store.columns[model_store_index]).size());
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        pointers.push_back(ir.call(ir.member_access(ir.named("cursor"), "column_pointer"),
                                   {ir.named("data"), layout_column(context, column)}));
    }
    make_data_body.add(ir.return_stmt(ir.init_list(std::move(pointers))));
    auto const make_data{ir.function("make_data_unchecked", "auto")};
    ir.set_function_parameters(
        make_data,
        {ir.parameter("Byte* const", "data"), ir.parameter("byte_size_type const", "blocks")});
    ir.set_function_body(make_data, make_data_body.build());
    ir.set_function_return_type(
        make_data, ir.types.intern("DataPointers<Byte>"), ReturnSyntax::trailing);
    ir.set_function_noexcept(make_data);
    ir.set_function_receiver(make_data, FunctionReceiver::static_);
    ir.set_function_template(
        make_data,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Byte"))}));
    auto const capacity_blocks{ir.function("capacity_blocks", "auto")};
    ir.set_function_body(
        capacity_blocks,
        {ir.return_stmt(ir.static_cast_expr(ir.types.intern("byte_size_type"),
                                            ir.binary(BinaryOperator::divide,
                                                      ir.named("capacity_"),
                                                      ir.named("capacity_granularity"))))});
    ir.set_function_return_type(
        capacity_blocks, ir.types.intern("byte_size_type"), ReturnSyntax::trailing);
    ir.set_function_receiver(capacity_blocks, FunctionReceiver::const_);
    ir.set_function_noexcept(capacity_blocks);

    NodeListBuilder result{ir};
    result.add(ir.friend_type(ir.types.intern("Operations")))
        .new_lines(1)
        .add(ir.friend_type(ir.types.intern("::ioj::soa_storage_detail::StorageRequirements")))
        .new_lines(1)
        .append(section_header(context, "Column pointers"))
        .new_lines(1)
        .append(adjacent(
            context,
            {inline_function(context, make_data), compact_function(context, capacity_blocks)}));
    return result.build();
}

auto default_construction_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder body{ir};
    body.add(ir.raw("using " + std::string{"ioj::native_soa::default_construct_n;"}));
    body.add(
        ir.variable("auto const",
                    "columns",
                    ir.binary(BinaryOperator::add,
                              ir.call(ir.named("make_data_unchecked"),
                                      {ir.named("data_"), ir.call(ir.named("capacity_blocks"))}),
                              ir.named("first"))));
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        body.add(ir.expr_stmt(ir.call(
            ir.named("default_construct_n",
                     ir.types.dependencies(column_store.type[column_store.index(column)])),
            {ir.member_access(
                 ir.named("columns"),
                 context.text(column_store.flattened_identifier[column_store.index(column)])),
             ir.named("count")})));
    }
    return inline_function(context,
                           make_function(ir,
                                         "default_construct_columns",
                                         "void",
                                         {ir.parameter("size_type const", "first"),
                                          ir.parameter("size_type const", "count")},
                                         body.build()));
}

auto column_copying_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    auto const swap_remove{make_function(ir,
                                         "swap_remove_columns",
                                         "void",
                                         {ir.parameter("size_type const", "index"),
                                          ir.parameter("size_type const", "source"),
                                          ir.parameter("size_type const", "move_count")},
                                         {ir.expr_stmt(ir.call(ir.named("copy_columns"),
                                                               {ir.call(ir.named("get_data")),
                                                                ir.named("index"),
                                                                ir.named("source"),
                                                                ir.named("move_count")}))})};

    NodeListBuilder copy_body{ir};
    copy_body.add(
        ir.raw("auto transfer = [count](auto* dst, auto const* src) {\n    " +
               std::string{"ioj::native_soa::transfer_n<Overlapping>(dst, src, count);\n};"}));
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        copy_body.add(ir.expr_stmt(ir.call(
            ir.named("transfer"),
            {ir.member_access(
                 ir.named("destination"),
                 context.text(column_store.flattened_identifier[column_store.index(column)])),
             ir.member_access(
                 ir.named("source"),
                 context.text(column_store.flattened_identifier[column_store.index(column)]))})));
    }
    auto const transfer{
        make_function(ir,
                      "transfer_columns",
                      "void",
                      {ir.parameter("DataPointers<std::byte> const&", "destination"),
                       ir.parameter("DataPointers<Byte> const&", "source"),
                       ir.parameter("size_type", "count")},
                      copy_body.build(),
                      FunctionReceiver::static_)};
    ir.set_function_template(
        transfer,
        ir.template_declaration(
            {ir.value_template_parameter(ir.types.intern("bool"), ir.strings.intern("Overlapping")),
             ir.type_template_parameter(ir.strings.intern("Byte"))}));
    auto const copy{
        make_function(ir,
                      "copy_columns",
                      "void",
                      {ir.parameter("DataPointers<std::byte> const&", "columns"),
                       ir.parameter("size_type", "index"),
                       ir.parameter("size_type", "source"),
                       ir.parameter("size_type", "move_count")},
                      {ir.expr_stmt(ir.call(
                          ir.template_id(ir.named("transfer_columns"), {ir.literal("false")}),
                          {ir.binary(BinaryOperator::add, ir.named("columns"), ir.named("index")),
                           ir.binary(BinaryOperator::add, ir.named("columns"), ir.named("source")),
                           ir.named("move_count")}))},
                      FunctionReceiver::static_)};

    auto const remove_indices{make_function(
        ir,
        "swap_remove_indices",
        "void",
        {ir.parameter("std::span<size_type const>", "indices")},
        {ir.variable("auto const", "columns", ir.call(ir.named("get_data"))),
         ir.expr_stmt(ir.raw_expr("ioj::soa_storage_detail::for_each_removal_run(num_, indices, "
                                  "[&](size_type index, size_type source, size_type count) { "
                                  "copy_columns(columns, index, source, count); })"))})};
    return adjacent(context,
                    {inline_function(context, transfer),
                     compact_function(context, swap_remove),
                     compact_function(context, copy),
                     inline_function(context, remove_indices)});
}

auto permutation_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const& columns{ir.lowering.single_allocation_columns};
    NodeListBuilder body{ir};
    body.add(ir.raw("assert(indices.size() == num_);"));
    body.add(ir.variable("auto const", "columns", ir.call(ir.named("get_data"))));
    for (auto const column : ir.lowering.items(models.columns[models.index(model)])) {
        auto const name{context.text(columns.flattened_identifier[columns.index(column)])};
        body.add(ir.expr_stmt(
            ir.call(ir.named("ioj::apply_permutation"),
                    {ir.init_list({ir.member_access(ir.named("columns"), name), ir.named("num_")},
                                  ir.types.intern("std::span")),
                     ir.named("indices")})));
    }
    auto const function{make_function(ir,
                                      "apply_permutation",
                                      "void",
                                      {ir.parameter("std::span<size_type>", "indices")},
                                      body.build())};
    return inline_function(context, function);
}

auto source_pointers_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder body{ir};
    body.add(ir.raw("using " + std::string{"ioj::native_soa::source_data;"}));
    std::map<LoweringPathId, StringId> groups;
    std::vector<Expr> pointers;
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        pointers.push_back(
            ir.call(ir.named("source_data"),
                    {column_access(context,
                                   column_store.member_path[column_store.index(column)],
                                   "source",
                                   groups,
                                   body)}));
    }
    body.add(ir.return_stmt(ir.init_list(std::move(pointers))));
    auto const function{make_function(ir,
                                      "source_pointers",
                                      "DataPointers<std::byte const>",
                                      {ir.parameter("Columns const&", "source")},
                                      body.build(),
                                      FunctionReceiver::static_,
                                      ReturnSyntax::trailing)};
    set_source_template(context, model, function);
    return inline_function(context, function);
}

auto source_copy_node(LoweringContext& context,
                      SingleAllocationModelId model,
                      bool const overlapping) -> NodeId {
    auto& ir{context.ir};
    auto const function{make_function(
        ir,
        overlapping ? "copy_columns_from" : "append_columns",
        "void",
        {ir.parameter("Columns const&", "source"),
         ir.parameter("size_type", "source_first"),
         ir.parameter("size_type", "first"),
         ir.parameter("size_type", "count")},
        {ir.expr_stmt(ir.call(ir.template_id(ir.named("transfer_columns"),
                                             {ir.literal(overlapping ? "true" : "false")}),
                              {ir.call(ir.named("get_data"), {ir.named("first")}),
                               ir.binary(BinaryOperator::add,
                                         ir.call(ir.named("source_pointers"), {ir.named("source")}),
                                         ir.named("source_first")),
                               ir.named("count")}))})};
    set_source_template(context, model, function);
    return inline_function(context, function);
}

auto live_column_copy_node(LoweringContext& context) -> NodeId {
    auto& ir{context.ir};
    NodeListBuilder copy_live{ir};
    copy_live.add(ir.variable("auto const", "old_blocks", ir.call(ir.named("capacity_blocks"))));
    copy_live.add(ir.variable("auto const",
                              "new_blocks",
                              ir.static_cast_expr(ir.types.intern("byte_size_type"),
                                                  ir.binary(BinaryOperator::divide,
                                                            ir.named("new_capacity"),
                                                            ir.named("capacity_granularity")))));
    copy_live.add(ir.variable(
        "auto const",
        "source",
        ir.call(ir.named("make_data_unchecked"),
                {ir.static_cast_expr(ir.types.intern("std::byte const*"), ir.named("data_")),
                 ir.named("old_blocks")})));
    copy_live.add(ir.variable(
        "auto const",
        "destination",
        ir.call(ir.named("make_data_unchecked"), {ir.named("new_data"), ir.named("new_blocks")})));
    copy_live.add(
        ir.expr_stmt(ir.call(ir.template_id(ir.named("transfer_columns"), {ir.literal("false")}),
                             {ir.named("destination"), ir.named("source"), ir.named("num_")})));

    auto const function{make_function(ir,
                                      "copy_live_columns",
                                      "void",
                                      {ir.parameter("std::byte* const", "new_data"),
                                       ir.parameter("size_type const", "new_capacity")},
                                      copy_live.build())};
    ir.set_function_noexcept(function);
    return inline_function(context, function);
}

auto nested_view(LoweringContext& context, SingleAllocationViewId view, SoaMemberId member)
    -> SingleAllocationViewId {
    auto& ir{context.ir};
    auto& views{ir.lowering.single_allocation_views};
    auto const parent_row{views.index(view)};
    auto const& members{context.declarations.soa_members};
    auto const member_row{members.index(member)};
    auto const name{members.name[member_row]};
    auto const model{views.model[parent_row]};
    auto const path{ir.lowering.paths.append(views.member_prefix[parent_row], name)};
    auto const key{std::pair{model, path}};
    if (auto const found{ir.lowering.view_lookup.find(key)};
        found != ir.lowering.view_lookup.end()) {
        return found->second;
    }
    auto const result{views.append()};
    auto const row{views.index(result)};
    views.model[row] = model;
    views.schema[row] =
        context.for_declaration(views.schema[parent_row]).nested_declaration(member);
    views.member_prefix[row] = path;
    views.view_name[row] =
        ir.strings.intern(context.text(views.view_name[parent_row]) + "_" + context.text(name));
    views.const_view_name[row] = ir.strings.intern(context.text(views.const_view_name[parent_row]) +
                                                   "_" + context.text(name));
    ir.lowering.view_lookup.emplace(key, result);
    return result;
}

auto schema_name(LoweringContext& context, DeclarationHandle schema) -> std::string {
    auto const scoped{context.for_declaration(schema)};
    auto const& declarations{context.declarations};
    auto const settings{declarations.module_settings(scoped.module)};
    auto const namespace_name{
        declarations.module_settings_store
            .namespace_name[declarations.module_settings_store.index(settings)]};
    return (namespace_name ? "::" + context.text(*namespace_name) + "::" : "::") +
           context.text(declarations.soas.name[declarations.soas.index(schema)]);
}

auto uses_canonical_resolved_view(LoweringContext& context, SingleAllocationViewId view) -> bool {
    auto const& views{context.ir.lowering.single_allocation_views};
    auto const row{views.index(view)};
    return !context.ir.lowering.paths.items(views.member_prefix[row]).empty() &&
           !context.declarations.soas
                .layout_only[context.declarations.soas.index(views.schema[row])];
}

auto resolved_view_name(LoweringContext& context, SingleAllocationViewId view, bool is_const)
    -> std::string {
    auto const& views{context.ir.lowering.single_allocation_views};
    auto const row{views.index(view)};
    auto name{context.text(is_const ? views.const_view_name[row] : views.view_name[row])};
    std::string prefix;
    if (uses_canonical_resolved_view(context, view)) {
        auto const& soas{context.declarations.soas};
        auto const schema{views.schema[row]};
        auto const schema_row{soas.index(schema)};
        auto const custom{is_const ? soas.const_view_name[schema_row] : soas.view_name[schema_row]};
        name = custom ? context.text(*custom)
                      : context.text(soas.name[schema_row]) + (is_const ? "ConstView" : "View");
        auto const qualified{schema_name(context, schema)};
        prefix = qualified.substr(0, qualified.rfind("::") + 2);
    }
    auto const position{name.find(is_const ? "ConstView" : "View")};
    name.insert(position == std::string::npos ? name.size() : position, "Resolved");
    return prefix + name;
}

auto view_columns(LoweringContext& context, SingleAllocationViewId view)
    -> std::vector<SingleAllocationColumnId> {
    auto const& storage{context.ir.lowering};
    auto const& views{storage.single_allocation_views};
    auto const row{views.index(view)};
    auto const& models{storage.single_allocation_models};
    auto const prefix{storage.paths.items(views.member_prefix[row])};
    std::vector<SingleAllocationColumnId> result;
    for (auto const column : storage.items(models.columns[models.index(views.model[row])])) {
        auto const path{
            storage.paths.items(storage.single_allocation_columns
                                    .member_path[storage.single_allocation_columns.index(column)])};
        if (path.size() >= prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), path.begin())) {
            result.push_back(column);
        }
    }
    return result;
}

auto emit_resolved_views(LoweringContext& context,
                         SingleAllocationViewId view,
                         NodeListBuilder& source) -> Nodes {
    if (uses_canonical_resolved_view(context, view)) {
        return {};
    }
    auto& ir{context.ir};
    auto& storage{ir.lowering};
    auto& views{storage.single_allocation_views};
    auto const row{views.index(view)};
    auto const schema{views.schema[row]};
    auto const model{views.model[row]};
    auto const& columns{storage.single_allocation_columns};
    auto const& members{context.declarations.soa_members};
    auto const selected{view_columns(context, view)};
    auto const mutable_name{resolved_view_name(context, view, false)};
    auto const const_name{resolved_view_name(context, view, true)};
    auto const implementation{mutable_name + "Impl"};
    NodeListBuilder result{ir};

    for (auto const member : context.declarations.items(
             context.declarations.soas.members[context.declarations.soas.index(schema)])) {
        if (members.kind[members.index(member)] == SoaMemberKind::nested) {
            result.append(emit_resolved_views(context, nested_view(context, view, member), source))
                .new_lines(1);
        }
    }
    result.add(ir.forward_declaration(ir.strings.intern(mutable_name)))
        .new_lines(1)
        .add(ir.forward_declaration(ir.strings.intern(const_name)))
        .new_lines(1);

    auto pointer_name = [&](SingleAllocationColumnId column) {
        return context.text(columns.flattened_identifier[columns.index(column)]) + "_";
    };
    auto element_type = [&](SingleAllocationColumnId column) {
        return "Element<" + std::string{ir.types.spelling(columns.type[columns.index(column)])} +
               ">";
    };
    NodeListBuilder children{ir};
    children
        .add(ir.raw("using size_type = std::uint32_t;\nusing View = " + mutable_name +
                    ";\nusing ConstView = " + const_name +
                    ";\nusing soa_schema = " + schema_name(context, schema) +
                    "Schema;\n"
                    "template <typename T> using Element = std::conditional_t<Const, T const, T>;"))
        .new_lines(1);
    auto const default_constructor{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_defaulted(default_constructor);
    children.add(ir.declaration(default_constructor)).new_lines(1);

    std::vector<FunctionParameterId> pointers;
    std::vector<FunctionParameterId> spans;
    std::vector<MemberInitializerId> initializers;
    std::vector<std::string> span_arguments;
    std::vector<Expr> size_validation_arguments{ir.named("count_")};
    std::vector<MemberInitializerId> conversions;
    NodeListBuilder validation{ir};
    std::vector<Expr> validation_arguments{ir.named("count_")};
    for (auto const column : selected) {
        auto const name{context.text(columns.flattened_identifier[columns.index(column)])};
        pointers.push_back(ir.parameter(element_type(column) + "*", name));
        spans.push_back(ir.parameter("std::span<" + element_type(column) + ">", name));
        initializers.push_back(
            ir.member_initializer(ir.strings.intern(pointer_name(column)), ir.store_text(name)));
        conversions.push_back(
            ir.member_initializer(ir.strings.intern(pointer_name(column)),
                                  ir.store_text("other." + pointer_name(column))));
        span_arguments.push_back(name + ".data()");
        size_validation_arguments.push_back(ir.named(name));
        validation_arguments.push_back(ir.named(pointer_name(column)));
    }
    validation.add(ir.expr_stmt(ir.call(
        ir.named("ioj::soa_storage_detail::validate_column_pointers"), validation_arguments)));
    pointers.push_back(ir.parameter("size_type", "count"));
    initializers.push_back(
        ir.member_initializer(ir.strings.intern("count_"), ir.store_text("count")));
    auto const pointer_constructor{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_parameters(pointer_constructor, pointers);
    ir.set_function_body(pointer_constructor, validation.build(), initializers);
    children.add(inline_function(context, pointer_constructor)).new_lines(1);

    auto const first_name{
        context.text(columns.flattened_identifier[columns.index(selected.front())])};
    span_arguments.push_back("ioj::soa_storage_detail::checked_row_count(" + first_name +
                             ".size())");
    auto const span_constructor{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_parameters(span_constructor, spans);
    ir.set_function_body(
        span_constructor,
        {ir.expr_stmt(ir.call(ir.named("ioj::soa_storage_detail::validate_column_sizes"),
                              size_validation_arguments))},
        {ir.member_initializer(ir.strings.intern(implementation),
                               ir.store_text(join(span_arguments, ", ")))});
    children.add(inline_function(context, span_constructor)).new_lines(1);

    conversions.push_back(
        ir.member_initializer(ir.strings.intern("count_"), ir.store_text("other.count_")));
    auto const conversion{ir.constructor(ir.strings.intern(implementation))};
    ir.set_function_parameters(conversion,
                               {ir.parameter(implementation + "<false> const&", "other")});
    ir.set_function_template(
        conversion,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.intern("bool"), ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion, ir.named("Enabled"));
    ir.set_function_body(conversion, {}, conversions);
    children.add(inline_function(context, conversion)).new_lines(1);

    std::vector<std::string> const_arguments;
    for (auto const column : selected) {
        const_arguments.push_back("self." + pointer_name(column));
    }
    const_arguments.push_back("self.count_");
    children.add(ir.raw("auto num() const noexcept -> size_type { return count_; }\n"
                        "auto is_empty() const noexcept -> bool { return count_ == 0; }\n"
                        "auto get_view(this auto const& self) { return self; }\n"
                        "auto get_const_view(this auto const& self) {\n"
                        "using Result = typename std::remove_cvref_t<decltype(self)>::ConstView;\n"
                        "return Result{" +
                        join(const_arguments, ", ") +
                        "};\n}\n"
                        "auto resolve(this auto const& self) { return self; }"));
    children.new_lines(1).add(compact_function(
        context,
        make_function(ir, "validate", "void", {}, validation.build(), FunctionReceiver::const_)));

    std::vector<std::string> slice_arguments;
    for (auto const column : selected) {
        auto const name{pointer_name(column)};
        slice_arguments.push_back("self." + name + " == nullptr ? nullptr : self." + name +
                                  " + offset");
    }
    slice_arguments.push_back("count");
    children.new_lines(1).add(ir.raw(
        "auto slice(this auto const& self, size_type offset, size_type count) {\n"
        "assert(offset <= self.count_ && count <= self.count_ - offset);\n"
        "using Result = std::remove_cvref_t<decltype(self)>;\n"
        "return Result{" +
        join(slice_arguments, ", ") +
        "};\n}\n"
        "auto get_view(this auto const& self, size_type offset, size_type count) { return "
        "self.slice(offset, count); }\n"
        "auto get_const_view(this auto const& self, size_type offset, size_type count) { return "
        "self.slice(offset, count).get_const_view(); }\n"
        "auto left(this auto const& self, size_type count) { return self.slice(0, count); }\n"
        "auto right(this auto const& self, size_type count) { assert(count <= self.num()); return "
        "self.slice(self.num() - count, count); }"));

    for (auto const member : context.declarations.items(
             context.declarations.soas.members[context.declarations.soas.index(schema)])) {
        auto const member_row{members.index(member)};
        auto const name{context.text(members.name[member_row])};
        if (members.kind[member_row] == SoaMemberKind::nested) {
            auto const nested{nested_view(context, view, member)};
            std::vector<std::string> arguments;
            for (auto const column : view_columns(context, nested)) {
                arguments.push_back(pointer_name(column));
            }
            arguments.push_back("count_");
            children.new_lines(1).add(ir.raw("auto view_" + name +
                                             "() const -> std::conditional_t<Const, " +
                                             resolved_view_name(context, nested, true) + ", " +
                                             resolved_view_name(context, nested, false) +
                                             "> { return {" + join(arguments, ", ") + "}; }"));
        } else {
            auto const path{
                storage.paths.append(views.member_prefix[row], members.name[member_row])};
            auto const column{column_for(context, model, path)};
            children.new_lines(1).add(ir.raw("auto " + name + "() const -> std::span<" +
                                             element_type(column) + "> { return {" +
                                             pointer_name(column) + ", count_}; }"));
        }
    }
    NodeListBuilder iteration{ir};
    for (auto const column : selected) {
        iteration.add(ir.raw("func(std::span<" + element_type(column) + ">{" +
                             pointer_name(column) + ", count_});"));
    }
    auto const each_column{make_function(ir,
                                         "each_column",
                                         "void",
                                         {ir.parameter("Func&&", "func")},
                                         iteration.build(),
                                         FunctionReceiver::const_)};
    ir.set_function_template(
        each_column,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Func"))}));
    children.new_lines(1)
        .add(inline_function(context, each_column))
        .new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .add(ir.friend_declaration(
            ir.strings.intern(implementation),
            RecordKind::struct_,
            ir.template_declaration({ir.value_template_parameter(ir.types.intern("bool"), {})})))
        .new_lines(1);
    for (auto const column : selected) {
        children
            .add(ir.instance_member(ir.types.intern(element_type(column) + "*"),
                                    ir.strings.intern(pointer_name(column)),
                                    ir.init_list({})))
            .new_lines(1);
    }
    children.add(ir.instance_member(
        ir.types.intern("size_type"), ir.strings.intern("count_"), ir.init_list({})));
    auto const record{
        ir.record(ir.strings.intern(implementation), children.build(), RecordKind::struct_)};
    describe_compact_scope(context, record, schema);
    ir.set_record_template(record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("bool"), ir.strings.intern("Const"))}));
    result.add(record).new_lines(1);
    for (bool const is_const : {true, false}) {
        auto const name{is_const ? const_name : mutable_name};
        auto const base{implementation + (is_const ? "<true>" : "<false>")};
        NodeListBuilder wrapper{ir};
        wrapper.add(ir.raw("using Base = " + base + ";\nusing Base::Base;")).new_lines(1);
        auto api{lower_soa_api(context,
                               schema,

                               is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view,
                               name,

                               storage.equivalent_constructors.at({model, schema}))};
        wrapper.append(api.header);
        source.append(api.source).new_lines(1);
        auto const type{ir.record(ir.strings.intern(name), wrapper.build(), RecordKind::struct_)};
        ir.set_record_bases(type, {ir.types.intern(base)});
        result.add(type).new_lines(1);
    }
    return result.build();
}

auto compact_view_node(LoweringContext& context, SingleAllocationViewId view) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto& view_store{context.ir.lowering.single_allocation_views};
    auto const view_store_index{view_store.index(view)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    auto const model{view_store.model[view_store_index]};
    auto const name{context.text(view_store.view_name[view_store_index]) + "Impl"};
    auto const conversion_constructor{ir.constructor(ir.strings.intern(name))};
    ir.set_function_parameters(conversion_constructor,
                               {ir.parameter(name + "<false> const&", "other")});
    ir.set_function_body(
        conversion_constructor,
        {},
        {ir.member_initializer(ir.strings.intern("Base"), ir.store_text("other"))});
    ir.set_function_template(
        conversion_constructor,
        ir.template_declaration({ir.value_template_parameter(
            ir.types.intern("bool"), ir.strings.intern("Enabled"), ir.named("Const"))}));
    ir.set_function_requires_clause(conversion_constructor, ir.named("Enabled"));
    NodeListBuilder children{ir};
    children.append(adjacent(
        context,
        {ir.using_declaration(
             ir.strings.intern("soa_schema"),
             ir.types.intern(
                 context.text(context.declarations.soas.name[context.declarations.soas.index(
                     view_store.schema[view_store_index])]) +
                 "Schema")),
         ir.using_declaration(ir.strings.intern("Base"),
                              ir.types.intern("ioj::soa_storage_detail::CompactViewBase<Const>")),
         ir.raw("using Base::Base;"),
         ir.using_declaration(
             ir.strings.intern("Layout"),
             ir.types.intern(context.text(model_store.layout_name[model_store.index(model)]))),
         ir.using_declaration(
             ir.strings.intern("View"),
             ir.types.intern(context.text(view_store.view_name[view_store_index]))),
         ir.using_declaration(
             ir.strings.intern("ConstView"),
             ir.types.intern(context.text(view_store.const_view_name[view_store_index]))),
         inline_function(context, conversion_constructor)}));
    auto accessor = [&](std::string_view function_name,
                        std::initializer_list<NodeId> body,
                        CppTypeId return_type = {}) {
        auto const function{
            make_function(ir, function_name, "auto", {}, body, FunctionReceiver::const_)};
        if (return_type.value != 0) {
            ir.set_function_return_type(function, return_type, ReturnSyntax::trailing);
        }
        children.new_lines(1).add(compact_function(context, function));
    };
    for (auto const member_id : context.declarations.items(
             context.declarations.soas
                 .members[context.declarations.soas.index(view_store.schema[view_store_index])])) {
        auto const member{member_id};
        auto const member_name{context.text(
            context.declarations.soa_members.name[context.declarations.soa_members.index(member)])};
        auto const path{ir.lowering.paths.append(
            view_store.member_prefix[view_store_index],
            context.declarations.soa_members.name[context.declarations.soa_members.index(member)])};
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
            SoaMemberKind::nested) {
            auto const nested{nested_view(context, view, member)};
            accessor("view_" + member_name,
                     {ir.return_stmt(ir.init_list({ir.named("this->state_"),
                                                   ir.named("this->offset_"),
                                                   ir.named("this->count_")}))},
                     ir.types.intern(
                         "std::conditional_t<Const, " +
                         context.text(view_store.const_view_name[view_store.index(nested)]) + ", " +
                         context.text(view_store.view_name[view_store.index(nested)]) + ">"));
        } else {
            auto const column{column_for(context, model, path)};
            accessor(
                member_name,
                {ir.return_stmt(ir.call(
                    ir.named("this->column",
                             ir.types.dependencies(column_store.type[column_store.index(column)])),
                    {layout_column(context, column)}))});
        }
    }
    NodeListBuilder body{ir};
    std::map<LoweringPathId, StringId> groups;
    for (auto const column :
         context.ir.lowering.items(model_store.columns[model_store.index(model)])) {
        auto const prefix{ir.lowering.paths.items(view_store.member_prefix[view_store_index])};
        auto const path{
            ir.lowering.paths.items(column_store.member_path[column_store.index(column)])};
        if (path.size() < prefix.size() ||
            !std::equal(prefix.begin(), prefix.end(), path.begin())) {
            continue;
        }
        auto const accessor{column_access(context,
                                          column_store.member_path[column_store.index(column)],
                                          "",
                                          groups,
                                          body,
                                          prefix.size())};
        body.add(ir.expr_stmt(ir.call(ir.named("func"), {accessor})));
    }
    auto const iteration{make_function(ir,
                                       "each_column",
                                       "void",
                                       {ir.parameter("Func&&", "func")},
                                       body.build(),
                                       FunctionReceiver::const_)};
    ir.set_function_template(
        iteration,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Func"))}));
    children.new_lines(1).add(compact_function(context, iteration));
    {
        std::vector<Expr> pointers;
        for (auto const column : view_columns(context, view)) {
            pointers.push_back(ir.call(ir.member_access(
                ir.call(ir.named("this->column"), {layout_column(context, column)}), "data")));
        }
        pointers.push_back(ir.named("this->count_"));
        auto const resolved{"std::conditional_t<Const, " + resolved_view_name(context, view, true) +
                            ", " + resolved_view_name(context, view, false) + ">"};
        accessor("resolve", {ir.return_stmt(ir.init_list(pointers))}, ir.types.intern(resolved));
    }
    auto const created_record{
        ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
    describe_compact_scope(context, created_record, view_store.schema[view_store_index]);
    ir.set_record_bases(created_record,
                        {ir.types.intern("ioj::soa_storage_detail::CompactViewBase<Const>")});
    ir.set_record_template(created_record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("bool"), ir.strings.intern("Const"))}));
    return created_record;
}

auto view_validation_nodes(LoweringContext& context, std::string const& name) -> Nodes {
    auto& ir{context.ir};
    return {ir.static_assertion(
        ir.store_text("ioj::soa_storage_detail::validate_compact_view<" + name + ">()"), {})};
}

} // namespace

auto emit_single_allocation_column_group(LoweringContext& context, DeclarationHandle schema)
    -> NodeId {
    auto& ir{context.ir};
    auto const scoped{context.for_declaration(schema)};
    auto const& soas{context.declarations.soas};
    auto const& members{context.declarations.soa_members};
    auto const row{soas.index(schema)};
    auto const name{context.text(soas.name[row]) + "LayoutColumns"};
    NodeListBuilder children{ir};
    children.add(ir.using_declaration(
        ir.strings.intern("ColLayout"),
        ir.types.intern("ioj::single_allocation_layout::ColumnLayout<T>"),
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})));
    std::vector<MemberInitializerId> initializers;
    std::set<std::string> names{"ColLayout", name};
    std::string previous{"previous"};
    for (auto const member : context.declarations.items(soas.members[row])) {
        auto const member_row{members.index(member)};
        auto const nested{members.kind[member_row] == SoaMemberKind::nested};
        auto const member_name{title_case_identifier(context.text(members.name[member_row])) +
                               (nested ? "" : "Column")};
        if (!names.insert(member_name).second) {
            throw std::invalid_argument{"Single-allocation column name collision: " + member_name};
        }
        auto const type{scoped.resolve(members.type[member_row])};
        auto const member_type{
            nested ? ir.types.intern(schema_name(context, scoped.nested_declaration(member)) +
                                         "LayoutColumns",
                                     ir.types.dependencies(type))
                   : ir.types.template_type(ir.types.intern("ColLayout"), {type})};
        children.new_lines(1).add(ir.instance_member(member_type, ir.strings.intern(member_name)));
        initializers.push_back(
            ir.member_initializer(ir.strings.intern(member_name), ir.store_text(previous)));
        previous = member_name + (nested ? ".last_column()" : "");
    }
    auto const constructor{ir.constructor(ir.strings.intern(name), true)};
    ir.set_function_parameters(
        constructor,
        {ir.parameter("ioj::single_allocation_layout::ColumnLayoutBase const&", "previous")});
    ir.set_function_noexcept(constructor);
    ir.function_flags(constructor).set_is_constexpr(true);
    ir.set_function_body(constructor, {}, initializers);
    auto const last{make_function(ir,
                                  "last_column",
                                  "ioj::single_allocation_layout::ColumnLayoutBase const&",
                                  {},
                                  {ir.return_stmt(ir.named(previous))},
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing)};
    ir.set_function_noexcept(last);
    ir.function_flags(last).set_is_constexpr(true);
    auto const copy{ir.constructor(ir.strings.intern(name))};
    ir.set_function_parameters(copy, {ir.parameter(name + " const&")});
    ir.set_function_deleted(copy);
    auto const assignment{ir.function("operator=", name + "&")};
    ir.set_function_parameters(assignment, {ir.parameter(name + " const&")});
    ir.set_function_return_type(
        assignment, ir.function_return_type(assignment), ReturnSyntax::trailing);
    ir.set_function_deleted(assignment);
    children.new_lines(2).append(adjacent(context,
                                          {inline_function(context, constructor),
                                           compact_function(context, last),
                                           ir.declaration(copy),
                                           ir.declaration(assignment)}));
    auto const result{ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
    ir.set_record_dependencies(
        result,
        {ir.types.dependency("single_allocation_layout", "ioj/single_allocation/layout.h")});
    return result;
}

auto emit_single_allocation_layout(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes {
    auto& ir{context.ir};
    auto const& models{ir.lowering.single_allocation_models};
    auto const row{models.index(model)};
    auto const layout{ir.record(models.layout_name[row], {}, RecordKind::struct_)};
    ir.set_record_bases(layout,
                        {ir.types.intern("ioj::single_allocation_layout::Layout<" +
                                         context.text(models.layout_name[row]) + "Columns>")});
    return adjacent(context,
                    {ir.forward_declaration(models.view_name[row]),
                     ir.forward_declaration(models.const_view_name[row]),
                     emit_single_allocation_column_group(context, models.schema[row]),
                     layout});
}

auto storage_implementation_nodes(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& ir{context.ir};
    auto copying{column_copying_nodes(context, model)};
    NodeListBuilder children{ir};
    children
        .append(adjacent(
            context,
            {ir.using_declaration(
                 ir.strings.intern("View"),
                 ir.types.intern(context.text(model_store.view_name[model_store_index]))),
             ir.using_declaration(
                 ir.strings.intern("ConstView"),
                 ir.types.intern(context.text(model_store.const_view_name[model_store_index])))}))
        .new_lines(2)
        .append(storage_lifetime_nodes(context, model))
        .new_lines(1)
        .add(ir.access(Access::protected_))
        .new_lines(1)
        .append(storage_access_nodes(context, model))
        .new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .append(column_pointer_nodes(context, model))
        .new_lines(2)
        .append(section_header(context, "Typed mutations and growth"))
        .new_lines(1)
        .add(default_construction_node(context, model))
        .new_lines(1)
        .append(std::move(copying))
        .new_lines(1)
        .add(source_pointers_node(context, model))
        .new_lines(1)
        .add(source_copy_node(context, model, false))
        .new_lines(1)
        .add(source_copy_node(context, model, true))
        .new_lines(1)
        .add(live_column_copy_node(context));

    return children.build();
}

namespace {
auto emit_views(LoweringContext& context, SingleAllocationViewId view, NodeListBuilder& source)
    -> Nodes {
    auto& view_store{context.ir.lowering.single_allocation_views};
    auto const view_store_index{view_store.index(view)};
    auto& ir{context.ir};
    auto const model{view_store.model[view_store_index]};
    auto const implementation{context.text(view_store.view_name[view_store_index]) + "Impl"};
    auto wrapper = [&](bool const is_const) -> NodeId {
        auto const& name{is_const ? context.text(view_store.const_view_name[view_store_index])
                                  : context.text(view_store.view_name[view_store_index])};
        auto const base{implementation + (is_const ? "<true>" : "<false>")};
        NodeListBuilder children{ir};
        children.append(
            adjacent(context,
                     {ir.using_declaration(ir.strings.intern("Base"), ir.types.intern(base)),
                      ir.raw("using Base::Base;")}));
        auto api{lower_soa_api(
            context,
            view_store.schema[view_store_index],

            is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view,
            name,

            ir.lowering.equivalent_constructors.at({model, view_store.schema[view_store_index]}))};
        children.new_lines(1).append(std::move(api.header));
        source.append(std::move(api.source)).new_lines(2);
        auto const created_record{
            ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
        ir.set_record_bases(created_record, {ir.types.intern(base)});
        ir.set_record_export_specifier(
            created_record,
            context.declarations.soas.export_specifier[context.declarations.soas.index(
                view_store.schema[view_store_index])]);
        return created_record;
    };
    NodeListBuilder result{ir};
    for (auto const member_id : context.declarations.items(
             context.declarations.soas
                 .members[context.declarations.soas.index(view_store.schema[view_store_index])])) {
        auto const member{member_id};
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
            SoaMemberKind::nested) {
            auto const nested{nested_view(context, view, member)};
            result.add(ir.forward_declaration(view_store.view_name[view_store.index(nested)]))
                .new_lines(1)
                .add(ir.forward_declaration(view_store.const_view_name[view_store.index(nested)]))
                .new_lines(1)
                .append(emit_views(context, nested, source))
                .new_lines(1);
        }
    }
    result.add(compact_view_node(context, view))
        .new_lines(1)
        .add(wrapper(true))
        .new_lines(1)
        .append(view_validation_nodes(context,
                                      context.text(view_store.const_view_name[view_store_index])))
        .new_lines(1)
        .add(wrapper(false))
        .new_lines(1)
        .append(
            view_validation_nodes(context, context.text(view_store.view_name[view_store_index])));
    return result.build();
}
} // namespace

auto emit_single_allocation_views(LoweringContext& context,
                                  SingleAllocationModelId model,
                                  NodeListBuilder& source) -> Nodes {
    auto& storage{context.ir.lowering};
    auto& views{storage.single_allocation_views};
    auto const& models{storage.single_allocation_models};
    auto const model_row{models.index(model)};
    auto const view{views.append()};
    auto const row{views.index(view)};
    views.model[row] = model;
    views.schema[row] = models.schema[model_row];
    views.view_name[row] = models.view_name[model_row];
    views.const_view_name[row] = models.const_view_name[model_row];
    NodeListBuilder result{context.ir};
    result.append(emit_resolved_views(context, view, source)).new_lines(1);
    result.append(emit_views(context, view, source));
    return result.build();
}

auto emit_single_allocation_container(LoweringContext& context,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& ir{context.ir};
    NodeListBuilder children{ir};
    auto const operations{std::string{"ioj::native_soa::StorageOperations"}};
    children
        .add(ir.using_declaration(
            ir.strings.intern("soa_schema"),
            ir.types.intern(
                context.text(context.declarations.soas.name[context.declarations.soas.index(
                    model_store.schema[model_store_index])]) +
                "Schema")))
        .new_lines(1);
    children.add(ir.using_declaration(ir.strings.intern("Operations"), ir.types.intern(operations)))
        .new_lines(1);
    children
        .add(ir.raw("using Layout = " + context.text(model_store.layout_name[model_store_index]) +
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

    auto api{lower_soa_api(
        context,
        model_store.schema[model_store_index],

        SoaReceiver::owner,
        context.text(model_store.owner_name[model_store_index]),

        ir.lowering.equivalent_constructors.at({model, model_store.schema[model_store_index]}))};
    children.new_lines(1).append(std::move(api.header));
    children.new_lines(1).add(permutation_node(context, model));
    source.append(std::move(api.source)).new_lines(2);
    children.new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .add(ir.instance_member(ir.types.intern("std::pmr::memory_resource*"),
                                ir.strings.intern("resource_"),
                                ir.init_list({})));
    auto const created_record{ir.record(
        model_store.owner_name[model_store_index], children.build(), RecordKind::struct_)};
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
