#include "lowering_utils.h"
#include "soa_internal.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};
TypeDependency const fixed_storage_dependency{
    "ml::TFixedStorage", "SandboxCore/fixed_storage.h", {}};
TypeDependency const move_temp{"MoveTemp", "Templates/UnrealTemplate.h", {}};
TypeDependency const std_memory{"std::addressof", "memory", {}};
TypeDependency const std_type_traits{"std::is_constructible_v", "type_traits", {}};

struct FixedLeaf {
    std::vector<std::string> path;
    CppType type;
};

struct FixedMemberLayout {
    SoaMemberSchema const* schema;
    ResolvedMember member;
    std::vector<FixedLeaf> leaves;
    std::optional<std::string> nested_storage_name;
};

struct FixedLayout {
    SoaSchema const* schema;
    std::vector<FixedMemberLayout> members;
    std::vector<FixedLeaf> leaves;
};

auto build_fixed_layout(SoaSchema const& schema,
                        std::map<std::string, SoaSchema const*> const& schemas,
                        std::map<std::string, CppType> const& types,
                        std::vector<std::string> prefix = {},
                        std::set<std::string> ancestors = {}) -> FixedLayout {
    if (!schema.fixed.has_value()) {
        throw std::invalid_argument{"SOA '" + schema.name + "' has no fixed configuration"};
    }
    if (!ancestors.insert(schema.name).second) {
        throw std::invalid_argument{"Fixed SOA schema cycle at '" + schema.name + "'"};
    }
    auto resolved{resolve_members(schema, types)};
    FixedLayout result{.schema = &schema};
    for (std::size_t index{0}; index < schema.members.size(); ++index) {
        auto const& member_schema{schema.members[index]};
        auto member_path{prefix};
        member_path.push_back(member_schema.name);
        FixedMemberLayout member{
            .schema = &member_schema,
            .member = resolved[index],
        };
        if (member_schema.kind == SoaMemberKind::array) {
            member.leaves.push_back(FixedLeaf{member_path, resolved[index].element_type});
        } else {
            if (!member_schema.fixed_schema.has_value()) {
                throw std::invalid_argument{"Fixed SOA '" + schema.name + "' nested member '" +
                                            member_schema.name + "' has no fixed_schema"};
            }
            auto const found{schemas.find(*member_schema.fixed_schema)};
            if (found == schemas.end() || !found->second->fixed.has_value()) {
                throw std::invalid_argument{"Unknown fixed nested schema: " +
                                            *member_schema.fixed_schema};
            }
            auto nested{build_fixed_layout(
                *found->second, schemas, types, member_path, ancestors)};
            member.leaves = nested.leaves;
            member.nested_storage_name = nested.schema->fixed->storage_name;
        }
        result.leaves.insert(result.leaves.end(), member.leaves.begin(), member.leaves.end());
        result.members.push_back(std::move(member));
    }
    return result;
}

auto leaf_argument(FixedLeaf const& leaf) -> std::string {
    return join(leaf.path, "_");
}

auto fixed_storage_type(FixedMemberLayout const& member) -> std::string {
    if (member.member.kind == SoaMemberKind::array) {
        return "ml::TFixedStorage<" + member.member.element_type.spelling + ", Capacity>";
    }
    return *member.nested_storage_name + "<Capacity>";
}

auto fixed_member_view(FixedMemberLayout const& member, bool is_const) -> std::string {
    if (member.member.kind == SoaMemberKind::array) {
        return std::string{is_const ? "TConstArrayView<" : "TArrayView<"} +
               member.member.element_type.spelling + ">{" + member.member.name +
               "_.data() + offset, count}";
    }
    return member.member.name + (is_const ? "_.get_const_view(offset, count)"
                                          : "_.get_view(offset, count)");
}

