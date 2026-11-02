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
        std::vector<CppTypeId> types;
        for (auto const column : selected) {
            types.push_back(columns.type[columns.index(column)]);
        }
        auto const base{template_type(
            named_type("ioj::single_allocation_layout::ColumnLayouts",
                       NameOrigin::global,
                       {dependency("single_allocation_layout", "ioj/single_allocation/layout.h")}),
            types)};
        auto children{node_list()};
        for (std::size_t index{}; index < selected.size(); ++index) {
            auto const row{columns.index(selected[index])};
            if (index != 0) {
                children.new_lines(1);
            }
            children.add(constexpr_member(
                std_types().size_t,
                store_text(ctx.text(columns.flattened_identifier[row]) + "_column"),
                literal(std::to_string(index))));
        }
        auto const layout{record(store_text("Layout"), children.build(), RecordKind::struct_)};
        set_record_bases(layout, {base});
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
        set_scope_bindings(layout, inherited);
        auto result{node_list()};
        result.append(section_header("Layout")).new_lines(1).add(layout);
        return result.build();
    }
    auto emit_single_allocation_views(LoweringContext& ctx,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source,
                                      NodeListBuilder& declarations) -> Nodes {
        auto const& models{lowering_storage().single_allocation_models};
        auto const schema{models.schema[models.index(model)]};
        auto const strided{is_strided_schema(ctx, schema)};
        declarations.add(forward_declaration(store_text("View")))
            .new_lines(1)
            .add(forward_declaration(store_text("ConstView")))
            .new_lines(1);
        auto result{node_list()};
        result.append(section_header("Column pointers"))
            .new_lines(1)
            .add(data_pointers_node(model))
            .new_lines(2)
            .add(schema_node(ctx, model))
            .new_lines(2)
            .append(emit_resolved_views(ctx, model, source, declarations))
            .new_lines(2)
            .append(section_header(strided ? "Strided views" : "Compact views"))
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
                template_type(named_type("ViewImpl"), {intern_type(is_const ? "true" : "false")}))};
            auto children{node_list()};
            children.add(using_declaration(store_text("Base"), base))
                .new_lines(1)
                .add(using_import(named_type("Base::Base")))
                .new_lines(1);
            auto api{lower_soa_api(ctx, schema, receiver, name, base)};
            children.append(api.header);
            if (!api.source.empty()) {
                source.append(api.source).new_lines(2);
            }
            auto const record_id{record(store_text(name), children.build(), RecordKind::struct_)};
            set_record_bases(record_id, {base});
            set_record_export_specifier(
                record_id,
                ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(schema)]);
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
        auto const operations{std::string{"ioj::native_soa::StorageOperations"}};
        auto const row_operations{soa_row_operations(ctx,
                                                     schema,
                                                     SoaReceiver::owner,
                                                     ctx.text(owner_name),
                                                     named_type(operations, NameOrigin::global))};
        children
            .add(using_declaration(
                store_text("soa_schema"),
                named_type(schema_namespace(ctx, model_store.schema[model_store_index]) +
                               "::Schema",
                           NameOrigin::global)))
            .new_lines(1);
        children
            .add(using_declaration(store_text("Operations"),
                                   named_type(operations, NameOrigin::global)))
            .new_lines(1);
        children
            .add(using_declaration(store_text("Layout"),
                                   named_type(scope + "::Layout", NameOrigin::global)))
            .new_lines(1);
        for (auto const name : {"size_type", "byte_size_type"}) {
            children
                .add(using_declaration(
                    store_text(name),
                    decorate_type(named_type("Layout"), "", "::" + std::string{name})))
                .new_lines(1);
        }
        children.new_lines(1);
        for (auto const name : {"capacity_granularity",
                                "allocation_alignment",
                                "capacity_block_bound",
                                "max_capacity"}) {
            children
                .add(constexpr_member(intern_type("auto"),
                                      store_text(name),
                                      type_reference(decorate_type(
                                          named_type("Layout"), "", "::" + std::string{name}))))
                .new_lines(1);
        }
        auto const bytes{make_function(
            "layout_bytes",
            "byte_size_type",
            {parameter("byte_size_type", "blocks")},
            {return_stmt(
                call(type_reference(decorate_type(named_type("Layout"), "", "::layout_bytes")),
                     {named("blocks")}))},
            FunctionReceiver::static_,
            ReturnSyntax::trailing)};
        function_flags(bytes).set_is_constexpr(true);
        function_flags(bytes).set_define_in_header(true);
        set_function_noexcept(bytes);
        children.new_lines(1)
            .add(header_function(bytes))
            .new_lines(2)
            .append(storage_implementation_nodes(ctx, model))
            .new_lines(1)
            .add(access(Access::public_))
            .new_lines(1);

        auto api{lower_soa_api(ctx,
                               model_store.schema[model_store_index],
                               SoaReceiver::owner,
                               ctx.text(owner_name),
                               row_operations)};
        children.new_lines(1).append(std::move(api.header));
        source.append(std::move(api.source)).new_lines(2);
        children.new_lines(1)
            .add(access(Access::private_))
            .new_lines(1)
            .add(friend_type(intern_type("Operations")))
            .new_lines(1)
            .add(friend_type(
                named_type("ioj::soa_storage_detail::StorageRequirements", NameOrigin::global)))
            .new_lines(1)
            .add(instance_member(decorate_type(std_types().pmr.memory_resource, "", "*"),
                                 store_text("resource_"),
                                 init_list({})));
        auto const created_record{record(owner_name, children.build(), RecordKind::struct_)};
        describe_compact_scope(ctx, created_record, model_store.schema[model_store_index]);
        set_record_bases(
            created_record,
            {decorate_type(
                 named_type("ioj::native_soa::StorageState", NameOrigin::global), "protected ", ""),
             row_operations});
        set_record_export_specifier(
            created_record,
            ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(
                model_store.schema[model_store_index])]);
        set_record_dependencies(
            created_record, lowering_storage().items(model_store.dependencies[model_store_index]));
        return created_record;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    void
        describe_compact_scope(LoweringContext& ctx, RecordId record_id, DeclarationHandle schema) {
        if (!ctx.declarations.soas.using_declarations[ctx.declarations.soas.index(schema)]
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
                                "view",
                                "const_view",
                                "slice",
                                "left",
                                "right"}) {
            names.push_back(store_text(name));
        }
        set_scope_bindings(record_id, names);
    }
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
    auto inline_function(FunctionId spec) -> NodeId {
        auto& flags{function_flags(spec)};
        flags.set_is_inline(true);
        flags.set_define_in_header(true);
        return header_function(spec);
    }
    auto compact_function(FunctionId spec) -> NodeId {
        function_formatting(spec).body_layout = FunctionBodyLayout::compact;
        return inline_function(spec);
    }
    template <typename Body = std::initializer_list<NodeId>>
    auto make_function(std::string_view name,
                       std::string_view return_type,
                       std::initializer_list<FunctionParameterId> parameters,
                       Body const& body,
                       FunctionReceiver receiver = FunctionReceiver::unqualified,
                       ReturnSyntax syntax = ReturnSyntax::direct) -> FunctionId {
        auto const function_id{function(name, return_type)};
        set_function_parameters(function_id, parameters);
        set_function_body(function_id, body);
        set_function_receiver(function_id, receiver);
        if (syntax == ReturnSyntax::trailing) {
            set_function_return_type(function_id, function_return_type(function_id), syntax);
        }
        return function_id;
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
    auto storage_lifetime_nodes(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
        auto& model_store{lowering_storage().single_allocation_models};
        auto const& soas{ctx.declarations.soas};
        auto const owner_name{soas.name[soas.index(model_store.schema[model_store.index(model)])]};
        auto const owner_type{named_type(ctx.text(owner_name))};
        auto const copy_parameter{parameter(decorate_type(owner_type, "", " const&"))};
        auto const move_parameter{parameter(decorate_type(owner_type, "", "&&"), "other")};
        auto const free_data{
            call(type_reference(decorate_type(named_type("Operations"), "", "::release_storage")),
                 {unary(UnaryOperator::dereference, named("this"))})};
        auto const default_constructor{constructor(owner_name)};
        set_function_noexcept(default_constructor);
        set_function_body(default_constructor,
                          {},
                          {member_initializer(
                              owner_name,
                              unary(UnaryOperator::dereference,
                                    call(type_reference(std_types().pmr.get_default_resource))))});
        auto const resource_constructor{constructor(owner_name, true)};
        set_function_parameters(
            resource_constructor,
            {parameter(decorate_type(std_types().pmr.memory_resource, "", "&"), "resource")});
        set_function_noexcept(resource_constructor);
        set_function_body(
            resource_constructor,
            {},
            {member_initializer(store_text("resource_"),
                                unary(UnaryOperator::address_of, named("resource")))});
        auto const resource_accessor{function("memory_resource", "auto")};
        set_function_body(resource_accessor, {return_stmt(named("resource_"))});
        set_function_return_type(resource_accessor,
                                 decorate_type(std_types().pmr.memory_resource, "", "*"),
                                 ReturnSyntax::trailing);
        set_function_receiver(resource_accessor, FunctionReceiver::const_);
        set_function_noexcept(resource_accessor);
        auto const destructor_value{destructor(owner_name)};
        set_function_body(destructor_value, {expr_stmt(free_data)});
        auto const copy_constructor{constructor(owner_name)};
        set_function_parameters(copy_constructor, {copy_parameter});
        set_function_deleted(copy_constructor);
        auto const copy_assignment{function("operator=", "auto")};
        set_function_parameters(copy_assignment, {copy_parameter});
        set_function_return_type(
            copy_assignment, decorate_type(owner_type, "", "&"), ReturnSyntax::trailing);
        set_function_deleted(copy_assignment);
        auto const move_constructor{constructor(owner_name)};
        set_function_parameters(move_constructor, {move_parameter});
        set_function_noexcept(move_constructor);
        set_function_body(
            move_constructor,
            {expr_stmt(
                call(type_reference(decorate_type(named_type("Operations"), "", "::take_storage")),
                     {unary(UnaryOperator::dereference, named("this")), named("other")}))},
            {member_initializer(store_text("resource_"),
                                member_access(named("other"), "resource_"))});
        auto const move_assignment{function("operator=", "auto")};
        set_function_parameters(move_assignment, {move_parameter});
        set_function_body(
            move_assignment,
            {return_stmt(
                call(type_reference(decorate_type(named_type("Operations"), "", "::move_assign")),
                     {unary(UnaryOperator::dereference, named("this")), named("other")}))});
        set_function_return_type(
            move_assignment, decorate_type(owner_type, "", "&"), ReturnSyntax::trailing);

        auto result{node_list()};
        result.append(section_header("Lifetime"))
            .new_lines(1)
            .append(adjacent({inline_function(default_constructor),
                              inline_function(resource_constructor),
                              compact_function(resource_accessor),
                              compact_function(destructor_value),
                              declaration(copy_constructor),
                              declaration(copy_assignment),
                              inline_function(move_constructor),
                              inline_function(move_assignment)}));
        return result.build();
    }
    auto data_pointers_node(SingleAllocationModelId model) -> NodeId {
        auto& model_store{lowering_storage().single_allocation_models};
        auto const model_store_index{model_store.index(model)};
        auto& column_store{lowering_storage().single_allocation_columns};
        auto children{node_list()};
        children.add(using_declaration(
            store_text("Element"),
            template_type(std_types().conditional_t,
                          {template_type(std_types().is_const_v, {named_type("Byte")}),
                           decorate_type(named_type("T"), "", " const"),
                           named_type("T")}),
            template_declaration({type_template_parameter(store_text("T"))})));
        for (auto const column : lowering_storage().items(model_store.columns[model_store_index])) {
            children.new_lines(1).add(instance_member(
                decorate_type(template_type(named_type("Element"),
                                            {column_store.type[column_store.index(column)]}),
                              "",
                              "*"),
                column_store.flattened_identifier[column_store.index(column)],
                init_list({})));
        }

        std::vector<Expr> arguments;
        for (auto const column : lowering_storage().items(model_store.columns[model_store_index])) {
            arguments.push_back(member_access(
                named("self"), column_store.flattened_identifier[column_store.index(column)]));
        }
        auto const visit{make_function("with_columns",
                                       "decltype(auto)",
                                       {parameter("Self&", "self"), parameter("Func&&", "func")},
                                       {return_stmt(call(named("func"), arguments))},
                                       FunctionReceiver::static_)};
        set_function_template(visit,
                              template_declaration({type_template_parameter(store_text("Self")),
                                                    type_template_parameter(store_text("Func"))}));
        children.new_lines(1).add(inline_function(visit));

        auto const created_record{
            record(store_text("DataPointers"), children.build(), RecordKind::struct_)};
        set_record_template(created_record,
                            template_declaration({type_template_parameter(store_text("Byte"))}));
        return created_record;
    }
    auto layout_column(LoweringContext& ctx, SingleAllocationColumnId column) -> Expr {
        auto const& columns{lowering_storage().single_allocation_columns};
        return type_reference(decorate_type(
            named_type("Layout"),
            "",
            "::" + ctx.text(columns.flattened_identifier[columns.index(column)]) + "_column"));
    }
    auto member_columns(SingleAllocationModelId model, StringId member)
        -> std::vector<SingleAllocationColumnId> {
        auto const& storage{lowering_storage()};
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
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        auto body{node_list()};
        std::map<LoweringPathId, StringId> groups;
        std::vector<Expr> arguments;
        for (auto const column : lowering_storage().items(models.columns[models.index(model)])) {
            arguments.push_back(column_access(
                ctx, columns.member_path[columns.index(column)], "source", groups, body));
        }
        body.add(return_stmt(call(named("func"), arguments)));
        auto const function_id{
            make_function("with_columns",
                          "auto",
                          {parameter("Columns const&", "source"), parameter("Func&&", "func")},
                          body.build(),
                          FunctionReceiver::static_)};
        set_function_template(function_id,
                              template_declaration({type_template_parameter(store_text("Columns")),
                                                    type_template_parameter(store_text("Func"))}));
        return record(store_text("Schema"), {inline_function(function_id)}, RecordKind::struct_);
    }
    auto shared_view_type(LoweringContext& ctx, DeclarationHandle schema, bool resolved)
        -> CppTypeId {
        auto const scope{schema_namespace(ctx, schema)};
        return template_type(
            std_types().conditional_t,
            {named_type("Const"),
             named_type(scope + (resolved ? "::ResolvedConstView" : "::ConstView"),
                        NameOrigin::global),
             named_type(scope + (resolved ? "::ResolvedView" : "::View"), NameOrigin::global)});
    }
    auto view_accessor_nodes(LoweringContext& ctx, SingleAllocationModelId model, bool resolved)
        -> Nodes {
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        auto const& members{ctx.declarations.soa_members};
        auto const model_row{models.index(model)};
        auto const schema{models.schema[model_row]};
        auto const schema_row{ctx.declarations.soas.index(schema)};
        auto const selected{lowering_storage().items(models.columns[model_row])};
        auto const strided{is_strided_schema(ctx, schema)};
        auto children{node_list()};
        auto accessor = [&](std::string_view name, Expr value, CppTypeId type = {}) {
            auto const function_id{
                make_function(name, "auto", {}, {return_stmt(value)}, FunctionReceiver::const_)};
            if (type.value != 0) {
                set_function_return_type(function_id, type, ReturnSyntax::trailing);
            }
            children.new_lines(1).add(compact_function(function_id));
        };
        auto column_span = [&](SingleAllocationColumnId column) {
            auto const index{std::find(selected.begin(), selected.end(), column) -
                             selected.begin()};
            if (resolved) {
                return call(
                    this_member("column"),
                    {member_access(this_member("columns_"),
                                   ctx.text(columns.flattened_identifier[columns.index(column)]))});
            }
            if (strided) {
                return call(this_member("column"), {literal(std::to_string(index))});
            }
            return call(template_id(this_member("template column"),
                                    {type_reference(columns.type[columns.index(column)])}),
                        {layout_column(ctx, column)});
        };
        for (auto const member :
             ctx.declarations.items(ctx.declarations.soas.members[schema_row])) {
            auto const row{members.index(member)};
            auto const name{ctx.text(members.name[row])};
            if (members.kind[row] == SoaMemberKind::nested) {
                auto const nested{ctx.for_declaration(schema).nested_declaration(member)};
                auto const nested_columns{member_columns(model, members.name[row])};
                auto const nested_strided{is_strided_schema(ctx, nested)};
                auto const type{shared_view_type(ctx, nested, resolved || !nested_strided)};
                if (!resolved && nested_strided && strided) {
                    accessor(
                        "view_" + name,
                        init_list({call(member_access(column_span(nested_columns.front()), "data")),
                                   call(this_member("byte_stride")),
                                   call(this_member("num"))}),
                        type);
                } else if (!resolved && nested_strided) {
                    auto const factory{template_id(
                        type_reference(named_type("ioj::soa_storage_detail::make_strided_view",
                                                  NameOrigin::global)),
                        {type_reference(type), type_reference(intern_type("Layout"))})};
                    accessor("view_" + name,
                             call(factory,
                                  {this_member("state_"),
                                   this_member("offset_"),
                                   call(this_member("num")),
                                   layout_column(ctx, nested_columns[0]),
                                   layout_column(ctx, nested_columns[1])}),
                             type);
                } else {
                    std::vector<Expr> pointers;
                    for (auto const column : nested_columns) {
                        pointers.push_back(
                            resolved
                                ? member_access(
                                      this_member("columns_"),
                                      ctx.text(columns.flattened_identifier[columns.index(column)]))
                                : call(member_access(column_span(column), "data")));
                    }
                    pointers.push_back(call(this_member("num")));
                    accessor("view_" + name, init_list(pointers), type);
                }
            } else {
                auto const column{
                    column_for(ctx, model, lowering_storage().paths.append({}, members.name[row]))};
                accessor(name, column_span(column));
            }
        }
        return children.build();
    }
    auto emit_resolved_views(LoweringContext& ctx,
                             SingleAllocationModelId model,
                             NodeListBuilder& source,
                             NodeListBuilder& declarations) -> Nodes {
        auto& storage{lowering_storage()};
        auto const& models{storage.single_allocation_models};
        auto const schema{models.schema[models.index(model)]};
        std::string const mutable_name{"ResolvedView"};
        std::string const const_name{"ResolvedConstView"};
        std::string const implementation{"ResolvedViewImpl"};
        auto result{node_list()};
        declarations.add(forward_declaration(store_text(mutable_name)))
            .new_lines(1)
            .add(forward_declaration(store_text(const_name)))
            .new_lines(1);
        result.append(section_header("Resolved views")).new_lines(1);

        auto const scope{schema_namespace(ctx, schema)};
        auto const base{template_type(
            named_type("ioj::soa_storage_detail::ResolvedViewBase", NameOrigin::global),
            {decorate_type(named_type("Layout"), "", "::column_types"),
             named_type("DataPointers"),
             named_type("Const")})};
        auto children{node_list()};
        children.append(adjacent(
            {using_declaration(store_text("Base"), base),
             using_import(named_type("Base::Base")),
             using_declaration(store_text("Layout"),
                               named_type(scope + "::Layout", NameOrigin::global)),
             using_declaration(store_text("view_type"),
                               named_type(scope + "::" + mutable_name, NameOrigin::global)),
             using_declaration(store_text("const_view_type"),
                               named_type(scope + "::" + const_name, NameOrigin::global)),
             using_declaration(store_text("soa_schema"),
                               named_type(scope + "::Schema", NameOrigin::global))}));

        auto const conversion{constructor(store_text(implementation))};
        set_function_parameters(conversion,
                                {parameter(decorate_type(template_type(named_type(implementation),
                                                                       {intern_type("false")}),
                                                         "",
                                                         " const&"),
                                           "other")});
        set_function_template(conversion,
                              template_declaration({value_template_parameter(
                                  std_types().bool_, store_text("Enabled"), named("Const"))}));
        set_function_requires_clause(conversion, named("Enabled"));
        set_function_body(conversion, {}, {member_initializer(store_text("Base"), named("other"))});
        children.new_lines(1).add(inline_function(conversion));

        children.append(view_accessor_nodes(ctx, model, true));
        auto const record_id{
            record(store_text(implementation), children.build(), RecordKind::struct_)};
        describe_compact_scope(ctx, record_id, schema);
        set_record_bases(record_id, {base});
        set_record_template(record_id,
                            template_declaration({value_template_parameter(std_types().bool_,
                                                                           store_text("Const"))}));
        result.add(record_id).new_lines(1);
        for (bool const is_const : {true, false}) {
            auto const name{is_const ? const_name : mutable_name};
            auto const receiver{is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view};
            auto const base{
                soa_row_operations(ctx,
                                   schema,
                                   receiver,
                                   name,
                                   template_type(named_type(implementation),
                                                 {intern_type(is_const ? "true" : "false")}))};
            auto wrapper{node_list()};
            wrapper.add(using_declaration(store_text("Base"), base))
                .new_lines(1)
                .add(using_import(named_type("Base::Base")))
                .new_lines(1);
            auto api{lower_soa_api(ctx, schema, receiver, name, base)};
            wrapper.append(api.header);
            if (!api.source.empty()) {
                source.append(api.source).new_lines(1);
            }
            auto const type{record(store_text(name), wrapper.build(), RecordKind::struct_)};
            set_record_bases(type, {base});
            result.add(type).new_lines(1);
        }
        return result.build();
    }
    auto compact_view_node(LoweringContext& ctx, SingleAllocationModelId model) -> NodeId {
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        auto const model_row{models.index(model)};
        auto const schema{models.schema[model_row]};
        auto const schema_row{ctx.declarations.soas.index(schema)};
        auto const scope{schema_namespace(ctx, schema)};
        auto const selected{lowering_storage().items(models.columns[model_row])};
        auto const strided{is_strided_schema(ctx, schema)};
        auto const base{
            strided
                ? template_type(
                      named_type("ioj::soa_storage_detail::StridedViewBase", NameOrigin::global),
                      {columns.type[columns.index(selected.front())], named_type("Const")})
                : template_type(
                      named_type("ioj::soa_storage_detail::CompactViewBase", NameOrigin::global),
                      {named_type("Const")})};
        auto children{node_list()};
        children.append(
            adjacent({using_declaration(store_text("soa_schema"),
                                        named_type(schema_namespace(ctx, schema) + "::Schema",
                                                   NameOrigin::global)),
                      using_declaration(store_text("Base"), base),
                      using_import(named_type("Base::Base")),
                      using_declaration(store_text("Layout"),
                                        named_type(scope + "::Layout", NameOrigin::global)),
                      using_declaration(store_text("view_type"),
                                        named_type(scope + "::View", NameOrigin::global)),
                      using_declaration(store_text("const_view_type"),
                                        named_type(scope + "::ConstView", NameOrigin::global)),
                      using_declaration(store_text("resolved_view_type"),
                                        shared_view_type(ctx, schema, true))}));
        if (strided && !ctx.declarations.soas.layout_only[schema_row]) {
            auto const state{
                named_type("ioj::soa_storage_detail::StorageState", NameOrigin::global)};
            children.new_lines(1).add(using_declaration(
                store_text("State"),
                template_type(std_types().conditional_t,
                              {named_type("Const"), decorate_type(state, "", " const"), state})));
            auto const constructor_id{constructor(store_text("ViewImpl"))};
            set_function_parameters(constructor_id,
                                    {parameter("State*", "state"),
                                     parameter(std_types().u32, "offset"),
                                     parameter(std_types().u32, "count")});
            std::vector<Expr> arguments{named("state"), named("offset"), named("count")};
            for (auto const column : selected.first(2)) {
                arguments.push_back(layout_column(ctx, column));
            }
            set_function_body(constructor_id,
                              {},
                              {member_initializer(
                                  store_text("Base"),
                                  call(template_id(type_reference(named_type(
                                                       "ioj::soa_storage_detail::make_strided_view",
                                                       NameOrigin::global)),
                                                   {type_reference(named_type("Base")),
                                                    type_reference(named_type("Layout"))}),
                                       arguments))});
            children.new_lines(1).add(inline_function(constructor_id));
        }
        auto const conversion{constructor(store_text("ViewImpl"))};
        set_function_parameters(
            conversion,
            {parameter(decorate_type(template_type(named_type("ViewImpl"), {intern_type("false")}),
                                     "",
                                     " const&"),
                       "other")});
        set_function_body(conversion, {}, {member_initializer(store_text("Base"), named("other"))});
        set_function_template(conversion,
                              template_declaration({value_template_parameter(
                                  std_types().bool_, store_text("Enabled"), named("Const"))}));
        set_function_requires_clause(conversion, named("Enabled"));
        children.new_lines(1).add(inline_function(conversion));
        children.append(view_accessor_nodes(ctx, model, false));
        auto const record_id{record(store_text("ViewImpl"), children.build(), RecordKind::struct_)};
        describe_compact_scope(ctx, record_id, schema);
        set_record_bases(record_id, {base});
        set_record_template(record_id,
                            template_declaration({value_template_parameter(std_types().bool_,
                                                                           store_text("Const"))}));
        return record_id;
    }
    auto view_validation_nodes(std::string const& name, bool strided) -> Nodes {
        auto const validation{template_id(
            type_reference(strided ? std_types().is_trivially_copyable_v
                                   : named_type("ioj::soa_storage_detail::validate_compact_view",
                                                NameOrigin::global)),
            {type_reference(named_type(name))})};
        return {static_assertion(strided ? validation : call(validation))};
    }
    auto storage_implementation_nodes(LoweringContext& ctx, SingleAllocationModelId model)
        -> Nodes {
        auto& model_store{lowering_storage().single_allocation_models};
        auto const scope{schema_namespace(ctx, model_store.schema[model_store.index(model)])};
        auto children{node_list()};
        children
            .append(adjacent(
                {using_declaration(store_text("View"),
                                   named_type(scope + "::View", NameOrigin::global)),
                 using_declaration(store_text("ConstView"),
                                   named_type(scope + "::ConstView", NameOrigin::global))}))
            .new_lines(2)
            .append(storage_lifetime_nodes(ctx, model))
            .new_lines(1)
            .add(access(Access::protected_))
            .new_lines(1)
            .add(using_declaration(
                store_text("DataPointers"),
                template_type(named_type(scope + "::DataPointers", NameOrigin::global),
                              {named_type("Byte")}),
                template_declaration({type_template_parameter(store_text("Byte"))})));

        return children.build();
    }
};

} // namespace

auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModelId model) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_layout(ctx, model);
}
auto emit_single_allocation_views(LoweringContext& ctx,
                                  SingleAllocationModelId model,
                                  NodeListBuilder& source,
                                  NodeListBuilder& declarations) -> Nodes {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_views(
        ctx, model, source, declarations);
}
auto emit_single_allocation_container(LoweringContext& ctx,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source) -> NodeId {
    return SingleAllocationEmitter{ctx.ir}.emit_single_allocation_container(ctx, model, source);
}

} // namespace lispb::codegen::detail
