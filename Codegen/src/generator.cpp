#include <codegen/generator.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen {
namespace {

TypeDependency const std_forward{"std::forward", "utility", {}};
TypeDependency const std_remove_const{"std::remove_const_t", "type_traits", {}};
TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const allow_shrinking{"EAllowShrinking", "Containers/AllowShrinking.h", {}};
TypeDependency const array_checks{
    "ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h", {}};
TypeDependency const container_ops{"ml::num", "SandboxCore/container_ops.h", {}};
TypeDependency const soa_concepts{
    "ml::SupportsApplyArrayPairsWith", "SandboxCore/soa_concepts.h", {}};
TypeDependency const soa_permutation{
    "ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const fill_indices{"ml::fill_indices", "SandboxCore/array_utils.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};
TypeDependency const fixed_storage_dependency{
    "ml::TFixedStorage", "SandboxCore/fixed_storage.h", {}};
TypeDependency const move_temp{"MoveTemp", "Templates/UnrealTemplate.h", {}};
TypeDependency const std_memory{"std::addressof", "memory", {}};
TypeDependency const std_type_traits{"std::is_constructible_v", "type_traits", {}};

struct ResolvedMember {
    std::string name;
    SoaMemberKind kind;
    CppType element_type;
    CppType container_type;
    CppType view_type;
    CppType const_view_type;
};

struct LoweredSoa {
    Nodes header;
    Nodes source;
};

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

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string {
    std::ostringstream output;
    auto const count{values.size()};
    for (std::size_t index{0}; index < count; ++index) {
        if (index != 0) {
            output << separator;
        }
        output << values[index];
    }
    return output.str();
}

auto join_lines(std::vector<std::string> const& values) -> std::string {
    return join(values, "\n");
}

auto composed(std::string spelling,
              TypeDependency outer,
              CppType const& contained) -> CppType {
    outer.spelling = spelling;
    outer.dependencies = contained.dependencies;
    return CppType{std::move(spelling), std::vector<TypeDependency>{std::move(outer)}};
}

auto resolve_members(SoaSchema const& schema,
                     std::map<std::string, CppType> const& types) -> std::vector<ResolvedMember> {
    std::vector<ResolvedMember> result;
    std::set<std::string> names;
    for (auto const& member : schema.members) {
        if (member.name.empty() || !names.insert(member.name).second) {
            throw std::invalid_argument{"SOA '" + schema.name +
                                        "' has an empty or duplicate member name: " + member.name};
        }
        auto element{resolve_type(member.type, types)};
        if (member.kind == SoaMemberKind::array) {
            auto container{composed("TArray<" + element.spelling + ">", tarray, element)};
            container.member_operations.emplace(TypeOperation::remove_at_swap, "RemoveAtSwap");
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = std::move(container),
                .view_type = composed("TArrayView<" + element.spelling + ">", tarray_view, element),
                .const_view_type =
                    composed("TConstArrayView<" + element.spelling + ">", tarray_view, element),
            });
        } else {
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = element,
                .view_type = CppType{element.spelling + "::View", element.dependencies},
                .const_view_type = CppType{element.spelling + "::ConstView", element.dependencies},
            });
        }
    }
    if (result.empty()) {
        throw std::invalid_argument{"SOA '" + schema.name + "' must have members"};
    }
    return result;
}

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

auto indent_lines(std::vector<std::string> const& values, int spaces = 8) -> std::string {
    std::vector<std::string> result;
    auto const prefix{std::string(static_cast<std::size_t>(spaces), ' ')};
    for (auto const& value : values) {
        result.push_back(prefix + value);
    }
    return join_lines(result);
}

auto fixed_storage_text(FixedLayout const& layout) -> std::string {
    auto const& schema{*layout.schema};
    auto const& storage_name{schema.fixed->storage_name};
    std::vector<std::string> template_parameters;
    std::vector<std::string> function_parameters;
    std::vector<std::string> forwarded;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{leaf_argument(layout.leaves[index])};
        template_parameters.push_back("typename TArg" + std::to_string(index));
        function_parameters.push_back("TArg" + std::to_string(index) + "&& new_" + argument);
        forwarded.push_back("std::forward<TArg" + std::to_string(index) + ">(new_" +
                            argument + ")");
    }
    std::vector<std::string> mutable_views;
    std::vector<std::string> const_views;
    std::vector<std::string> copy_assign;
    std::vector<std::string> move_assign;
    std::vector<std::string> storage_members;
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
        storage_members.push_back(fixed_storage_type(member) + " " + name + ";");
    }
    std::vector<std::string> destroy;
    for (auto iterator{layout.members.rbegin()}; iterator != layout.members.rend(); ++iterator) {
        destroy.push_back(iterator->member.name + "_.destroy_at(index);");
    }
    return "template <int32 Capacity>\n"
           "    requires (Capacity >= 0)\n"
           "struct " + storage_name + " {\n"
           "    using View = " + schema.view_name.value_or(schema.name + "View") + ";\n"
           "    using ConstView = " +
           schema.const_view_name.value_or(schema.name + "ConstView") + ";\n\n"
           "    auto get_view(int32 const offset, int32 const count) -> View {\n"
           "        return View{" + join(mutable_views, ", ") + "};\n    }\n"
           "    auto get_const_view(int32 const offset, int32 const count) const -> ConstView {\n"
           "        return ConstView{" + join(const_views, ", ") + "};\n    }\n\n"
           "    template <" + join(template_parameters, ", ") + ">\n"
           "    void construct_at(int32 const index, " + join(function_parameters, ", ") + ") {\n" +
           indent_lines(fixed_construct_lines(layout, "arguments", forwarded)) + "\n    }\n\n"
           "    void default_construct_at(int32 const index) {\n" +
           indent_lines(fixed_construct_lines(layout, "default")) + "\n    }\n\n"
           "    void copy_construct_at(int32 const index, " + storage_name +
           " const& other, int32 const other_index) {\n" +
           indent_lines(fixed_construct_lines(layout, "copy")) + "\n    }\n\n"
           "    void move_construct_at(int32 const index, " + storage_name +
           "& other, int32 const other_index) {\n" +
           indent_lines(fixed_construct_lines(layout, "move")) + "\n    }\n\n"
           "    template <typename SourceView>\n"
           "    void construct_from_view_at(int32 const index, SourceView const& source, int32 const source_index) {\n" +
           indent_lines(fixed_construct_lines(layout, "view")) + "\n    }\n\n"
           "    template <typename SourceView>\n"
           "    void copy_assign_from_view_at(int32 const dst_index, SourceView const& source, int32 const source_index) {\n" +
           indent_lines(copy_assign) + "\n    }\n\n"
           "    void move_assign_at(int32 const dst_index, " + storage_name +
           "& other, int32 const source_index) {\n" + indent_lines(move_assign) +
           "\n    }\n\n"
           "    void destroy_at(int32 const index) noexcept {\n" + indent_lines(destroy) +
           "\n    }\n\n" + indent_lines(storage_members, 4) + "\n};";
}

auto fixed_trait(FixedLayout const& layout, std::string const& trait) -> std::string {
    std::vector<std::string> values;
    for (auto const& leaf : layout.leaves) {
        values.push_back("std::" + trait + "<" + leaf.type.spelling + ">");
    }
    return join(values, " && ");
}

