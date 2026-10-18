#include "single_allocation_soa_emission.h"

#include "lowering_context.h"
#include "single_allocation_soa_model.h"
#include "soa_api.h"
#include "soa_receiver.h"
#include "soa_representation.h"

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
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto adjacent(LoweringContext& context, Nodes nodes) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    auto const count{nodes.size()};
    for (std::size_t index{}; index < count; ++index) {
        result.add(std::move(nodes[index]));
        if (index + 1 < count) {
            result.new_lines(1);
        }
    }
    return result.build();
}

auto section_header(LoweringContext& context, std::string title) -> Nodes {
    auto& ir{context.ir};
    return adjacent(context,
                    {ir.block_comment("****************************************"),
                     ir.line_comment(std::move(title)),
                     ir.block_comment("****************************************")});
}

auto inline_function(LoweringContext& context, FunctionId spec) -> NodeId {
    auto& ir{context.ir};
    context.ir.function_flags(spec).set_is_inline(true);
    context.ir.function_flags(spec).set_define_in_header(true);
    return ir.header_function(std::move(spec));
}

auto compact_function(LoweringContext& context, FunctionId spec) -> NodeId {
    context.ir.function_formatting(spec).body_layout = FunctionBodyLayout::compact;
    return inline_function(context, std::move(spec));
}

auto column_access(LoweringContext& context,
                   LoweringPathId path,
                   std::string receiver,
                   std::map<LoweringPathId, IdentifierId>& groups,
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
                               ir.store_text("std::pmr::get_default_resource()"))});
    auto const resource_constructor{
        ir.constructor(model_store.owner_name[model_store_index], true)};
    ir.set_function_parameters(resource_constructor,
                               {ir.parameter("std::pmr::memory_resource*", "resource")});
    ir.set_function_noexcept(resource_constructor);
    ir.set_function_body(
        resource_constructor,
        {ir.raw("assert(resource != nullptr);")},
        {ir.member_initializer(ir.strings.intern("resource_"), ir.store_text("resource"))});
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
        {ir.raw("Operations::take_storage(*this, other);")},
        {ir.member_initializer(ir.strings.intern("resource_"), ir.store_text("other.resource_"))});
    auto const move_assignment{ir.function("operator=", "auto")};
    ir.set_function_parameters(move_assignment, {move_parameter});
    ir.set_function_body(move_assignment,
                         {ir.raw("return Operations::move_assign(*this, other);")});
    ir.set_function_return_type(
        move_assignment,
        ir.types.intern(context.text(model_store.owner_name[model_store_index]) + "&"),
        ReturnSyntax::trailing);

    NodeListBuilder result{ir};
    result.append(section_header(context, "Lifetime"))
        .new_lines(1)
        .append(adjacent(context,
                         {inline_function(context, std::move(default_constructor)),
                          inline_function(context, std::move(resource_constructor)),
                          compact_function(context, std::move(resource_accessor)),
                          compact_function(context, std::move(destructor)),
                          ir.declaration(std::move(copy_constructor)),
                          ir.declaration(std::move(copy_assignment)),
                          inline_function(context, std::move(move_constructor)),
                          inline_function(context, std::move(move_assignment))}));
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
            ir.types.intern(
                "Element<" +
                    std::string{ir.types.spelling(column_store.type[column_store.index(column)])} +
                    ">*",
                ir.types.dependencies(column_store.type[column_store.index(column)])),
            column_store.flattened_identifier[column_store.index(column)],
            ir.raw_expr("")));
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
    children.new_lines(1).add(inline_function(context, [&] {
        auto const created_function{ir.function("operator+", "auto")};
        ir.set_function_parameters(created_function, {ir.parameter("size_type const", "offset")});
        ir.set_function_body(
            created_function,
            {ir.if_stmt(
                 ir.binary(
                     BinaryOperator::equal,
                     ir.named(context.text(column_store.flattened_identifier[column_store.index(
                         context.ir.lowering.items(model_store.columns[model_store_index])
                             .front())])),
                     ir.literal("nullptr")),
                 ir.block({ir.return_stmt(ir.init_list({}))})),
             ir.return_stmt(ir.init_list(std::move(shifted_columns)))});
        ir.set_function_return_type(
            created_function, ir.types.intern("DataPointers"), ReturnSyntax::trailing);
        ir.set_function_receiver(created_function, FunctionReceiver::const_);
        ir.set_function_noexcept(created_function);
        return created_function;
    }()));
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
                     inline_function(context, std::move(get_data)),
                     inline_function(context, std::move(get_data_offset))});
}

