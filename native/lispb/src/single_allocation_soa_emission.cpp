#include "lowering_utils.h"
#include "single_allocation_soa_internal.h"
#include "soa_api.h"

#include <algorithm>
#include <utility>

namespace codegen::detail {
namespace {

auto adjacent(Nodes nodes) -> Nodes {
    NodeListBuilder result;
    auto const count{nodes.size()};
    for (std::size_t index{}; index < count; ++index) {
        result.add(std::move(nodes[index]));
        if (index + 1 < count) {
            result.new_lines(1);
        }
    }
    return result.build();
}

auto section_header(std::string title) -> Nodes {
    return adjacent({BlockComment{"****************************************"},
                     LineComment{std::move(title)},
                     BlockComment{"****************************************"}});
}

auto inline_function(FunctionSpec spec) -> Node {
    spec.is_inline = true;
    return header_function(std::move(spec));
}

auto compact_function(FunctionSpec spec) -> Node {
    spec.formatting.body_layout = FunctionFormatting::BodyLayout::compact;
    return inline_function(std::move(spec));
}

auto column_access(std::span<std::string const> path,
                   std::string receiver,
                   std::map<std::string, std::string>& groups,
                   NodeListBuilder& body) -> Expr {
    std::string group_path;
    auto const group_count{path.size() - 1};
    for (std::size_t index{}; index < group_count; ++index) {
        if (index > 0) {
            group_path += '_';
        }
        group_path += path[index];
        auto const [group, inserted]{groups.try_emplace(group_path, group_path + "_view")};
        if (inserted) {
            auto const accessor{receiver.empty() ? "view_" + path[index]
                                                 : receiver + ".view_" + path[index]};
            body.add(VariableDeclarationStmt{"auto const&", group->second, call(named(accessor))});
        }
        receiver = group->second;
    }
    return call(named(receiver.empty() ? path.back() : receiver + "." + path.back()));
}

auto storage_lifetime_nodes(SingleAllocationModel const& model) -> Nodes {
    auto const free_data{call(named("Operations::release_storage"), {named("*this")})};
    auto default_constructor{FunctionSpec{
        .name = model.owner_name,
        .qualifiers = {.is_noexcept = true},
        .member_initializers = {{model.owner_name, "std::pmr::get_default_resource()"}},
    }};
    auto resource_constructor{FunctionSpec{
        .name = model.owner_name,
        .return_type = "explicit",
        .parameters = {FunctionParameter{"std::pmr::memory_resource*", "resource"}},
        .body = {raw("assert(resource != nullptr);")},
        .qualifiers = {.is_noexcept = true},
        .member_initializers = {{"resource_", "resource"}},
    }};
    auto resource_accessor{FunctionSpec{
        .name = "get_memory_resource",
        .return_type = "auto",
        .body = {ReturnStmt{named("resource_")}},
        .qualifiers = {.trailing_return_type = CppType{"std::pmr::memory_resource*"},
                       .is_const = true,
                       .is_noexcept = true},
    }};
    auto destructor{FunctionSpec{
        .name = "~" + model.owner_name,
        .body = {ExpressionStmt{free_data}},
    }};
    auto copy_constructor{FunctionSpec{
        .name = model.owner_name,
        .parameters = {FunctionParameter{model.owner_name + " const&", ""}},
        .qualifiers = {.disposition = FunctionDisposition::deleted},
    }};
    auto copy_assignment{FunctionSpec{
        .name = "operator=",
        .return_type = "auto",
        .parameters = {FunctionParameter{model.owner_name + " const&", ""}},
        .qualifiers = {.trailing_return_type = CppType{model.owner_name + "&"},
                       .disposition = FunctionDisposition::deleted},
    }};
    auto move_constructor{FunctionSpec{
        .name = model.owner_name,
        .parameters = {FunctionParameter{model.owner_name + "&&", "other"}},
        .body = {raw("Operations::take_storage(*this, other);")},
        .qualifiers = {.is_noexcept = true},
        .member_initializers = {{"resource_", "other.resource_"}},
    }};
    auto move_assignment{FunctionSpec{
        .name = "operator=",
        .return_type = "auto",
        .parameters = {FunctionParameter{model.owner_name + "&&", "other"}},
        .body = {raw("return Operations::move_assign(*this, other);")},
        .qualifiers = {.trailing_return_type = CppType{model.owner_name + "&"}},
    }};

    NodeListBuilder result;
    result.append(section_header("Lifetime"))
        .new_lines(1)
        .append(adjacent({inline_function(std::move(default_constructor)),
                          inline_function(std::move(resource_constructor)),
                          compact_function(std::move(resource_accessor)),
                          compact_function(std::move(destructor)),
                          declaration(std::move(copy_constructor)),
                          declaration(std::move(copy_assignment)),
                          inline_function(std::move(move_constructor)),
                          inline_function(std::move(move_assignment))}));
    return result.build();
}

auto data_pointers_node(SingleAllocationModel const& model) -> Node {
    NodeListBuilder children;
    children.add(raw("template <typename T>\nusing Element = "
                     "std::conditional_t<std::is_const_v<Byte>, T const, T>;"));
    for (auto const& column : model.columns) {
        children.new_lines(1).add(
            Member{CppType{"Element<" + column.type.spelling + ">*", column.type.dependencies},
                   column.flattened_identifier,
                   RawExpr{""}});
    }

    std::vector<Expr> shifted_columns;
    shifted_columns.reserve(model.columns.size());
    for (auto const& column : model.columns) {
        shifted_columns.push_back(
            binary(BinaryOperator::add, named(column.flattened_identifier), named("offset")));
    }
    children.new_lines(1).add(inline_function(FunctionSpec{
        .name = "operator+",
        .return_type = "auto",
        .parameters = {FunctionParameter{"size_type const", "offset"}},
        .body = {IfStmt{binary(BinaryOperator::equal,
                               named(model.columns.front().flattened_identifier),
                               literal("nullptr")),
                        Block{{ReturnStmt{init_list({})}}}},
                 ReturnStmt{init_list(std::move(shifted_columns))}},
        .qualifiers = {.trailing_return_type = CppType{"DataPointers"},
                       .is_const = true,
                       .is_noexcept = true},
    }));
    return Struct{
        .name = "DataPointers",
        .children = children.build(),
        .template_parameters = "typename Byte",
    };
}

auto storage_access_nodes(SingleAllocationModel const& model) -> Nodes {
    auto get_data{FunctionSpec{
        .name = "get_data",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this Self&", "self"}},
        .body = {UsingDeclaration{
                     "Byte",
                     "std::conditional_t<std::is_const_v<Self>, std::byte const, std::byte>"},
                 IfStmt{binary(BinaryOperator::equal,
                               member_access(named("self"), "data_"),
                               literal("nullptr")),
                        Block{{ReturnStmt{init_list({}, CppType{"DataPointers<Byte>"})}}}},
                 ReturnStmt{call(named("make_data_unchecked"),
                                 {static_cast_expr("Byte*", member_access(named("self"), "data_")),
                                  call(member_access(named("self"), "capacity_blocks"))})}},
        .qualifiers = {.is_noexcept = true},
        .template_parameters = "typename Self",
    }};
    auto get_data_offset{FunctionSpec{
        .name = "get_data",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this Self&", "self"},
                       FunctionParameter{"size_type const", "offset"}},
        .body = {ReturnStmt{binary(
            BinaryOperator::add, call(member_access(named("self"), "get_data")), named("offset"))}},
        .qualifiers = {.is_noexcept = true},
        .template_parameters = "typename Self",
    }};
    return adjacent({data_pointers_node(model),
                     inline_function(std::move(get_data)),
                     inline_function(std::move(get_data_offset))});
}

auto column_pointer_nodes(SingleAllocationModel const& model) -> Nodes {
    NodeListBuilder make_data_body;
    make_data_body.add(raw(model.dialect.runtime_namespace + "LayoutCursor cursor{blocks};"));
    std::vector<Expr> pointers;
    pointers.reserve(model.columns.size());
    for (auto const& column : model.columns) {
        pointers.push_back(call(member_access(named("cursor"), "column_pointer"),
                                {named("data"), named("Layout::" + column.layout_identifier)}));
    }
    make_data_body.add(ReturnStmt{init_list(std::move(pointers))});
    auto make_data{FunctionSpec{
        .name = "make_data_unchecked",
        .return_type = "auto",
        .parameters = {FunctionParameter{"Byte* const", "data"},
                       FunctionParameter{"byte_size_type const", "blocks"}},
        .body = make_data_body.build(),
        .qualifiers = {.trailing_return_type = CppType{"DataPointers<Byte>"}, .is_noexcept = true},
        .is_static = true,
        .template_parameters = "typename Byte",
    }};
    auto capacity_blocks{FunctionSpec{
        .name = "capacity_blocks",
        .return_type = "auto",
        .body = {ReturnStmt{static_cast_expr(
            "byte_size_type",
            binary(BinaryOperator::divide, named("capacity_"), named("capacity_granularity")))}},
        .qualifiers = {.trailing_return_type = CppType{"byte_size_type"},
                       .is_const = true,
                       .is_noexcept = true},
    }};

    NodeListBuilder result;
    result.add(raw("friend Operations;\nfriend ::ml::soa_storage_detail::StorageRequirements;"))
        .new_lines(1)
        .append(section_header("Column pointers"))
        .new_lines(1)
        .append(adjacent(
            {inline_function(std::move(make_data)), compact_function(std::move(capacity_blocks))}));
    return result.build();
}

auto default_construction_node(SingleAllocationModel const& model) -> Node {
    NodeListBuilder body;
    body.add(VariableDeclarationStmt{
        "auto const",
        "columns",
        binary(BinaryOperator::add,
               call(named("make_data_unchecked"), {named("data_"), call(named("capacity_blocks"))}),
               named("first"))});
    for (auto const& column : model.columns) {
        body.add(ExpressionStmt{
            call(named(model.dialect.runtime_namespace + "default_construct_n",
                       column.type.dependencies),
                 {member_access(named("columns"), column.flattened_identifier), named("count")})});
    }
    return inline_function(FunctionSpec{
        .name = "default_construct_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"size_type const", "first"},
                       FunctionParameter{"size_type const", "count"}},
        .body = body.build(),
    });
}