auto fixed_container_text(FixedLayout const& layout,
                          std::string const& name,
                          std::map<std::string, CppType> const& types) -> std::string {
    auto const& schema{*layout.schema};
    std::vector<std::string> template_parameters;
    std::vector<std::string> function_parameters;
    std::vector<std::string> forwarded;
    std::vector<std::string> constructible_values;
    std::vector<std::string> trivial_values;
    for (std::size_t index{0}; index < layout.leaves.size(); ++index) {
        auto const argument{leaf_argument(layout.leaves[index])};
        auto const index_text{std::to_string(index)};
        template_parameters.push_back("typename TArg" + index_text);
        function_parameters.push_back("TArg" + index_text + "&& new_" + argument);
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
    auto equivalent_alias{std::string{}};
    auto equivalent_access{std::string{}};
    if (schema.equivalent_type.has_value()) {
        auto const equivalent{resolve_type(*schema.equivalent_type, types)};
        equivalent_alias = "\n    using equivalent_type = " + equivalent.spelling + ";";
        equivalent_access =
            "\n\n    auto operator[](size_type const index) const -> equivalent_type {\n"
            "        return get_const_view()[index];\n"
            "    }\n"
            "    auto at(size_type const index) const -> equivalent_type {\n"
            "        check_index(index);\n"
            "        return (*this)[index];\n"
            "    }";
    }
    return "template <int32 Capacity>\n"
           "    requires (Capacity >= 0)\n"
           "struct " + name + " {\n"
           "    using size_type = int32;\n"
           "    using View = " + view_name + ";\n"
           "    using ConstView = " + const_view_name + ";" + equivalent_alias + "\n\n"
           "    static constexpr size_type capacity_value{Capacity};\n\n"
           "    " + name + "() noexcept = default;\n"
           "    " + name + "(" + name + " const& other)\n"
           "        requires (" + copy_constructible + ")\n"
           "    {\n"
           "        for (size_type i{}; i < other.size_; ++i) {\n"
           "            storage_.copy_construct_at(size_, other.storage_, i);\n"
           "            ++size_;\n"
           "        }\n"
           "    }\n"
           "    " + name + "(" + name + " const&)\n"
           "        requires (!(" + copy_constructible + "))\n"
           "    = delete;\n"
           "    " + name + "(" + name + "&& other) noexcept(" + nothrow_move + ")\n"
           "        requires (" + move_constructible + ")\n"
           "    {\n"
           "        for (size_type i{}; i < other.size_; ++i) {\n"
           "            storage_.move_construct_at(size_, other.storage_, i);\n"
           "            ++size_;\n"
           "        }\n"
           "        other.reset();\n"
           "    }\n"
           "    " + name + "(" + name + "&&)\n"
           "        requires (!(" + move_constructible + "))\n"
           "    = delete;\n"
           "    ~" + name + "() { reset(); }\n\n"
           "    auto operator=(" + name + " const& other) -> " + name + "&\n"
           "        requires (" + copy_constructible + ")\n"
           "    {\n"
           "        if (this != std::addressof(other)) {\n"
           "            reset();\n"
           "            append_from(other.get_const_view());\n"
           "        }\n"
           "        return *this;\n"
           "    }\n"
           "    auto operator=(" + name + " const&) -> " + name + "&\n"
           "        requires (!(" + copy_constructible + "))\n"
           "    = delete;\n"
           "    auto operator=(" + name + "&& other) noexcept(" + nothrow_move +
           ") -> " + name + "&\n"
           "        requires (" + move_constructible + ")\n"
           "    {\n"
           "        if (this != std::addressof(other)) {\n"
           "            reset();\n"
           "            for (size_type i{}; i < other.size_; ++i) {\n"
           "                storage_.move_construct_at(size_, other.storage_, i);\n"
           "                ++size_;\n"
           "            }\n"
           "            other.reset();\n"
           "        }\n"
           "        return *this;\n"
           "    }\n"
           "    auto operator=(" + name + "&&) -> " + name + "&\n"
           "        requires (!(" + move_constructible + "))\n"
           "    = delete;\n\n"
           "    static constexpr auto capacity() noexcept -> size_type { return capacity_value; }\n"
           "    auto num() const noexcept -> size_type { return size_; }\n"
           "    auto is_empty() const noexcept -> bool { return size_ == 0; }\n"
           "    auto is_full() const noexcept -> bool { return size_ == capacity(); }\n\n"
           "    auto get_view() -> View { return storage_.get_view(0, size_); }\n"
           "    auto get_view(size_type const offset, size_type const count) -> View { check_view_range(offset, count); return storage_.get_view(offset, count); }\n"
           "    auto get_view() const -> ConstView { return get_const_view(); }\n"
           "    auto get_view(size_type const offset, size_type const count) const -> ConstView { return get_const_view(offset, count); }\n"
           "    auto get_const_view() const -> ConstView { return storage_.get_const_view(0, size_); }\n"
           "    auto get_const_view(size_type const offset, size_type const count) const -> ConstView { check_view_range(offset, count); return storage_.get_const_view(offset, count); }\n"
           "    auto slice(size_type const offset, size_type const count) -> View { return get_view(offset, count); }\n"
           "    auto left(size_type const count) -> View { return slice(0, count); }\n"
           "    auto right(size_type const count) -> View { return slice(size_ - count, count); }\n"
           "    auto slice(size_type const offset, size_type const count) const -> ConstView { return get_const_view(offset, count); }\n"
           "    auto left(size_type const count) const -> ConstView { return slice(0, count); }\n"
           "    auto right(size_type const count) const -> ConstView { return slice(size_ - count, count); }\n\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) -> decltype(auto) { auto view{get_view()}; return view.apply_arrays(std::forward<TFunc>(func)); }\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) const -> decltype(auto) { auto view{get_const_view()}; return view.apply_arrays(std::forward<TFunc>(func)); }" +
           equivalent_access + "\n\n"
           "    template <" + join(template_parameters, ", ") + ">\n"
           "        requires (" + constructible + ")\n"
           "    auto emplace_back(" + join(function_parameters, ", ") + ") -> size_type {\n"
           "        check_has_sufficient_capacity(1);\n"
           "        auto const index{size_};\n"
           "        storage_.construct_at(index, " + join(forwarded, ", ") + ");\n"
           "        ++size_;\n"
           "        return index;\n"
           "    }\n"
           "    template <" + join(template_parameters, ", ") + ">\n"
           "        requires (" + constructible + ")\n"
           "    auto add(" + join(function_parameters, ", ") + ") -> size_type { return emplace_back(" +
           join(forwarded, ", ") + "); }\n\n"
           "    void add_defaulted(size_type const count = 1) requires (" +
           default_constructible + ") { check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.default_construct_at(size_); ++size_; } }\n"
           "    void set_num(size_type const new_size) requires (" + default_constructible +
           ") { check(new_size >= 0); check(new_size <= capacity()); if (new_size < size_) { destroy_from(new_size); return; } add_defaulted(new_size - size_); }\n"
           "    void set_num(size_type const new_size, EAllowShrinking const) requires (" +
           default_constructible + ") { set_num(new_size); }\n"
           "    auto capacity_view() -> View requires (" + trivial +
           ") { return storage_.get_view(0, capacity()); }\n"
           "    void set_num_uninitialised(size_type const new_size) requires (" + trivial +
           ") { check(new_size >= 0); check(new_size <= capacity()); size_ = new_size; }\n"
           "    void add_uninitialised(size_type const count) requires (" + trivial +
           ") { check_has_sufficient_capacity(count); size_ += count; }\n\n"
           "    void pop() { check(!is_empty()); --size_; storage_.destroy_at(size_); }\n"
           "    void reset() noexcept { destroy_from(0); }\n"
           "    void empty() noexcept { reset(); }\n"
           "    void reserve(size_type const requested_capacity) const { check(requested_capacity >= 0); check(requested_capacity <= capacity()); }\n"
           "    void remove_at_swap(size_type const index, size_type const count, EAllowShrinking const) {\n"
           "        check(index >= 0); check(count >= 0); check(index + count <= size_);\n"
           "        if (count == 0) { return; }\n"
           "        auto const available_tail{size_ - (index + count)};\n"
           "        auto const move_count{count < available_tail ? count : available_tail};\n"
           "        auto const source_begin{size_ - move_count};\n"
           "        for (size_type i{}; i < move_count; ++i) { storage_.move_assign_at(index + i, storage_, source_begin + i); }\n"
           "        destroy_from(size_ - count);\n"
           "    }\n"
           "    template <typename Other> void copy_element(size_type const dst_index, Other const& other, size_type const source_index) { check_index(dst_index); auto const source{other.get_const_view()}; check(source_index >= 0); check(source_index < source.num()); storage_.copy_assign_from_view_at(dst_index, source, source_index); }\n"
           "    template <typename Other> void copy_elements(size_type const dst_index, Other const& other, size_type const source_index, size_type const count) { check(dst_index >= 0); check(source_index >= 0); check(count >= 0); check(dst_index + count <= size_); auto const source{other.get_const_view()}; check(source_index + count <= source.num()); for (size_type i{}; i < count; ++i) { storage_.copy_assign_from_view_at(dst_index + i, source, source_index + i); } }\n"
           "    template <typename Other> void append_from(Other const& other) { auto const source{other.get_const_view()}; auto const count{source.num()}; check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { storage_.construct_from_view_at(size_, source, i); ++size_; } }\n\n"
           "  private:\n"
           "    void check_index(size_type const index) const { check(index >= 0); check(index < size_); }\n"
           "    void check_view_range(size_type const offset, size_type const count) const { check(offset >= 0); check(count >= 0); check(offset + count <= size_); }\n"
           "    void check_has_sufficient_capacity(size_type const count) const { check(count >= 0); check(count <= capacity() - size_); }\n"
           "    void destroy_from(size_type const first_index) noexcept { for (size_type i{size_}; i > first_index; --i) { storage_.destroy_at(i - 1); } size_ = first_index; }\n\n"
           "    " + schema.fixed->storage_name + "<Capacity> storage_;\n"
           "    size_type size_{};\n"
           "};";
}

auto fixed_nodes(FixedLayout const& layout,
                 std::map<std::string, CppType> const& types) -> Nodes {
    std::vector<TypeDependency> dependencies{
        fixed_storage_dependency, tarray_view, std_forward, move_temp};
    for (auto const& leaf : layout.leaves) {
        dependencies.insert(dependencies.end(),
                            leaf.type.dependencies.begin(),
                            leaf.type.dependencies.end());
    }
    Nodes result{raw(fixed_storage_text(layout), dependencies)};
    for (auto const& container : layout.schema->fixed->containers) {
        result.push_back(lines(2));
        auto container_dependencies{dependencies};
        container_dependencies.insert(container_dependencies.end(),
                                      {allow_shrinking,
                                       check_dependency,
                                       std_memory,
                                       std_type_traits});
        result.push_back(raw(fixed_container_text(layout, container, types),
                             std::move(container_dependencies)));
    }
    return result;
}

auto view_expression(ResolvedMember const& member, bool const_view) -> std::string {
    if (member.kind == SoaMemberKind::nested) {
        return member.name + (const_view ? ".get_const_view(offset, count)"
                                         : ".get_view(offset, count)");
    }
    auto const& type{const_view ? member.const_view_type : member.view_type};
    return type.spelling + "{" + member.name + "}.Slice(offset, count)";
}

auto equivalent_expression(ResolvedMember const& member, std::string const& index)
    -> std::string {
    if (member.kind == SoaMemberKind::array) {
        return member.name + ".GetData()[" + index + "]";
    }
    return member.name + "[" + index + "]";
}

auto apply_arrays_function(std::vector<ResolvedMember> const& members) -> Node {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{members.size()};
    for (std::size_t index{0}; index < count; ++index) {
        body.push_back("    self." + members[index].name + (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    return header_function(FunctionSpec{
        .name = "apply_arrays",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this auto&&", "self"},
                       FunctionParameter{"TFunc&&", "func"}},
        .body = {raw(join_lines(body), {std_forward})},
        .suffix = " -> decltype(auto)",
        .is_inline = true,
        .template_parameters = "typename TFunc",
    });
}

auto apply_array_pairs_function(std::vector<ResolvedMember> const& members) -> Node {
    std::vector<std::string> body{"return std::forward<TFunc>(func)("};
    auto const count{members.size()};
    for (std::size_t index{0}; index < count; ++index) {
        auto const& name{members[index].name};
        body.push_back("    self." + name + ", other." + name +
                       (index + 1 < count ? "," : ""));
    }
    body.emplace_back(");");
    return header_function(FunctionSpec{
        .name = "apply_array_pairs",
        .return_type = "auto",
        .parameters = {FunctionParameter{"this Self&&", "self"},
                       FunctionParameter{"Other&&", "other"},
                       FunctionParameter{"TFunc&&", "func"}},
        .body = {raw(join_lines(body), {std_forward})},
        .suffix = "\n    -> decltype(auto)",
        .is_inline = true,
        .template_parameters = "typename Self, typename Other, typename TFunc",
    });
}

auto equivalent_nodes(TypeRef const& equivalent_reference,
                      std::vector<ResolvedMember> const& members,
                      std::map<std::string, CppType> const& types) -> Nodes {
    auto const equivalent{resolve_type(equivalent_reference, types)};
    std::vector<std::string> values;
    for (auto const& member : members) {
        values.push_back(equivalent_expression(member, "index"));
    }
    return {
        UsingDeclaration{"equivalent_type", equivalent},
        lines(2),
        header_function(FunctionSpec{
            .name = "operator[]",
            .return_type = "auto",
            .parameters = {FunctionParameter{"int32 const", "index"}},
            .body = {raw("return {" + join(values, ", ") + "};")},
            .suffix = " const -> " + equivalent.spelling,
            .is_inline = true,
        }),
        lines(1),
        header_function(FunctionSpec{
            .name = "at",
            .return_type = "auto",
            .parameters = {FunctionParameter{"int32 const", "index"}},
            .body = {raw("validate_array_sizes();\n"
                         "check(index >= 0);\n"
                         "check(index < num());\n"
                         "return (*this)[index];",
                         {check_dependency})},
            .suffix = " const -> " + equivalent.spelling,
            .is_inline = true,
        }),
    };
}

auto view_specs(std::string const& view_name,
                std::string const& const_view_name,
                std::vector<ResolvedMember> const& members,
                bool const_only) -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto add = [&](std::string name,
                   CppType return_type,
                   std::vector<FunctionParameter> parameters,
                   std::string body,
                   std::string suffix,
                   std::vector<TypeDependency> dependencies = {}) {
        result.push_back(FunctionSpec{
            .name = std::move(name),
            .return_type = std::move(return_type),
            .parameters = std::move(parameters),
            .body = {raw(std::move(body), std::move(dependencies))},
            .suffix = std::move(suffix),
        });
    };

    if (!const_only) {
        add("get_view", "auto", {}, "return get_view(0, num());", " -> View");
        std::vector<std::string> values;
        for (auto const& member : members) {
            values.push_back("    " + view_expression(member, false) + ",");
        }
        add("get_view",
            "auto",
            {FunctionParameter{"int32 const", "offset"},
             FunctionParameter{"int32 const", "count"}},
            "return View{\n" + join_lines(values) + "\n};",
            " -> View");
    }

    add("get_view", "auto", {}, "return get_view(0, num());", " const -> ConstView");
    std::vector<std::string> const_values;
    for (auto const& member : members) {
        const_values.push_back("    " + view_expression(member, true) + ",");
    }
    add("get_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        " const -> ConstView");
    add("get_const_view", "auto", {}, "return get_const_view(0, num());", " const -> ConstView");
    add("get_const_view",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        " const -> ConstView");
    add("num",
        "auto",
        {},
        "return ml::num(" + members.front().name + ");",
        " const noexcept -> int32",
        {container_ops});
    add("is_empty", "auto", {}, "return num() == 0;", " const noexcept -> bool");
    std::vector<std::string> nums;
    for (auto const& member : members) {
        nums.push_back("    ml::num(" + member.name + "),");
    }
    add("validate_array_sizes",
        "void",
        {},
        "ml::fatal_if_nums_not_equal({\n" + join_lines(nums) + "\n});",
        " const",
        {array_checks, container_ops});
    if (!const_only) {
        add("slice",
            "auto",
            {FunctionParameter{"int32 const", "offset"},
             FunctionParameter{"int32 const", "count"}},
            "return get_view(offset, count);",
            " -> View");
        add("left",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(0, count);",
            " -> View");
        add("right",
            "auto",
            {FunctionParameter{"int32 const", "count"}},
            "return slice(num() - count, count);",
            " -> View");
    }
    add("slice",
        "auto",
        {FunctionParameter{"int32 const", "offset"},
         FunctionParameter{"int32 const", "count"}},
        "return get_view(offset, count);",
        " const -> ConstView");
    add("left",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(0, count);",
        " const -> ConstView");
    add("right",
        "auto",
        {FunctionParameter{"int32 const", "count"}},
        "return slice(num() - count, count);",
        " const -> ConstView");
    static_cast<void>(view_name);
    static_cast<void>(const_view_name);
    return result;
}

auto function_nodes(std::vector<FunctionSpec> const& specs, bool definitions,
                    std::string const& owner = {}) -> Nodes {
    Nodes result;
    auto const count{specs.size()};
    for (std::size_t index{0}; index < count; ++index) {
        result.push_back(definitions ? definition(specs[index], owner) : declaration(specs[index]));
        if (index + 1 < count) {
            result.push_back(lines(1));
        }
    }
    return result;
}

auto view_struct(std::string name,
                 std::string const& view_name,
                 std::string const& const_view_name,
                 std::vector<ResolvedMember> const& members,
                 std::optional<TypeRef> const& equivalent,
                 std::map<std::string, CppType> const& types,
                 std::optional<std::string> const& export_specifier,
                 bool const_only) -> Struct {
    Nodes nodes{
        UsingDeclaration{"View", CppType{view_name}},
        lines(1),
        UsingDeclaration{"ConstView", CppType{const_view_name}},
        lines(2),
    };
    if (equivalent.has_value()) {
        auto extra = equivalent_nodes(*equivalent, members, types);
        nodes.insert(nodes.end(), extra.begin(), extra.end());
        nodes.push_back(lines(2));
    }
    nodes.push_back(apply_arrays_function(members));
    nodes.push_back(lines(2));
    auto common{function_nodes(view_specs(view_name, const_view_name, members, const_only), false)};
    nodes.insert(nodes.end(), common.begin(), common.end());
    nodes.push_back(lines(2));
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.push_back(Member{const_only ? members[index].const_view_type : members[index].view_type,
                               members[index].name});
        if (index + 1 < members.size()) {
            nodes.push_back(lines(1));
        }
    }
    return Struct{
        .name = std::move(name),
        .children = std::move(nodes),
        .export_specifier = export_specifier,
    };
}

auto storage_operation_specs(SoaSchema const& schema,
                             std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec> {
    std::vector<FunctionSpec> result;
    auto member_calls = [&](std::string const& function, std::vector<std::string> const& arguments) {
        std::vector<std::string> calls;
        for (auto const& member : members) {
            auto values{std::vector<std::string>{member.name}};
            values.insert(values.end(), arguments.begin(), arguments.end());
            calls.push_back(function + "(" + join(values, ", ") + ");");
        }
        return join_lines(calls);
    };
    auto contains = [&](StorageOperation operation) {
        return std::ranges::find(schema.operations, operation) != schema.operations.end();
    };
    if (contains(StorageOperation::reset)) {
        result.push_back(FunctionSpec{.name = "reset",
                                      .return_type = "void",
                                      .body = {raw(member_calls("ml::reset", {}), {container_ops})}});
    }
    if (contains(StorageOperation::reserve)) {
        result.push_back(FunctionSpec{
            .name = "reserve",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = {raw(member_calls("ml::reserve", {"count"}), {container_ops})},
        });
    }
    if (contains(StorageOperation::add_uninitialised)) {
        result.push_back(FunctionSpec{
            .name = "add_uninitialised",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = {raw(member_calls("ml::add_uninitialised", {"count"}), {container_ops})},
        });
    }
    if (contains(StorageOperation::add_defaulted)) {
        result.push_back(FunctionSpec{
            .name = "add_defaulted",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"}},
            .body = {raw(member_calls("ml::add_defaulted", {"count"}), {container_ops})},
        });
    }
    if (contains(StorageOperation::remove_at_swap)) {
        std::vector<std::string> calls;
        for (auto const& member : members) {
            auto const operation{member.container_type.operation(TypeOperation::remove_at_swap)};
            calls.push_back(operation.has_value()
                                ? member.name + "." + *operation +
                                      "(index, count, allow_shrinking);"
                                : "ml::remove_at_swap(" + member.name +
                                      ", index, count, allow_shrinking);");
        }
        result.push_back(FunctionSpec{
            .name = "remove_at_swap",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "index"},
                           FunctionParameter{"int32 const", "count"},
                           FunctionParameter{CppType{"EAllowShrinking const", {allow_shrinking}},
                                             "allow_shrinking"}},
            .body = {raw(join_lines(calls), {container_ops})},
            .is_inline = true,
        });
    }
    if (contains(StorageOperation::set_num)) {
        result.push_back(FunctionSpec{
            .name = "set_num",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "count"},
                           FunctionParameter{CppType{"EAllowShrinking const", {allow_shrinking}},
                                             "allow_shrinking"}},
            .body = {raw(member_calls("ml::set_num", {"count", "allow_shrinking"}),
                         {container_ops})},
        });
    }
    if (contains(StorageOperation::copy_element)) {
        std::vector<std::string> copy_one;
        std::vector<std::string> copy_range;
        for (auto const& member : members) {
            if (schema.copy_element_memberwise) {
                copy_one.push_back(member.name + "[dst_i] = other." + member.name + "[src_i];");
            } else {
                copy_one.push_back("ml::copy_element(" + member.name + ", dst_i, other." +
                                   member.name + ", src_i);");
            }
            copy_range.push_back("ml::copy_elements(" + member.name + ", dst_i, other." +
                                 member.name + ", src_i, count);");
        }
        result.push_back(FunctionSpec{
            .name = "copy_element",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "dst_i"},
                           FunctionParameter{"Other const&", "other"},
                           FunctionParameter{"int32 const", "src_i"}},
            .body = {raw(join_lines(copy_one), {container_ops})},
            .is_inline = true,
            .template_parameters = "typename Other",
        });
        result.push_back(FunctionSpec{
            .name = "copy_elements",
            .return_type = "void",
            .parameters = {FunctionParameter{"int32 const", "dst_i"},
                           FunctionParameter{"Other const&", "other"},
                           FunctionParameter{"int32 const", "src_i"},
                           FunctionParameter{"int32 const", "count"}},
            .body = {raw(join_lines(copy_range), {container_ops})},
            .is_inline = true,
            .template_parameters = "typename Other",
        });
        result.push_back(FunctionSpec{
            .name = "copy_to_tail",
            .return_type = "void",
            .parameters = {FunctionParameter{"Other const&", "other"}},
            .body = {raw("auto const count{other.num()};\ncheck(num() >= count);\n"
                         "copy_elements(num() - count, other, 0, count);",
                         {check_dependency})},
            .is_inline = true,
            .template_parameters = "typename Other",
        });
    }
    if (contains(StorageOperation::append_from)) {
        std::vector<std::string> calls;
        for (auto const& member : members) {
            calls.push_back("ml::append_from(" + member.name + ", other." + member.name + ");");
        }
        result.push_back(FunctionSpec{
            .name = "append_from",
            .return_type = "void",
            .parameters = {FunctionParameter{"Other const&", "other"}},
            .body = {raw(join_lines(calls), {container_ops, soa_concepts})},
            .is_inline = true,
            .template_parameters = "typename Other",
            .requires_clause = "ml::SupportsApplyArrayPairsWith<" + schema.name + ", Other>",
        });
    }
    return result;
}

