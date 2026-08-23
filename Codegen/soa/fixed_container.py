from __future__ import annotations

from Codegen.cpp import Raw, TypeDependency, comma_separated, type_spelling

STD_FORWARD = TypeDependency("std::forward", "utility")
ALLOW_SHRINKING = TypeDependency(
    "EAllowShrinking", "Containers/AllowShrinking.h"
)
CHECK = TypeDependency("check", "CoreMinimal.h")
MOVE_TEMP = TypeDependency("MoveTemp", "Templates/UnrealTemplate.h")
from Codegen.soa.model import FixedSoAContainer
from Codegen.soa.fixed_layout import FixedSoALayout


def _fixed_trait_expression(layout: FixedSoALayout, trait: str) -> str:
    return " && ".join(
        f"std::{trait}<{type_spelling(leaf.element_type)}>" for leaf in layout.leaves
    )


def fixed_array_struct(layout: FixedSoALayout, declaration: FixedSoAContainer) -> Raw:
    soa = layout.schema
    storage_name = layout.storage_name
    leaves = layout.leaves
    template_parameters = comma_separated(f"typename TArg{i}" for i in range(len(leaves)))
    function_parameters = comma_separated(
        f"TArg{i}&& new_{leaf.argument_name}" for i, leaf in enumerate(leaves)
    )
    forwarded_arguments = comma_separated(
        f"std::forward<TArg{i}>(new_{leaf.argument_name})"
        for i, leaf in enumerate(leaves)
    )
    constructible = " && ".join(
        f"std::is_constructible_v<{type_spelling(leaf.element_type)}, TArg{i}&&>"
        for i, leaf in enumerate(leaves)
    )
    copy_constructible = _fixed_trait_expression(layout, "is_copy_constructible_v")
    move_constructible = _fixed_trait_expression(layout, "is_move_constructible_v")
    nothrow_move_constructible = _fixed_trait_expression(
        layout, "is_nothrow_move_constructible_v"
    )
    default_constructible = _fixed_trait_expression(layout, "is_default_constructible_v")
    trivial = " && ".join(
        f"(std::is_trivially_copyable_v<{type_spelling(leaf.element_type)}> && "
        f"std::is_trivially_destructible_v<{type_spelling(leaf.element_type)}>)"
        for leaf in leaves
    )
    equivalent_alias = (
        f"\n    using equivalent_type = {type_spelling(soa.equivalent_type)};"
        if soa.equivalent_type is not None
        else ""
    )
    equivalent_access = (
        """

    auto operator[](size_type const index) const -> equivalent_type {
        return get_const_view()[index];
    }
    auto at(size_type const index) const -> equivalent_type {
        check_index(index);
        return (*this)[index];
    }"""
        if soa.equivalent_type is not None
        else ""
    )
    name = declaration.name
    text = f"""template <int32 Capacity>
    requires (Capacity >= 0)
struct {name} {{
    using size_type = int32;
    using View = {soa.names.view_name};
    using ConstView = {soa.names.const_view_name};{equivalent_alias}

    static constexpr size_type capacity_value{{Capacity}};

    {name}() noexcept = default;
    {name}({name} const& other)
        requires ({copy_constructible})
    {{
        for (size_type i{{}}; i < other.size_; ++i) {{
            storage_.copy_construct_at(size_, other.storage_, i);
            ++size_;
        }}
    }}
    {name}({name} const&)
        requires (!({copy_constructible}))
    = delete;
    {name}({name}&& other) noexcept({nothrow_move_constructible})
        requires ({move_constructible})
    {{
        for (size_type i{{}}; i < other.size_; ++i) {{
            storage_.move_construct_at(size_, other.storage_, i);
            ++size_;
        }}
        other.reset();
    }}
    {name}({name}&&)
        requires (!({move_constructible}))
    = delete;
    ~{name}() {{ reset(); }}

    auto operator=({name} const& other) -> {name}&
        requires ({copy_constructible})
    {{
        if (this != std::addressof(other)) {{
            reset();
            auto const source{{other.get_const_view()}};
            append_from(source);
        }}
        return *this;
    }}
    auto operator=({name} const&) -> {name}&
        requires (!({copy_constructible}))
    = delete;
    auto operator=({name}&& other) noexcept({nothrow_move_constructible}) -> {name}&
        requires ({move_constructible})
    {{
        if (this != std::addressof(other)) {{
            reset();
            for (size_type i{{}}; i < other.size_; ++i) {{
                storage_.move_construct_at(size_, other.storage_, i);
                ++size_;
            }}
            other.reset();
        }}
        return *this;
    }}
    auto operator=({name}&&) -> {name}&
        requires (!({move_constructible}))
    = delete;

    static constexpr auto capacity() noexcept -> size_type {{ return capacity_value; }}
    auto num() const noexcept -> size_type {{ return size_; }}
    auto is_empty() const noexcept -> bool {{ return size_ == 0; }}
    auto is_full() const noexcept -> bool {{ return size_ == capacity(); }}

    auto get_view() -> View {{ return storage_.get_view(0, size_); }}
    auto get_view(size_type const offset, size_type const count) -> View {{
        check_view_range(offset, count);
        return storage_.get_view(offset, count);
    }}
    auto get_view() const -> ConstView {{ return get_const_view(); }}
    auto get_view(size_type const offset, size_type const count) const -> ConstView {{
        return get_const_view(offset, count);
    }}
    auto get_const_view() const -> ConstView {{ return storage_.get_const_view(0, size_); }}
    auto get_const_view(size_type const offset, size_type const count) const -> ConstView {{
        check_view_range(offset, count);
        return storage_.get_const_view(offset, count);
    }}
    auto slice(size_type const offset, size_type const count) -> View {{ return get_view(offset, count); }}
    auto left(size_type const count) -> View {{ return slice(0, count); }}
    auto right(size_type const count) -> View {{ return slice(size_ - count, count); }}
    auto slice(size_type const offset, size_type const count) const -> ConstView {{ return get_const_view(offset, count); }}
    auto left(size_type const count) const -> ConstView {{ return slice(0, count); }}
    auto right(size_type const count) const -> ConstView {{ return slice(size_ - count, count); }}

    template <typename TFunc>
    auto apply_arrays(TFunc&& func) -> decltype(auto) {{
        auto view{{get_view()}};
        return view.apply_arrays(std::forward<TFunc>(func));
    }}
    template <typename TFunc>
    auto apply_arrays(TFunc&& func) const -> decltype(auto) {{
        auto view{{get_const_view()}};
        return view.apply_arrays(std::forward<TFunc>(func));
    }}{equivalent_access}

    template <{template_parameters}>
        requires ({constructible})
    auto emplace_back({function_parameters}) -> size_type {{
        check_has_sufficient_capacity(1);
        auto const index{{size_}};
        storage_.construct_at(index, {forwarded_arguments});
        ++size_;
        return index;
    }}
    template <{template_parameters}>
        requires ({constructible})
    auto add({function_parameters}) -> size_type {{
        return emplace_back({forwarded_arguments});
    }}

    void add_defaulted(size_type const count = 1)
        requires ({default_constructible})
    {{
        check_has_sufficient_capacity(count);
        for (size_type i{{}}; i < count; ++i) {{
            storage_.default_construct_at(size_);
            ++size_;
        }}
    }}
    void set_num(size_type const new_size)
        requires ({default_constructible})
    {{
        check(new_size >= 0);
        check(new_size <= capacity());
        if (new_size < size_) {{
            destroy_from(new_size);
            return;
        }}
        add_defaulted(new_size - size_);
    }}
    void set_num(size_type const new_size, EAllowShrinking const)
        requires ({default_constructible})
    {{
        set_num(new_size);
    }}

    auto capacity_view() -> View
        requires ({trivial})
    {{
        return storage_.get_view(0, capacity());
    }}
    void set_num_uninitialised(size_type const new_size)
        requires ({trivial})
    {{
        check(new_size >= 0);
        check(new_size <= capacity());
        size_ = new_size;
    }}
    void add_uninitialised(size_type const count)
        requires ({trivial})
    {{
        check_has_sufficient_capacity(count);
        size_ += count;
    }}

    void pop() {{
        check(!is_empty());
        --size_;
        storage_.destroy_at(size_);
    }}
    void reset() noexcept {{ destroy_from(0); }}
    void empty() noexcept {{ reset(); }}
    void reserve(size_type const requested_capacity) const {{
        check(requested_capacity >= 0);
        check(requested_capacity <= capacity());
    }}

    void remove_at_swap(size_type const index,
                        size_type const count,
                        EAllowShrinking const) {{
        check(index >= 0);
        check(count >= 0);
        check(index + count <= size_);
        if (count == 0) {{
            return;
        }}
        auto const available_tail{{size_ - (index + count)}};
        auto const move_count{{count < available_tail ? count : available_tail}};
        auto const source_begin{{size_ - move_count}};
        for (size_type i{{}}; i < move_count; ++i) {{
            storage_.move_assign_at(index + i, storage_, source_begin + i);
        }}
        destroy_from(size_ - count);
    }}

    template <typename Other>
    void copy_element(size_type const dst_index, Other const& other, size_type const source_index) {{
        check_index(dst_index);
        auto const source{{other.get_const_view()}};
        check(source_index >= 0);
        check(source_index < source.num());
        storage_.copy_assign_from_view_at(dst_index, source, source_index);
    }}
    template <typename Other>
    void copy_elements(size_type const dst_index,
                       Other const& other,
                       size_type const source_index,
                       size_type const count) {{
        check(dst_index >= 0);
        check(source_index >= 0);
        check(count >= 0);
        check(dst_index + count <= size_);
        auto const source{{other.get_const_view()}};
        check(source_index + count <= source.num());
        for (size_type i{{}}; i < count; ++i) {{
            storage_.copy_assign_from_view_at(dst_index + i, source, source_index + i);
        }}
    }}
    template <typename Other>
    void append_from(Other const& other) {{
        auto const source{{other.get_const_view()}};
        auto const count{{source.num()}};
        check_has_sufficient_capacity(count);
        for (size_type i{{}}; i < count; ++i) {{
            storage_.construct_from_view_at(size_, source, i);
            ++size_;
        }}
    }}

  private:
    void check_index(size_type const index) const {{
        check(index >= 0);
        check(index < size_);
    }}
    void check_view_range(size_type const offset, size_type const count) const {{
        check(offset >= 0);
        check(count >= 0);
        check(offset + count <= size_);
    }}
    void check_has_sufficient_capacity(size_type const count) const {{
        check(count >= 0);
        check(count <= capacity() - size_);
    }}
    void destroy_from(size_type const first_index) noexcept {{
        for (size_type i{{size_}}; i > first_index; --i) {{
            storage_.destroy_at(i - 1);
        }}
        size_ = first_index;
    }}

    {storage_name}<Capacity> storage_;
    size_type size_{{}};
}};"""
    dependencies: list[TypeDependency] = [
        ALLOW_SHRINKING,
        CHECK,
        MOVE_TEMP,
        STD_FORWARD,
        TypeDependency("std::addressof", "memory"),
        TypeDependency("std::is_constructible_v", "type_traits"),
    ]
    for leaf in leaves:
        dependencies.extend(leaf.element_type.type_dependencies)
    return Raw(text, dependencies)