auto fixed_construct_lines(FixedLayout const& layout,
                           std::string const& operation,
                           std::vector<std::string> const& arguments = {})
    -> std::vector<std::string> {
    std::vector<std::string> result;
    std::size_t argument_index{0};
    for (auto const& member : layout.members) {
        auto const name{member.member.name + "_"};
        auto const leaf_count{member.leaves.size()};
        std::vector<std::string> member_arguments;
        for (std::size_t index{0}; index < leaf_count && argument_index < arguments.size(); ++index) {
            member_arguments.push_back(arguments[argument_index++]);
        }
        if (operation == "arguments") {
            result.push_back(name + ".construct_at(index, " +
                             join(member_arguments, ", ") + ");");
        } else if (operation == "default") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index);"
                                 : name + ".default_construct_at(index);");
        } else if (operation == "copy") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index, other." + name +
                                       "[other_index]);"
                                 : name + ".copy_construct_at(index, other." + name +
                                       ", other_index);");
        } else if (operation == "move") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index, MoveTemp(other." + name +
                                       "[other_index]));"
                                 : name + ".move_construct_at(index, other." + name +
                                       ", other_index);");
        } else if (operation == "view") {
            result.push_back(member.member.kind == SoaMemberKind::array
                                 ? name + ".construct_at(index, source." + member.member.name +
                                       "[source_index]);"
                                 : name + ".construct_from_view_at(index, source." +
                                       member.member.name + ", source_index);");
        }
    }
    return result;
}