auto permutation_specs(std::vector<ResolvedMember> const& members) -> std::vector<FunctionSpec> {
    std::vector<std::string> apply{
        "validate_array_sizes();", "check(indices.Num() == num());"};
    for (auto const& member : members) {
        apply.push_back("ml::apply_permutation(" + member.name + ", indices);");
    }
    std::string const common{
        "validate_array_sizes();\n"
        "auto const n{num()};\n"
        "check(scratch_indices.Num() == n);\n"
        "ml::fill_indices(scratch_indices);\n"
        "// indices[new_index] is the old row index that belongs at new_index.\n"};
    return {
        FunctionSpec{
            .name = "apply_permutation",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "indices"}},
            .body = {raw(join_lines(apply), {soa_permutation, check_dependency})},
        },
        FunctionSpec{
            .name = "sort",
            .return_type = "void",
            .parameters = {FunctionParameter{"Compare&&", "compare"},
                           FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "scratch_indices"}},
            .body = {raw(common +
                             "scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) {\n"
                             "    return compare(*this, lhs, rhs);\n"
                             "});\n"
                             "apply_permutation(scratch_indices);",
                         {fill_indices, check_dependency})},
            .is_inline = true,
            .template_parameters = "typename Compare",
        },
        FunctionSpec{
            .name = "sort",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "scratch_indices"}},
            .body = {raw(common +
                             "scratch_indices.Sort([this](int32 const lhs, int32 const rhs) {\n"
                             "    return Compare(*this, lhs, rhs);\n"
                             "});\n"
                             "apply_permutation(scratch_indices);",
                         {fill_indices, check_dependency})},
            .is_inline = true,
            .template_parameters = "auto Compare",
        },
    };
}

