#include "lowering_utils.h"
#include "single_allocation_soa_internal.h"

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

auto free_data_expression(SingleAllocationModel const& model) -> Expr {
    std::vector<Expr> arguments{named("data_")};
    if (model.free_requires_alignment) {
        arguments.push_back(named("allocation_alignment"));
    }
    return call(named(model.free_function.spelling, model.free_function.dependencies),
                std::move(arguments));
}

auto copy_function(SingleAllocationModel const& model) -> Expr {
    return named(model.dialect.runtime_namespace + "copy_n");
}

auto source_member_expression(SingleAllocationColumn const& column) -> Expr {
    auto result{named("source")};
    for (auto const& member : column.member_path) {
        result = member_access(std::move(result), member);
    }
    return result;
}

auto source_data_expression(SingleAllocationModel const& model,
                            SingleAllocationColumn const& column) -> Expr {
    return call(named(model.dialect.runtime_namespace + "source_data"),
                {source_member_expression(column)});
}

auto storage_lifetime_nodes(SingleAllocationModel const& model) -> Nodes {
    auto const free_data{free_data_expression(model)};
    auto default_constructor{FunctionSpec{
        .name = model.owner_name,
        .qualifiers = {.is_noexcept = true, .disposition = FunctionDisposition::defaulted},
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
        .qualifiers = {.is_noexcept = true},
        .member_initializers =
            {{"StorageState",
              "std::exchange(other.data_, nullptr), std::exchange(other.num_, 0), "
              "std::exchange(other.capacity_, 0)"}},
    }};
    auto exchange = [](std::string member, Expr replacement) {
        return call(named("std::exchange", {{"std::exchange", "utility", {}}}),
                    {member_access(named("other"), std::move(member)), std::move(replacement)});
    };
    auto move_assignment{FunctionSpec{
        .name = "operator=",
        .return_type = "auto",
        .parameters = {FunctionParameter{model.owner_name + "&&", "other"}},
        .body = {IfStmt{
                     binary(BinaryOperator::not_equal,
                            named("this"),
                            unary(UnaryOperator::address_of, named("other"))),
                     Block{{ExpressionStmt{free_data},
                            AssignmentStmt{named("data_"), exchange("data_", literal("nullptr"))},
                            AssignmentStmt{named("num_"), exchange("num_", literal("0"))},
                            AssignmentStmt{named("capacity_"),
                                           exchange("capacity_", literal("0"))}}}},
                 ReturnStmt{unary(UnaryOperator::dereference, named("this"))}},
        .qualifiers = {.trailing_return_type = CppType{model.owner_name + "&"},
                       .is_noexcept = true},
    }};

    NodeListBuilder result;
    result.append(section_header("Lifetime"))
        .new_lines(1)
        .append(adjacent({inline_function(std::move(default_constructor)),
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
    make_data_body.add(
        raw("auto const pointer_at = [data](auto const& column, byte_size_type offset) noexcept {\n"
            "    using Column = std::remove_cvref_t<decltype(column)>;\n"
            "    using Pointer = std::conditional_t<std::is_const_v<Byte>,\n"
            "                                       typename Column::const_pointer,\n"
            "                                       typename Column::pointer>;\n"
            "    return std::launder(reinterpret_cast<Pointer>(data + offset));\n"
            "};"));
    std::vector<Expr> pointers;
    pointers.reserve(model.columns.size());
    for (auto const& column : model.columns) {
        pointers.push_back(call(named("pointer_at"),
                                {named("Layout::" + column.layout_identifier),
                                 call(member_access(named("cursor"), "advance"),
                                      {named("Layout::" + column.layout_identifier)})}));
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
    result.add(FriendDeclaration{model.dialect.runtime_namespace + "StorageOperations", "struct"})
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
    for (auto const& column : model.columns) {
        auto const pointer{member_access(named("columns"), column.flattened_identifier)};
        copy_body.add(ExpressionStmt{call(copy_function(model),
                                          {binary(BinaryOperator::add, pointer, named("index")),
                                           binary(BinaryOperator::add, pointer, named("source")),
                                           named("move_count")})});
    }
    auto copy{inline_function(FunctionSpec{
        .name = "copy_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"DataPointers<std::byte> const&", "columns"},
                       FunctionParameter{"size_type", "index"},
                       FunctionParameter{"size_type", "source"},
                       FunctionParameter{"size_type", "move_count"}},
        .body = copy_body.build(),
        .is_static = true,
    })};

    auto remove_indices{inline_function(FunctionSpec{
        .name = "swap_remove_indices",
        .return_type = "void",
        .parameters = {FunctionParameter{"std::span<size_type const>", "indices"}},
        .body = {VariableDeclarationStmt{"auto const", "columns", call(named("get_data"))},
                 ExpressionStmt{
                     RawExpr{"ml::soa_storage_detail::for_each_removal_run(num_, indices, " +
                             model.dialect.runtime_namespace +
                             "require, [&](size_type index, size_type source, size_type count) { "
                             "copy_columns(columns, index, source, count); })"}}},
    })};
    return adjacent({std::move(swap_remove), std::move(copy), std::move(remove_indices)});
}

auto append_node(SingleAllocationModel const& model) -> Node {
    NodeListBuilder body;
    body.add(VariableDeclarationStmt{
        "auto const", "destination", call(named("get_data"), {named("first")})});
    for (auto const& column : model.columns) {
        body.add(
            ExpressionStmt{call(copy_function(model),
                                {member_access(named("destination"), column.flattened_identifier),
                                 source_data_expression(model, column),
                                 named("count")})});
    }
    return inline_function(FunctionSpec{
        .name = "append_columns",
        .return_type = "void",
        .parameters = {FunctionParameter{"Columns const&", "source"},
                       FunctionParameter{"size_type", "first"},
                       FunctionParameter{"size_type", "count"}},
        .body = body.build(),
        .template_parameters = "typename Columns",
    });
}

auto ordinary_source_alias_node(SingleAllocationModel const& model) -> Node {
    NodeListBuilder body;
    body.add(IfStmt{binary(BinaryOperator::equal, named("data_"), literal("nullptr")),
                    Block{{ReturnStmt{literal("false")}}}})
        .add(raw("auto const allocation_begin{reinterpret_cast<std::uintptr_t>(data_)};"))
        .add(VariableDeclarationStmt{
            "auto const",
            "allocation_end",
            binary(BinaryOperator::add,
                   named("allocation_begin"),
                   call(named("layout_bytes"), {call(named("capacity_blocks"))}))})
        .add(raw("auto const aliases = [allocation_begin, allocation_end](auto const* pointer) "
                 "noexcept {\n"
                 "    auto const address{reinterpret_cast<std::uintptr_t>(pointer)};\n"
                 "    return address >= allocation_begin && address < allocation_end;\n"
                 "};"));

    body.add(ReturnStmt{call(named(model.dialect.runtime_namespace + "any_column"),
                             {named("source"), named("aliases")})});

    return inline_function(FunctionSpec{
        .name = "ordinary_source_aliases_storage",
        .return_type = "auto",
        .parameters = {FunctionParameter{model.schema_const_view_name + " const&", "source"}},
        .body = body.build(),
        .qualifiers = {.trailing_return_type = CppType{"bool"},
                       .is_const = true,
                       .is_noexcept = true},
    });
}

auto ordinary_append_node(SingleAllocationModel const& model) -> Node {
    auto const& runtime{model.dialect.runtime_namespace};
    NodeListBuilder growth;
    growth
        .add(ExpressionStmt{
            call(named(runtime + "require"),
                 {unary(UnaryOperator::logical_not,
                        call(named("ordinary_source_aliases_storage"), {named("source")}))})})
        .add(ExpressionStmt{
            call(named("reallocate"),
                 {call(named(runtime + "growth_capacity"),
                       {named("new_num"), named("capacity_"), named("capacity_block_bound")})})});

    return inline_function(FunctionSpec{
        .name = "append_from",
        .return_type = "auto",
        .parameters = {FunctionParameter{model.schema_const_view_name + " const&", "source"}},
        .body =
            {
                ExpressionStmt{call(member_access(named("source"), "validate_array_sizes"))},
                VariableDeclarationStmt{
                    "auto const", "count", call(member_access(named("source"), "num"))},
                VariableDeclarationStmt{"auto const", "first", named("num_")},
                ExpressionStmt{
                    call(named(runtime + "require"), {RawExpr{"count <= max_capacity - first"}})},
                IfStmt{binary(BinaryOperator::equal, named("count"), literal("0")),
                       Block{{ReturnStmt{named("first")}}}},
                VariableDeclarationStmt{
                    "auto const",
                    "new_num",
                    binary(BinaryOperator::add, named("first"), named("count"))},
                IfStmt{binary(BinaryOperator::greater, named("new_num"), named("capacity_")),
                       Block{growth.build()}},
                ExpressionStmt{call(named("append_columns"),
                                    {named("source"), named("first"), named("count")})},
                AssignmentStmt{named("num_"), named("new_num")},
                ReturnStmt{named("first")},
            },
        .qualifiers = {.trailing_return_type = CppType{"size_type"}},
    });
}

auto reallocation_node(SingleAllocationModel const& model) -> Node {
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
    for (auto const& column : model.columns) {
        copy_live.add(
            ExpressionStmt{call(copy_function(model),
                                {member_access(named("destination"), column.flattened_identifier),
                                 member_access(named("source"), column.flattened_identifier),
                                 named("num_")})});
    }

    return inline_function(FunctionSpec{
        .name = "reallocate",
        .return_type = "void",
        .parameters = {FunctionParameter{"size_type const", "new_capacity"}},
        .body = {VariableDeclarationStmt{
                     "auto* const",
                     "new_data",
                     call(named(model.allocate_function.spelling,
                                model.allocate_function.dependencies),
                          {call(named("layout_bytes"),
                                {static_cast_expr("byte_size_type",
                                                  binary(BinaryOperator::divide,
                                                         named("new_capacity"),
                                                         named("capacity_granularity")))}),
                           static_cast_expr(model.dialect.alignment_argument_type,
                                            named("allocation_alignment"))})},
                 IfStmt{binary(BinaryOperator::greater, named("num_"), literal("0")),
                        Block{copy_live.build()}},
                 ExpressionStmt{free_data_expression(model)},
                 AssignmentStmt{named("data_"), named("new_data")},
                 AssignmentStmt{named("capacity_"), named("new_capacity")}},
    });
}

auto compact_columns_expression(SingleAllocationModel const& model,
                                SoaSchema const& schema,
                                std::vector<std::string> const& prefix) -> Expr {
    auto const mutable_view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<Expr> values;
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            values.push_back(
                compact_columns_expression(model, *model.schemas->at(*member.nested_schema), path));
        } else {
            auto const& column{column_for(model, path)};
            auto const offset{call(
                member_access(named(model.layout_name + "::" + column.layout_identifier), "offset"),
                {named("blocks")})};
            values.push_back(init_list(
                {call(named("this->template column_data_unchecked<" + column.type.spelling + ">",
                            column.type.dependencies),
                      {offset}),
                 model.dialect.span_count(named("count_"))}));
        }
    }
    return init_list(
        std::move(values),
        CppType{"std::conditional_t<Const, " + const_view + ", " + mutable_view + ">"});
}

auto compact_columns_function(SingleAllocationModel const& model,
                              SoaSchema const& target,
                              std::vector<std::string> const& prefix,
                              std::string const& function) -> Node {
    auto const type{"std::conditional_t<Const, " +
                    target.const_view_name.value_or(target.name + "ConstView") + ", " +
                    target.view_name.value_or(target.name + "View") + ">"};
    return inline_function(FunctionSpec{
        .name = function,
        .return_type = "auto",
        .body = {ExpressionStmt{call(named("validate"))},
                 IfStmt{binary(BinaryOperator::logical_or,
                               unary(UnaryOperator::logical_not, named("state_")),
                               unary(UnaryOperator::logical_not,
                                     pointer_member_access(named("state_"), "data_"))),
                        Block{{ReturnStmt{init_list({})}}}},
                 VariableDeclarationStmt{"auto const", "blocks", call(named("capacity_blocks"))},
                 ReturnStmt{compact_columns_expression(model, target, prefix)}},
        .qualifiers = {.trailing_return_type = CppType{type}, .is_const = true},
    });
}

auto compact_view_node(SingleAllocationModel const& model) -> Node {
    auto const name{model.view_name + "Impl"};
    auto const base{model.dialect.runtime_namespace + "CompactViewState<Const>"};
    NodeListBuilder children;
    children
        .append(adjacent({UsingDeclaration{"Base", CppType{base}},
                          raw("using Base::Base;"),
                          raw("using Base::validate;\n"
                              "using size_type = typename Base::size_type;\n"
                              "template <typename T>\n"
                              "using Element = typename Base::template Element<T>;"),
                          UsingDeclaration{"View", CppType{model.view_name}},
                          UsingDeclaration{"ConstView", CppType{model.const_view_name}},
                          declaration(FunctionSpec{
                              .name = name,
                              .qualifiers = {.disposition = FunctionDisposition::defaulted},
                          }),
                          inline_function(FunctionSpec{
                              .name = name,
                              .parameters = {FunctionParameter{name + "<false> const&", "other"}},
                              .template_parameters = "bool Enabled = Const",
                              .requires_clause = "Enabled",
                              .member_initializers = {{"Base", "other"}},
                          })}))
        .new_lines(1)
        .add(AccessSpecifier{"protected"})
        .new_lines(1)
        .add(raw("using Base::state_;\n"
                 "using Base::count_;\n"
                 "using Base::capacity_blocks;\n"
                 "using Base::column_data;\n"
                 "using Base::column_data_unchecked;"))
        .new_lines(1)
        .add(AccessSpecifier{"public"});

    for (auto const& member : model.schema->members) {
        std::vector<std::string> const path{member.name};
        if (member.kind == SoaMemberKind::nested) {
            auto const& nested{*model.schemas->at(*member.nested_schema)};
            auto const* vector{compact_vector_for(model, path)};
            if (vector == nullptr) {
                children.new_lines(1).add(
                    compact_columns_function(model, nested, path, "view_" + member.name));
                continue;
            }

            auto xs_path{path};
            xs_path.push_back("xs");
            auto ys_path{path};
            ys_path.push_back("ys");
            auto const& first_column{column_for(model, xs_path)};
            auto const& second_column{column_for(model, ys_path)};
            auto const vector_prefix{model.dialect.vector_namespace + "Vector" +
                                     std::to_string(vector->dimensions)};
            auto const vector_type{"std::conditional_t<Const, " + vector_prefix + "ConstView<" +
                                   vector->element_type + ">, " + vector_prefix + "View<" +
                                   vector->element_type + ">>"};
            children.new_lines(1).add(inline_function(FunctionSpec{
                .name = "view_" + member.name,
                .return_type = "auto",
                .body = {ExpressionStmt{call(named("validate"))},
                         IfStmt{binary(BinaryOperator::logical_or,
                                       unary(UnaryOperator::logical_not, named("state_")),
                                       unary(UnaryOperator::logical_not,
                                             pointer_member_access(named("state_"), "data_"))),
                                Block{{ReturnStmt{init_list({})}}}},
                         VariableDeclarationStmt{
                             "auto const", "blocks", call(named("capacity_blocks"))},
                         VariableDeclarationStmt{
                             "auto const",
                             "first",
                             call(member_access(named(model.layout_name +
                                                      "::" + first_column.layout_identifier),
                                                "offset"),
                                  {named("blocks")})},
                         VariableDeclarationStmt{
                             "auto const",
                             "stride",
                             binary(BinaryOperator::subtract,
                                    call(member_access(named(model.layout_name + "::" +
                                                             second_column.layout_identifier),
                                                       "offset"),
                                         {named("blocks")}),
                                    named("first"))},
                         ReturnStmt{init_list({call(named("this->template column_data_unchecked<" +
                                                          vector->element_type + ">"),
                                                    {named("first")}),
                                               named("stride"),
                                               named("count_")})}},
                .qualifiers = {.trailing_return_type = CppType{vector_type}, .is_const = true},
            }));
        } else {
            auto const& column{column_for(model, path)};
            children.new_lines(1).add(compact_function(FunctionSpec{
                .name = member.name,
                .return_type = "auto",
                .body = {ReturnStmt{init_list(
                    {call(named("this->template column_data<" + column.type.spelling + ">",
                                column.type.dependencies),
                          {call(member_access(
                                    named(model.layout_name + "::" + column.layout_identifier),
                                    "offset"),
                                {call(named("capacity_blocks"))})}),
                     model.dialect.span_count(named("count_"))})}},
                .qualifiers = {.trailing_return_type =
                                   CppType{model.dialect.span_template + "<Element<" +
                                           column.type.spelling + ">>"},
                               .is_const = true},
            }));
        }
    }
    children.new_lines(1).add(compact_columns_function(model, *model.schema, {}, "columns"));
    auto const forward{
        call(named("std::forward<Func>", {{"std::forward", "utility", {}}}), {named("func")})};
    Nodes body;
    if (model.dialect.column_iteration_returns_result) {
        body.push_back(VariableDeclarationStmt{"auto", "arrays", call(named("columns"))});
        body.push_back(ReturnStmt{call(
            member_access(named("arrays"), model.dialect.column_application_function), {forward})});
    } else {
        body.push_back(ExpressionStmt{
            call(member_access(call(named("columns")), model.dialect.column_application_function),
                 {forward})});
    }
    children.new_lines(1).add(compact_function(FunctionSpec{
        .name = model.dialect.column_iteration_function,
        .return_type = model.dialect.column_iteration_returns_result ? "auto" : "void",
        .parameters = {FunctionParameter{"Func&&", "func"}},
        .body = std::move(body),
        .qualifiers = {.trailing_return_type = model.dialect.column_iteration_returns_result
                                                 ? std::optional<CppType>{"decltype(auto)"}
                                                 : std::nullopt,
                       .is_const = true},
        .template_parameters = "typename Func",
    }));
    return Struct{.name = name,
                  .children = children.build(),
                  .bases = {CppType{base}},
                  .template_parameters = "bool Const"};
}

auto view_validation_nodes(std::string const& name) -> Nodes {
    return adjacent({StaticAssert{"sizeof(" + name + ") == 16", {}},
                     StaticAssert{"std::is_trivially_copyable_v<" + name + ">", {}}});
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
        .add(StaticAssert{"allocation_alignment <= std::numeric_limits<" +
                              dialect.alignment_argument_type + ">::max()",
                          "Single-allocation alignment must fit the allocator's 32-bit alignment "
                          "argument."})
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
        .append(adjacent(
            {UsingDeclaration{"View", CppType{model.view_name}},
             UsingDeclaration{"ConstView", CppType{model.const_view_name}},
             UsingDeclaration{"SchemaConstView", CppType{model.schema_const_view_name}},
             raw("using " + model.dialect.runtime_namespace + "StorageOperations::append_from;"),
             ordinary_append_node(model)}))
        .new_lines(1)
        .append(storage_lifetime_nodes(model))
        .new_lines(1)
        .add(AccessSpecifier{"protected"})
        .new_lines(1)
        .append(storage_access_nodes(model))
        .new_lines(1)
        .add(AccessSpecifier{"private"})
        .new_lines(1)
        .append(column_pointer_nodes(model))
        .new_lines(2)
        .append(section_header("Typed mutations and growth"))
        .new_lines(1)
        .add(default_construction_node(model))
        .new_lines(1)
        .append(std::move(copying))
        .new_lines(1)
        .add(ordinary_source_alias_node(model))
        .new_lines(1)
        .add(append_node(model))
        .new_lines(1)
        .add(reallocation_node(model));

    return children.build();
}

auto emit_single_allocation_views(SingleAllocationModel const& model) -> Nodes {
    auto const implementation{model.view_name + "Impl"};
    auto wrapper = [&](bool const is_const) -> Node {
        auto const& name{is_const ? model.const_view_name : model.view_name};
        auto const base{implementation + (is_const ? "<true>" : "<false>")};
        NodeListBuilder children;
        children.append(adjacent({UsingDeclaration{"Base", CppType{base}},
                                  raw("using Base::Base;"),
                                  UsingDeclaration{"View", CppType{model.view_name}},
                                  UsingDeclaration{"ConstView", CppType{model.const_view_name}},
                                  declaration(FunctionSpec{
                                      .name = name,
                                      .qualifiers = {.disposition = FunctionDisposition::defaulted},
                                  })}));
        if (is_const) {
            children.new_lines(1).add(declaration(FunctionSpec{
                .name = name,
                .parameters = {FunctionParameter{model.view_name + " const&", "other"}},
            }));
        }
        children.new_lines(1).append(adjacent({
            compact_function(FunctionSpec{
                .name = "get_const_view",
                .return_type = "auto",
                .body = {ReturnStmt{unary(UnaryOperator::dereference, named("this"))}},
                .qualifiers = {.trailing_return_type = CppType{"ConstView"}, .is_const = true},
            }),
            compact_function(FunctionSpec{
                .name = "get_const_view",
                .return_type = "auto",
                .parameters = {FunctionParameter{"size_type", "offset"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{call(named("slice"), {named("offset"), named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ConstView"}, .is_const = true},
            }),
        }));
        return Struct{.name = name, .children = children.build(), .bases = {CppType{base}}};
    };
    NodeListBuilder result;
    result.add(compact_view_node(model))
        .new_lines(1)
        .add(wrapper(true))
        .new_lines(1)
        .append(view_validation_nodes(model.const_view_name))
        .new_lines(1)
        .add(wrapper(false))
        .new_lines(1)
        .append(view_validation_nodes(model.view_name))
        .new_lines(1)
        .add(raw("inline " + model.const_view_name + "::" + model.const_view_name + "(" +
                 model.view_name + " const& other) : Base{other} {}"));
    return result.build();
}

auto emit_single_allocation_container(SingleAllocationModel const& model) -> Node {
    NodeListBuilder children;
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

    children
        .add(raw("template <typename Self>\n"
                 "using ViewFor = std::conditional_t<std::is_const_v<"
                 "std::remove_reference_t<Self>>, ConstView, View>;"))
        .new_lines(1)
        .append(adjacent({
            compact_function(FunctionSpec{
                .name = "get_view",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"}},
                .body = {ReturnStmt{init_list({unary(UnaryOperator::address_of, named("self")),
                                               literal("0"),
                                               call(member_access(named("self"), "num"))})}},
                .qualifiers = {.trailing_return_type = CppType{"ViewFor<Self>"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "get_view",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"},
                               FunctionParameter{"size_type", "offset"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{init_list({unary(UnaryOperator::address_of, named("self")),
                                               named("offset"),
                                               named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ViewFor<Self>"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "slice",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"},
                               FunctionParameter{"size_type", "offset"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{call(member_access(named("self"), "get_view"),
                                         {named("offset"), named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ViewFor<Self>"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "left",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{
                    call(member_access(call(member_access(named("self"), "get_view")), "left"),
                         {named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ViewFor<Self>"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "right",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{
                    call(member_access(call(member_access(named("self"), "get_view")), "right"),
                         {named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ViewFor<Self>"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "get_const_view",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"}},
                .body = {ReturnStmt{init_list({unary(UnaryOperator::address_of, named("self")),
                                               literal("0"),
                                               call(member_access(named("self"), "num"))})}},
                .qualifiers = {.trailing_return_type = CppType{"ConstView"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
            compact_function(FunctionSpec{
                .name = "get_const_view",
                .return_type = "auto",
                .parameters = {FunctionParameter{"this Self&&", "self"},
                               FunctionParameter{"size_type", "offset"},
                               FunctionParameter{"size_type", "count"}},
                .body = {ReturnStmt{init_list({unary(UnaryOperator::address_of, named("self")),
                                               named("offset"),
                                               named("count")})}},
                .qualifiers = {.trailing_return_type = CppType{"ConstView"}},
                .template_parameters = "typename Self",
                .requires_clause = "std::is_lvalue_reference_v<Self>",
            }),
        }));
    return Struct{
        .name = model.owner_name,
        .children = children.build(),
        .bases = {CppType{"protected " + model.dialect.runtime_namespace + "StorageState"},
                  CppType{model.dialect.runtime_namespace + "StorageOperations"}},
        .dependencies = model.dependencies,
    };
}

} // namespace codegen::detail