auto column_copying_nodes(SingleAllocationModel const& model) -> Nodes {
    auto swap_remove{compact_function(FunctionSpec{
        .name = "swap_remove_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"size_type const", "index"},
                       FunctionParameter{"size_type const", "source"},
                       FunctionParameter{"size_type const", "move_count"}},
        .body = {ExpressionStmt{
            call(named("copy_columns"),
                 {call(named("get_data")), named("index"), named("source"), named("move_count")})}},
    })};

    NodeListBuilder copy_body;
    copy_body.add(raw("auto transfer = [count](auto* dst, auto const* src) {\n    " +
                      model.dialect.runtime_namespace +
                      "transfer_n<Overlapping>(dst, src, count);\n};"));
    for (auto const& column : model.columns) {
        copy_body.add(
            ExpressionStmt{call(named("transfer"),
                                {member_access(named("destination"), column.flattened_identifier),
                                 member_access(named("source"), column.flattened_identifier)})});
    }
    auto transfer{inline_function(FunctionSpec{
        .name = "transfer_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"DataPointers<std::byte> const&", "destination"},
                       FunctionParameter{"DataPointers<Byte> const&", "source"},
                       FunctionParameter{"size_type", "count"}},
        .body = copy_body.build(),
        .is_static = true,
        .template_parameters = "bool Overlapping, typename Byte",
    })};
    auto copy{compact_function(FunctionSpec{
        .name = "copy_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"DataPointers<std::byte> const&", "columns"},
                       FunctionParameter{"size_type", "index"},
                       FunctionParameter{"size_type", "source"},
                       FunctionParameter{"size_type", "move_count"}},
        .body = {raw("transfer_columns<false>(columns + index, columns + source, move_count);")},
        .is_static = true,
    })};

    auto remove_indices{inline_function(FunctionSpec{
        .name = "swap_remove_indices",
        .return_type = "void",
        .parameters = {FunctionParameter{"std::span<size_type const>", "indices"}},
        .body = {VariableDeclarationStmt{"auto const", "columns", call(named("get_data"))},
                 ExpressionStmt{
                     RawExpr{"ml::soa_storage_detail::for_each_removal_run(num_, indices, "
                             "[&](size_type index, size_type source, size_type count) { "
                             "copy_columns(columns, index, source, count); })"}}},
    })};
    return adjacent(
        {std::move(transfer), std::move(swap_remove), std::move(copy), std::move(remove_indices)});
}