auto dependency_for_key(std::string const& key,
                        std::map<std::string, CppType> const& types) -> TypeDependency {
    auto const found{types.find(key)};
    if (found == types.end() || found->second.dependencies.empty()) {
        throw std::invalid_argument{"Unknown dependency type: " + key};
    }
    return found->second.dependencies.front();
}

auto custom_function_spec(FunctionSchema const& schema,
                          std::map<std::string, CppType> const& types) -> FunctionSpec {
    std::vector<FunctionParameter> parameters;
    for (auto const& parameter : schema.parameters) {
        auto resolved{resolve_type(parameter.type, types)};
        if (parameter.default_value.has_value()) {
            parameters.emplace_back(std::move(resolved), parameter.name, *parameter.default_value);
        } else {
            parameters.emplace_back(std::move(resolved), parameter.name);
        }
    }
    std::vector<TypeDependency> dependencies;
    for (auto const& key : schema.dependencies) {
        dependencies.push_back(dependency_for_key(key, types));
    }
    return FunctionSpec{
        .name = schema.name,
        .return_type = resolve_type(schema.return_type, types),
        .parameters = std::move(parameters),
        .body = {raw(join_lines(schema.body_lines), std::move(dependencies))},
        .suffix = schema.suffix,
        .is_static = schema.is_static,
        .is_inline = schema.is_inline,
        .template_parameters = schema.template_parameters,
        .requires_clause = schema.requires_clause,
    };
}