auto fixed_storage_node(FixedLayout const& layout) -> Node {
    auto const& schema{*layout.schema};
    auto const& storage_name{schema.fixed->storage_name};
    std::vector<std::string> template_parameters;
    std::vector<FunctionParameter> function_parameters;
    std::vector<std::string> forwarded;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{leaf_argument(layout.leaves[index])};
        template_parameters.push_back("typename TArg" + std::to_string(index));
        function_parameters.emplace_back("TArg" + std::to_string(index) + "&&",
                                         "new_" + argument);
        forwarded.push_back("std::forward<TArg" + std::to_string(index) + ">(new_" +
                            argument + ")");
    }
    std::vector<std::string> mutable_views;
    std::vector<std::string> const_views;
    std::vector<std::string> copy_assign;
    std::vector<std::string> move_assign;
    for (auto const& member : layout.members) {
        mutable_views.push_back(fixed_member_view(member, false));
        const_views.push_back(fixed_member_view(member, true));
        auto const name{member.member.name + "_"};
        copy_assign.push_back(member.member.kind == SoaMemberKind::array
                                  ? name + "[dst_index] = source." + member.member.name +
                                        "[source_index];"
                                  : name + ".copy_assign_from_view_at(dst_index, source." +
                                        member.member.name + ", source_index);");
        move_assign.push_back(member.member.kind == SoaMemberKind::array
                                  ? name + "[dst_index] = MoveTemp(other." + name +
                                        "[source_index]);"
                                  : name + ".move_assign_at(dst_index, other." + name +
                                        ", source_index);");
    }
    std::vector<std::string> destroy;
    for (auto iterator{layout.members.rbegin()}; iterator != layout.members.rend(); ++iterator) {
        destroy.push_back(iterator->member.name + "_.destroy_at(index);");
    }
    auto function = [](std::string name,
                       CppType return_type,
                       std::vector<FunctionParameter> parameters,
                       std::string body,
                       std::string suffix = {},
                       std::vector<TypeDependency> dependencies = {},
                       std::optional<std::string> function_template = std::nullopt) {
        return header_function(FunctionSpec{
            .name = std::move(name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body), std::move(dependencies))},
            .suffix = std::move(suffix),
            .is_inline = true,
            .template_parameters = std::move(function_template),
        });
    };

    Nodes children{
        UsingDeclaration{"View", CppType{schema.view_name.value_or(schema.name + "View")}},
        lines(1),
        UsingDeclaration{"ConstView",
                         CppType{schema.const_view_name.value_or(schema.name + "ConstView")}},
        lines(2),
        function("get_view",
                 "auto",
                 {FunctionParameter{"int32 const", "offset"},
                  FunctionParameter{"int32 const", "count"}},
                 "return View{" + join(mutable_views, ", ") + "};",
                 " -> View",
                 {tarray_view}),
        lines(1),
        function("get_const_view",
                 "auto",
                 {FunctionParameter{"int32 const", "offset"},
                  FunctionParameter{"int32 const", "count"}},
                 "return ConstView{" + join(const_views, ", ") + "};",
                 " const -> ConstView",
                 {tarray_view}),
        lines(2),
        function("construct_at",
                 "void",
                 [&] {
                     auto parameters{function_parameters};
                     parameters.insert(parameters.begin(),
                                       FunctionParameter{"int32 const", "index"});
                     return parameters;
                 }(),
                 join_lines(fixed_construct_lines(layout, "arguments", forwarded)),
                 {},
                 {std_forward},
                 join(template_parameters, ", ")),
        lines(2),
        function("default_construct_at",
                 "void",
                 {FunctionParameter{"int32 const", "index"}},
                 join_lines(fixed_construct_lines(layout, "default"))),
        lines(2),
        function("copy_construct_at",
                 "void",
                 {FunctionParameter{"int32 const", "index"},
                  FunctionParameter{storage_name + " const&", "other"},
                  FunctionParameter{"int32 const", "other_index"}},
                 join_lines(fixed_construct_lines(layout, "copy"))),
        lines(2),
        function("move_construct_at",
                 "void",
                 {FunctionParameter{"int32 const", "index"},
                  FunctionParameter{storage_name + "&", "other"},
                  FunctionParameter{"int32 const", "other_index"}},
                 join_lines(fixed_construct_lines(layout, "move")),
                 {},
                 {move_temp}),
        lines(2),
        function("construct_from_view_at",
                 "void",
                 {FunctionParameter{"int32 const", "index"},
                  FunctionParameter{"SourceView const&", "source"},
                  FunctionParameter{"int32 const", "source_index"}},
                 join_lines(fixed_construct_lines(layout, "view")),
                 {},
                 {},
                 "typename SourceView"),
        lines(2),
        function("copy_assign_from_view_at",
                 "void",
                 {FunctionParameter{"int32 const", "dst_index"},
                  FunctionParameter{"SourceView const&", "source"},
                  FunctionParameter{"int32 const", "source_index"}},
                 join_lines(copy_assign),
                 {},
                 {},
                 "typename SourceView"),
        lines(2),
        function("move_assign_at",
                 "void",
                 {FunctionParameter{"int32 const", "dst_index"},
                  FunctionParameter{storage_name + "&", "other"},
                  FunctionParameter{"int32 const", "source_index"}},
                 join_lines(move_assign),
                 {},
                 {move_temp}),
        lines(2),
        function("destroy_at",
                 "void",
                 {FunctionParameter{"int32 const", "index"}},
                 join_lines(destroy),
                 " noexcept"),
        lines(2),
    };
    for (std::size_t index{0}; index < layout.members.size(); ++index) {
        auto const& member{layout.members[index]};
        auto dependencies{std::vector<TypeDependency>{fixed_storage_dependency}};
        dependencies.insert(dependencies.end(),
                            member.member.element_type.dependencies.begin(),
                            member.member.element_type.dependencies.end());
        children.push_back(Member{
            CppType{fixed_storage_type(member), std::move(dependencies)},
            member.member.name + "_",
        });
        if (index + 1 < layout.members.size()) {
            children.push_back(lines(1));
        }
    }

    return Struct{
        .name = storage_name,
        .children = std::move(children),
        .template_parameters = "int32 Capacity",
        .requires_clause = "(Capacity >= 0)",
    };
}

auto fixed_trait(FixedLayout const& layout, std::string const& trait) -> std::string {
    std::vector<std::string> values;
    for (auto const& leaf : layout.leaves) {
        values.push_back("std::" + trait + "<" + leaf.type.spelling + ">");
    }
    return join(values, " && ");
}