auto column_pointer_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder make_data_body{ir};
    make_data_body.add(ir.raw(context.text(model_store.runtime_namespace[model_store_index]) +
                              "LayoutCursor cursor{blocks};"));
    std::vector<Expr> pointers;
    pointers.reserve(context.ir.lowering.items(model_store.columns[model_store_index]).size());
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        pointers.push_back(ir.call(
            ir.member_access(ir.named("cursor"), "column_pointer"),
            {ir.named("data"),
             ir.named("Layout::columns." +
                      context.text(column_store.layout_identifier[column_store.index(column)]))}));
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
    result.add(ir.raw("friend Operations;\nfriend ::ioj::soa_storage_detail::StorageRequirements;"))
        .new_lines(1)
        .append(section_header(context, "Column pointers"))
        .new_lines(1)
        .append(adjacent(context,
                         {inline_function(context, std::move(make_data)),
                          compact_function(context, std::move(capacity_blocks))}));
    return result.build();
}

auto default_construction_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder body{ir};
    body.add(ir.raw("using " + context.text(model_store.runtime_namespace[model_store_index]) +
                    "default_construct_n;"));
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
    return inline_function(context, [&] {
        auto const created_function{ir.function("default_construct_columns", "void")};
        ir.set_function_parameters(
            created_function,
            {ir.parameter("size_type const", "first"), ir.parameter("size_type const", "count")});
        ir.set_function_body(created_function, body.build());
        return created_function;
    }());
}

auto column_copying_nodes(LoweringContext& context, SingleAllocationModelId model) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    auto swap_remove{compact_function(context, [&] {
        auto const created_function{ir.function("swap_remove_columns", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("size_type const", "index"),
                                    ir.parameter("size_type const", "source"),
                                    ir.parameter("size_type const", "move_count")});
        ir.set_function_body(created_function,
                             {ir.expr_stmt(ir.call(ir.named("copy_columns"),
                                                   {ir.call(ir.named("get_data")),
                                                    ir.named("index"),
                                                    ir.named("source"),
                                                    ir.named("move_count")}))});
        return created_function;
    }())};

    NodeListBuilder copy_body{ir};
    copy_body.add(ir.raw("auto transfer = [count](auto* dst, auto const* src) {\n    " +
                         context.text(model_store.runtime_namespace[model_store_index]) +
                         "transfer_n<Overlapping>(dst, src, count);\n};"));
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
    auto transfer{inline_function(context, [&] {
        auto const created_function{ir.function("transfer_columns", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("DataPointers<std::byte> const&", "destination"),
                                    ir.parameter("DataPointers<Byte> const&", "source"),
                                    ir.parameter("size_type", "count")});
        ir.set_function_body(created_function, copy_body.build());
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.value_template_parameter(ir.types.intern("bool"),
                                                                 ir.strings.intern("Overlapping")),
                                     ir.type_template_parameter(ir.strings.intern("Byte"))}));
        return created_function;
    }())};
    auto copy{compact_function(context, [&] {
        auto const created_function{ir.function("copy_columns", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("DataPointers<std::byte> const&", "columns"),
                                    ir.parameter("size_type", "index"),
                                    ir.parameter("size_type", "source"),
                                    ir.parameter("size_type", "move_count")});
        ir.set_function_body(
            created_function,
            {ir.raw("transfer_columns<false>(columns + index, columns + source, move_count);")});
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        return created_function;
    }())};

    auto remove_indices{inline_function(context, [&] {
        auto const created_function{ir.function("swap_remove_indices", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("std::span<size_type const>", "indices")});
        ir.set_function_body(created_function,
                             {ir.variable("auto const", "columns", ir.call(ir.named("get_data"))),
                              ir.expr_stmt(ir.raw_expr(
                                  "ioj::soa_storage_detail::for_each_removal_run(num_, indices, "
                                  "[&](size_type index, size_type source, size_type count) { "
                                  "copy_columns(columns, index, source, count); })"))});
        return created_function;
    }())};
    return adjacent(
        context,
        {std::move(transfer), std::move(swap_remove), std::move(copy), std::move(remove_indices)});
}