auto storage_struct(SoaSchema const& schema,
                    std::vector<ResolvedMember> const& members,
                    std::string const& view_name,
                    std::string const& const_view_name,
                    std::map<std::string, CppType> const& types,
                    std::vector<FunctionSpec>& custom_source) -> Struct {
    Nodes nodes{
        UsingDeclaration{"View", CppType{view_name}},
        lines(1),
        UsingDeclaration{"ConstView", CppType{const_view_name}},
        lines(2),
    };
    if (schema.equivalent_type.has_value()) {
        auto extra = equivalent_nodes(*schema.equivalent_type, members, types);
        nodes.insert(nodes.end(), extra.begin(), extra.end());
        nodes.push_back(lines(2));
    }
    for (auto const& declaration_text : schema.using_declarations) {
        nodes.push_back(raw("using " + declaration_text + ";"));
        nodes.push_back(lines(2));
    }
    for (auto const& function : schema.functions) {
        auto spec{custom_function_spec(function, types)};
        if (function.definition_in_source) {
            custom_source.push_back(spec);
            nodes.push_back(declaration(spec));
        } else {
            nodes.push_back(header_function(spec));
        }
        nodes.push_back(lines(2));
    }
    auto operations{storage_operation_specs(schema, members)};
    for (auto const& spec : operations) {
        nodes.push_back(spec.is_inline ? header_function(spec) : declaration(spec));
        nodes.push_back(lines(2));
    }
    auto permutations{permutation_specs(members)};
    for (auto const& spec : permutations) {
        nodes.push_back(spec.is_inline ? header_function(spec) : declaration(spec));
        nodes.push_back(lines(2));
    }
    nodes.push_back(apply_arrays_function(members));
    nodes.push_back(lines(2));
    nodes.push_back(apply_array_pairs_function(members));
    nodes.push_back(lines(2));
    auto common{function_nodes(view_specs(view_name, const_view_name, members, false), false)};
    nodes.insert(nodes.end(), common.begin(), common.end());
    nodes.push_back(lines(2));
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.push_back(Member{members[index].container_type, members[index].name});
        if (index + 1 < members.size()) {
            nodes.push_back(lines(1));
        }
    }
    return Struct{
        .name = schema.name,
        .children = std::move(nodes),
        .export_specifier = schema.export_specifier,
    };
}

auto lower_soa(SoaSchema const& schema,
               std::map<std::string, CppType> const& types) -> LoweredSoa {
    auto const members{resolve_members(schema, types)};
    auto const view_name{schema.view_name.value_or(schema.name + "View")};
    auto const const_view_name{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<FunctionSpec> custom_source;
    auto storage{storage_struct(
        schema, members, view_name, const_view_name, types, custom_source)};
    Nodes header{
        ForwardDeclaration{view_name},
        lines(1),
        ForwardDeclaration{const_view_name},
        lines(2),
        view_struct(const_view_name,
                    view_name,
                    const_view_name,
                    members,
                    schema.equivalent_type,
                    types,
                    schema.export_specifier,
                    true),
        lines(2),
        view_struct(view_name,
                    view_name,
                    const_view_name,
                    members,
                    schema.equivalent_type,
                    types,
                    schema.export_specifier,
                    false),
        lines(2),
        std::move(storage),
    };

    Nodes source;
    for (auto const& spec : custom_source) {
        source.push_back(definition(spec, schema.name));
        source.push_back(lines(2));
    }
    auto append_definitions = [&](std::vector<FunctionSpec> const& specs, std::string const& owner) {
        for (auto const& spec : specs) {
            source.push_back(definition(spec, owner));
            source.push_back(lines(2));
        }
    };
    append_definitions(view_specs(view_name, const_view_name, members, true), const_view_name);
    append_definitions(view_specs(view_name, const_view_name, members, false), view_name);
    for (auto const& spec : storage_operation_specs(schema, members)) {
        if (!spec.is_inline) {
            source.push_back(definition(spec, schema.name));
            source.push_back(lines(2));
        }
    }
    auto const permutations{permutation_specs(members)};
    source.push_back(definition(permutations.front(), schema.name));
    source.push_back(lines(2));
    append_definitions(view_specs(view_name, const_view_name, members, false), schema.name);
    if (!source.empty()) {
        source.pop_back();
    }
    return LoweredSoa{std::move(header), std::move(source)};
}

auto source_include(ModuleSettings const& settings) -> std::string {
    if (settings.header_include.has_value()) {
        return *settings.header_include;
    }
    return settings.header.filename().generic_string();
}

auto lower_soa_module(SoaModuleSchema const& module,
                      std::map<std::string, CppType> const& types) -> Module {
    std::map<std::string, SoaSchema const*> schemas;
    for (auto const& schema : module.structs) {
        if (!schemas.emplace(schema.name, &schema).second) {
            throw std::invalid_argument{"Duplicate SOA schema name: " + schema.name};
        }
    }
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.push_back(raw(join_lines(module.settings.prelude_lines)));
        header_nodes.push_back(lines(2));
    }
    Nodes definitions;
    for (auto const& schema : module.structs) {
        auto lowered{lower_soa(schema, types)};
        if (schema.fixed.has_value()) {
            lowered.header.push_back(lines(2));
            auto fixed{fixed_nodes(build_fixed_layout(schema, schemas, types), types)};
            lowered.header.insert(lowered.header.end(), fixed.begin(), fixed.end());
        }
        if (!definitions.empty()) {
            definitions.push_back(lines(2));
        }
        definitions.insert(definitions.end(), lowered.header.begin(), lowered.header.end());
    }
    if (module.settings.namespace_name.has_value()) {
        header_nodes.push_back(Namespace{*module.settings.namespace_name, std::move(definitions)});
    } else {
        header_nodes.insert(header_nodes.end(), definitions.begin(), definitions.end());
    }
    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (module.settings.source.has_value()) {
        Nodes source_definitions;
        for (auto const& schema : module.structs) {
            auto lowered{lower_soa(schema, types)};
            if (!source_definitions.empty()) {
                source_definitions.push_back(lines(2));
            }
            source_definitions.insert(source_definitions.end(),
                                      lowered.source.begin(),
                                      lowered.source.end());
        }
        if (module.settings.namespace_name.has_value()) {
            source_definitions = {
                Namespace{*module.settings.namespace_name, std::move(source_definitions)}};
        }
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = {
                Include{source_include(module.settings), false},
                lines(2),
                IncludeDependencies{},
                lines(2),
                raw(""),
            },
            .pragma_once = false,
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        };
        auto& nodes{result.source->nodes};
        nodes.pop_back();
        nodes.insert(nodes.end(), source_definitions.begin(), source_definitions.end());
    }
    return result;
}

