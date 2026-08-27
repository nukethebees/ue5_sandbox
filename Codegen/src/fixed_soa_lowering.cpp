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

} // namespace

auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       std::map<std::string, CppType> const& types) -> Nodes {
    return fixed_nodes(build_fixed_layout(schema, schemas, types), types);
}

} // namespace codegen::detail