auto source_pointers_node(SingleAllocationModel const& model) -> Node {
    NodeListBuilder body;
    std::map<std::string, std::string> groups;
    std::vector<Expr> pointers;
    for (auto const& column : model.columns) {
        pointers.push_back(call(named(model.dialect.runtime_namespace + "source_data"),
                                {column_access(column.member_path, "source", groups, body)}));
    }
    body.add(ReturnStmt{init_list(std::move(pointers))});
    return inline_function(FunctionSpec{
        .name = "source_pointers",
        .return_type = "auto",
        .parameters = {FunctionParameter{"Columns const&", "source"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"DataPointers<std::byte const>"}},
        .is_static = true,
        .template_parameters = "typename Columns",
        .requires_clause =
            "ml::soa_storage_detail::SoaSourceFor<Columns, " + model.owner_name + ", size_type>",
    });
}

auto source_copy_node(SingleAllocationModel const& model, bool const overlapping) -> Node {
    return inline_function(FunctionSpec{
        .name = overlapping ? "copy_columns_from" : "append_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"Columns const&", "source"},
                       FunctionParameter{"size_type", "source_first"},
                       FunctionParameter{"size_type", "first"},
                       FunctionParameter{"size_type", "count"}},
        .body = {raw(std::string{"transfer_columns<"} + (overlapping ? "true" : "false") +
                     ">(get_data(first), source_pointers(source) + source_first, count);")},
        .template_parameters = "typename Columns",
        .requires_clause =
            "ml::soa_storage_detail::SoaSourceFor<Columns, " + model.owner_name + ", size_type>",
    });
}