auto lower_umbrella(UmbrellaModuleSchema const& module) -> Module {
    Nodes nodes;
    for (auto const& header : module.headers) {
        nodes.push_back(Include{header, false});
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
}

auto homogeneous_view_text(HomogeneousLayoutSchema const& layout) -> std::string {
    auto const view_name{"T" + layout.name + "View"};
    auto const components{join(layout.components, ", ")};
    std::vector<std::string> member_lines;
    for (auto const& component : layout.components) {
        member_lines.push_back("    TArrayView<T> " + component + ";");
    }
    auto slice_values = [&](std::string const& operation) {
        std::vector<std::string> values;
        for (auto const& component : layout.components) {
            values.push_back(component + "." + operation);
        }
        return join(values, ", ");
    };
    return "template <typename T>\n"
           "struct " + view_name + " {\n"
           "    using size_type = TArrayView<T>::SizeType;\n"
           "    using value_type = std::remove_const_t<T>;\n"
           "    using View = " + view_name + "<T>;\n"
           "    using ConstView = " + view_name + "<value_type const>;\n\n" +
           join_lines(member_lines) + "\n\n"
           "    auto get_view() -> View { return View{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) -> View {\n"
           "        return get_view().slice(offset, count);\n"
           "    }\n"
           "    auto get_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) const -> ConstView {\n"
           "        return get_view().slice(offset, count);\n"
           "    }\n"
           "    auto get_const_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_const_view(size_type const offset, size_type const count) const -> ConstView {\n"
           "        return get_const_view().slice(offset, count);\n"
           "    }\n"
           "    template <typename TFunc>\n"
           "    auto apply_arrays(TFunc&& func) -> decltype(auto) {\n"
           "        return std::forward<TFunc>(func)(" + components + ");\n"
           "    }\n"
           "    template <typename TFunc>\n"
           "    auto apply_arrays(TFunc&& func) const -> decltype(auto) {\n"
           "        return std::forward<TFunc>(func)(" + components + ");\n"
           "    }\n"
           "    auto num() const -> size_type { return " + layout.components.front() + ".Num(); }\n"
           "    auto is_empty() const -> bool { return num() == 0; }\n"
           "    auto slice(size_type const offset, size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Slice(offset, count)") + "};\n"
           "    }\n"
           "    auto left(size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Left(count)") + "};\n"
           "    }\n"
           "    auto right(size_type const count) const -> " + view_name + " {\n"
           "        return " + view_name + "{" + slice_values("Right(count)") + "};\n"
           "    }\n"
           "};";
}

auto homogeneous_storage_text(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> std::string {
    auto const value_type{resolve_type(value.type, types)};
    auto const view_name{"T" + layout.name + "View"};
    auto const storage_name{"F" + layout.name + value.suffix};
    auto const export_prefix{layout.export_specifier.has_value()
                                 ? *layout.export_specifier + " "
                                 : std::string{}};
    std::vector<std::string> data_members;
    std::vector<std::string> const_data_members;
    std::vector<std::string> arrays;
    std::vector<std::string> pointers;
    std::vector<std::string> calls;
    for (auto const& component : layout.components) {
        data_members.push_back("        value_type* " + component + ";");
        const_data_members.push_back("        value_type const* " + component + ";");
        arrays.push_back("    TArray<value_type> " + component + ";");
        pointers.push_back(component + ".GetData()");
    }
    auto each = [&](std::string const& expression) {
        std::vector<std::string> lines;
        for (auto const& component : layout.components) {
            auto line{expression};
            auto marker{line.find("{}")};
            while (marker != std::string::npos) {
                line.replace(marker, 2, component);
                marker = line.find("{}", marker + component.size());
            }
            lines.push_back("        " + line);
        }
        return join_lines(lines);
    };
    auto const components{join(layout.components, ", ")};
    std::vector<std::string> validation;
    for (std::size_t index{1}; index < layout.components.size(); ++index) {
        validation.push_back("        check(" + layout.components[index] + ".Num() == " +
                             layout.components.front() + ".Num());");
    }
    static_cast<void>(calls);
    return "struct " + export_prefix + storage_name + " {\n"
           "    using value_type = " + value_type.spelling + ";\n"
           "    using size_type = TArray<value_type>::SizeType;\n"
           "    using View = " + view_name + "<value_type>;\n"
           "    using ConstView = " + view_name + "<value_type const>;\n\n"
           "    struct Data {\n" + join_lines(data_members) + "\n    };\n\n"
           "    struct ConstData {\n" + join_lines(const_data_members) + "\n    };\n\n" +
           join_lines(arrays) + "\n\n"
           "    auto get_data() -> Data { return Data{" + join(pointers, ", ") + "}; }\n"
           "    auto get_data() const -> ConstData { return ConstData{" + join(pointers, ", ") + "}; }\n"
           "    auto get_view() -> View { return View{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) -> View { return get_view().slice(offset, count); }\n"
           "    auto get_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_view(size_type const offset, size_type const count) const -> ConstView { return get_view().slice(offset, count); }\n"
           "    auto get_const_view() const -> ConstView { return ConstView{" + components + "}; }\n"
           "    auto get_const_view(size_type const offset, size_type const count) const -> ConstView { return get_const_view().slice(offset, count); }\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) -> decltype(auto) { return std::forward<TFunc>(func)(" + components + "); }\n"
           "    template <typename TFunc> auto apply_arrays(TFunc&& func) const -> decltype(auto) { return std::forward<TFunc>(func)(" + components + "); }\n"
           "    auto num() const -> size_type { return " + layout.components.front() + ".Num(); }\n"
           "    void validate_array_sizes() const {\n" + join_lines(validation) + "\n    }\n"
           "    auto is_empty() const -> bool { return num() == 0; }\n"
           "    template <typename Other> auto copy_element(size_type const dst_i, Other const& src, size_type const src_i) -> void {\n" +
           each("{}[dst_i] = src.{}[src_i];") + "\n    }\n"
           "    template <typename Other> auto copy_elements(size_type const dst_i, Other const& src, size_type const src_i, size_type const count) -> void {\n"
           "        for (auto i{0}; i < count; ++i) {\n" +
           each("    {}[dst_i + i] = src.{}[src_i + i];") + "\n        }\n    }\n"
           "    template <typename Other> auto copy_to_tail(Other const& src) -> void {\n"
           "        auto const count{src.num()};\n        check(num() >= count);\n"
           "        copy_elements(num() - count, src, 0, count);\n    }\n"
           "    template <typename Other> void append_from(Other const& other) {\n" +
           each("{}.Append(other.{});") + "\n    }\n"
           "    void apply_permutation(TArrayView<int32> indices);\n"
           "    template <typename Compare> void sort(Compare&& compare, TArrayView<int32> scratch_indices) {\n"
           "        validate_array_sizes();\n        auto const n{num()};\n"
           "        check(scratch_indices.Num() == n);\n        ml::fill_indices(scratch_indices);\n"
           "        scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) { return compare(*this, lhs, rhs); });\n"
           "        apply_permutation(scratch_indices);\n    }\n"
           "    template <auto Compare> void sort(TArrayView<int32> scratch_indices) {\n"
           "        validate_array_sizes();\n        auto const n{num()};\n"
           "        check(scratch_indices.Num() == n);\n        ml::fill_indices(scratch_indices);\n"
           "        scratch_indices.Sort([this](int32 const lhs, int32 const rhs) { return Compare(*this, lhs, rhs); });\n"
           "        apply_permutation(scratch_indices);\n    }\n"
           "    auto reset() -> void {\n" + each("{}.Reset();") + "\n    }\n"
           "    auto empty() -> void {\n" + each("{}.Empty();") + "\n    }\n"
           "    auto reserve(size_type const count) -> void {\n" + each("{}.Reserve(count);") + "\n    }\n"
           "    auto set_num(size_type const count, EAllowShrinking const allow_shrinking) -> void {\n" + each("{}.SetNum(count, allow_shrinking);") + "\n    }\n"
           "    auto set_num_uninitialised(size_type const count) -> void {\n" + each("{}.SetNumUninitialized(count);") + "\n    }\n"
           "    auto add_uninitialised(size_type const count) -> void {\n" + each("{}.AddUninitialized(count);") + "\n    }\n"
           "    auto remove_at_swap(size_type const index, size_type const count, EAllowShrinking const allow_shrinking) -> void {\n" + each("{}.RemoveAtSwap(index, count, allow_shrinking);") + "\n    }\n"
           "    auto add_zeroed(size_type const count) -> void {\n" + each("{}.AddZeroed(count);") + "\n    }\n"
           "    auto add_defaulted(size_type const count) -> void {\n" + each("{}.AddDefaulted(count);") + "\n    }\n"
           "};";
}

auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module {
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    Nodes source_nodes{Include{source_include(module.settings), false},
                       lines(2),
                       IncludeDependencies{},
                       lines(2)};
    for (std::size_t layout_index{0}; layout_index < module.layouts.size(); ++layout_index) {
        auto const& layout{module.layouts[layout_index]};
        header_nodes.push_back(raw(homogeneous_view_text(layout),
                                   {tarray_view, std_remove_const, std_forward}));
        header_nodes.push_back(lines(2));
        for (std::size_t value_index{0}; value_index < layout.value_types.size(); ++value_index) {
            auto const& value{layout.value_types[value_index]};
            auto value_type{resolve_type(value.type, types)};
            std::vector<TypeDependency> dependencies{
                tarray, tarray_view, allow_shrinking, check_dependency, fill_indices, std_forward};
            dependencies.insert(dependencies.end(),
                                value_type.dependencies.begin(),
                                value_type.dependencies.end());
            header_nodes.push_back(raw(homogeneous_storage_text(layout, value, types),
                                       std::move(dependencies)));
            if (value_index + 1 < layout.value_types.size()) {
                header_nodes.push_back(lines(2));
            }

            auto const storage_name{"F" + layout.name + value.suffix};
            std::vector<std::string> body{
                "validate_array_sizes();", "check(indices.Num() == num());"};
            for (auto const& component : layout.components) {
                body.push_back("ml::apply_permutation(" + component + ", indices);");
            }
            source_nodes.push_back(raw(
                "void " + storage_name + "::apply_permutation(TArrayView<int32> indices) {\n" +
                    join_lines([&] {
                        std::vector<std::string> indented;
                        for (auto const& line : body) {
                            indented.push_back("    " + line);
                        }
                        return indented;
                    }()) +
                    "\n}",
                {tarray_view, check_dependency, soa_permutation}));
            if (layout_index + 1 < module.layouts.size() ||
                value_index + 1 < layout.value_types.size()) {
                source_nodes.push_back(lines(2));
            }
        }
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{
                            .path = *module.settings.source,
                            .nodes = std::move(source_nodes),
                            .pragma_once = false,
                            .clang_format_off = true,
                            .include_order = module.settings.include_order,
                        }}
                      : std::nullopt,
    };
}

