#include "fixed_soa_internal.h"
#include "lowering_utils.h"

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

auto fixed_trait(FixedLayout const& layout, std::string const& trait) -> std::string {
    std::vector<std::string> values;
    for (auto const& leaf : layout.leaves) {
        values.push_back("std::" + trait + "<" + leaf.type.spelling + ">");
    }
    return join(values, " && ");
}

auto compact_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionFormatting::BodyLayout::compact,
        .requires_placement = FunctionFormatting::RequiresPlacement::trailing_same_line,
        .template_placement = FunctionFormatting::TemplatePlacement::same_line,
    };
}

auto expanded_constrained_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .opening_brace_placement = FunctionFormatting::OpeningBracePlacement::separate_line,
    };
}

auto expanded_prefixed_constraint_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .requires_placement = FunctionFormatting::RequiresPlacement::before_signature,
    };
}

auto compact_prefixed_constraint_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionFormatting::BodyLayout::compact,
        .requires_placement = FunctionFormatting::RequiresPlacement::before_signature,
    };
}

auto fixed_function(std::string name,
                    CppType return_type,
                    std::vector<FunctionParameter> parameters,
                    std::string body,
                    std::string suffix = {},
                    std::optional<std::string> function_template = std::nullopt,
                    std::optional<std::string> requires_clause = std::nullopt,
                    FunctionFormatting formatting = {}) -> Node {
    return header_function(FunctionSpec{
        .name = std::move(name),
        .return_type = std::move(return_type),
        .parameters = std::move(parameters),
        .body = {raw(std::move(body))},
        .suffix = std::move(suffix),
        .is_inline = true,
        .template_parameters = std::move(function_template),
        .requires_clause = std::move(requires_clause),
        .formatting = formatting,
    });
}

auto deleted_fixed_function(std::string name,
                            std::vector<FunctionParameter> parameters,
                            std::string requirement,
                            CppType return_type = {},
                            std::string suffix = {}) -> Node {
    return declaration(FunctionSpec{
        .name = std::move(name),
        .return_type = std::move(return_type),
        .parameters = std::move(parameters),
        .suffix = std::move(suffix),
        .requires_clause = std::move(requirement) + "\n= delete",
    });
}

void add_compact_function(NodeListBuilder& nodes,
                          std::string name,
                          CppType return_type,
                          std::vector<FunctionParameter> parameters,
                          std::string body,
                          std::string suffix = {},
                          std::optional<std::string> function_template = std::nullopt,
                          std::optional<std::string> requirement = std::nullopt,
                          int trailing_new_lines = 1) {
    nodes.add(fixed_function(std::move(name),
                             std::move(return_type),
                             std::move(parameters),
                             std::move(body),
                             std::move(suffix),
                             std::move(function_template),
                             std::move(requirement),
                             compact_function_formatting()),
              trailing_new_lines);
}

auto fixed_container_prelude_nodes(SoaSchema const& schema,
                                   std::map<std::string, CppType> const& types) -> Nodes {
    NodeListBuilder result;
    result.add(UsingDeclaration{"size_type", CppType{"int32"}}, 1)
        .add(UsingDeclaration{"View", CppType{schema.view_name.value_or(schema.name + "View")}}, 1)
        .add(UsingDeclaration{"ConstView",
                              CppType{schema.const_view_name.value_or(schema.name + "ConstView")}});
    if (schema.equivalent_type.has_value()) {
        result.new_lines().add(
            UsingDeclaration{"equivalent_type", resolve_type(*schema.equivalent_type, types)});
    }
    result.new_lines(2)
        .add(Member{CppType{"static constexpr size_type"}, "capacity_value", "Capacity"}, 2);
    return result.build();
}

