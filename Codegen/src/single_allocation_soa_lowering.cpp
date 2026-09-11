#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace codegen::detail {

static auto column_name(std::string_view const id) -> std::string {
    std::string result;
    result.reserve(id.size());
    bool capitalize{true};
    for (auto const character : id) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        result.push_back(
            capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                       : character);
        capitalize = false;
    }
    return result;
}

static auto layout_column_name(std::string_view const id,
                               std::set<std::string> const& type_identifiers) -> std::string {
    auto result{column_name(id)};
    if (type_identifiers.contains(result)) {
        result += "Column";
    }
    return result;
}

static auto vector_element(SoaSchema const& schema,
                           std::map<std::string, CppType> const& leaves,
                           std::string const& prefix) -> std::string {
    auto const dimensions{schema.members.size()};
    if (dimensions != 2 && dimensions != 3) {
        return {};
    }
    std::string type;
    for (std::size_t index{}; index < dimensions; ++index) {
        auto const& member{schema.members[index]};
        if (member.kind != SoaMemberKind::array ||
            member.name != std::string(1, "xyz"[index]) + "s") {
            return {};
        }
        auto const& element{leaves.at(prefix + "_" + member.name).spelling};
        if (index == 0) {
            type = element;
        } else if (element != type) {
            return {};
        }
    }
    static std::set<std::string> const scalars{"float",
                                               "double",
                                               "int8",
                                               "uint8",
                                               "int16",
                                               "uint16",
                                               "int32",
                                               "uint32",
                                               "int64",
                                               "uint64",
                                               "std::int8_t",
                                               "std::uint8_t",
                                               "std::int16_t",
                                               "std::uint16_t",
                                               "std::int32_t",
                                               "std::uint32_t",
                                               "std::int64_t",
                                               "std::uint64_t"};
    return scalars.contains(type) ? type : std::string{};
}

static auto compact_columns_expression(SoaSchema const& schema,
                                       std::map<std::string, SoaSchema const*> const& schemas,
                                       std::map<std::string, CppType> const& leaves,
                                       std::set<std::string> const& type_identifiers,
                                       std::vector<std::string> const& prefix,
                                       std::string const& layout,
                                       bool native,
                                       bool is_const) -> Expr {
    auto const mutable_view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<Expr> values;
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            values.push_back(compact_columns_expression(*schemas.at(*member.nested_schema),
                                                        schemas,
                                                        leaves,
                                                        type_identifiers,
                                                        path,
                                                        layout,
                                                        native,
                                                        is_const));
        } else {
            auto const id{join(path, "_")};
            auto const& type{leaves.at(id)};
            auto const offset{
                call(member_access(named(layout + "::" + layout_column_name(id, type_identifiers)),
                                   "offset"),
                     {named("blocks")})};
            values.push_back(init_list(
                {call(named("column_data_unchecked<" + type.spelling + ">", type.dependencies),
                      {offset}),
                 native ? static_cast_expr("std::size_t", named("count_")) : named("count_")}));
        }
    }
    return init_list(std::move(values), CppType{is_const ? const_view : mutable_view});
}