auto lower_vector_module(VectorModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    auto const equivalent{resolve_type(module.equivalent_type, types)};
    std::vector<SoaMemberSchema> members;
    for (auto const& component : module.components) {
        members.push_back(SoaMemberSchema{component, SoaMemberKind::array, module.value_type});
    }
    std::vector<ParameterSchema> add_parameters;
    for (auto const& component : module.components) {
        add_parameters.push_back(ParameterSchema{
            TypeRef{.name = "value_type", .suffix = " const"},
            std::string(1, component.front()),
        });
    }
    std::vector<std::string> add_body{
        "auto const index{" + module.components.front() + ".Add(" +
            std::string(1, module.components.front().front()) + ")};"};
    for (std::size_t index{1}; index < module.components.size(); ++index) {
        add_body.push_back(module.components[index] + ".Add(" +
                           std::string(1, module.components[index].front()) + ");");
    }
    add_body.emplace_back("return index;");
    std::vector<std::string> equivalent_arguments;
    static std::vector<std::string> const axes{"X", "Y", "Z"};
    for (std::size_t index{0}; index < module.components.size(); ++index) {
        equivalent_arguments.push_back("value." + axes[index]);
    }
    std::vector<FunctionSchema> functions{
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return Data{" + [&] {
                           std::vector<std::string> values;
                           for (auto const& component : module.components) {
                               values.push_back(component + ".GetData()");
                           }
                           return join(values, ", ");
                       }() + "};"},
                       .suffix = " -> Data",
                       .is_inline = true},
        FunctionSchema{.name = "get_data",
                       .return_type = TypeRef{"auto"},
                       .body_lines = {"return ConstData{" + [&] {
                           std::vector<std::string> values;
                           for (auto const& component : module.components) {
                               values.push_back(component + ".GetData()");
                           }
                           return join(values, ", ");
                       }() + "};"},
                       .suffix = " const -> ConstData",
                       .is_inline = true},
        FunctionSchema{.name = "add",
                       .return_type = TypeRef{"auto"},
                       .parameters = std::move(add_parameters),
                       .body_lines = std::move(add_body),
                       .suffix = " -> size_type",
                       .is_inline = true},
        FunctionSchema{.name = "add",
                       .return_type = TypeRef{"auto"},
                       .parameters = {ParameterSchema{
                           TypeRef{.name = module.equivalent_type.name,
                                   .suffix = " const&"},
                           "value"}},
                       .body_lines = {"return add(" + join(equivalent_arguments, ", ") + ");"},
                       .suffix = " -> size_type",
                       .is_inline = true},
    };
    for (auto const& [name, method] : std::vector<std::pair<std::string, std::string>>{
             {"empty", "Empty()"},
             {"set_num_uninitialised", "SetNumUninitialized(count)"},
             {"add_zeroed", "AddZeroed(count)"}}) {
        std::vector<std::string> body;
        for (auto const& component : module.components) {
            body.push_back(component + "." + method + ";");
        }
        functions.push_back(FunctionSchema{
            .name = name,
            .return_type = TypeRef{"void"},
            .parameters = name == "empty"
                              ? std::vector<ParameterSchema>{}
                              : std::vector<ParameterSchema>{ParameterSchema{
                                    TypeRef{.name = "size_type", .suffix = " const"},
                                    "count"}},
            .body_lines = std::move(body),
            .is_inline = true,
        });
    }
    SoaSchema schema{
        .name = module.storage_name,
        .members = std::move(members),
        .operations = all_storage_operations(),
        .export_specifier = module.export_specifier,
        .functions = std::move(functions),
        .using_declarations = {"value_type = " + resolve_type(module.value_type, types).spelling,
                               "size_type = TArray<value_type>::SizeType"},
        .equivalent_type = module.equivalent_type,
        .copy_element_memberwise = true,
        .fixed = module.fixed,
    };
    auto lowered{lower_soa(schema, types)};
    auto* storage{lowered.header.back().get_if<Struct>()};
    if (storage == nullptr) {
        throw std::logic_error{"Vector SOA lowering did not produce a storage struct"};
    }
    Nodes data_nodes{
        Struct{.name = "Data",
               .children = [&] {
                   Nodes values;
                   auto const pointer_type{resolve_type(module.value_type, types).spelling + "*"};
                   for (auto const& component : module.components) {
                       values.push_back(Member{CppType{pointer_type}, component});
                   }
                   return values;
               }()},
        lines(2),
        Struct{.name = "ConstData",
               .children = [&] {
                   Nodes values;
                   auto const pointer_type{resolve_type(module.value_type, types).spelling +
                                           " const*"};
                   for (auto const& component : module.components) {
                       values.push_back(Member{CppType{pointer_type}, component});
                   }
                   return values;
               }()},
        lines(2),
    };
    storage->children.insert(storage->children.begin() + 4,
                             data_nodes.begin(),
                             data_nodes.end());
    if (schema.fixed.has_value()) {
        std::map<std::string, SoaSchema const*> const schemas{{schema.name, &schema}};
        lowered.header.push_back(lines(2));
        auto fixed{fixed_nodes(build_fixed_layout(schema, schemas, types), types)};
        lowered.header.insert(lowered.header.end(), fixed.begin(), fixed.end());
    }
    Nodes header_nodes{IncludeDependencies{}, lines(2)};
    header_nodes.insert(header_nodes.end(), lowered.header.begin(), lowered.header.end());
    Nodes source_nodes{Include{source_include(module.settings), false},
                       lines(2),
                       IncludeDependencies{},
                       lines(2)};
    source_nodes.insert(source_nodes.end(), lowered.source.begin(), lowered.source.end());
    return Module{
        .name = module.settings.name,
        .header = CppFile{.path = module.settings.header,
                          .nodes = std::move(header_nodes),
                          .clang_format_off = true,
                          .include_order = module.settings.include_order},
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{.path = *module.settings.source,
                                                       .nodes = std::move(source_nodes),
                                                       .pragma_once = false,
                                                       .clang_format_off = true,
                                                       .include_order =
                                                           module.settings.include_order}}
                      : std::nullopt,
    };
}