auto fixed_container_lifecycle_nodes(FixedLayout const& layout,
                                     std::string const& name) -> Nodes {
    auto const copy_constructible{fixed_trait(layout, "is_copy_constructible_v")};
    auto const move_constructible{fixed_trait(layout, "is_move_constructible_v")};
    auto const nothrow_move{fixed_trait(layout, "is_nothrow_move_constructible_v")};
    return {
        declaration(FunctionSpec{.name = name, .suffix = " noexcept = default"}),
        lines(1),
        fixed_function(name,
                       {},
                       {FunctionParameter{name + " const&", "other"}},
                       "for (size_type i{}; i < other.size_; ++i) {\n"
                       "    storage_.copy_construct_at(size_, other.storage_, i);\n"
                       "    ++size_;\n"
                       "}",
                       {},
                       std::nullopt,
                       "(" + copy_constructible + ")",
                       expanded_constrained_function_formatting()),
        lines(1),
        deleted_fixed_function(name,
                               {FunctionParameter{name + " const&", {}}},
                               "(!(" + copy_constructible + "))"),
        lines(1),
        fixed_function(name,
                       {},
                       {FunctionParameter{name + "&&", "other"}},
                       "for (size_type i{}; i < other.size_; ++i) {\n"
                       "    storage_.move_construct_at(size_, other.storage_, i);\n"
                       "    ++size_;\n"
                       "}\n"
                       "other.reset();",
                       " noexcept(" + nothrow_move + ")",
                       std::nullopt,
                       "(" + move_constructible + ")",
                       expanded_constrained_function_formatting()),
        lines(1),
        deleted_fixed_function(name,
                               {FunctionParameter{name + "&&", {}}},
                               "(!(" + move_constructible + "))"),
        lines(1),
        fixed_function("~" + name,
                       {},
                       {},
                       "reset();",
                       {},
                       std::nullopt,
                       std::nullopt,
                       compact_function_formatting()),
        lines(2),
        fixed_function("operator=",
                       "auto",
                       {FunctionParameter{name + " const&", "other"}},
                       "if (this != std::addressof(other)) {\n"
                       "    reset();\n"
                       "    append_from(other.get_const_view());\n"
                       "}\n"
                       "return *this;",
                       " -> " + name + "&",
                       std::nullopt,
                       "(" + copy_constructible + ")",
                       expanded_constrained_function_formatting()),
        lines(1),
        deleted_fixed_function("operator=",
                               {FunctionParameter{name + " const&", {}}},
                               "(!(" + copy_constructible + "))",
                               "auto",
                               " -> " + name + "&"),
        lines(1),
        fixed_function("operator=",
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
                       "(" + move_constructible + ")",
                       expanded_constrained_function_formatting()),
        lines(1),
        deleted_fixed_function("operator=",
                               {FunctionParameter{name + "&&", {}}},
                               "(!(" + move_constructible + "))",
                               "auto",
                               " -> " + name + "&"),
        lines(2),
    };
}

