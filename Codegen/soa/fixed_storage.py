from __future__ import annotations

from Codegen.cpp import CppWriter, Raw, TypeDependency, comma_separated

STD_FORWARD = TypeDependency("std::forward", "utility")
TARRAY_VIEW = TypeDependency("TArrayView", "Containers/ArrayView.h")
FIXED_STORAGE = TypeDependency("ml::TFixedStorage", "SandboxCore/fixed_storage.h")
MOVE_TEMP = TypeDependency("MoveTemp", "Templates/UnrealTemplate.h")
from Codegen.soa.model import ArraySoAMember
from Codegen.soa.fixed_layout import (
    FixedSoALayout, fixed_construct_lines, fixed_member_view,
)


def fixed_storage_projection(layout: FixedSoALayout) -> Raw:
    soa = layout.schema
    storage_name = layout.storage_name
    leaves = layout.leaves
    forwarded_arguments = tuple(
        f"std::forward<TArg{i}>(new_{'_'.join(path)})"
        for i, leaf in enumerate(leaves)
        for path in (leaf.path,)
    )
    template_parameters = comma_separated(
        f"typename TArg{i}" for i in range(len(leaves))
    )
    function_parameters = comma_separated(
        f"TArg{i}&& new_{leaf.argument_name}" for i, leaf in enumerate(leaves)
    )
    mutable_views = comma_separated(fixed_member_view(member, False) for member in soa.members)
    const_views = comma_separated(fixed_member_view(member, True) for member in soa.members)
    copy_assign = tuple(
        (
            f"{member.name}_[dst_index] = source.{member.name}[source_index];"
            if isinstance(member, ArraySoAMember)
            else f"{member.name}_.copy_assign_from_view_at(dst_index, source.{member.name}, source_index);"
        )
        for member in soa.members
    )
    move_assign = tuple(
        (
            f"{member.name}_[dst_index] = MoveTemp(other.{member.name}_[source_index]);"
            if isinstance(member, ArraySoAMember)
            else f"{member.name}_.move_assign_at(dst_index, other.{member.name}_, source_index);"
        )
        for member in soa.members
    )
    writer = CppWriter()
    writer.line("template <int32 Capacity>")
    writer.line("    requires (Capacity >= 0)")
    with writer.block(f"struct {storage_name}", "};"):
        writer.line(f"using View = {soa.names.view_name};")
        writer.line(f"using ConstView = {soa.names.const_view_name};")
        writer.line()
        with writer.block("auto get_view(int32 const offset, int32 const count) -> View"):
            writer.line(f"return View{{{mutable_views}}};")
        with writer.block(
            "auto get_const_view(int32 const offset, int32 const count) const -> ConstView"
        ):
            writer.line(f"return ConstView{{{const_views}}};")
        writer.line()
        writer.line(f"template <{template_parameters}>")
        with writer.block(f"void construct_at(int32 const index, {function_parameters})"):
            writer.lines(fixed_construct_lines(layout, "arguments", forwarded_arguments))
        writer.line()
        with writer.block("void default_construct_at(int32 const index)"):
            writer.lines(fixed_construct_lines(layout, "default"))
        writer.line()
        with writer.block(
            f"void copy_construct_at(int32 const index, {storage_name} const& other, int32 const other_index)"
        ):
            writer.lines(fixed_construct_lines(layout, "copy"))
        writer.line()
        with writer.block(
            f"void move_construct_at(int32 const index, {storage_name}& other, int32 const other_index)"
        ):
            writer.lines(fixed_construct_lines(layout, "move"))
        writer.line()
        writer.line("template <typename SourceView>")
        with writer.block(
            "void construct_from_view_at(int32 const index, SourceView const& source, int32 const source_index)"
        ):
            writer.lines(fixed_construct_lines(layout, "view"))
        writer.line()
        writer.line("template <typename SourceView>")
        with writer.block(
            "void copy_assign_from_view_at(int32 const dst_index, SourceView const& source, int32 const source_index)"
        ):
            writer.lines(copy_assign)
        writer.line()
        with writer.block(
            f"void move_assign_at(int32 const dst_index, {storage_name}& other, int32 const source_index)"
        ):
            writer.lines(move_assign)
        writer.line()
        with writer.block("void destroy_at(int32 const index) noexcept"):
            writer.lines(
                f"{member.name}_.destroy_at(index);"
                for member in reversed(soa.members)
            )
        writer.line()
        writer.lines(
            f"{member_layout.storage_type} {member_layout.member.name}_;"
            for member_layout in layout.members
        )
    dependencies: list[TypeDependency] = [
        FIXED_STORAGE,
        TARRAY_VIEW,
        STD_FORWARD,
        MOVE_TEMP,
    ]
    for leaf in leaves:
        dependencies.extend(leaf.element_type.type_dependencies)
    return Raw(writer.render(), dependencies)