auto qualify(CppType type, std::string const& suffix) -> CppType {
    type.spelling += suffix;
    return type;
}

auto lower_facade_module(FacadeModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    auto const& facade{module.facade};
    if (facade.methods.empty()) {
        throw std::invalid_argument{"Facade '" + facade.name + "' must have methods"};
    }
    auto target_type{resolve_type(facade.target_type, types)};
    auto const definitions_in_source{facade.definitions_in_source};
    auto bind{FunctionSpec{
        .name = "bind",
        .return_type = "void",
        .parameters = {FunctionParameter{qualify(target_type, "&"), "new_target"}},
        .body = {raw(facade.target_member_name + " = &new_target;")},
        .is_inline = !definitions_in_source,
    }};
    std::vector<FunctionSpec> methods;
    for (auto const& method : facade.methods) {
        std::vector<FunctionParameter> parameters;
        std::vector<std::string> arguments;
        for (auto const& parameter : method.parameters) {
            auto type{resolve_type(parameter.type, types)};
            if (parameter.default_value.has_value()) {
                parameters.emplace_back(std::move(type), parameter.name, *parameter.default_value);
            } else {
                parameters.emplace_back(std::move(type), parameter.name);
            }
            arguments.push_back(parameter.name);
        }
        std::vector<TypeDependency> validation_dependencies;
        for (auto const& key : facade.validation_dependencies) {
            validation_dependencies.push_back(dependency_for_key(key, types));
        }
        std::vector<std::string> body{facade.validation_lines};
        auto call{facade.target_member_name + "->" + method.target_name.value_or(method.name) +
                  "(" + join(arguments, ", ") + ");"};
        auto const return_type{resolve_type(method.return_type, types)};
        if (return_type.spelling != "void") {
            call = "return " + call;
        }
        body.push_back(std::move(call));
        methods.push_back(FunctionSpec{
            .name = method.name,
            .return_type = return_type,
            .parameters = std::move(parameters),
            .body = {raw(join_lines(body), std::move(validation_dependencies))},
            .suffix = method.suffix,
            .is_inline = !definitions_in_source,
        });
    }

    Nodes public_nodes;
    Nodes private_nodes;
    auto add_method = [&](std::string const& access, FunctionSpec const& spec) {
        auto& destination{access == "public" ? public_nodes : private_nodes};
        destination.push_back(spec.is_inline ? header_function(spec) : declaration(spec));
        destination.push_back(lines(2));
    };
    add_method(facade.bind_access, bind);
    for (auto const& method : methods) {
        add_method(facade.method_access, method);
    }
    for (auto const& friend_name : facade.friends) {
        private_nodes.push_back(raw("friend class " + friend_name + ";"));
        private_nodes.push_back(lines(1));
    }
    private_nodes.push_back(Member{qualify(target_type, "*"), facade.target_member_name, "nullptr"});

    Nodes class_nodes;
    if (!public_nodes.empty()) {
        if (public_nodes.back().is<NewLines>()) {
            public_nodes.pop_back();
        }
        class_nodes.push_back(raw("public:"));
        class_nodes.push_back(lines(1));
        class_nodes.insert(class_nodes.end(), public_nodes.begin(), public_nodes.end());
        class_nodes.push_back(lines(2));
    }
    if (private_nodes.back().is<NewLines>()) {
        private_nodes.pop_back();
    }
    class_nodes.push_back(raw("private:"));
    class_nodes.push_back(lines(1));
    class_nodes.insert(class_nodes.end(), private_nodes.begin(), private_nodes.end());

    Nodes header_nodes{
        IncludeDependencies{},
        lines(2),
        Struct{
            .name = facade.name,
            .children = std::move(class_nodes),
            .export_specifier = facade.export_specifier,
            .record_kind = "class",
        },
    };
    if (module.settings.namespace_name.has_value()) {
        auto declaration_node{std::move(header_nodes.back())};
        header_nodes.pop_back();
        header_nodes.push_back(
            Namespace{*module.settings.namespace_name, {std::move(declaration_node)}});
    }
    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (module.settings.source.has_value()) {
        Nodes definitions{definition(bind, facade.name), lines(2)};
        for (std::size_t index{0}; index < methods.size(); ++index) {
            definitions.push_back(definition(methods[index], facade.name));
            if (index + 1 < methods.size()) {
                definitions.push_back(lines(2));
            }
        }
        if (module.settings.namespace_name.has_value()) {
            definitions = {Namespace{*module.settings.namespace_name, std::move(definitions)}};
        }
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = {Include{source_include(module.settings), false},
                      lines(2),
                      IncludeDependencies{},
                      lines(2)},
            .pragma_once = false,
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        };
        result.source->nodes.insert(result.source->nodes.end(),
                                    definitions.begin(),
                                    definitions.end());
    }
    return result;
}

} // namespace

auto lower_modules(Manifest const& manifest) -> std::vector<Module> {
    std::vector<Module> result;
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    result.push_back(lower_soa_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    result.push_back(lower_facade_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    result.push_back(lower_homogeneous_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    result.push_back(lower_vector_module(module, manifest.types));
                } else if constexpr (std::is_same_v<T, UmbrellaModuleSchema>) {
                    result.push_back(lower_umbrella(module));
                } else {
                    throw std::invalid_argument{"Module kind is not lowered yet: " +
                                                module.settings.name};
                }
            },
            schema);
    }
    return result;
}

auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile> {
    std::vector<GeneratedFile> result;
    std::set<std::filesystem::path> paths;
    for (auto const& module : modules) {
        for (auto const* file : {module.header ? &*module.header : nullptr,
                                module.source ? &*module.source : nullptr}) {
            if (file == nullptr) {
                continue;
            }
            auto const normalized{file->path.lexically_normal()};
            if (!paths.insert(normalized).second) {
                throw std::invalid_argument{"Duplicate generated output path: " +
                                            normalized.string()};
            }
            result.push_back(GeneratedFile{normalized, render(*file)});
        }
    }
    return result;
}

auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int {
    bool stale{false};
    for (auto const& file : files) {
        auto relative{file.path};
        if (relative.is_absolute()) {
            relative = std::filesystem::relative(relative, project_root);
        }
        auto const destination{output_root / relative};
        std::string current;
        if (std::ifstream input{destination, std::ios::binary}; input) {
            current.assign(std::istreambuf_iterator<char>{input},
                           std::istreambuf_iterator<char>{});
        }
        if (current == file.content) {
            std::cout << "Unchanged " << relative.generic_string() << '\n';
            continue;
        }
        if (check_only) {
            stale = true;
            std::cout << "Stale " << relative.generic_string() << '\n';
            continue;
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output{destination, std::ios::binary | std::ios::trunc};
        if (!output) {
            throw std::runtime_error{"Cannot write generated file: " + destination.string()};
        }
        output << file.content;
        std::cout << "Wrote " << relative.generic_string() << '\n';
    }
    if (stale) {
        std::cout << "Generated files are stale. Run the generate-code CMake workflow.\n";
        return 1;
    }
    return 0;
}

} // namespace codegen