auto fixed_container_access_nodes(SoaSchema const& schema) -> Nodes {
    NodeListBuilder result;
    add_compact_function(result, "capacity", "static constexpr auto", {}, "return capacity_value;", " noexcept -> size_type");
    add_compact_function(result, "num", "auto", {}, "return size_;", " const noexcept -> size_type");
    add_compact_function(result, "is_empty", "auto", {}, "return size_ == 0;", " const noexcept -> bool");
    add_compact_function(result, "is_full", "auto", {}, "return size_ == capacity();", " const noexcept -> bool", std::nullopt, std::nullopt, 2);
    add_compact_function(result, "get_view", "auto", {}, "return storage_.get_view(0, size_);", " -> View");
    add_compact_function(result, "get_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check_view_range(offset, count); return storage_.get_view(offset, count);", " -> View");
    add_compact_function(result, "get_view", "auto", {}, "return get_const_view();", " const -> ConstView");
    add_compact_function(result, "get_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_const_view(offset, count);", " const -> ConstView");
    add_compact_function(result, "get_const_view", "auto", {}, "return storage_.get_const_view(0, size_);", " const -> ConstView");
    add_compact_function(result, "get_const_view", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check_view_range(offset, count); return storage_.get_const_view(offset, count);", " const -> ConstView");
    add_compact_function(result, "slice", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_view(offset, count);", " -> View");
    add_compact_function(result, "left", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(0, count);", " -> View");
    add_compact_function(result, "right", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(size_ - count, count);", " -> View");
    add_compact_function(result, "slice", "auto", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "return get_const_view(offset, count);", " const -> ConstView");
    add_compact_function(result, "left", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(0, count);", " const -> ConstView");
    add_compact_function(result, "right", "auto", {FunctionParameter{"size_type const", "count"}}, "return slice(size_ - count, count);", " const -> ConstView", std::nullopt, std::nullopt, 2);
    add_compact_function(result, "apply_arrays", "auto", {FunctionParameter{"TFunc&&", "func"}}, "auto view{get_view()}; return view.apply_arrays(std::forward<TFunc>(func));", " -> decltype(auto)", "typename TFunc");
    add_compact_function(result, "apply_arrays", "auto", {FunctionParameter{"TFunc&&", "func"}}, "auto view{get_const_view()}; return view.apply_arrays(std::forward<TFunc>(func));", " const -> decltype(auto)", "typename TFunc", std::nullopt, 2);
    if (schema.equivalent_type.has_value()) {
        result.add(fixed_function("operator[]", "auto", {FunctionParameter{"size_type const", "index"}}, "return get_const_view()[index];", " const -> equivalent_type"), 1);
        result.add(fixed_function("at", "auto", {FunctionParameter{"size_type const", "index"}}, "check_index(index);\nreturn (*this)[index];", " const -> equivalent_type"));
    }
    result.new_lines(2);
    return result.build();
}

auto fixed_container_construction_nodes(FixedLayout const& layout) -> Nodes {
    std::vector<std::string> template_parameters;
    std::vector<FunctionParameter> parameters;
    std::vector<std::string> forwarded;
    std::vector<std::string> constructible_values;
    std::vector<std::string> trivial_values;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{fixed_leaf_argument(layout.leaves[index])};
        auto const index_text{std::to_string(index)};
        template_parameters.push_back("typename TArg" + index_text);
        parameters.emplace_back("TArg" + index_text + "&&", "new_" + argument);
        forwarded.push_back("std::forward<TArg" + index_text + ">(new_" + argument + ")");
        constructible_values.push_back("std::is_constructible_v<" +
                                       layout.leaves[index].type.spelling + ", TArg" +
                                       index_text + "&&>");
        trivial_values.push_back("(std::is_trivially_copyable_v<" +
                                 layout.leaves[index].type.spelling +
                                 "> && std::is_trivially_destructible_v<" +
                                 layout.leaves[index].type.spelling + ">)");
    }
    auto const function_template{join(template_parameters, ", ")};
    auto const forwarded_values{join(forwarded, ", ")};
    auto const constructible{"(" + join(constructible_values, " && ") + ")"};
    auto const default_constructible{
        "(" + fixed_trait(layout, "is_default_constructible_v") + ")"};
    auto const trivial{"(" + join(trivial_values, " && ") + ")"};
    NodeListBuilder result;
    result.add(fixed_function("emplace_back",
                              "auto",
                              parameters,
                              "check_has_sufficient_capacity(1);\n"
                              "auto const index{size_};\n"
                              "storage_.construct_at(index, " + forwarded_values + ");\n"
                              "++size_;\n"
                              "return index;",
                              " -> size_type",
                              function_template,
                              constructible,
                              expanded_prefixed_constraint_formatting()),
               1)
        .add(fixed_function("add",
                            "auto",
                            parameters,
                            "return emplace_back(" + forwarded_values + ");",
                            " -> size_type",
                            function_template,
                            constructible,
                            compact_prefixed_constraint_formatting()),
             2);
    add_compact_function(result, "add_defaulted", "void", {FunctionParameter{"size_type const", "count", "1"}}, "check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.default_construct_at(size_); ++size_; }", {}, std::nullopt, default_constructible);
    add_compact_function(result, "set_num", "void", {FunctionParameter{"size_type const", "new_size"}}, "check(new_size >= 0); check(new_size <= capacity()); if (new_size < size_) { destroy_from(new_size); return; } add_defaulted(new_size - size_);", {}, std::nullopt, default_constructible);
    add_compact_function(result, "set_num", "void", {FunctionParameter{"size_type const", "new_size"}, FunctionParameter{"EAllowShrinking const", {}}}, "set_num(new_size);", {}, std::nullopt, default_constructible);
    add_compact_function(result, "capacity_view", "auto", {}, "return storage_.get_view(0, capacity());", " -> View", std::nullopt, trivial);
    add_compact_function(result, "set_num_uninitialised", "void", {FunctionParameter{"size_type const", "new_size"}}, "check(new_size >= 0); check(new_size <= capacity()); size_ = new_size;", {}, std::nullopt, trivial);
    add_compact_function(result, "add_uninitialised", "void", {FunctionParameter{"size_type const", "count"}}, "check_has_sufficient_capacity(count); size_ += count;", {}, std::nullopt, trivial, 2);
    return result.build();
}

auto fixed_container_mutation_nodes() -> Nodes {
    NodeListBuilder result;
    add_compact_function(result, "pop", "void", {}, "check(!is_empty()); --size_; storage_.destroy_at(size_);");
    add_compact_function(result, "reset", "void", {}, "destroy_from(0);", " noexcept");
    add_compact_function(result, "empty", "void", {}, "reset();", " noexcept");
    add_compact_function(result, "reserve", "void", {FunctionParameter{"size_type const", "requested_capacity"}}, "check(requested_capacity >= 0); check(requested_capacity <= capacity());", " const");
    result.add(fixed_function("remove_at_swap", "void", {FunctionParameter{"size_type const", "index"}, FunctionParameter{"size_type const", "count"}, FunctionParameter{"EAllowShrinking const", {}}}, "check(index >= 0); check(count >= 0); check(index + count <= size_);\nif (count == 0) { return; }\nauto const available_tail{size_ - (index + count)};\nauto const move_count{count < available_tail ? count : available_tail};\nauto const source_begin{size_ - move_count};\nfor (size_type i{}; i < move_count; ++i) { storage_.move_assign_at(index + i, storage_, source_begin + i); }\ndestroy_from(size_ - count);"), 1);
    add_compact_function(result, "copy_element", "void", {FunctionParameter{"size_type const", "dst_index"}, FunctionParameter{"Other const&", "other"}, FunctionParameter{"size_type const", "source_index"}}, "check_index(dst_index); auto const source{other.get_const_view()}; check(source_index >= 0); check(source_index < source.num()); storage_.copy_assign_from_view_at(dst_index, source, source_index);", {}, "typename Other");
    add_compact_function(result, "copy_elements", "void", {FunctionParameter{"size_type const", "dst_index"}, FunctionParameter{"Other const&", "other"}, FunctionParameter{"size_type const", "source_index"}, FunctionParameter{"size_type const", "count"}}, "check(dst_index >= 0); check(source_index >= 0); check(count >= 0); check(dst_index + count <= size_); auto const source{other.get_const_view()}; check(source_index + count <= source.num()); for (size_type i{}; i < count; ++i) { storage_.copy_assign_from_view_at(dst_index + i, source, source_index + i); }", {}, "typename Other");
    add_compact_function(result, "append_from", "void", {FunctionParameter{"Other const&", "other"}}, "auto const source{other.get_const_view()}; auto const count{source.num()}; check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.construct_from_view_at(size_, source, i); ++size_; }", {}, "typename Other", std::nullopt, 2);
    return result.build();
}

auto fixed_container_private_nodes(SoaSchema const& schema) -> Nodes {
    NodeListBuilder result;
    result.add(AccessSpecifier{"private"}, 1);
    add_compact_function(result, "check_index", "void", {FunctionParameter{"size_type const", "index"}}, "check(index >= 0); check(index < size_);", " const");
    add_compact_function(result, "check_view_range", "void", {FunctionParameter{"size_type const", "offset"}, FunctionParameter{"size_type const", "count"}}, "check(offset >= 0); check(count >= 0); check(offset + count <= size_);", " const");
    add_compact_function(result, "check_has_sufficient_capacity", "void", {FunctionParameter{"size_type const", "count"}}, "check(count >= 0); check(count <= capacity() - size_);", " const");
    add_compact_function(result, "destroy_from", "void", {FunctionParameter{"size_type const", "first_index"}}, "for (size_type i{size_}; i > first_index; --i) { storage_.destroy_at(i - 1); } size_ = first_index;", " noexcept", std::nullopt, std::nullopt, 2);
    result.add(Member{CppType{schema.fixed->storage_name + "<Capacity>"}, "storage_"}, 1)
        .add(Member{CppType{"size_type"}, "size_", {}});
    return result.build();
}

auto fixed_container_dependencies(FixedLayout const& layout) -> std::vector<TypeDependency> {
    std::vector<TypeDependency> result{fixed_storage_dependency,
                                       tarray_view,
                                       std_forward,
                                       move_temp,
                                       allow_shrinking,
                                       check_dependency,
                                       std_memory,
                                       std_type_traits};
    for (auto const& leaf : layout.leaves) {
        result.insert(result.end(), leaf.type.dependencies.begin(), leaf.type.dependencies.end());
    }
    return result;
}

} // namespace

auto fixed_container_node(FixedLayout const& layout,
                          std::string const& name,
                          std::map<std::string, CppType> const& types) -> Node {
    auto const& schema{*layout.schema};
    NodeListBuilder children;
    children.append(fixed_container_prelude_nodes(schema, types))
        .append(fixed_container_lifecycle_nodes(layout, name))
        .append(fixed_container_access_nodes(schema))
        .append(fixed_container_construction_nodes(layout))
        .append(fixed_container_mutation_nodes())
        .append(fixed_container_private_nodes(schema));

    return Struct{
        .name = name,
        .children = children.build(),
        .template_parameters = "int32 Capacity",
        .requires_clause = "(Capacity >= 0)",
        .dependencies = fixed_container_dependencies(layout),
    };
}

} // namespace codegen::detail