auto live_column_copy_node() -> Node {
    NodeListBuilder copy_live;
    copy_live.add(
        VariableDeclarationStmt{"auto const", "old_blocks", call(named("capacity_blocks"))});
    copy_live.add(VariableDeclarationStmt{
        "auto const",
        "new_blocks",
        static_cast_expr(
            "byte_size_type",
            binary(BinaryOperator::divide, named("new_capacity"), named("capacity_granularity")))});
    copy_live.add(VariableDeclarationStmt{
        "auto const",
        "source",
        call(named("make_data_unchecked"),
             {static_cast_expr("std::byte const*", named("data_")), named("old_blocks")})});
    copy_live.add(VariableDeclarationStmt{
        "auto const",
        "destination",
        call(named("make_data_unchecked"), {named("new_data"), named("new_blocks")})});
    copy_live.add(raw("transfer_columns<false>(destination, source, num_);"));

    return inline_function(FunctionSpec{
        .name = "copy_live_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"std::byte* const", "new_data"},
                       FunctionParameter{"size_type const", "new_capacity"}},
        .body = copy_live.build(),
        .qualifiers = {.is_noexcept = true},
    });
}

auto nested_model(SingleAllocationModel const& model, SoaMemberSchema const& member)
    -> SingleAllocationModel {
    auto result{model};
    result.schema = model.schemas->at(*member.nested_schema);
    result.member_prefix.push_back(member.name);
    result.view_name = model.view_name + "_" + member.name;
    result.const_view_name = model.const_view_name + "_" + member.name;
    return result;
}

