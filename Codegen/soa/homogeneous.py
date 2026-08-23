from __future__ import annotations

from collections.abc import Iterable

from Codegen.cpp import (
    FunctionParameter,
    MemberFunctionOperation,
    MemberFunctionSpec,
    Member,
    NewLines,
    Node,
    Raw,
    Struct,
    TypeDependency,
    UsingDeclaration,
    comma_separated,
    composed_type,
    type_spelling,
)


STD_FORWARD = TypeDependency("std::forward", "utility")
STD_REMOVE_CONST = TypeDependency("std::remove_const_t", "type_traits")
TARRAY = TypeDependency("TArray", "Containers/Array.h")
TARRAY_VIEW = TypeDependency("TArrayView", "Containers/ArrayView.h")
ALLOW_SHRINKING = TypeDependency(
    "EAllowShrinking", "Containers/AllowShrinking.h"
)
ARRAY_CHECKS = TypeDependency(
    "ml::fatal_if_nums_not_equal", "SandboxCore/array_checks.h"
)
CONTAINER_OPS = TypeDependency("ml::num", "SandboxCore/container_ops.h")
SOA_CONCEPTS = TypeDependency(
    "ml::SupportsApplyArrayPairsWith", "SandboxCore/soa_concepts.h"
)
SOA_PERMUTATION = TypeDependency("ml::apply_permutation", "SandboxCore/soa_permutation.h")
FILL_INDICES = TypeDependency("ml::fill_indices", "SandboxCore/array_utils.h")
CHECK = TypeDependency("check", "CoreMinimal.h")
FIXED_STORAGE = TypeDependency("ml::TFixedStorage", "SandboxCore/fixed_storage.h")
MOVE_TEMP = TypeDependency("MoveTemp", "Templates/UnrealTemplate.h")
TARRAY_REMOVE_AT_SWAP = MemberFunctionOperation("RemoveAtSwap")

from Codegen.soa.dynamic import (
    permutation_function_nodes,
    permutation_source_nodes,
    separate,
)
from Codegen.soa.model import HomogeneousSoALayout, HomogeneousSoAValueType

def _homogeneous_body(
    lines: Iterable[str], dependencies: Iterable[TypeDependency] = ()
) -> Raw:
    return Raw("\n".join(lines), dependencies)


