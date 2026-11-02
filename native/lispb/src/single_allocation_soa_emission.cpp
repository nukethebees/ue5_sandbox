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
        auto const layout{add_record({.name = named(store_text("Layout")),
                                      .children = children.build(),
                                      .bases = {base},
                                      .scope_bindings = inherited})};
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
            auto const record_id{add_record(
                {.name = named(store_text(name)),
                 .children = children.build(),
                 .bases = {base},
                 .export_specifier =
                     ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(schema)]})};
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
                .add(using_declaration(store_text(name), nested_type(named_type("Layout"), name)))
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
                                      scope_member(named_type("Layout"), name)))
                .new_lines(1);
        }
        FunctionDefinition const bytes{
            .name = named("layout_bytes"),
            .return_type = named_type("byte_size_type"),
            .return_syntax = ReturnSyntax::trailing,
            .parameters = {parameter("byte_size_type", "blocks")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(
                                   call(scope_member(named_type("Layout"), "layout_bytes"),
                                        {named("blocks")}))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_constexpr = true, .define_in_header = true}};
        children.new_lines(1)
            .add(header_function(add_function(bytes)))
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
        auto const created_record{add_record(
            {.name = named(owner_name),
             .children = children.build(),
             .bases = {decorate_type(
                           named_type("ioj::native_soa::StorageState", NameOrigin::global),
                           "protected ",
                           ""),
                       row_operations},
             .export_specifier = ctx.declarations.soas.export_specifier[ctx.declarations.soas.index(
                 model_store.schema[model_store_index])],
             .dependencies = {std::from_range,
                              lowering_storage().items(
                                  model_store.dependencies[model_store_index])},
             .scope_bindings =
                 compact_scope_bindings(ctx, model_store.schema[model_store_index])})};
        return created_record;
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto compact_scope_bindings(LoweringContext& ctx, DeclarationHandle schema)
        -> std::optional<std::vector<StringId>> {
        if (!ctx.declarations.soas.using_declarations[ctx.declarations.soas.index(schema)]
                 .empty()) {
            return std::nullopt;
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
        return names;
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
        auto const free_data{call(scope_member(named_type("Operations"), "release_storage"),
                                  {unary(UnaryOperator::dereference, named("this"))})};
        FunctionDefinition const default_constructor{
            .name = named(owner_name),
            .kind = FunctionKind::constructor,
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {},
                               .initializers = {member_initializer(
                                   owner_name,
                                   unary(UnaryOperator::dereference,
                                         call(std_types().pmr.get_default_resource)))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .flags = {.is_inline = true, .define_in_header = true}};
        FunctionDefinition const resource_constructor{
            .name = named(owner_name),
            .kind = FunctionKind::explicit_constructor,
            .parameters = {parameter(decorate_type(std_types().pmr.memory_resource, "", "&"),
                                     "resource")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {},
                               .initializers = {member_initializer(
                                   store_text("resource_"),
                                   unary(UnaryOperator::address_of, named("resource")))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .flags = {.is_inline = true, .define_in_header = true}};
        FunctionDefinition const resource_accessor{
            .name = named("memory_resource"),
            .return_type = decorate_type(std_types().pmr.memory_resource, "", "*"),
            .return_syntax = ReturnSyntax::trailing,
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(named("resource_"))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .receiver = FunctionReceiver::const_,
            .flags = {.is_inline = true, .define_in_header = true},
            .formatting = {.body_layout = FunctionBodyLayout::compact}};
        FunctionDefinition const destructor_value{
            .name = named("~" + std::string{text_view(owner_name)}),
            .kind = FunctionKind::destructor,
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {expr_stmt(free_data)}},
            .flags = {.is_inline = true, .define_in_header = true},
            .formatting = {.body_layout = FunctionBodyLayout::compact}};
        FunctionDefinition const copy_constructor{
            .name = named(owner_name),
            .kind = FunctionKind::constructor,
            .parameters = {copy_parameter},
            .implementation = {.kind = FunctionImplementationKind::deleted}};
        FunctionDefinition const copy_assignment{
            .name = named("operator="),
            .return_type = decorate_type(owner_type, "", "&"),
            .return_syntax = ReturnSyntax::trailing,
            .parameters = {copy_parameter},
            .implementation = {.kind = FunctionImplementationKind::deleted}};
        FunctionDefinition const move_constructor{
            .name = named(owner_name),
            .kind = FunctionKind::constructor,
            .parameters = {move_parameter},
            .implementation =
                {.kind = FunctionImplementationKind::body,
                 .body = {expr_stmt(
                     call(scope_member(named_type("Operations"), "take_storage"),
                          {unary(UnaryOperator::dereference, named("this")), named("other")}))},
                 .initializers = {member_initializer(store_text("resource_"),
                                                     member_access(named("other"), "resource_"))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .flags = {.is_inline = true, .define_in_header = true}};
        FunctionDefinition const move_assignment{
            .name = named("operator="),
            .return_type = decorate_type(owner_type, "", "&"),
            .return_syntax = ReturnSyntax::trailing,
            .parameters = {move_parameter},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(
                                   call(scope_member(named_type("Operations"), "move_assign"),
                                        {unary(UnaryOperator::dereference, named("this")),
                                         named("other")}))}},
            .flags = {.is_inline = true, .define_in_header = true}};

        auto result{node_list()};
        result.append(section_header("Lifetime"))
            .new_lines(1)
            .append(adjacent({header_function(add_function(default_constructor)),
                              header_function(add_function(resource_constructor)),
                              header_function(add_function(resource_accessor)),
                              header_function(add_function(destructor_value)),
                              declaration(add_function(copy_constructor)),
                              declaration(add_function(copy_assignment)),
                              header_function(add_function(move_constructor)),
                              header_function(add_function(move_assignment))}));
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
        FunctionDefinition const visit{
            .name = named("with_columns"),
            .return_type = intern_type("decltype(auto)"),
            .parameters = {parameter("Self&", "self"), parameter("Func&&", "func")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(call(named("func"), arguments))}},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_inline = true, .define_in_header = true},
            .template_declaration =
                template_declaration({type_template_parameter(store_text("Self")),
                                      type_template_parameter(store_text("Func"))})};
        children.new_lines(1).add(header_function(add_function(visit)));

        auto const created_record{add_record({.name = named(store_text("DataPointers")),
                                              .children = children.build(),
                                              .template_declaration = template_declaration(
                                                  {type_template_parameter(store_text("Byte"))})})};
        return created_record;
    }
    auto layout_column(LoweringContext& ctx, SingleAllocationColumnId column) -> Expr {
        auto const& columns{lowering_storage().single_allocation_columns};
        return scope_member(named_type("Layout"),
                            ctx.text(columns.flattened_identifier[columns.index(column)]) +
                                "_column");
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
        FunctionDefinition const function{
            .name = named("with_columns"),
            .return_type = intern_type("auto"),
            .parameters = {parameter("Columns const&", "source"), parameter("Func&&", "func")},
            .implementation = {.kind = FunctionImplementationKind::body, .body = body.build()},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_inline = true, .define_in_header = true},
            .template_declaration =
                template_declaration({type_template_parameter(store_text("Columns")),
                                      type_template_parameter(store_text("Func"))})};
        return add_record({.name = named(store_text("Schema")),
                           .children = {header_function(add_function(function))}});
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
            FunctionDefinition const function{
                .name = named(name),
                .return_type = type.value != 0 ? type : intern_type("auto"),
                .return_syntax = type.value != 0 ? ReturnSyntax::trailing : ReturnSyntax::direct,
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {return_stmt(value)}},
                .receiver = FunctionReceiver::const_,
                .flags = {.is_inline = true, .define_in_header = true},
                .formatting = {.body_layout = FunctionBodyLayout::compact}};
            children.new_lines(1).add(header_function(add_function(function)));
        };
        auto column_span = [&](SingleAllocationColumnId column) {
            auto const index{std::find(selected.begin(), selected.end(), column) -
                             selected.begin()};
            if (resolved) {
                return this_member_call(
                    "column",
                    {member_access(this_member("columns_"),
                                   ctx.text(columns.flattened_identifier[columns.index(column)]))});
            }
            if (strided) {
                return this_member_call("column", {literal(std::to_string(index))});
            }
            return template_call(this_member("template column"),
                                 {type_reference(columns.type[columns.index(column)])},
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
                    accessor("view_" + name,
                             init_list({member_call(column_span(nested_columns.front()), "data"),
                                        this_member_call("byte_stride"),
                                        this_member_call("num")}),
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
                                   this_member_call("num"),
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
                                : member_call(column_span(column), "data"));
                    }
                    pointers.push_back(this_member_call("num"));
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
            {nested_type(named_type("Layout"), "column_types"),
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

        FunctionDefinition const conversion{
            .name = named(store_text(implementation)),
            .kind = FunctionKind::constructor,
            .parameters = {parameter(
                decorate_type(template_type(named_type(implementation), {intern_type("false")}),
                              "",
                              " const&"),
                "other")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {},
                               .initializers = {member_initializer(store_text("Base"),
                                                                   named("other"))}},
            .flags = {.is_inline = true, .define_in_header = true},
            .template_declaration = template_declaration({value_template_parameter(
                std_types().bool_, store_text("Enabled"), named("Const"))}),
            .requires_clause = named("Enabled")};
        children.new_lines(1).add(header_function(add_function(conversion)));

        children.append(view_accessor_nodes(ctx, model, true));
        auto const record_id{
            add_record({.name = named(store_text(implementation)),
                        .children = children.build(),
                        .bases = {base},
                        .template_declaration = template_declaration(
                            {value_template_parameter(std_types().bool_, store_text("Const"))}),
                        .scope_bindings = compact_scope_bindings(ctx, schema)})};
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
            auto const type{add_record(
                {.name = named(store_text(name)), .children = wrapper.build(), .bases = {base}})};
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
            std::vector<Expr> arguments{named("state"), named("offset"), named("count")};
            for (auto const column : selected.first(2)) {
                arguments.push_back(layout_column(ctx, column));
            }
            FunctionDefinition const constructor{
                .name = named(store_text("ViewImpl")),
                .kind = FunctionKind::constructor,
                .parameters = {parameter("State*", "state"),
                               parameter(std_types().u32, "offset"),
                               parameter(std_types().u32, "count")},
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {},
                                   .initializers = {member_initializer(
                                       store_text("Base"),
                                       template_call(
                                           type_reference(named_type(
                                               "ioj::soa_storage_detail::make_strided_view",
                                               NameOrigin::global)),
                                           {type_reference(named_type("Base")),
                                            type_reference(named_type("Layout"))},
                                           arguments))}},
                .flags = {.is_inline = true, .define_in_header = true}};
            children.new_lines(1).add(header_function(add_function(constructor)));
        }
        FunctionDefinition const conversion{
            .name = named(store_text("ViewImpl")),
            .kind = FunctionKind::constructor,
            .parameters = {parameter(
                decorate_type(
                    template_type(named_type("ViewImpl"), {intern_type("false")}), "", " const&"),
                "other")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {},
                               .initializers = {member_initializer(store_text("Base"),
                                                                   named("other"))}},
            .flags = {.is_inline = true, .define_in_header = true},
            .template_declaration = template_declaration({value_template_parameter(
                std_types().bool_, store_text("Enabled"), named("Const"))}),
            .requires_clause = named("Enabled")};
        children.new_lines(1).add(header_function(add_function(conversion)));
        children.append(view_accessor_nodes(ctx, model, false));
        auto const record_id{
            add_record({.name = named(store_text("ViewImpl")),
                        .children = children.build(),
                        .bases = {base},
                        .template_declaration = template_declaration(
                            {value_template_parameter(std_types().bool_, store_text("Const"))}),
                        .scope_bindings = compact_scope_bindings(ctx, schema)})};
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