auto compact_view_node(SingleAllocationModel const& model) -> Node {
    auto const name{model.view_name + "Impl"};
    NodeListBuilder children;
    children.append(adjacent(
        {UsingDeclaration{"soa_schema", CppType{model.schema->name + "Schema"}},
         UsingDeclaration{"size_type", CppType{model.dialect.size_type}},
         UsingDeclaration{"Layout", CppType{model.layout_name}},
         UsingDeclaration{"Storage", CppType{model.dialect.runtime_namespace + "StorageState"}},
         raw("using State = std::conditional_t<Const, Storage const, Storage>;\n"
             "template <typename T>\n"
             "using Element = std::conditional_t<Const, T const, T>;"),
         UsingDeclaration{"View", CppType{model.view_name}},
         UsingDeclaration{"ConstView", CppType{model.const_view_name}},
         declaration(FunctionSpec{
             .name = name,
             .qualifiers = {.disposition = FunctionDisposition::defaulted},
         }),
         inline_function(FunctionSpec{
             .name = name,
             .parameters = {FunctionParameter{"State*", "state"},
                            FunctionParameter{"size_type", "offset"},
                            FunctionParameter{"size_type", "count"}},
             .member_initializers =
                 {{"state_", "state"}, {"offset_", "offset"}, {"count_", "count"}},
         }),
         inline_function(FunctionSpec{
             .name = name,
             .parameters = {FunctionParameter{name + "<false> const&", "other"}},
             .template_parameters = "bool Enabled = Const",
             .requires_clause = "Enabled",
             .member_initializers = {{"state_", "other.state_"},
                                     {"offset_", "other.offset_"},
                                     {"count_", "other.count_"}},
         })}));

    for (auto const& member : model.schema->members) {
        auto path{model.member_prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            auto const* vector{compact_vector_for(model, path)};
            if (vector == nullptr) {
                auto const nested{nested_model(model, member)};
                children.new_lines(1).add(compact_function(FunctionSpec{
                    .name = "view_" + member.name,
                    .return_type = "auto",
                    .body = {ReturnStmt{
                        init_list({named("state_"), named("offset_"), named("count_")})}},
                    .qualifiers = {.trailing_return_type = CppType{"std::conditional_t<Const, " +
                                                                   nested.const_view_name + ", " +
                                                                   nested.view_name + ">"},
                                   .is_const = true},
                }));
                continue;
            }

            auto const rotation_view{vector->runtime_prefix == "RotatorSoA"};
            auto const pointer_view{rotation_view ||
                                    (model.backend == SoaBackend::standard_library &&
                                     vector->runtime_prefix == "Vector3" &&
                                     vector->element_type == "float")};
            auto const prefix{model.dialect.vector_namespace + vector->runtime_prefix};
            auto const element{vector->element_type + (vector->equivalent_type.empty()
                                                           ? ""
                                                           : ", " + vector->equivalent_type)};
            auto view_type{CppType{"std::conditional_t<Const, " + prefix + "ConstView<" + element +
                                   ">, " + prefix + "View<" + element + ">>"}};
            if (pointer_view && !rotation_view) {
                view_type = CppType{"ml::Vector3SoAView<Element<float>>",
                                    {{"vector_soa_view", "sandbox/core/vector_soa_view.h", {}}}};
            }
            std::vector<Expr> arguments{named("state_"), named("offset_"), named("count_")};
            auto const columns{pointer_view ? vector->components.size() : 2};
            for (std::size_t index{}; index < columns; ++index) {
                auto component_path{path};
                component_path.push_back(vector->components[index]);
                arguments.push_back(
                    named("Layout::" + column_for(model, component_path).layout_identifier));
            }
            children.new_lines(1).add(compact_function(FunctionSpec{
                .name = "view_" + member.name,
                .return_type = "auto",
                .body = {raw("using namespace ml::soa_storage_detail;"),
                         ReturnStmt{call(named(std::string{pointer_view ? "three_column_view<"
                                                                        : "strided_vector_view<"} +
                                                   view_type.spelling + ">",
                                               view_type.dependencies),
                                         std::move(arguments))}},
                .qualifiers = {.is_const = true},
            }));
        } else {
            auto const& column{column_for(model, path)};
            auto const view_type{model.dialect.span_template + "<Element<" + column.type.spelling +
                                 ">>"};
            children.new_lines(1).add(compact_function(FunctionSpec{
                .name = member.name,
                .return_type = "auto",
                .body = {ReturnStmt{
                    call(named("ml::soa_storage_detail::column_view<" + view_type + ">",
                               column.type.dependencies),
                         {named("state_"),
                          named("offset_"),
                          named("count_"),
                          named("Layout::" + column.layout_identifier)})}},
                .qualifiers = {.is_const = true},
            }));
        }
    }
    NodeListBuilder body;
    std::map<std::string, std::string> groups;
    std::vector<Expr> arguments;
    for (auto const& column : model.columns) {
        if (!std::equal(model.member_prefix.begin(),
                        model.member_prefix.end(),
                        column.member_path.begin(),
                        column.member_path.begin() +
                            std::min(model.member_prefix.size(), column.member_path.size()))) {
            continue;
        }
        auto const accessor{column_access(
            std::span{column.member_path}.subspan(model.member_prefix.size()), "", groups, body)};
        if (model.dialect.column_iteration_returns_result) {
            auto const name{"column_" + std::to_string(arguments.size())};
            body.add(VariableDeclarationStmt{"auto", name, accessor});
            arguments.push_back(named(name));
        } else {
            body.add(ExpressionStmt{call(named("func"), {accessor})});
        }
    }
    if (model.dialect.column_iteration_returns_result) {
        body.add(
            ReturnStmt{call(named("std::forward<Func>(func)", {{"std::forward", "utility", {}}}),
                            std::move(arguments))});
    }
    children.new_lines(1).add(compact_function(FunctionSpec{
        .name = model.dialect.column_iteration_function,
        .return_type = model.dialect.column_iteration_returns_result ? "auto" : "void",
        .parameters = {FunctionParameter{"Func&&", "func"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = model.dialect.column_iteration_returns_result
                                                 ? std::optional<CppType>{"decltype(auto)"}
                                                 : std::nullopt,
                       .is_const = true},
        .template_parameters = "typename Func",
    }));
    children.new_lines(1)
        .add(AccessSpecifier{"private"})
        .new_lines(1)
        .add(raw("friend ml::soa_storage_detail::CompactViewOperations;\ntemplate <bool>\nfriend "
                 "struct " +
                 name + ";"))
        .new_lines(1)
        .append(adjacent({Member{"State*", "state_", RawExpr{""}},
                          Member{"size_type", "offset_", RawExpr{""}},
                          Member{"size_type", "count_", RawExpr{""}}}));
    return Struct{.name = name,
                  .children = children.build(),
                  .bases = {CppType{"ml::soa_storage_detail::CompactViewOperations"}},
                  .template_parameters = "bool Const"};
}

auto view_validation_nodes(std::string const& name) -> Nodes {
    return {StaticAssert{"ml::soa_storage_detail::validate_compact_view<" + name + ">()", {}}};
}

} // namespace