def _homogeneous_view_functions(layout: HomogeneousSoALayout) -> tuple[Node, ...]:
    components = comma_separated(layout.components)
    view_name = layout.view_name
    offset = FunctionParameter("size_type const", "offset")
    count = FunctionParameter("size_type const", "count")
    func = FunctionParameter("TFunc&&", "func")
    functions: list[Node] = [
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw("return View{" + components + "};"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw("return ConstView{" + components + "};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (),
            Raw("return ConstView{" + components + "};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (offset, count),
            Raw("return get_const_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "apply_arrays",
            "auto",
            (func,),
            Raw(
                f"return std::forward<TFunc>(func)({components});",
                (STD_FORWARD,),
            ),
            suffix=" -> decltype(auto)",
            is_inline=True,
            template_parameters="typename TFunc",
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "apply_arrays",
            "auto",
            (func,),
            Raw(
                f"return std::forward<TFunc>(func)({components});",
                (STD_FORWARD,),
            ),
            suffix=" const -> decltype(auto)",
            is_inline=True,
            template_parameters="typename TFunc",
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "num",
            "auto",
            (),
            Raw(f"return {layout.components[0]}.Num();"),
            suffix=" const -> size_type",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "is_empty",
            "auto",
            (),
            Raw("return num() == 0;"),
            suffix=" const -> bool",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "slice",
            "auto",
            (offset, count),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Slice(offset, count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "left",
            "auto",
            (count,),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Left(count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "right",
            "auto",
            (count,),
            Raw(
                f"return {view_name}{{{comma_separated(f'{component}.Right(count)' for component in layout.components)}}};"
            ),
            suffix=f" const -> {view_name}",
            is_inline=True,
        ).header_node(),
    ]
    if layout.has_equivalent_type:
        functions.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "operator[]",
                    "equivalent_type",
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        f"return {{{comma_separated(f'{component}.GetData()[index]' for component in layout.components)}}};"
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
                NewLines(1),
                MemberFunctionSpec(
                    "at",
                    "equivalent_type",
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        "check(index >= 0);\n"
                        "check(index < num());\n"
                        "return (*this)[index];",
                        (CHECK,),
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
            )
        )
    return tuple(functions)


def _homogeneous_view_struct(layout: HomogeneousSoALayout) -> Struct:
    return Struct(
        layout.view_name,
        (
            UsingDeclaration(
                "size_type", composed_type("TArrayView<T>::SizeType", TARRAY_VIEW)
            ),
            NewLines(1),
            UsingDeclaration(
                "value_type",
                composed_type("std::remove_const_t<T>", STD_REMOVE_CONST),
            ),
            NewLines(1),
            UsingDeclaration("View", f"{layout.view_name}<T>"),
            NewLines(1),
            UsingDeclaration("ConstView", f"{layout.view_name}<value_type const>"),
            *(
                (
                    NewLines(1),
                    UsingDeclaration(
                        "equivalent_type",
                        f"typename {layout.equivalent_type_trait_name}<value_type>::type",
                    ),
                )
                if layout.has_equivalent_type
                else ()
            ),
            NewLines(2),
            *separate(
                (
                    Member(composed_type("TArrayView<T>", TARRAY_VIEW), component)
                    for component in layout.components
                ),
                1,
            ),
            NewLines(2),
            *_homogeneous_view_functions(layout),
        ),
        template="typename T",
    )


def _homogeneous_data_struct(
    layout: HomogeneousSoALayout, name: str, pointer_type: str
) -> Struct:
    return Struct(
        name, (Member(pointer_type, component) for component in layout.components)
    )


def _homogeneous_storage_functions(
    layout: HomogeneousSoALayout, value_type: HomogeneousSoAValueType
) -> tuple[Node, ...]:
    components = comma_separated(layout.components)
    offset = FunctionParameter("size_type const", "offset")
    count = FunctionParameter("size_type const", "count")
    allow_shrinking = FunctionParameter(
        composed_type("EAllowShrinking const", ALLOW_SHRINKING), "allow_shrinking"
    )
    func = FunctionParameter("TFunc&&", "func")
    get_data = (
        MemberFunctionSpec(
            "get_data",
            "auto",
            (),
            Raw(
                f"return Data{{{comma_separated(f'{component}.GetData()' for component in layout.components)}}};"
            ),
            suffix=" -> Data",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_data",
            "auto",
            (),
            Raw(
                f"return ConstData{{{comma_separated(f'{component}.GetData()' for component in layout.components)}}};"
            ),
            suffix=" const -> ConstData",
            is_inline=True,
        ).header_node(),
    )
    views = (
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw(f"return View{{{components}}};"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" -> View",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (),
            Raw(f"return ConstView{{{components}}};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_view",
            "auto",
            (offset, count),
            Raw("return get_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (),
            Raw(f"return ConstView{{{components}}};"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
        NewLines(1),
        MemberFunctionSpec(
            "get_const_view",
            "auto",
            (offset, count),
            Raw("return get_const_view().slice(offset, count);"),
            suffix=" const -> ConstView",
            is_inline=True,
        ).header_node(),
    )
    operations = (
        ("reset", (), "Reset"),
        ("empty", (), "Empty"),
        ("reserve", (count,), "Reserve"),
        ("set_num", (count, allow_shrinking), "SetNum"),
        ("set_num_uninitialised", (count,), "SetNumUninitialized"),
        ("add_uninitialised", (count,), "AddUninitialized"),
        (
            "remove_at_swap",
            (FunctionParameter("size_type const", "index"), count, allow_shrinking),
            "RemoveAtSwap",
        ),
        ("add_zeroed", (count,), "AddZeroed"),
        ("add_defaulted", (count,), "AddDefaulted"),
    )
    operation_nodes: list[Node] = []
    for index, (operation_name, parameters, array_operation) in enumerate(operations):
        if index:
            operation_nodes.append(NewLines(1))
        arguments = comma_separated(parameter.name for parameter in parameters)
        operation_nodes.append(
            MemberFunctionSpec(
                operation_name,
                "auto",
                parameters,
                _homogeneous_body(
                    f"{component}.{array_operation}({arguments});"
                    for component in layout.components
                ),
                suffix=" -> void",
                is_inline=True,
            ).header_node()
        )
    nodes: list[Node] = [*get_data, NewLines(2), *views, NewLines(2)]
    nodes.extend(
        (
            MemberFunctionSpec(
                "apply_arrays",
                "auto",
                (func,),
                Raw(
                    f"return std::forward<TFunc>(func)({components});",
                    (STD_FORWARD,),
                ),
                suffix=" -> decltype(auto)",
                is_inline=True,
                template_parameters="typename TFunc",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "apply_arrays",
                "auto",
                (func,),
                Raw(
                    f"return std::forward<TFunc>(func)({components});",
                    (STD_FORWARD,),
                ),
                suffix=" const -> decltype(auto)",
                is_inline=True,
                template_parameters="typename TFunc",
            ).header_node(),
            NewLines(2),
            MemberFunctionSpec(
                "num",
                "auto",
                (),
                Raw(f"return {layout.components[0]}.Num();"),
                suffix=" const -> size_type",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "validate_array_sizes",
                "void",
                (),
                _homogeneous_body(
                    f"check({component}.Num() == {layout.components[0]}.Num());"
                    for component in layout.components[1:]
                ),
                suffix=" const",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "is_empty",
                "auto",
                (),
                Raw("return num() == 0;"),
                suffix=" const -> bool",
                is_inline=True,
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_element",
                "auto",
                (
                    FunctionParameter("size_type const", "dst_i"),
                    FunctionParameter("Other const&", "src"),
                    FunctionParameter("size_type const", "src_i"),
                ),
                _homogeneous_body(
                    f"{component}[dst_i] = src.{component}[src_i];"
                    for component in layout.components
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_elements",
                "auto",
                (
                    FunctionParameter("size_type const", "dst_i"),
                    FunctionParameter("Other const&", "src"),
                    FunctionParameter("size_type const", "src_i"),
                    FunctionParameter("size_type const", "count"),
                ),
                Raw(
                    "for (auto i{0}; i < count; ++i) {\n"
                    + "\n".join(
                        f"    {component}[dst_i + i] = src.{component}[src_i + i];"
                        for component in layout.components
                    )
                    + "\n}"
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "copy_to_tail",
                "auto",
                (FunctionParameter("Other const&", "src"),),
                Raw(
                    "auto const count{src.num()};\n"
                    "check(num() >= count);\n"
                    "copy_elements(num() - count, src, 0, count);",
                    (CHECK,),
                ),
                suffix=" -> void",
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
            NewLines(1),
            MemberFunctionSpec(
                "append_from",
                "void",
                (FunctionParameter("Other const&", "other"),),
                _homogeneous_body(
                    f"{component}.Append(other.{component});"
                    for component in layout.components
                ),
                is_inline=True,
                template_parameters="typename Other",
            ).header_node(),
        )
    )
    nodes.extend((NewLines(2), *permutation_function_nodes(layout.components)))
    if value_type.equivalent_type:
        nodes.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "operator[]",
                    value_type.equivalent_type,
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        f"return {{{comma_separated(f'{component}.GetData()[index]' for component in layout.components)}}};"
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
                NewLines(1),
                MemberFunctionSpec(
                    "at",
                    value_type.equivalent_type,
                    (FunctionParameter("size_type const", "index"),),
                    Raw(
                        "validate_array_sizes();\n"
                        "check(index >= 0);\n"
                        "check(index < num());\n"
                        "return (*this)[index];",
                        (CHECK,),
                    ),
                    suffix=" const",
                    is_inline=True,
                ).header_node(),
            )
        )
    if value_type.input_types:
        component_parameters = tuple(
            FunctionParameter("value_type const", component[0])
            for component in layout.components
        )
        nodes.extend(
            (
                NewLines(1),
                MemberFunctionSpec(
                    "add",
                    "auto",
                    component_parameters,
                    _homogeneous_body(
                        (
                            f"auto const index{{{layout.components[0]}.Add({layout.components[0][0]})}};",
                            *(
                                f"{component}.Add({component[0]});"
                                for component in layout.components[1:]
                            ),
                            "return index;",
                        )
                    ),
                    suffix=" -> size_type",
                    is_inline=True,
                ).header_node(),
                *(
                    node
                    for input_type in value_type.input_types
                    for node in (
                        NewLines(1),
                        MemberFunctionSpec(
                            "add",
                            "auto",
                            (
                                FunctionParameter(
                                    composed_type(
                                        f"{type_spelling(input_type)} const&", input_type
                                    ),
                                    "value",
                                ),
                            ),
                            Raw(
                                f"return add({comma_separated(f'value.{axis}' for axis in ('X', 'Y', 'Z')[: len(layout.components)])});"
                            ),
                            suffix=" -> size_type",
                            is_inline=True,
                        ).header_node(),
                    )
                ),
            )
        )
    nodes.extend((NewLines(2), *operation_nodes))
    return tuple(nodes)


def _homogeneous_storage_struct(
    layout: HomogeneousSoALayout, value_type: HomogeneousSoAValueType
) -> Struct:
    aliases: list[Node] = [UsingDeclaration("value_type", value_type.cpp_type)]
    if value_type.equivalent_type:
        aliases.extend(
            (NewLines(1), UsingDeclaration("equivalent_type", value_type.equivalent_type))
        )
    aliases.extend(
        (
            NewLines(1),
            UsingDeclaration(
                "size_type", composed_type("TArray<value_type>::SizeType", TARRAY)
            ),
            NewLines(1),
            UsingDeclaration("View", f"{layout.view_name}<value_type>"),
            NewLines(1),
            UsingDeclaration("ConstView", f"{layout.view_name}<value_type const>"),
        )
    )
    return Struct(
        layout.storage_name(value_type),
        (
            *aliases,
            NewLines(2),
            _homogeneous_data_struct(layout, "Data", "value_type*"),
            NewLines(2),
            _homogeneous_data_struct(layout, "ConstData", "value_type const*"),
            NewLines(2),
            *separate(
                (
                    Member(composed_type("TArray<value_type>", TARRAY), component)
                    for component in layout.components
                ),
                1,
            ),
            NewLines(2),
            *_homogeneous_storage_functions(layout, value_type),
        ),
        export_specifier=layout.storage_export_specifier,
    )


def _homogeneous_equivalent_type_nodes(
    layout: HomogeneousSoALayout,
) -> tuple[Node, ...]:
    if not layout.has_equivalent_type:
        return ()

    specialisations = tuple(
        Raw(
            "\n".join(
                (
                    "template <>",
                    f"struct {layout.equivalent_type_trait_name}<{type_spelling(value.cpp_type)}> {{",
                    f"    using type = {type_spelling(value.equivalent_type)};",
                    "};",
                )
            ),
        )
        for value in layout.value_types
        if value.equivalent_type is not None
    )
    return (
        Raw(f"template <typename T>\nstruct {layout.equivalent_type_trait_name};"),
        NewLines(2),
        *separate(specialisations, 2),
    )


def lower_homogeneous_soa_layouts(
    layouts: Iterable[HomogeneousSoALayout],
) -> tuple[Node, ...]:
    layout_list = tuple(layouts)
    nodes: list[Node] = []
    for layout in layout_list:
        if nodes:
            nodes.append(NewLines(2))
        nodes.extend(_homogeneous_equivalent_type_nodes(layout))
        if layout.has_equivalent_type:
            nodes.append(NewLines(2))
        nodes.append(_homogeneous_view_struct(layout))
    for layout in layout_list:
        for value_type in layout.value_types:
            nodes.extend((NewLines(2), _homogeneous_storage_struct(layout, value_type)))
    return tuple(nodes)


def lower_homogeneous_soa_permutation_definitions(
    layouts: Iterable[HomogeneousSoALayout],
) -> tuple[Node, ...]:
    nodes: list[Node] = []
    for layout in layouts:
        for value_type in layout.value_types:
            if nodes:
                nodes.append(NewLines(2))
            nodes.extend(
                permutation_source_nodes(
                    layout.components, layout.storage_name(value_type)
                )
            )
    return tuple(nodes)
