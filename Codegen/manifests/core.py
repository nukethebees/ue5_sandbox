from __future__ import annotations

from Codegen.cpp import (
    CppFile,
    FunctionParameter,
    Include,
    IncludeDependencies,
    Member,
    MemberFunctionSpec,
    Module,
    NewLines,
    Raw,
    Struct,
    TypeLike,
    UsingDeclaration,
)
from Codegen.project_types import (
    F_INT_POINT,
    F_INT_VECTOR,
    F_UINT_POINT,
    F_UINT_VECTOR_3,
    F_VECTOR_2D,
    F_VECTOR_2F,
    F_VECTOR_3D,
    F_VECTOR_3F,
    qualified_type,
)
from Codegen.soa import (
    FixedSoAConfig,
    FixedSoAContainer,
    HomogeneousSoALayout,
    HomogeneousSoAValueType,
    SoAStruct,
    SoAStructNames,
    lower_soa_structs_with_source,
    tarray_member,
)

from Codegen.manifests.common import (
    ALL_STORAGE_OPERATIONS,
    ARRAY_MATH,
    INCLUDE_ORDER,
    PROJECT_ROOT,
    SANDBOX_CORE_API,
    SANDBOX_CORE_PRIVATE_DIR,
    SANDBOX_CORE_PUBLIC_DIR,
    homogeneous_soa_module,
    soa_source_file,
)


def soa_vectors_module() -> tuple[Module, ...]:
    vector_specs = (
        ("FVectors2f", "soa_vectors_2f.h", "float", ("xs", "ys"), F_VECTOR_2F),
        ("FVectors2d", "soa_vectors_2d.h", "double", ("xs", "ys"), F_VECTOR_2D),
        ("FVectors2i32", "soa_vectors_2i32.h", "int32", ("xs", "ys"), F_INT_POINT),
        ("FVectors2u32", "soa_vectors_2u32.h", "uint32", ("xs", "ys"), F_UINT_POINT),
        ("FVectors3f", "soa_vectors_3f.h", "float", ("xs", "ys", "zs"), F_VECTOR_3F),
        ("FVectors3d", "soa_vectors_3d.h", "double", ("xs", "ys", "zs"), F_VECTOR_3D),
        ("FVectors3i32", "soa_vectors_3i32.h", "int32", ("xs", "ys", "zs"), F_INT_VECTOR),
        ("FVectors3u32", "soa_vectors_3u32.h", "uint32", ("xs", "ys", "zs"), F_UINT_VECTOR_3),
    )
    return tuple(vector_soa_module(*spec) for spec in vector_specs)