auto emit_single_allocation_layout(SingleAllocationModel const& model) -> Nodes {
    auto const& dialect{model.dialect};
    NodeListBuilder children;
    children
        .append(adjacent({UsingDeclaration{"size_type", CppType{dialect.size_type}},
                          UsingDeclaration{"byte_size_type", CppType{dialect.byte_size_type}}}))
        .new_lines(2)
        .append(adjacent({
            Member{"size_type",
                   "capacity_granularity",
                   named(dialect.runtime_namespace + "LayoutPolicy::capacity_granularity"),
                   {.is_inline = true, .is_static = true, .is_constexpr = true}},
            Member{"byte_size_type",
                   "column_gap",
                   named(dialect.runtime_namespace + "LayoutPolicy::column_gap"),
                   {.is_inline = true, .is_static = true, .is_constexpr = true}},
        }))
        .new_lines(2)
        .add(raw("template <typename T>\nusing ColLayout = " + dialect.runtime_namespace +
                 "ColumnLayout<T>;"))
        .new_lines(1)
        .add(Member{dialect.runtime_namespace + "ColumnLayoutStart",
                    "LayoutStart",
                    RawExpr{""},
                    {.is_inline = true, .is_static = true, .is_constexpr = true}})
        .new_lines(2);

    std::string previous_column{"LayoutStart"};
    auto const column_count{model.columns.size()};
    for (std::size_t index{}; index < column_count; ++index) {
        auto const& column{model.columns[index]};
        children.add(Member{"ColLayout<" + column.type.spelling + ">",
                            column.layout_identifier,
                            named(previous_column),
                            {.is_inline = true, .is_static = true, .is_constexpr = true}});
        if (index + 1 < column_count) {
            children.new_lines(1);
        }
        previous_column = column.layout_identifier;
    }
    children.new_lines(2)
        .add(Member{
            "byte_size_type",
            "allocation_alignment",
            member_access(named(model.columns.back().layout_identifier), "allocation_alignment"),
            {.is_inline = true, .is_static = true, .is_constexpr = true}})
        .new_lines(2)
        .add(LineComment{
            "Conservative per-block bound for checked capacity arithmetic; gaps do not scale "
            "with capacity."})
        .new_lines(1)
        .append(adjacent({
            Member{"byte_size_type",
                   "capacity_block_bound",
                   call(named(dialect.runtime_namespace + "capacity_block_bound"),
                        {named(model.columns.back().layout_identifier)}),
                   {.is_inline = true, .is_static = true, .is_constexpr = true}},
            Member{"size_type",
                   "max_capacity",
                   call(named(dialect.runtime_namespace + "maximum_capacity"),
                        {named("capacity_block_bound")}),
                   {.is_inline = true, .is_static = true, .is_constexpr = true}},
            inline_function(FunctionSpec{
                .name = "layout_bytes",
                .return_type = "auto",
                .parameters = {FunctionParameter{"byte_size_type", "blocks"}},
                .body = {ReturnStmt{
                    RawExpr{"blocks == 0 ? 0 : " + model.columns.back().layout_identifier +
                            ".data_end(blocks)"}}},
                .qualifiers = {.trailing_return_type = CppType{"byte_size_type"},
                               .is_noexcept = true},
                .is_static = true,
                .is_constexpr = true,
            }),
        }))
        .new_lines(1)
        .add(StaticAssert{"max_capacity >= capacity_granularity", {}});

    return adjacent({ForwardDeclaration{model.view_name},
                     ForwardDeclaration{model.const_view_name},
                     Struct{.name = model.layout_name, .children = children.build()}});
}