auto source_pointers_node(LoweringContext& context, SingleAllocationModelId model) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    NodeListBuilder body{ir};
    body.add(ir.raw("using " + context.text(model_store.runtime_namespace[model_store_index]) +
                    "source_data;"));
    std::map<LoweringPathId, IdentifierId> groups;
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
    return inline_function(context, [&] {
        auto const created_function{ir.function("source_pointers", "auto")};
        ir.set_function_parameters(created_function, {ir.parameter("Columns const&", "source")});
        ir.set_function_body(created_function, body.build());
        ir.set_function_return_type(created_function,
                                    ir.types.intern("DataPointers<std::byte const>"),
                                    ReturnSyntax::trailing);
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Columns"))}));
        ir.set_function_requires_clause(
            created_function,
            ir.template_id(ir.named("ioj::soa_storage_detail::SoaSourceFor"),
                           {ir.type_reference(ir.types.intern("Columns")),
                            ir.type_reference(ir.types.intern(
                                context.text(model_store.owner_name[model_store_index]))),
                            ir.type_reference(ir.types.intern("size_type"))}));
        return created_function;
    }());
}

auto source_copy_node(LoweringContext& context,
                      SingleAllocationModelId model,
                      bool const overlapping) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto& ir{context.ir};
    return inline_function(context, [&] {
        auto const created_function{
            ir.function(overlapping ? "copy_columns_from" : "append_columns", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("Columns const&", "source"),
                                    ir.parameter("size_type", "source_first"),
                                    ir.parameter("size_type", "first"),
                                    ir.parameter("size_type", "count")});
        ir.set_function_body(
            created_function,
            {ir.raw(std::string{"transfer_columns<"} + (overlapping ? "true" : "false") +
                    ">(get_data(first), source_pointers(source) + source_first, count);")});
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Columns"))}));
        ir.set_function_requires_clause(
            created_function,
            ir.template_id(ir.named("ioj::soa_storage_detail::SoaSourceFor"),
                           {ir.type_reference(ir.types.intern("Columns")),
                            ir.type_reference(ir.types.intern(
                                context.text(model_store.owner_name[model_store.index(model)]))),
                            ir.type_reference(ir.types.intern("size_type"))}));
        return created_function;
    }());
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
    copy_live.add(ir.raw("transfer_columns<false>(destination, source, num_);"));

    return inline_function(context, [&] {
        auto const created_function{ir.function("copy_live_columns", "void")};
        ir.set_function_parameters(created_function,
                                   {ir.parameter("std::byte* const", "new_data"),
                                    ir.parameter("size_type const", "new_capacity")});
        ir.set_function_body(created_function, copy_live.build());
        ir.set_function_noexcept(created_function);
        return created_function;
    }());
}

auto nested_view(LoweringContext& context,
                 SingleAllocationViewId view,
                 SoaMemberId member,
                 std::map<IdentifierId, DeclarationHandle> const& schemas)
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
    views.schema[row] = schemas.at(*members.nested_schema[member_row]);
    views.member_prefix[row] = path;
    views.view_name[row] =
        ir.strings.intern(context.text(views.view_name[parent_row]) + "_" + context.text(name));
    views.const_view_name[row] = ir.strings.intern(context.text(views.const_view_name[parent_row]) +
                                                   "_" + context.text(name));
    ir.lowering.view_lookup.emplace(key, result);
    return result;
}