auto fixed_container_node(FixedLayout const& layout,
                          std::string const& name,
                          std::map<std::string, CppType> const& types) -> Node {
    auto const& schema{*layout.schema};
    std::vector<std::string> template_parameters;
    std::vector<FunctionParameter> function_parameters;
    std::vector<std::string> forwarded;
    std::vector<std::string> constructible_values;
    std::vector<std::string> trivial_values;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{leaf_argument(layout.leaves[index])};
        auto const index_text{std::to_string(index)};
        template_parameters.push_back("typename TArg" + index_text);
        function_parameters.emplace_back("TArg" + index_text + "&&", "new_" + argument);
        forwarded.push_back("std::forward<TArg" + index_text + ">(new_" + argument + ")");
        constructible_values.push_back("std::is_constructible_v<" +
                                       layout.leaves[index].type.spelling + ", TArg" +
                                       index_text + "&&>");
        trivial_values.push_back("(std::is_trivially_copyable_v<" +
                                 layout.leaves[index].type.spelling +
                                 "> && std::is_trivially_destructible_v<" +
                                 layout.leaves[index].type.spelling + ">)");
    }
    auto const copy_constructible{fixed_trait(layout, "is_copy_constructible_v")};
    auto const move_constructible{fixed_trait(layout, "is_move_constructible_v")};
    auto const nothrow_move{fixed_trait(layout, "is_nothrow_move_constructible_v")};
    auto const default_constructible{fixed_trait(layout, "is_default_constructible_v")};
    auto const constructible{join(constructible_values, " && ")};
    auto const trivial{join(trivial_values, " && ")};
    auto const view_name{schema.view_name.value_or(schema.name + "View")};
    auto const const_view_name{schema.const_view_name.value_or(schema.name + "ConstView")};
    auto function = [](std::string function_name,
                       CppType return_type,
                       std::vector<FunctionParameter> parameters,
                       std::string body,
                       std::string suffix = {},
                       std::optional<std::string> function_template = std::nullopt,
                       std::optional<std::string> requires_clause = std::nullopt,
                       bool compact = false,
                       bool requires_before_signature = false) {
        auto const opening_brace_on_new_line{
            !compact && requires_clause.has_value() && !requires_before_signature};
        auto const template_on_same_line{
            compact && function_template.has_value() && !requires_clause.has_value()};
        return header_function(FunctionSpec{
            .name = std::move(function_name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body))},
            .suffix = std::move(suffix),
            .is_inline = true,
            .template_parameters = std::move(function_template),
            .requires_clause = std::move(requires_clause),
            .compact_body = compact,
            .requires_on_new_line = !compact,
            .requires_before_signature = requires_before_signature,
            .opening_brace_on_new_line = opening_brace_on_new_line,
            .template_on_same_line = template_on_same_line,
        });
    };
    auto deleted = [](std::string function_name,
                      std::vector<FunctionParameter> parameters,
                      std::string requirement,
                      CppType return_type = {},
                      std::string suffix = {}) {
        return declaration(FunctionSpec{
            .name = std::move(function_name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .suffix = std::move(suffix),
            .requires_clause = std::move(requirement) + "\n= delete",
        });
    };

    Nodes children{
        UsingDeclaration{"size_type", CppType{"int32"}},
        lines(1),
        UsingDeclaration{"View", CppType{view_name}},
        lines(1),
        UsingDeclaration{"ConstView", CppType{const_view_name}},
    };
    if (schema.equivalent_type.has_value()) {
        children.push_back(lines(1));
        children.push_back(
            UsingDeclaration{"equivalent_type", resolve_type(*schema.equivalent_type, types)});
    }
    children.insert(children.end(), {
        lines(2),
        Member{CppType{"static constexpr size_type"}, "capacity_value", "Capacity"},
        lines(2),
        declaration(FunctionSpec{.name = name, .suffix = " noexcept = default"}),
        lines(1),
        function(name,
                 {},
                 {FunctionParameter{name + " const&", "other"}},
                 "for (size_type i{}; i < other.size_; ++i) {\n"
                 "    storage_.copy_construct_at(size_, other.storage_, i);\n"
                 "    ++size_;\n"
                 "}",
                 {},
                 std::nullopt,
                 "(" + copy_constructible + ")"),
        lines(1),
        deleted(name,
                {FunctionParameter{name + " const&", {}}},
                "(!(" + copy_constructible + "))"),
        lines(1),
        function(name,
                 {},
                 {FunctionParameter{name + "&&", "other"}},
                 "for (size_type i{}; i < other.size_; ++i) {\n"
                 "    storage_.move_construct_at(size_, other.storage_, i);\n"
                 "    ++size_;\n"
                 "}\n"
                 "other.reset();",
                 " noexcept(" + nothrow_move + ")",
                 std::nullopt,
                 "(" + move_constructible + ")"),
        lines(1),
        deleted(name,
                {FunctionParameter{name + "&&", {}}},
                "(!(" + move_constructible + "))"),
        lines(1),
        function("~" + name, {}, {}, "reset();", {}, std::nullopt, std::nullopt, true),
        lines(2),
        function("operator=",
                 "auto",
                 {FunctionParameter{name + " const&", "other"}},
                 "if (this != std::addressof(other)) {\n"
                 "    reset();\n"
                 "    append_from(other.get_const_view());\n"
                 "}\n"
                 "return *this;",
                 " -> " + name + "&",
                 std::nullopt,
                 "(" + copy_constructible + ")"),
        lines(1),
        deleted("operator=",
                {FunctionParameter{name + " const&", {}}},
                "(!(" + copy_constructible + "))",
                "auto",
                " -> " + name + "&"),
        lines(1),
        function("operator=",
                 "auto",
                 {FunctionParameter{name + "&&", "other"}},
                 "if (this != std::addressof(other)) {\n"
                 "    reset();\n"
                 "    for (size_type i{}; i < other.size_; ++i) {\n"
                 "        storage_.move_construct_at(size_, other.storage_, i);\n"
                 "        ++size_;\n"
                 "    }\n"
                 "    other.reset();\n"
                 "}\n"
                 "return *this;",
                 " noexcept(" + nothrow_move + ") -> " + name + "&",
                 std::nullopt,
                 "(" + move_constructible + ")"),
        lines(1),
        deleted("operator=",
                {FunctionParameter{name + "&&", {}}},
                "(!(" + move_constructible + "))",
                "auto",
                " -> " + name + "&"),
        lines(2),
        function("capacity", "static constexpr auto", {}, "return capacity_value;", " noexcept -> size_type", std::nullopt, std::nullopt, true),
        lines(1),
        function("num", "auto", {}, "return size_;", " const noexcept -> size_type", std::nullopt, std::nullopt, true),
        lines(1),
        function("is_empty", "auto", {}, "return size_ == 0;", " const noexcept -> bool", std::nullopt, std::nullopt, true),
        lines(1),
        function("is_full", "auto", {}, "return size_ == capacity();", " const noexcept -> bool", std::nullopt, std::nullopt, true),
        lines(2),
    });
    auto add_compact = [&](std::string function_name,
                           CppType return_type,
                           std::vector<FunctionParameter> parameters,
                           std::string body,
                           std::string suffix = {},
                           std::optional<std::string> function_template = std::nullopt,
                           std::optional<std::string> requirement = std::nullopt) {
        children.push_back(function(std::move(function_name),
                                    std::move(return_type),
                                    std::move(parameters),
                                    std::move(body),
                                    std::move(suffix),
                                    std::move(function_template),
                                    std::move(requirement),
                                    true));
        children.push_back(lines(1));
    };
    add_compact("get_view", "auto", {}, "return storage_.get_view(0, size_);", " -> View");
    add_compact("get_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check_view_range(offset, count); return storage_.get_view(offset, count);", " -> View");
    add_compact("get_view", "auto", {}, "return get_const_view();", " const -> ConstView");
    add_compact("get_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_const_view(offset, count);", " const -> ConstView");
    add_compact("get_const_view", "auto", {}, "return storage_.get_const_view(0, size_);", " const -> ConstView");
    add_compact("get_const_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check_view_range(offset, count); return storage_.get_const_view(offset, count);", " const -> ConstView");
    add_compact("slice", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_view(offset, count);", " -> View");
    add_compact("left", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(0, count);", " -> View");
    add_compact("right", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(size_ - count, count);", " -> View");
    add_compact("slice", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_const_view(offset, count);", " const -> ConstView");
    add_compact("left", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(0, count);", " const -> ConstView");
    add_compact("right", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(size_ - count, count);", " const -> ConstView");
    children.back() = lines(2);
    add_compact("apply_arrays", "auto", {FunctionParameter{"TFunc&&", "func"}}, "auto view{get_view()}; return view.apply_arrays(std::forward<TFunc>(func));", " -> decltype(auto)", "typename TFunc");
    add_compact("apply_arrays", "auto", {FunctionParameter{"TFunc&&", "func"}}, "auto view{get_const_view()}; return view.apply_arrays(std::forward<TFunc>(func));", " const -> decltype(auto)", "typename TFunc");
    if (schema.equivalent_type.has_value()) {
        children.push_back(lines(2));
        children.push_back(function("operator[]", "auto", {FunctionParameter{"size_type const", "index"}}, "return get_const_view()[index];", " const -> equivalent_type"));
        children.push_back(lines(1));
        children.push_back(function("at", "auto", {FunctionParameter{"size_type const", "index"}}, "check_index(index);\nreturn (*this)[index];", " const -> equivalent_type"));
    }
    children.push_back(lines(2));
    children.push_back(function("emplace_back", "auto", function_parameters, "check_has_sufficient_capacity(1);\nauto const index{size_};\nstorage_.construct_at(index, " + join(forwarded, ", ") + ");\n++size_;\nreturn index;", " -> size_type", join(template_parameters, ", "), "(" + constructible + ")", false, true));
    children.push_back(lines(1));
    children.push_back(function("add", "auto", function_parameters, "return emplace_back(" + join(forwarded, ", ") + ");", " -> size_type", join(template_parameters, ", "), "(" + constructible + ")", true, true));
    children.push_back(lines(2));

    add_compact("add_defaulted", "void", {FunctionParameter{"size_type const", "count", "1"}}, "check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.default_construct_at(size_); ++size_; }", {}, std::nullopt, "(" + default_constructible + ")");
    add_compact("set_num", "void", {FunctionParameter{"size_type const", "new_size"}}, "check(new_size >= 0); check(new_size <= capacity()); if (new_size < size_) { destroy_from(new_size); return; } add_defaulted(new_size - size_);", {}, std::nullopt, "(" + default_constructible + ")");
    add_compact("set_num", "void", {FunctionParameter{"size_type const", "new_size"}, FunctionParameter{"EAllowShrinking const", {}}}, "set_num(new_size);", {}, std::nullopt, "(" + default_constructible + ")");
    add_compact("capacity_view", "auto", {}, "return storage_.get_view(0, capacity());", " -> View", std::nullopt, "(" + trivial + ")");
    add_compact("set_num_uninitialised", "void", {FunctionParameter{"size_type const", "new_size"}}, "check(new_size >= 0); check(new_size <= capacity()); size_ = new_size;", {}, std::nullopt, "(" + trivial + ")");
    add_compact("add_uninitialised", "void", {FunctionParameter{"size_type const", "count"}}, "check_has_sufficient_capacity(count); size_ += count;", {}, std::nullopt, "(" + trivial + ")");
    children.back() = lines(2);
    add_compact("pop", "void", {}, "check(!is_empty()); --size_; storage_.destroy_at(size_);");
    add_compact("reset", "void", {}, "destroy_from(0);", " noexcept");
    add_compact("empty", "void", {}, "reset();", " noexcept");
    add_compact("reserve", "void", {FunctionParameter{"size_type const", "requested_capacity"}}, "check(requested_capacity >= 0); check(requested_capacity <= capacity());", " const");
    children.push_back(function("remove_at_swap", "void", {FunctionParameter{"size_type const", "index"}, FunctionParameter{"size_type const", "count"}, FunctionParameter{"EAllowShrinking const", {}}}, "check(index >= 0); check(count >= 0); check(index + count <= size_);\nif (count == 0) { return; }\nauto const available_tail{size_ - (index + count)};\nauto const move_count{count < available_tail ? count : available_tail};\nauto const source_begin{size_ - move_count};\nfor (size_type i{}; i < move_count; ++i) { storage_.move_assign_at(index + i, storage_, source_begin + i); }\ndestroy_from(size_ - count);"));
    children.push_back(lines(1));
    add_compact("copy_element", "void", {FunctionParameter{"size_type const", "dst_index"}, FunctionParameter{"Other const&", "other"}, FunctionParameter{"size_type const", "source_index"}}, "check_index(dst_index); auto const source{other.get_const_view()}; check(source_index >= 0); check(source_index < source.num()); storage_.copy_assign_from_view_at(dst_index, source, source_index);", {}, "typename Other");
    add_compact("copy_elements", "void", {FunctionParameter{"size_type const", "dst_index"}, FunctionParameter{"Other const&", "other"}, FunctionParameter{"size_type const", "source_index"}, FunctionParameter{"size_type const", "count"}}, "check(dst_index >= 0); check(source_index >= 0); check(count >= 0); check(dst_index + count <= size_); auto const source{other.get_const_view()}; check(source_index + count <= source.num()); for (size_type i{}; i < count; ++i) { storage_.copy_assign_from_view_at(dst_index + i, source, source_index + i); }", {}, "typename Other");
    add_compact("append_from", "void", {FunctionParameter{"Other const&", "other"}}, "auto const source{other.get_const_view()}; auto const count{source.num()}; check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.construct_from_view_at(size_, source, i); ++size_; }", {}, "typename Other");
    children.back() = lines(2);
    children.push_back(AccessSpecifier{"private"});
    children.push_back(lines(1));
    add_compact("check_index", "void", {FunctionParameter{"size_type const", "index"}}, "check(index >= 0); check(index < size_);", " const");
    add_compact("check_view_range", "void", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check(offset >= 0); check(count >= 0); check(offset + count <= size_);", " const");
    add_compact("check_has_sufficient_capacity", "void", {FunctionParameter{"size_type const", "count"}}, "check(count >= 0); check(count <= capacity() - size_);", " const");
    add_compact("destroy_from", "void", {FunctionParameter{"size_type const", "first_index"}}, "for (size_type i{size_}; i > first_index; --i) { storage_.destroy_at(i - 1); } size_ = first_index;", " noexcept");
    children.back() = lines(2);
    children.push_back(Member{CppType{schema.fixed->storage_name + "<Capacity>"}, "storage_"});
    children.push_back(lines(1));
    children.push_back(Member{CppType{"size_type"}, "size_", {}});

    std::vector<TypeDependency> dependencies{fixed_storage_dependency,
                                             tarray_view,
                                             std_forward,
                                             move_temp,
                                             allow_shrinking,
                                             check_dependency,
                                             std_memory,
                                             std_type_traits};
    for (auto const& leaf : layout.leaves) {
        dependencies.insert(dependencies.end(),
                            leaf.type.dependencies.begin(),
                            leaf.type.dependencies.end());
    }
    return Struct{
        .name = name,
        .children = std::move(children),
        .template_parameters = "int32 Capacity",
        .requires_clause = "(Capacity >= 0)",
        .dependencies = std::move(dependencies),
    };
}

auto fixed_nodes(FixedLayout const& layout,
                 std::map<std::string, CppType> const& types) -> Nodes {
    Nodes result{fixed_storage_node(layout)};
    for (auto const& container : layout.schema->fixed->containers) {
        result.push_back(lines(2));
        result.push_back(fixed_container_node(layout, container, types));
    }
    return result;
}

} // namespace

auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       std::map<std::string, CppType> const& types) -> Nodes {
    return fixed_nodes(build_fixed_layout(schema, schemas, types), types);
}

} // namespace codegen::detail