auto storage_implementation_nodes(SingleAllocationModel const& model) -> Nodes {
    auto copying{column_copying_nodes(model)};
    NodeListBuilder children;
    children
        .append(adjacent({UsingDeclaration{"View", CppType{model.view_name}},
                          UsingDeclaration{"ConstView", CppType{model.const_view_name}}}))
        .new_lines(1)
        .append(storage_lifetime_nodes(model))
        .new_lines(1)
        .add(AccessSpecifier{"protected"})
        .new_lines(1)
        .append(storage_access_nodes(model))
        .new_lines(1)
        .add(AccessSpecifier{"private"})
        .new_lines(1)
        .add(Member{"std::pmr::memory_resource*", "resource_", RawExpr{""}})
        .new_lines(1)
        .append(column_pointer_nodes(model))
        .new_lines(2)
        .append(section_header("Typed mutations and growth"))
        .new_lines(1)
        .add(default_construction_node(model))
        .new_lines(1)
        .append(std::move(copying))
        .new_lines(1)
        .add(source_pointers_node(model))
        .new_lines(1)
        .add(source_copy_node(model, false))
        .new_lines(1)
        .add(source_copy_node(model, true))
        .new_lines(1)
        .add(live_column_copy_node());

    return children.build();
}

auto emit_single_allocation_views(SingleAllocationModel const& model, NodeListBuilder& source)
    -> Nodes {
    auto const implementation{model.view_name + "Impl"};
    auto wrapper = [&](bool const is_const) -> Node {
        auto const& name{is_const ? model.const_view_name : model.view_name};
        auto const base{implementation + (is_const ? "<true>" : "<false>")};
        NodeListBuilder children;
        children.append(
            adjacent({UsingDeclaration{"Base", CppType{base}}, raw("using Base::Base;")}));
        auto api{lower_soa_api(*model.schema,
                               *model.types,
                               SoaRepresentation::compact,
                               is_const ? SoaReceiver::const_view : SoaReceiver::mutable_view,
                               name,
                               model.schemas,
                               model.backend,
                               model.equivalent_constructors.at(model.schema->name))};
        children.new_lines(1).append(std::move(api.header));
        source.append(std::move(api.source)).new_lines(2);
        return Struct{.name = name,
                      .children = children.build(),
                      .bases = {CppType{base}},
                      .export_specifier = model.schema->export_specifier};
    };
    NodeListBuilder result;
    for (auto const& member : model.schema->members) {
        auto path{model.member_prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested && compact_vector_for(model, path) == nullptr) {
            auto const nested{nested_model(model, member)};
            result.add(ForwardDeclaration{nested.view_name})
                .new_lines(1)
                .add(ForwardDeclaration{nested.const_view_name})
                .new_lines(1)
                .append(emit_single_allocation_views(nested, source))
                .new_lines(1);
        }
    }
    result.add(compact_view_node(model))
        .new_lines(1)
        .add(wrapper(true))
        .new_lines(1)
        .append(view_validation_nodes(model.const_view_name))
        .new_lines(1)
        .add(wrapper(false))
        .new_lines(1)
        .append(view_validation_nodes(model.view_name));
    return result.build();
}