auto compact_view_node(LoweringContext& context,
                       SingleAllocationViewId view,
                       std::map<IdentifierId, DeclarationHandle> const& schemas) -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto& view_store{context.ir.lowering.single_allocation_views};
    auto const view_store_index{view_store.index(view)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& compact_store{context.ir.lowering.compact_vectors};
    auto& ir{context.ir};
    auto const model{view_store.model[view_store_index]};
    auto const name{context.text(view_store.view_name[view_store_index]) + "Impl"};
    NodeListBuilder children{ir};
    children.append(adjacent(
        context,
        {ir.using_declaration(
             ir.strings.intern("soa_schema"),
             ir.types.intern(
                 context.text(context.declarations.soas.name[context.declarations.soas.index(
                     view_store.schema[view_store_index])]) +
                 "Schema")),
         ir.using_declaration(ir.strings.intern("size_type"),
                              model_store.size_type[model_store.index(model)]),
         ir.using_declaration(
             ir.strings.intern("Layout"),
             ir.types.intern(context.text(model_store.layout_name[model_store.index(model)]))),
         ir.using_declaration(
             ir.strings.intern("Storage"),
             ir.types.intern(context.text(model_store.runtime_namespace[model_store.index(model)]) +
                             "StorageState")),
         ir.using_declaration(ir.strings.intern("State"),
                              ir.types.intern("std::conditional_t<Const, Storage const, Storage>")),
         ir.using_declaration(
             ir.strings.intern("Element"),
             ir.types.intern("std::conditional_t<Const, T const, T>"),
             ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})),
         ir.using_declaration(
             ir.strings.intern("View"),
             ir.types.intern(context.text(view_store.view_name[view_store_index]))),
         ir.using_declaration(
             ir.strings.intern("ConstView"),
             ir.types.intern(context.text(view_store.const_view_name[view_store_index]))),
         ir.declaration([&] {
             auto const created_function{ir.constructor(ir.strings.intern(name))};
             ir.set_function_defaulted(created_function);
             return created_function;
         }()),
         inline_function(context,
                         [&] {
                             auto const created_function{ir.constructor(ir.strings.intern(name))};
                             ir.set_function_parameters(created_function,
                                                        {ir.parameter("State*", "state"),
                                                         ir.parameter("size_type", "offset"),
                                                         ir.parameter("size_type", "count")});
                             ir.set_function_body(
                                 created_function,
                                 {},
                                 {ir.member_initializer(ir.strings.intern("state_"),
                                                        ir.store_text("state")),
                                  ir.member_initializer(ir.strings.intern("offset_"),
                                                        ir.store_text("offset")),
                                  ir.member_initializer(ir.strings.intern("count_"),
                                                        ir.store_text("count"))});
                             return created_function;
                         }()),
         inline_function(context, [&] {
             auto const created_function{ir.constructor(ir.strings.intern(name))};
             ir.set_function_parameters(created_function,
                                        {ir.parameter(name + "<false> const&", "other")});
             ir.set_function_template(
                 created_function,
                 ir.template_declaration({ir.value_template_parameter(
                     ir.types.intern("bool"), ir.strings.intern("Enabled"), ir.named("Const"))}));
             ir.set_function_requires_clause(created_function, ir.named("Enabled"));
             ir.set_function_body(
                 created_function,
                 {},
                 {ir.member_initializer(ir.strings.intern("state_"), ir.store_text("other.state_")),
                  ir.member_initializer(ir.strings.intern("offset_"),
                                        ir.store_text("other.offset_")),
                  ir.member_initializer(ir.strings.intern("count_"),
                                        ir.store_text("other.count_"))});
             return created_function;
         }())}));
    for (auto const member_id : context.declarations.items(
             context.declarations.soas
                 .members[context.declarations.soas.index(view_store.schema[view_store_index])])) {
        auto const member{member_id};
        auto const path{ir.lowering.paths.append(
            view_store.member_prefix[view_store_index],
            context.declarations.soa_members.name[context.declarations.soa_members.index(member)])};
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
            SoaMemberKind::nested) {
            auto const vector{compact_vector_for(context, model, path)};
            if (vector.value == 0) {
                auto const nested{nested_view(context, view, member, schemas)};
                children.new_lines(1).add(compact_function(context, [&] {
                    auto const created_function{ir.function(
                        "view_" +
                            context.text(context.declarations.soa_members
                                             .name[context.declarations.soa_members.index(member)]),
                        "auto")};
                    ir.set_function_body(
                        created_function,
                        {ir.return_stmt(ir.init_list(
                            {ir.named("state_"), ir.named("offset_"), ir.named("count_")}))});
                    ir.set_function_return_type(
                        created_function,
                        ir.types.intern(
                            "std::conditional_t<Const, " +
                            context.text(view_store.const_view_name[view_store.index(nested)]) +
                            ", " + context.text(view_store.view_name[view_store.index(nested)]) +
                            ">"),
                        ReturnSyntax::trailing);
                    ir.set_function_receiver(created_function, FunctionReceiver::const_);
                    return created_function;
                }()));
                continue;
            }

            auto const rotation_view{compact_store.kind[compact_store.index(vector)] ==
                                     CompactVectorKind::rotator};
            auto const pointer_view{
                rotation_view ||
                (model_store.backend[model_store.index(model)] == SoaBackend::standard_library &&
                 compact_store.kind[compact_store.index(vector)] == CompactVectorKind::vector3 &&
                 std::string{ir.types.spelling(
                     compact_store.element_type[compact_store.index(vector)])} == "float")};
            auto const prefix{context.text(model_store.vector_namespace[model_store.index(model)]) +
                              std::string{compact_vector_prefix(
                                  compact_store.kind[compact_store.index(vector)])}};
            auto const element{
                std::string{
                    ir.types.spelling(compact_store.element_type[compact_store.index(vector)])} +
                (compact_store.equivalent_type[compact_store.index(vector)].value == 0
                     ? ""
                     : ", " + std::string{ir.types.spelling(
                                  compact_store.equivalent_type[compact_store.index(vector)])})};
            auto view_type{ir.types.intern("std::conditional_t<Const, " + prefix + "ConstView<" +
                                           element + ">, " + prefix + "View<" + element + ">>")};
            if (pointer_view && !rotation_view) {
                view_type = ir.types.intern(
                    "ioj::Vector3SoAView<Element<float>>",
                    {ir.types.dependency("vector_soa_view", "ioj/vector_soa_view.h")});
            }
            std::vector<Expr> arguments{
                ir.named("state_"), ir.named("offset_"), ir.named("count_")};
            auto const columns{
                pointer_view
                    ? ir.lowering.items(compact_store.components[compact_store.index(vector)])
                          .size()
                    : 2};
            for (std::size_t index{}; index < columns; ++index) {
                auto const component_path{ir.lowering.paths.append(
                    path,
                    ir.lowering.items(
                        compact_store.components[compact_store.index(vector)])[index])};
                arguments.push_back(
                    ir.named("Layout::columns." +
                             context.text(column_store.layout_identifier[column_store.index(
                                 column_for(context, model, component_path))])));
            }
            children.new_lines(1).add(compact_function(context, [&] {
                auto const created_function{ir.function(
                    "view_" +
                        context.text(context.declarations.soa_members
                                         .name[context.declarations.soa_members.index(member)]),
                    "auto")};
                ir.set_function_body(
                    created_function,
                    {ir.raw(std::string{"using ioj::soa_storage_detail::"} +
                            (pointer_view ? "three_column_view;" : "strided_vector_view;")),
                     ir.return_stmt(
                         ir.call(ir.named(std::string{pointer_view ? "three_column_view<"
                                                                   : "strided_vector_view<"} +
                                              std::string{ir.types.spelling(view_type)} + ">",
                                          ir.types.dependencies(view_type)),
                                 std::move(arguments)))});
                ir.set_function_receiver(created_function, FunctionReceiver::const_);
                return created_function;
            }()));
        } else {
            auto const column{column_for(context, model, path)};
            children.new_lines(1).add(compact_function(context, [&] {
                auto const created_function{
                    ir.function(context.declarations.soa_members
                                    .name[context.declarations.soa_members.index(member)],
                                ir.types.intern("auto"))};
                ir.set_function_body(
                    created_function,
                    {ir.return_stmt(ir.call(
                        ir.named(
                            "make_column_view",
                            ir.types.dependencies(column_store.type[column_store.index(column)])),
                        {ir.named(
                            "Layout::columns." +
                            context.text(
                                column_store.layout_identifier[column_store.index(column)]))}))});
                ir.set_function_receiver(created_function, FunctionReceiver::const_);
                return created_function;
            }()));
        }
    }
    NodeListBuilder body{ir};
    std::map<LoweringPathId, IdentifierId> groups;
    std::vector<Expr> arguments;
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
        if (model_store.column_iteration_returns_result[model_store.index(model)]) {
            auto const name{"column_" + std::to_string(arguments.size())};
            body.add(ir.variable("auto", name, accessor));
            arguments.push_back(ir.named(name));
        } else {
            body.add(ir.expr_stmt(ir.call(ir.named("func"), {accessor})));
        }
    }
    if (model_store.column_iteration_returns_result[model_store.index(model)]) {
        body.add(ir.return_stmt(ir.call(
            ir.named("std::forward<Func>(func)", {ir.types.dependency("std::forward", "utility")}),
            std::move(arguments))));
    }
    children.new_lines(1).add(compact_function(context, [&] {
        auto const created_function{ir.function(
            model_store.column_iteration_function[model_store.index(model)],
            ir.types.intern(model_store.column_iteration_returns_result[model_store.index(model)]
                                ? "auto"
                                : "void"))};
        ir.set_function_parameters(created_function, {ir.parameter("Func&&", "func")});
        ir.set_function_body(created_function, body.build());
        if (model_store.column_iteration_returns_result[model_store.index(model)]) {
            ir.set_function_return_type(
                created_function, ir.types.intern("decltype(auto)"), ReturnSyntax::trailing);
        }
        ir.set_function_receiver(created_function, FunctionReceiver::const_);
        ir.set_function_template(
            created_function,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Func"))}));
        return created_function;
    }()));
    children.new_lines(1)
        .add(ir.access(Access::private_))
        .new_lines(1)
        .add(compact_function(
            context,
            [&] {
                auto const created_function{ir.function("make_column_view", "auto")};
                ir.set_function_parameters(
                    created_function,
                    {ir.parameter(
                        context.text(model_store.runtime_namespace[model_store.index(model)]) +
                            "ColumnLayout<T> const&",
                        "layout")});
                ir.set_function_body(
                    created_function,
                    {ir.return_stmt(ir.call(
                        ir.named("ioj::soa_storage_detail::column_view<" +
                                 context.text(model_store.span_template[model_store.index(model)]) +
                                 "<Element<T>>>"),
                        {ir.named("state_"),
                         ir.named("offset_"),
                         ir.named("count_"),
                         ir.named("layout")}))});
                ir.set_function_receiver(created_function, FunctionReceiver::const_);
                ir.set_function_template(
                    created_function,
                    ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))}));
                return created_function;
            }()))
        .new_lines(1)
        .add(ir.friend_type(ir.types.intern("ioj::soa_storage_detail::CompactViewOperations")))
        .new_lines(1)
        .add(ir.friend_declaration(
            ir.strings.intern(name),
            RecordKind::struct_,
            ir.template_declaration({ir.value_template_parameter(ir.types.intern("bool"), {})})))
        .new_lines(1)
        .append(adjacent(
            context,
            {ir.instance_member(
                 ir.types.intern("State*"), ir.strings.intern("state_"), ir.raw_expr("")),
             ir.instance_member(
                 ir.types.intern("size_type"), ir.strings.intern("offset_"), ir.raw_expr("")),
             ir.instance_member(
                 ir.types.intern("size_type"), ir.strings.intern("count_"), ir.raw_expr(""))}));
    auto const created_record{
        ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
    ir.set_record_bases(created_record,
                        {ir.types.intern("ioj::soa_storage_detail::CompactViewOperations")});
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

auto emit_single_allocation_layout(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    auto const columns_name{context.text(model_store.layout_name[model_store_index]) + "Columns"};
    NodeListBuilder columns{ir};
    columns
        .add(ir.using_declaration(
            ir.strings.intern("ColLayout"),
            ir.types.intern(context.text(model_store.runtime_namespace[model_store_index]) +
                            "ColumnLayout<T>"),
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})))
        .new_lines(2)
        .add(ir.instance_member(
            ir.types.intern(context.text(model_store.runtime_namespace[model_store_index]) +
                            "ColumnLayoutStart"),
            ir.strings.intern("LayoutStart"),
            ir.raw_expr("")))
        .new_lines(2);
    std::vector<MemberInitializerId> constructor_initializers;
    auto const constructor{ir.constructor(ir.strings.intern(columns_name))};
    ir.set_function_noexcept(constructor);
    ir.function_flags(constructor).set_is_constexpr(true);
    std::string previous_column{"LayoutStart"};
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        columns
            .add(ir.instance_member(
                ir.types.intern(
                    "ColLayout<" +
                    std::string{ir.types.spelling(column_store.type[column_store.index(column)])} +
                    ">"),
                column_store.layout_identifier[column_store.index(column)]))
            .new_lines(1);
        constructor_initializers.push_back(
            ir.member_initializer(column_store.layout_identifier[column_store.index(column)],
                                  ir.strings.add(previous_column)));
        previous_column = context.text(column_store.layout_identifier[column_store.index(column)]);
    }
    ir.set_function_body(constructor, {}, constructor_initializers);
    columns.new_lines(2)
        .add(inline_function(context, std::move(constructor)))
        .new_lines(1)
        .add(ir.raw(columns_name + "(" + columns_name + " const&) = delete;\nauto operator=(" +
                    columns_name + " const&) -> " + columns_name + "& = delete;"));
    auto const last_column{
        "columns." +
        context.text(column_store.layout_identifier[column_store.index(
            context.ir.lowering.items(model_store.columns[model_store_index]).back())])};
    NodeListBuilder children{ir};
    children
        .append(adjacent(context,
                         {ir.using_declaration(ir.strings.intern("size_type"),
                                               model_store.size_type[model_store_index]),
                          ir.using_declaration(ir.strings.intern("byte_size_type"),
                                               model_store.byte_size_type[model_store_index])}))
        .new_lines(2)
        .append(adjacent(
            context,
            {
                ir.constexpr_member(
                    ir.types.intern("size_type"),
                    ir.strings.intern("capacity_granularity"),
                    ir.named(context.text(model_store.runtime_namespace[model_store_index]) +
                             "LayoutPolicy::capacity_granularity")),
                ir.constexpr_member(
                    ir.types.intern("byte_size_type"),
                    ir.strings.intern("column_gap"),
                    ir.named(context.text(model_store.runtime_namespace[model_store_index]) +
                             "LayoutPolicy::column_gap")),
            }))
        .new_lines(2)
        .add(ir.constexpr_member(
            ir.types.intern(columns_name), ir.strings.intern("columns"), ir.raw_expr("")))
        .new_lines(2);
    children
        .add(ir.constexpr_member(ir.types.intern("byte_size_type"),
                                 ir.strings.intern("allocation_alignment"),
                                 ir.member_access(ir.named(last_column), "allocation_alignment")))
        .new_lines(2)
        .add(ir.line_comment(
            "Conservative per-block bound for checked capacity arithmetic; gaps do not scale "
            "with capacity."))
        .new_lines(1)
        .append(adjacent(
            context,
            {
                ir.constexpr_member(
                    ir.types.intern("byte_size_type"),
                    ir.strings.intern("capacity_block_bound"),
                    ir.call(
                        ir.named(context.text(model_store.runtime_namespace[model_store_index]) +
                                 "capacity_block_bound"),
                        {ir.named(last_column)})),
                ir.constexpr_member(
                    ir.types.intern("size_type"),
                    ir.strings.intern("max_capacity"),
                    ir.call(
                        ir.named(context.text(model_store.runtime_namespace[model_store_index]) +
                                 "maximum_capacity"),
                        {ir.named("capacity_block_bound")})),
                inline_function(
                    context,
                    [&] {
                        auto const created_function{ir.function("layout_bytes", "auto")};
                        ir.set_function_parameters(created_function,
                                                   {ir.parameter("byte_size_type", "blocks")});
                        ir.set_function_body(
                            created_function,
                            {ir.return_stmt(ir.raw_expr("blocks == 0 ? 0 : " + last_column +
                                                        ".data_end(blocks)"))});
                        ir.set_function_return_type(created_function,
                                                    ir.types.intern("byte_size_type"),
                                                    ReturnSyntax::trailing);
                        ir.set_function_noexcept(created_function);
                        ir.set_function_receiver(created_function, FunctionReceiver::static_);
                        ir.function_flags(created_function).set_is_constexpr(true);
                        return created_function;
                    }()),
            }))
        .new_lines(1)
        .add(ir.static_assertion(ir.store_text("max_capacity >= capacity_granularity"), {}));

    return adjacent(
        context,
        {ir.forward_declaration(model_store.view_name[model_store_index]),
         ir.forward_declaration(model_store.const_view_name[model_store_index]),
         ir.record(ir.strings.intern(columns_name), columns.build(), RecordKind::struct_),
         ir.record(
             model_store.layout_name[model_store_index], children.build(), RecordKind::struct_)});
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
        .add(ir.instance_member(ir.types.intern("std::pmr::memory_resource*"),
                                ir.strings.intern("resource_"),
                                ir.raw_expr("")))
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
auto emit_views(LoweringContext& context,
                SingleAllocationViewId view,
                NodeListBuilder& source,
                std::map<IdentifierId, DeclarationHandle> const& schemas) -> Nodes {
    auto& model_store{context.ir.lowering.single_allocation_models};
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
            SoaRepresentation::compact,
            is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view,
            name,
            &schemas,
            model_store.backend[model_store.index(model)],
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
        auto const path{ir.lowering.paths.append(
            view_store.member_prefix[view_store_index],
            context.declarations.soa_members.name[context.declarations.soa_members.index(member)])};
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
                SoaMemberKind::nested &&
            compact_vector_for(context, model, path).value == 0) {
            auto const nested{nested_view(context, view, member, schemas)};
            result.add(ir.forward_declaration(view_store.view_name[view_store.index(nested)]))
                .new_lines(1)
                .add(ir.forward_declaration(view_store.const_view_name[view_store.index(nested)]))
                .new_lines(1)
                .append(emit_views(context, nested, source, schemas))
                .new_lines(1);
        }
    }
    result.add(compact_view_node(context, view, schemas))
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
                                  NodeListBuilder& source,
                                  std::map<IdentifierId, DeclarationHandle> const& schemas)
    -> Nodes {
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
    return emit_views(context, view, source, schemas);
}