def vector_soa_module(
    name: str,
    header_name: str,
    value_type: str,
    components: tuple[str, ...],
    equivalent_type: TypeLike,
) -> Module:
    component_parameters = tuple(
        FunctionParameter("value_type const", component[0]) for component in components
    )
    add_components = MemberFunctionSpec(
        "add",
        "auto",
        component_parameters,
        Raw(
            "\n".join(
                (
                    f"auto const index{{{components[0]}.Add({components[0][0]})}};",
                    *(f"{component}.Add({component[0]});" for component in components[1:]),
                    "return index;",
                )
            )
        ),
        suffix=" -> size_type",
        is_inline=True,
    )
    add_equivalent = MemberFunctionSpec(
        "add",
        "auto",
        (FunctionParameter(qualified_type(equivalent_type, " const&"), "value"),),
        Raw(
            f"return add({', '.join(f'value.{axis}' for axis in ('X', 'Y', 'Z')[:len(components)])});"
        ),
        suffix=" -> size_type",
        is_inline=True,
    )
    get_data = MemberFunctionSpec(
        "get_data",
        "auto",
        (),
        Raw(f"return Data{{{', '.join(f'{component}.GetData()' for component in components)}}};"),
        suffix=" -> Data",
        is_inline=True,
    )
    get_const_data = MemberFunctionSpec(
        "get_data",
        "auto",
        (),
        Raw(f"return ConstData{{{', '.join(f'{component}.GetData()' for component in components)}}};"),
        suffix=" const -> ConstData",
        is_inline=True,
    )
    empty = MemberFunctionSpec(
        "empty",
        "void",
        (),
        Raw("\n".join(f"{component}.Empty();" for component in components)),
        is_inline=True,
    )
    set_num_uninitialised = MemberFunctionSpec(
        "set_num_uninitialised",
        "void",
        (FunctionParameter("size_type const", "count"),),
        Raw("\n".join(f"{component}.SetNumUninitialized(count);" for component in components)),
        is_inline=True,
    )
    add_zeroed = MemberFunctionSpec(
        "add_zeroed",
        "void",
        (FunctionParameter("size_type const", "count"),),
        Raw("\n".join(f"{component}.AddZeroed(count);" for component in components)),
        is_inline=True,
    )
    vectors = SoAStruct(
        SoAStructNames(name),
        tuple(tarray_member(component, value_type) for component in components),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_CORE_API,
        nodes=(
            UsingDeclaration("value_type", value_type),
            NewLines(1),
            UsingDeclaration("size_type", "TArray<value_type>::SizeType"),
            NewLines(2),
            Struct("Data", tuple(Member("value_type*", component) for component in components)),
            NewLines(2),
            Struct("ConstData", tuple(Member("value_type const*", component) for component in components)),
            NewLines(2),
            get_data.header_node(),
            NewLines(1),
            get_const_data.header_node(),
            NewLines(2),
            add_components.header_node(),
            NewLines(1),
            add_equivalent.header_node(),
            NewLines(2),
            empty.header_node(),
            NewLines(1),
            set_num_uninitialised.header_node(),
            NewLines(1),
            add_zeroed.header_node(),
        ),
        equivalent_type=equivalent_type,
        copy_element_memberwise=True,
        fixed=(
            FixedSoAConfig(
                "TVectors3fFixedStorage",
                (FixedSoAContainer("TFixedVectors3f"),),
            )
            if name == "FVectors3f"
            else None
        ),
    )
    lowered = lower_soa_structs_with_source((vectors,))
    header_path = SANDBOX_CORE_PUBLIC_DIR / header_name
    return Module(
        name=header_name.removesuffix(".h"),
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(IncludeDependencies(), NewLines(2), *lowered.header_nodes),
        ),
        source=soa_source_file(
            header_path,
            lowered.source_nodes,
            source_path=SANDBOX_CORE_PRIVATE_DIR / header_path.with_suffix(".cpp").name,
            header_include_path=f"SandboxCore/{header_name}",
        ),
    )


def soa_vectors_umbrella_module() -> Module:
    headers = (
        "soa_vectors_2f.h",
        "soa_vectors_2d.h",
        "soa_vectors_2i32.h",
        "soa_vectors_2u32.h",
        "soa_vectors_3f.h",
        "soa_vectors_3d.h",
        "soa_vectors_3i32.h",
        "soa_vectors_3u32.h",
    )
    return Module(
        name="sandbox_core_soa_vectors",
        header=CppFile(
            path=SANDBOX_CORE_PUBLIC_DIR / "soa_vectors.h",
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=tuple(Include(f"SandboxCore/{header}", system=False) for header in headers),
        ),
    )


def soa_rotators_module() -> Module:
    return homogeneous_soa_module(
        "sandbox_core_soa_rotators",
        "soa_rotators.h",
        (
            HomogeneousSoALayout(
                "Rotators",
                ("pitches", "yaws", "rolls"),
                (
                    HomogeneousSoAValueType("float", "f"),
                    HomogeneousSoAValueType("double", "d"),
                ),
                storage_export_specifier=SANDBOX_CORE_API,
            ),
        ),
    )


def countdown_timers_module() -> Module:
    tick = MemberFunctionSpec(
        "tick",
        "void",
        (FunctionParameter("float const", "dt"),),
        Raw("ml::subtract_in_place(remaining_times, dt);", (ARRAY_MATH,)),
        suffix=" noexcept",
    )
    countdown_timers = SoAStruct(
        SoAStructNames("FCountdownTimers"),
        (tarray_member("remaining_times", "float"),),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_CORE_API,
        nodes=(tick.header_node(),),
        source_nodes=(tick.definition_node("FCountdownTimers"),),
    )
    lowered = lower_soa_structs_with_source((countdown_timers,))
    header_path = SANDBOX_CORE_PUBLIC_DIR / "countdown_timers.h"
    source_path = (
        PROJECT_ROOT
        / "Plugins"
        / "SandboxCore"
        / "Source"
        / "SandboxCore"
        / "Private"
        / "countdown_timers.cpp"
    )
    return Module(
        name="sandbox_core_countdown_timers",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(
            header_path,
            lowered.source_nodes,
            source_path=source_path,
            header_include_path="SandboxCore/countdown_timers.h",
        ),
    )