auto emit_single_allocation_container(SingleAllocationModel const& model, NodeListBuilder& source)
    -> Node {
    NodeListBuilder children;
    auto const operations{model.dialect.runtime_namespace + "StorageOperations"};
    children.add(UsingDeclaration{"soa_schema", CppType{model.schema->name + "Schema"}})
        .new_lines(1);
    children.add(raw("using Operations = " + operations + ";")).new_lines(1);
    children
        .add(raw("using Layout = " + model.layout_name +
                 ";\n"
                 "using size_type = Layout::size_type;\n"
                 "using byte_size_type = Layout::byte_size_type;\n"
                 "inline static constexpr auto capacity_granularity = "
                 "Layout::capacity_granularity;\n"
                 "inline static constexpr auto allocation_alignment = "
                 "Layout::allocation_alignment;\n"
                 "inline static constexpr auto capacity_block_bound = "
                 "Layout::capacity_block_bound;\n"
                 "inline static constexpr auto max_capacity = Layout::max_capacity;\n"
                 "static constexpr auto layout_bytes(byte_size_type blocks) noexcept "
                 "-> byte_size_type { return Layout::layout_bytes(blocks); }"))
        .new_lines(1)
        .append(storage_implementation_nodes(model))
        .new_lines(1)
        .add(AccessSpecifier{"public"})
        .new_lines(1);

    auto api{lower_soa_api(*model.schema,
                           *model.types,
                           SoaRepresentation::compact,
                           SoaReceiver::owner,
                           model.owner_name,
                           model.schemas,
                           model.backend,
                           model.equivalent_constructors.at(model.schema->name))};
    children.new_lines(1).append(std::move(api.header));
    source.append(std::move(api.source)).new_lines(2);
    return Struct{
        .name = model.owner_name,
        .children = children.build(),
        .bases = {CppType{"protected " + model.dialect.runtime_namespace + "StorageState"},
                  CppType{operations}},
        .export_specifier = model.schema->export_specifier,
        .dependencies = model.dependencies,
    };
}

} // namespace codegen::detail