auto emit_single_allocation_container(LoweringContext& context,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source,
                                      std::map<IdentifierId, DeclarationHandle> const& schemas)
    -> NodeId {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& ir{context.ir};
    NodeListBuilder children{ir};
    auto const operations{context.text(model_store.runtime_namespace[model_store_index]) +
                          "StorageOperations"};
    children
        .add(ir.using_declaration(
            ir.strings.intern("soa_schema"),
            ir.types.intern(
                context.text(context.declarations.soas.name[context.declarations.soas.index(
                    model_store.schema[model_store_index])]) +
                "Schema")))
        .new_lines(1);
    children.add(ir.raw("using Operations = " + operations + ";")).new_lines(1);
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
        SoaRepresentation::compact,
        SoaReceiver::owner,
        context.text(model_store.owner_name[model_store_index]),
        &schemas,
        model_store.backend[model_store_index],
        ir.lowering.equivalent_constructors.at({model, model_store.schema[model_store_index]}))};
    children.new_lines(1).append(std::move(api.header));
    source.append(std::move(api.source)).new_lines(2);
    auto const created_record{ir.record(
        model_store.owner_name[model_store_index], children.build(), RecordKind::struct_)};
    ir.set_record_bases(
        created_record,
        {ir.types.intern("protected " +
                         context.text(model_store.runtime_namespace[model_store_index]) +
                         "StorageState"),
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