static auto compact_view_nodes(SoaSchema const& schema,
                               std::map<std::string, SoaSchema const*> const& schemas,
                               std::map<std::string, CppType> const& leaves,
                               std::set<std::string> const& type_identifiers,
                               std::string const& layout,
                               std::string const& runtime,
                               bool native) -> Nodes {
    auto const view{schema.name + "SingleView"};
    auto const const_view{schema.name + "SingleConstView"};
    NodeListBuilder result;
    for (bool const is_const : {true, false}) {
        auto const name{is_const ? const_view : view};
        auto const base{runtime + "CompactViewState<" + (is_const ? "true" : "false") + ">"};
        NodeListBuilder members;
        members.add(UsingDeclaration{"Base", base}, 1)
            .add(raw("using Base::Base;"), 1)
            .add(UsingDeclaration{"View", view}, 1)
            .add(UsingDeclaration{"ConstView", const_view}, 1)
            .add(Function{FunctionSpec{
                     .name = name, .qualifiers = {.disposition = FunctionDisposition::defaulted}}},
                 1);
        if (is_const) {
            members.add(declaration(FunctionSpec{.name = name,
                                                 .parameters = {{view + " const&", "other"}}}),
                        1);
        }
        auto add_function = [&](std::string function,
                                CppType type,
                                std::vector<FunctionParameter> parameters,
                                Nodes body,
                                bool compact = false) {
            members.add(
                Function{FunctionSpec{
                    .name = std::move(function),
                    .return_type = "auto",
                    .parameters = std::move(parameters),
                    .body = std::move(body),
                    .qualifiers = {.trailing_return_type = std::move(type), .is_const = true},
                    .formatting = {.body_layout = compact
                                                    ? FunctionFormatting::BodyLayout::compact
                                                    : FunctionFormatting::BodyLayout::expanded}}},
                1);
        };
        add_function("get_const_view",
                     "ConstView",
                     {},
                     {ReturnStmt{unary(UnaryOperator::dereference, named("this"))}},
                     true);
        add_function("get_const_view",
                     "ConstView",
                     {{"size_type", "offset"}, {"size_type", "count"}},
                     {ReturnStmt{call(named("slice"), {named("offset"), named("count")})}},
                     true);
        auto columns_body = [&] {
            return Nodes{
                ExpressionStmt{call(named("validate"))},
                IfStmt{binary(BinaryOperator::logical_or,
                              unary(UnaryOperator::logical_not, named("state_")),
                              unary(UnaryOperator::logical_not,
                                    pointer_member_access(named("state_"), "data_"))),
                       Block{{ReturnStmt{init_list({})}}}},
                VariableDeclarationStmt{"auto const", "blocks", call(named("capacity_blocks"))}};
        };
        auto offset = [&](std::string const& id, Expr blocks) {
            return call(
                member_access(named(layout + "::" + layout_column_name(id, type_identifiers)),
                              "offset"),
                {std::move(blocks)});
        };
        auto add_columns = [&](SoaSchema const& target,
                               std::vector<std::string> const& prefix,
                               std::string function) {
            auto const type{is_const ? target.const_view_name.value_or(target.name + "ConstView")
                                     : target.view_name.value_or(target.name + "View")};
            auto body{columns_body()};
            body.push_back(ReturnStmt{compact_columns_expression(
                target, schemas, leaves, type_identifiers, prefix, layout, native, is_const)});
            add_function(std::move(function), type, {}, std::move(body));
        };
        for (auto const& member : schema.members) {
            if (member.kind == SoaMemberKind::nested) {
                auto const& nested{*schemas.at(*member.nested_schema)};
                auto const element{vector_element(nested, leaves, member.name)};
                if (element.empty()) {
                    add_columns(nested, {member.name}, "view_" + member.name);
                    continue;
                }
                auto const vector_type{std::string{native ? "ml::native_soa::" : "ml::soa::"} +
                                       "Vector" + std::to_string(nested.members.size()) +
                                       (is_const ? "ConstView<" : "View<") + element + ">"};
                auto body{columns_body()};
                body.push_back(VariableDeclarationStmt{
                    "auto const", "first", offset(member.name + "_xs", named("blocks"))});
                body.push_back(
                    VariableDeclarationStmt{"auto const",
                                            "stride",
                                            binary(BinaryOperator::subtract,
                                                   offset(member.name + "_ys", named("blocks")),
                                                   named("first"))});
                body.push_back(ReturnStmt{init_list(
                    {call(named("column_data_unchecked<" + element + ">"), {named("first")}),
                     named("stride"),
                     named("count_")})});
                add_function("view_" + member.name, vector_type, {}, std::move(body));
            } else {
                auto const& type{leaves.at(member.name)};
                CppType view_type{std::string{native ? "std::span<" : "TArrayView<"} +
                                      type.spelling + (is_const ? " const>" : ">"),
                                  type.dependencies};
                add_function(
                    member.name,
                    std::move(view_type),
                    {},
                    {ReturnStmt{init_list(
                        {call(named("column_data<" + type.spelling + ">", type.dependencies),
                              {offset(member.name, call(named("capacity_blocks")))}),
                         native ? static_cast_expr("std::size_t", named("count_"))
                                : named("count_")})}},
                    true);
            }
        }
        add_columns(schema, {}, "columns");
        auto const forward{
            call(named("std::forward<Func>", {{"std::forward", "utility", {}}}), {named("func")})};
        Nodes body;
        if (native) {
            body.push_back(ExpressionStmt{
                call(member_access(call(named("columns")), "each_column"), {forward})});
        } else {
            body.push_back(VariableDeclarationStmt{"auto", "arrays", call(named("columns"))});
            body.push_back(
                ReturnStmt{call(member_access(named("arrays"), "apply_arrays"), {forward})});
        }
        members.add(
            Function{FunctionSpec{
                .name = native ? "each_column" : "apply_arrays",
                .return_type = native ? "void" : "auto",
                .parameters = {{"Func&&", "func"}},
                .body = std::move(body),
                .qualifiers = {.trailing_return_type =
                                   native ? std::nullopt : std::optional<CppType>{"decltype(auto)"},
                               .is_const = true},
                .template_parameters = "typename Func",
                .formatting = {.body_layout = FunctionFormatting::BodyLayout::compact,
                               .template_placement =
                                   FunctionFormatting::TemplatePlacement::same_line}}},
            1);
        result.add(Struct{.name = name, .children = members.build(), .bases = {base}}, 1);
        result.add(StaticAssert{"sizeof(" + name + ") == 16", {}}, 1)
            .add(StaticAssert{"std::is_trivially_copyable_v<" + name + ">", {}}, 1);
    }
    result.add(Function{FunctionSpec{.name = const_view,
                                     .parameters = {{view + " const&", "other"}},
                                     .is_inline = true,
                                     .member_initializers = {{"Base", "other"}}},
                        const_view,
                        false,
                        true},
               1);
    return result.build();
}
auto lower_single_allocation_nodes(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   std::map<std::string, CppType> const& types,
                                   bool const native) -> Nodes {
    auto const* runtime{native ? "ml::native_soa::" : "ml::soa_storage::"};
    auto const* copy{native ? "std::memcpy" : "FMemory::Memcpy"};
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto const& name{*schema.single_allocation};
    auto const storage_name{name + "Storage"};
    auto const layout_name{schema.name + "SingleLayout"};
    auto const compact_view{schema.name + "SingleView"};
    std::vector<TypeDependency> dependencies;
    if (native) {
        dependencies.push_back({"single_allocation_storage", "native_soa/storage.h", {}});
    } else {
        dependencies.push_back(
            {"single_allocation_operations", "SandboxCore/single_allocation/operations.h", {}});
        dependencies.push_back(
            {"single_allocation_removal", "SandboxCore/single_allocation/removal.h", {}});
        dependencies.push_back(
            {"single_allocation_vector_views", "SandboxCore/single_allocation/vector_views.h", {}});
    }
    auto allocator_scope{std::string{runtime} + (native ? "" : "MimallocStorageAllocator::")};
    std::vector<TypeDependency> allocator_dependencies{
        {"single_allocation_allocator",
         native ? "native_soa/storage.h" : "SandboxCore/mimalloc_storage_allocator.h",
         {}}};
    std::vector<Expr> free_arguments{named("data_")};
    if (native) {
        free_arguments.push_back(named("allocation_alignment"));
    }
    if (schema.single_allocation_allocator) {
        auto const allocator{resolve_type(*schema.single_allocation_allocator, types)};
        allocator_scope = allocator.spelling + "::";
        allocator_dependencies = allocator.dependencies;
        free_arguments = {named("data_")};
    }
    auto const allocate{named(allocator_scope + "allocate", allocator_dependencies)};
    auto const free_data{
        call(named(allocator_scope + "free", allocator_dependencies), std::move(free_arguments))};
    std::set<std::string> names;
    std::map<std::string, std::string> type_ids;
    std::vector<FixedLeaf const*> unique_types;
    std::set<std::string> type_identifiers;
    for (auto const& leaf : layout.leaves) {
        if (type_ids.emplace(leaf.type.spelling, fixed_leaf_argument(leaf)).second) {
            unique_types.push_back(&leaf);
        }
        for (std::size_t start{}; start < leaf.type.spelling.size();) {
            if (auto const character{static_cast<unsigned char>(leaf.type.spelling[start])};
                !std::isalpha(character) && character != '_') {
                ++start;
                continue;
            }
            auto end{start + 1};
            while (end < leaf.type.spelling.size()) {
                auto const character{static_cast<unsigned char>(leaf.type.spelling[end])};
                if (!std::isalnum(character) && character != '_') {
                    break;
                }
                ++end;
            }
            type_identifiers.emplace(leaf.type.spelling.substr(start, end - start));
            start = end;
        }
    }
    std::ostringstream assertions;
    auto append_assertion = [&](std::string condition, std::string message = {}) {
        assertions << render(Node{StaticAssert{std::move(condition), std::move(message)}}) << '\n';
    };
    std::ostringstream out;
    std::ostringstream layout_output;
    out << "struct " << layout_name << " {\n"
        << "using size_type = " << (native ? "std::int32_t" : "int32")
        << ";\nusing byte_size_type = " << (native ? "std::size_t" : "SIZE_T") << ";\n\n"
        << "inline static constexpr byte_size_type "
           "max_allocation_size{std::numeric_limits<byte_size_type>::max()};\n"
        << "inline static constexpr size_type capacity_granularity{64};\n"
        << "inline static constexpr byte_size_type column_gap{192};\n\n"
        << "template <typename T>\nusing ColLayout = " << runtime << "ColumnLayout<T>;\n"
        << "inline static constexpr " << runtime
        << "ColumnLayoutStart LayoutStart{capacity_granularity, column_gap, 64};\n\n";

    std::set<std::string> column_names{"ColLayout", "LayoutStart", layout_name};
    std::vector<std::string> columns;
    std::string previous_column{"LayoutStart"};
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        if (id.find("__") != std::string::npos || id.back() == '_') {
            throw std::invalid_argument{
                "Single-allocation leaf would generate reserved identifiers: " + id};
        }
        if (!names.insert(id).second) {
            throw std::invalid_argument{"Single-allocation flattened leaf name collision: " + id};
        }
        auto const column{layout_column_name(id, type_identifiers)};
        if (!column_names.insert(column).second) {
            throw std::invalid_argument{"Single-allocation column name collision: " + column};
        }

        dependencies.insert(
            dependencies.end(), leaf.type.dependencies.begin(), leaf.type.dependencies.end());
        out << "inline static constexpr ColLayout<" << leaf.type.spelling << "> " << column << "{"
            << previous_column << "};\n";
        columns.push_back(column);
        previous_column = column;
    }
    out << "\ninline static constexpr byte_size_type allocation_alignment{" << runtime
        << "maximum_alignment(" << join(columns, ", ") << ")};\n\n";

    for (auto const* leaf : unique_types) {
        auto const& type{leaf->type.spelling};
        append_assertion(std::string{runtime} + "supported_leaf<" + type + ">",
                         "Single-allocation leaf " + join(leaf->path, ".") +
                             " requires a non-cv, trivially copyable/copy-constructible/"
                             "destructible, nothrow default-constructible object type.");
    }
    assertions << '\n';
    append_assertion("allocation_alignment <= std::numeric_limits<" +
                         std::string{native ? "std::uint32_t" : "uint32"} + ">::max()",
                     "Single-allocation alignment must fit the allocator's 32-bit alignment "
                     "argument.");
    for (std::size_t index{}; index < layout.leaves.size(); ++index) {
        auto const& type{layout.leaves[index].type.spelling};
        append_assertion("sizeof(" + type + ") <= (max_allocation_size - " + columns[index] +
                         ".block_offset) / capacity_granularity");
    }
    append_assertion(std::to_string(layout.leaves.size() - 1) +
                     " <= (max_allocation_size - " + runtime + "layout_align(" + columns.back() +
                     ".block_end, allocation_alignment)) / (column_gap + allocation_alignment - "
                     "1)");
    out << "// Conservative per-block bound for checked capacity arithmetic; gaps do not scale "
           "with capacity.\n";
    out << "inline static constexpr byte_size_type "
           "capacity_block_bound{"
        << runtime << "layout_align(" << columns.back() << ".block_end, allocation_alignment) + "
        << (layout.leaves.size() - 1) << " * (column_gap + allocation_alignment - 1)};\n"
        << "inline static constexpr size_type "
           "max_capacity{"
        << runtime << "maximum_capacity(capacity_block_bound)};\n"
        << "static constexpr auto layout_bytes(byte_size_type blocks) noexcept -> byte_size_type "
           "{\n"
        << "return blocks == 0 ? 0 : " << columns.back() << ".data_end(blocks);\n}\n"
        << "\nprivate:\ninline static constexpr auto validate_layout = []() consteval -> bool {\n"
        << assertions.str()
        << render(Node{StaticAssert{"max_capacity >= capacity_granularity", {}}})
        << "\nreturn true;\n};\n"
        << render(Node{StaticAssert{"validate_layout()", {}}}) << "\n};\n\n";
    layout_output << out.str();
    out.str({});
    if (!schema.single_allocation_allocator) {
        out << "struct " << compact_view << ";\nstruct " << schema.name << "SingleConstView;\n"
            << layout_output.str();
    }
    NodeListBuilder result;
    result.add(raw(out.str()));
    out.str({});
    NodeListBuilder storage;
    storage.add(UsingDeclaration{"View", compact_view}, 1)
        .add(UsingDeclaration{"ConstView", schema.name + "SingleConstView"}, 1)
        .add(raw("/* **************************************** */\n// Lifetime\n"
                 "/* **************************************** */"),
             1);
    storage.add(
        Function{FunctionSpec{
            .name = storage_name,
            .qualifiers = {.is_noexcept = true, .disposition = FunctionDisposition::defaulted}}},
        1);
    storage.add(Function{FunctionSpec{
                    .name = "~" + storage_name,
                    .body = {ExpressionStmt{free_data}},
                    .formatting = {.body_layout = FunctionFormatting::BodyLayout::compact}}},
                1);
    storage.add(Function{FunctionSpec{.name = storage_name,
                                      .parameters = {{storage_name + " const&", ""}},
                                      .qualifiers = {.disposition = FunctionDisposition::deleted}}},
                1);
    storage.add(
        Function{FunctionSpec{.name = "operator=",
                              .return_type = "auto",
                              .parameters = {{storage_name + " const&", ""}},
                              .qualifiers = {.trailing_return_type = CppType{storage_name + "&"},
                                             .disposition = FunctionDisposition::deleted}}},
        1);
    storage.add(Function{FunctionSpec{
                    .name = storage_name,
                    .parameters = {{storage_name + "&&", "other"}},
                    .qualifiers = {.is_noexcept = true},
                    .member_initializers =
                        {{"StorageState",
                          "std::exchange(other.data_, nullptr), std::exchange(other.num_, 0), "
                          "std::exchange(other.capacity_, 0)"}}}},
                1);
    auto exchange = [](std::string member, Expr replacement) {
        return call(named("std::exchange", {{"std::exchange", "utility", {}}}),
                    {member_access(named("other"), std::move(member)), std::move(replacement)});
    };
    storage.add(
        Function{FunctionSpec{
            .name = "operator=",
            .return_type = "auto",
            .parameters = {{storage_name + "&&", "other"}},
            .body = {IfStmt{binary(BinaryOperator::not_equal,
                                   named("this"),
                                   unary(UnaryOperator::address_of, named("other"))),
                            Block{{ExpressionStmt{free_data},
                                   AssignmentStmt{named("data_"),
                                                  exchange("data_", literal("nullptr"))},
                                   AssignmentStmt{named("num_"), exchange("num_", literal("0"))},
                                   AssignmentStmt{named("capacity_"),
                                                  exchange("capacity_", literal("0"))}}}},
                     ReturnStmt{unary(UnaryOperator::dereference, named("this"))}},
            .qualifiers = {.trailing_return_type = CppType{storage_name + "&"},
                           .is_noexcept = true}}},
        2);
    storage.add(AccessSpecifier{"protected"}, 1);
    NodeListBuilder pointers;
    pointers.add(raw("template <typename T> using Element = "
                     "std::conditional_t<std::is_const_v<Byte>, T const, T>;"),
                 1);
    std::vector<Expr> shifted;
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        pointers.add(Member{CppType{"Element<" + leaf.type.spelling + ">*", leaf.type.dependencies},
                            id,
                            RawExpr{""}},
                     1);
        shifted.push_back(binary(BinaryOperator::add, named(id), named("offset")));
    }
    pointers.add(Function{FunctionSpec{
                     .name = "operator+",
                     .return_type = "auto",
                     .parameters = {{"size_type const", "offset"}},
                     .body = {IfStmt{binary(BinaryOperator::equal,
                                            named(fixed_leaf_argument(layout.leaves.front())),
                                            literal("nullptr")),
                                     Block{{ReturnStmt{init_list({})}}}},
                              ReturnStmt{init_list(std::move(shifted))}},
                     .qualifiers = {.trailing_return_type = CppType{"DataPointers"},
                                    .is_const = true,
                                    .is_noexcept = true}}},
                 1);
    storage.add(Struct{.name = "DataPointers",
                       .children = pointers.build(),
                       .template_parameters = "typename Byte"},
                1);
    storage.add(
        Function{FunctionSpec{
            .name = "get_data",
            .return_type = "auto",
            .parameters = {{"this Self&", "self"}},
            .body = {UsingDeclaration{
                         "Byte",
                         "std::conditional_t<std::is_const_v<Self>, std::byte const, std::byte>"},
                     IfStmt{binary(BinaryOperator::equal,
                                   member_access(named("self"), "data_"),
                                   literal("nullptr")),
                            Block{{ReturnStmt{init_list({}, CppType{"DataPointers<Byte>"})}}}},
                     ReturnStmt{
                         call(named("make_data_unchecked"),
                              {static_cast_expr("Byte*", member_access(named("self"), "data_")),
                               call(member_access(named("self"), "capacity_blocks"))})}},
            .qualifiers = {.is_noexcept = true},
            .template_parameters = "typename Self",
            .formatting = {.template_placement =
                               FunctionFormatting::TemplatePlacement::same_line}}},
        1);
    storage.add(Function{FunctionSpec{
                    .name = "get_data",
                    .return_type = "auto",
                    .parameters = {{"this Self&", "self"}, {"size_type const", "offset"}},
                    .body = {ReturnStmt{binary(BinaryOperator::add,
                                               call(member_access(named("self"), "get_data")),
                                               named("offset"))}},
                    .qualifiers = {.is_noexcept = true},
                    .template_parameters = "typename Self",
                    .formatting = {.template_placement =
                                       FunctionFormatting::TemplatePlacement::same_line}}},
                2);
    storage.add(AccessSpecifier{"private"}, 1)
        .add(FriendDeclaration{std::string{runtime} + "StorageOperations", "struct"}, 1)
        .add(raw("/* **************************************** */\n// Column pointers\n"
                 "/* **************************************** */"),
             1);
    NodeListBuilder pointer_body;
    pointer_body.add(raw("auto const pointer_at = [data, blocks](auto const& column) noexcept {\n"
                         "    using Column = std::remove_cvref_t<decltype(column)>;\n"
                         "    using Pointer = std::conditional_t<std::is_const_v<Byte>,\n"
                         "                                       typename Column::const_pointer,\n"
                         "                                       typename Column::pointer>;\n"
                         "    return std::launder(\n"
                         "        reinterpret_cast<Pointer>(data + column.offset(blocks)));\n"
                         "};"),
                     1);
    std::vector<Expr> values;
    for (auto const& leaf : layout.leaves) {
        values.push_back(
            call(named("pointer_at"),
                 {named(layout_column_name(fixed_leaf_argument(leaf), type_identifiers))}));
    }
    pointer_body.add(ReturnStmt{init_list(std::move(values))});
    storage.add(Function{FunctionSpec{
                    .name = "make_data_unchecked",
                    .return_type = "auto",
                    .parameters = {{"Byte* const", "data"}, {"byte_size_type const", "blocks"}},
                    .body = pointer_body.build(),
                    .qualifiers = {.trailing_return_type = CppType{"DataPointers<Byte>"},
                                   .is_noexcept = true},
                    .is_static = true,
                    .template_parameters = "typename Byte",
                    .formatting = {.template_placement =
                                       FunctionFormatting::TemplatePlacement::same_line}}},
                1);
    storage.add(Function{FunctionSpec{
                    .name = "capacity_blocks",
                    .return_type = "auto",
                    .body = {ReturnStmt{static_cast_expr("byte_size_type",
                                                         binary(BinaryOperator::divide,
                                                                named("capacity_"),
                                                                named("capacity_granularity")))}},
                    .qualifiers = {.trailing_return_type = CppType{"byte_size_type"},
                                   .is_const = true,
                                   .is_noexcept = true},
                    .formatting = {.body_layout = FunctionFormatting::BodyLayout::compact}}},
                2);
    storage.add(
        raw("/* **************************************** */\n// Typed mutations and growth\n"
            "/* **************************************** */"),
        1);
    NodeListBuilder construct_body;
    construct_body.add(VariableDeclarationStmt{
        "auto const",
        "columns",
        binary(BinaryOperator::add,
               call(named("make_data_unchecked"), {named("data_"), call(named("capacity_blocks"))}),
               named("first"))});
    for (auto const& leaf : layout.leaves) {
        auto const function{std::string{native ? "std::uninitialized_value_construct_n<"
                                               : "DefaultConstructItems<"} +
                            leaf.type.spelling + (native ? "*>" : ">")};
        auto function_dependencies{leaf.type.dependencies};
        function_dependencies.push_back(
            {function, native ? "memory" : "Templates/MemoryOps.h", {}});
        construct_body.add(ExpressionStmt{
            call(named(function, std::move(function_dependencies)),
                 {member_access(named("columns"), fixed_leaf_argument(leaf)), named("count")})});
    }
    storage.add(Function{FunctionSpec{
                    .name = "default_construct_columns",
                    .return_type = "void",
                    .parameters = {{"size_type const", "first"}, {"size_type const", "count"}},
                    .body = construct_body.build()}},
                1);
    storage.add(Function{FunctionSpec{.name = "swap_remove_columns",
                                      .return_type = "void",
                                      .parameters = {{"size_type const", "index"},
                                                     {"size_type const", "source"},
                                                     {"size_type const", "move_count"}},
                                      .body = {ExpressionStmt{call(named("copy_columns"),
                                                                   {call(named("get_data")),
                                                                    named("index"),
                                                                    named("source"),
                                                                    named("move_count")})}}}},
                1);
    auto copy_function = [&] {
        return named(copy, {{copy, native ? "cstring" : "HAL/UnrealMemory.h", {}}});
    };
    auto copy_sizes = [&](NodeListBuilder& body, std::string const& count) {
        for (auto const* leaf : unique_types) {
            body.add(VariableDeclarationStmt{
                "auto const",
                fixed_leaf_argument(*leaf) + "_bytes",
                binary(BinaryOperator::multiply, named(count), sizeof_type(leaf->type))});
        }
    };
    NodeListBuilder copy_body;
    copy_body.add(VariableDeclarationStmt{
        "auto const", "elements_to_move", static_cast_expr("byte_size_type", named("move_count"))});
    copy_sizes(copy_body, "elements_to_move");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        auto const column{member_access(named("columns"), id)};
        copy_body.add(ExpressionStmt{call(copy_function(),
                                          {binary(BinaryOperator::add, column, named("index")),
                                           binary(BinaryOperator::add, column, named("source")),
                                           named(type_ids.at(leaf.type.spelling) + "_bytes")})});
    }
    storage.add(Function{FunctionSpec{.name = "copy_columns",
                                      .return_type = "void",
                                      .parameters = {{"DataPointers<std::byte> const&", "columns"},
                                                     {"size_type", "index"},
                                                     {"size_type", "source"},
                                                     {"size_type", "move_count"}},
                                      .body = copy_body.build(),
                                      .is_static = true}},
                1);
    storage.add(
        Function{FunctionSpec{
            .name = "swap_remove_indices",
            .return_type = "void",
            .parameters = {{"std::span<size_type const>", "indices"}},
            .body = {VariableDeclarationStmt{"auto const", "columns", call(named("get_data"))},
                     raw(std::string{
                             "ml::soa_storage_detail::for_each_removal_run(num_, indices, "} +
                         runtime +
                         "require, [&](size_type index, size_type source, size_type count) { "
                         "copy_columns(columns, index, source, count); });")}}},
        1);
    NodeListBuilder append_body;
    append_body.add(VariableDeclarationStmt{
        "auto const", "destination", call(named("get_data"), {named("first")})});
    append_body.add(VariableDeclarationStmt{
        "auto const", "elements_to_copy", static_cast_expr("byte_size_type", named("count"))});
    copy_sizes(append_body, "elements_to_copy");
    for (auto const& leaf : layout.leaves) {
        auto source{named("source")};
        for (auto const& member : leaf.path) {
            source = member_access(std::move(source), member);
        }
        append_body.add(ExpressionStmt{
            call(copy_function(),
                 {member_access(named("destination"), fixed_leaf_argument(leaf)),
                  call(member_access(std::move(source), native ? "data" : "GetData")),
                  named(type_ids.at(leaf.type.spelling) + "_bytes")})});
    }
    storage.add(Function{FunctionSpec{
                    .name = "append_columns",
                    .return_type = "void",
                    .parameters = {{"Columns const&", "source"},
                                   {"size_type", "first"},
                                   {"size_type", "count"}},
                    .body = append_body.build(),
                    .template_parameters = "typename Columns",
                    .formatting = {.template_placement =
                                       FunctionFormatting::TemplatePlacement::same_line}}},
                1);
    NodeListBuilder reallocate_body;
    reallocate_body.add(VariableDeclarationStmt{
        "auto* const",
        "new_data",
        call(allocate,
             {call(named("layout_bytes"),
                   {static_cast_expr("byte_size_type",
                                     binary(BinaryOperator::divide,
                                            named("new_capacity"),
                                            named("capacity_granularity")))}),
              static_cast_expr(native ? "std::uint32_t" : "uint32",
                               named("allocation_alignment"))})});
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
    copy_live.add(VariableDeclarationStmt{
        "auto const", "live_count", static_cast_expr("byte_size_type", named("num_"))});
    copy_sizes(copy_live, "live_count");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        copy_live.add(ExpressionStmt{call(copy_function(),
                                          {member_access(named("destination"), id),
                                           member_access(named("source"), id),
                                           named(type_ids.at(leaf.type.spelling) + "_bytes")})});
    }
    reallocate_body.add(IfStmt{binary(BinaryOperator::greater, named("num_"), literal("0")),
                               Block{copy_live.build()}});
    reallocate_body.add(ExpressionStmt{free_data});
    reallocate_body.add(AssignmentStmt{named("data_"), named("new_data")});
    reallocate_body.add(AssignmentStmt{named("capacity_"), named("new_capacity")});
    storage.add(Function{FunctionSpec{.name = "reallocate",
                                      .return_type = "void",
                                      .parameters = {{"size_type const", "new_capacity"}},
                                      .body = reallocate_body.build()}},
                1);
    result.add(Struct{.name = storage_name,
                      .children = storage.build(),
                      .bases = {layout_name,
                                std::string{"protected "} + runtime + "StorageState",
                                std::string{runtime} + "StorageOperations"},
                      .dependencies = dependencies});
    if (!schema.single_allocation_allocator) {
        std::map<std::string, CppType> leaf_types;
        for (auto const& leaf : layout.leaves) {
            leaf_types.emplace(fixed_leaf_argument(leaf), leaf.type);
        }
        result.append(compact_view_nodes(
            schema, schemas, leaf_types, type_identifiers, layout_name, runtime, native));
    }
    NodeListBuilder owner;
    auto special_member = [&](std::string function,
                              CppType return_type,
                              std::vector<FunctionParameter> parameters,
                              FunctionDisposition disposition,
                              bool is_noexcept) {
        FunctionSpec spec{.name = std::move(function),
                          .return_type = std::move(return_type),
                          .parameters = std::move(parameters),
                          .qualifiers = {.is_noexcept = is_noexcept, .disposition = disposition}};
        if (spec.return_type.spelling == "auto") {
            spec.qualifiers.trailing_return_type = CppType{name + "&"};
        }
        owner.add(Function{std::move(spec)}, 1);
    };
    special_member(name, "", {}, FunctionDisposition::defaulted, true);
    special_member(name, "", {{name + " const&", ""}}, FunctionDisposition::deleted, false);
    special_member(
        "operator=", "auto", {{name + " const&", ""}}, FunctionDisposition::deleted, false);
    special_member(name, "", {{name + "&&", ""}}, FunctionDisposition::defaulted, true);
    special_member("operator=", "auto", {{name + "&&", ""}}, FunctionDisposition::defaulted, true);
    auto borrow_function = [&](std::string function,
                               CppType type,
                               std::vector<FunctionParameter> parameters,
                               Expr value,
                               bool is_const) {
        FunctionSpec spec{.name = std::move(function),
                          .return_type = "auto",
                          .parameters = std::move(parameters),
                          .body = {ReturnStmt{std::move(value)}},
                          .qualifiers = {.trailing_return_type = std::move(type),
                                         .is_const = is_const,
                                         .ref_qualifier = RefQualifier::lvalue},
                          .formatting = {.body_layout = FunctionFormatting::BodyLayout::compact}};
        return spec;
    };
    for (bool const is_const : {false, true}) {
        auto const view{is_const ? "ConstView" : "View"};
        std::vector<FunctionSpec> functions;
        functions.push_back(
            borrow_function("get_view",
                            view,
                            {},
                            init_list({named("this"), literal("0"), call(named("num"))}),
                            is_const));
        functions.push_back(
            borrow_function("get_view",
                            view,
                            {{"size_type", "offset"}, {"size_type", "count"}},
                            init_list({named("this"), named("offset"), named("count")}),
                            is_const));
        functions.push_back(
            borrow_function("slice",
                            view,
                            {{"size_type", "offset"}, {"size_type", "count"}},
                            call(named("get_view"), {named("offset"), named("count")}),
                            is_const));
        for (auto const* function : {"left", "right"}) {
            functions.push_back(borrow_function(
                function,
                view,
                {{"size_type", "count"}},
                call(member_access(call(named("get_view")), function), {named("count")}),
                is_const));
        }
        for (auto const& function : functions) {
            owner.add(Function{function}, 1);
        }
        for (auto& function : functions) {
            function.body.clear();
            function.qualifiers.ref_qualifier = RefQualifier::rvalue;
            function.qualifiers.disposition = FunctionDisposition::deleted;
            for (auto& parameter : function.parameters) {
                parameter.name.clear();
            }
            owner.add(Function{std::move(function)}, 1);
        }
    }
    auto const_view{
        borrow_function("get_const_view", "ConstView", {}, call(named("get_view")), true)};
    auto const_slice{borrow_function("get_const_view",
                                     "ConstView",
                                     {{"size_type", "offset"}, {"size_type", "count"}},
                                     call(named("get_view"), {named("offset"), named("count")}),
                                     true)};
    owner.add(Function{const_view}, 1).add(Function{const_slice}, 1);
    for (auto* function : {&const_view, &const_slice}) {
        function->body.clear();
        function->qualifiers.ref_qualifier = RefQualifier::rvalue;
        function->qualifiers.disposition = FunctionDisposition::deleted;
        for (auto& parameter : function->parameters) {
            parameter.name.clear();
        }
        owner.add(Function{std::move(*function)}, 1);
    }
    result.add(Struct{.name = name, .children = owner.build(), .bases = {storage_name}});
    return result.build();
}

} // namespace codegen::detail
