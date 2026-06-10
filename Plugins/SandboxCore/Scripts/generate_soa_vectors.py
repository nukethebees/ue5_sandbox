from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_DIR = SCRIPT_DIR.parent
PUBLIC_OUTPUT_DIR = PLUGIN_DIR / "Source" / "SandboxCore" / "Public" / "SandboxCore"


TYPE_SUFFIXES = {
    "float": "f",
    "double": "d",
    "int8": "i8",
    "uint8": "u8",
    "int16": "i16",
    "uint16": "u16",
    "int32": "i32",
    "uint32": "u32",
    "int64": "i64",
    "uint64": "u64",
}


@dataclass(frozen=True)
class FnSpec:
    name: str
    array_fn: str
    args: list[str]
    pass_args: list[str]


@dataclass(frozen=True)
class LayoutSpec:
    base_name: str
    components: list[str]
    functions: list[FnSpec]


@dataclass(frozen=True)
class HeaderSpec:
    layouts: list[LayoutSpec]
    value_types: list[str]


def default_functions() -> list[FnSpec]:
    return [
        FnSpec(name="reset", array_fn="Reset", args=[], pass_args=[]),
        FnSpec(name="empty", array_fn="Empty", args=[], pass_args=[]),
        FnSpec(
            name="reserve",
            array_fn="Reserve",
            args=["size_type const count"],
            pass_args=["count"],
        ),
        FnSpec(
            name="set_num_uninitialized",
            array_fn="SetNumUninitialized",
            args=["size_type const count"],
            pass_args=["count"],
        ),
        FnSpec(
            name="add_uninitialized",
            array_fn="AddUninitialized",
            args=["size_type const count"],
            pass_args=["count"],
        ),
        FnSpec(
            name="remove_at_swap",
            array_fn="RemoveAtSwap",
            args=[
                "size_type const index",
                "size_type const count",
                "EAllowShrinking const allow_shrinking",
            ],
            pass_args=["index", "count", "allow_shrinking"],
        ),
    ]


def type_suffix(value_type: str) -> str:
    try:
        return TYPE_SUFFIXES[value_type]
    except KeyError as error:
        valid_types = ", ".join(sorted(TYPE_SUFFIXES))
        raise ValueError(
            f"Unsupported value type {value_type!r}. Valid types: {valid_types}"
        ) from error


def view_name(layout: LayoutSpec) -> str:
    return f"T{layout.base_name}View"


def storage_name(layout: LayoutSpec, value_type: str) -> str:
    return f"F{layout.base_name}{type_suffix(value_type)}"


def join_args(args: list[str]) -> str:
    return ", ".join(args)


def generate_view(layout: LayoutSpec) -> str:
    lines = [
        "template <typename T>",
        f"struct {view_name(layout)}",
        "{",
        "    using size_type = TArrayView<T>::SizeType;",
        "",
    ]
    for component in layout.components:
        lines.append(f"    TArrayView<T> {component};")
    lines.extend(
        [
            "",
            *generate_apply_arrays(layout),
            "",
            "    auto num() const -> size_type",
            "    {",
            f"        return {layout.components[0]}.Num();",
            "    }",
            "",
            *generate_view_slice_functions(layout),
        ]
    )
    lines.append("};")
    return "\n".join(lines)


def generate_view_slice_functions(layout: LayoutSpec) -> list[str]:
    return [
        "    auto slice(size_type const offset, size_type const count) const -> "
        f"{view_name(layout)}",
        "    {",
        "        return "
        f"{view_name(layout)}{{"
        f"{join_args([f'{component}.Slice(offset, count)' for component in layout.components])}"
        "};",
        "    }",
        "",
        f"    auto left(size_type const count) const -> {view_name(layout)}",
        "    {",
        "        return "
        f"{view_name(layout)}{{"
        f"{join_args([f'{component}.Left(count)' for component in layout.components])}"
        "};",
        "    }",
        "",
        f"    auto right(size_type const count) const -> {view_name(layout)}",
        "    {",
        "        return "
        f"{view_name(layout)}{{"
        f"{join_args([f'{component}.Right(count)' for component in layout.components])}"
        "};",
        "    }",
    ]


def generate_apply_arrays(layout: LayoutSpec) -> list[str]:
    components = join_args(layout.components)
    lines = [
        "    template <typename TFunc>",
        "    auto apply_arrays(TFunc&& func) -> decltype(auto)",
        "    {",
        f"        return std::forward<TFunc>(func)({components});",
    ]
    lines.extend(
        [
            "    }",
            "",
            "    template <typename TFunc>",
            "    auto apply_arrays(TFunc&& func) const -> decltype(auto)",
            "    {",
            f"        return std::forward<TFunc>(func)({components});",
        ]
    )
    lines.append("    }")
    return lines


def generate_member_arrays(layout: LayoutSpec, value_type: str) -> list[str]:
    lines: list[str] = []
    for component in layout.components:
        lines.append("    UPROPERTY()")
        lines.append(f"    TArray<{value_type}> {component};")
    return lines


def generate_array_function(layout: LayoutSpec, fn: FnSpec) -> list[str]:
    lines = [
        f"    auto {fn.name}({join_args(fn.args)}) -> void",
        "    {",
    ]
    pass_args = join_args(fn.pass_args)
    for component in layout.components:
        lines.append(f"        {component}.{fn.array_fn}({pass_args});")
    lines.append("    }")
    return lines


def generate_view_return(
    layout: LayoutSpec, return_type: str, function_name: str
) -> list[str]:
    components = join_args(layout.components)
    return [
        f"    auto {function_name}() const -> {return_type}"
        if return_type == "ConstView"
        else f"    auto {function_name}() -> {return_type}",
        "    {",
        f"        return {return_type}{{{components}}};",
        "    }",
    ]


def generate_storage_struct(layout: LayoutSpec, value_type: str) -> str:
    name = storage_name(layout, value_type)
    lines = [
        "",
        "USTRUCT()",
        f"struct {name}",
        "{",
        "    GENERATED_BODY()",
        "",
        f"    using value_type = {value_type};",
        "    using size_type = TArray<value_type>::SizeType;",
        f"    using View = {view_name(layout)}<value_type>;",
        f"    using ConstView = {view_name(layout)}<value_type const>;",
        "",
        *generate_member_arrays(layout, value_type),
        "",
        *generate_view_return(layout, "View", "get_view"),
        "",
        *generate_view_return(layout, "ConstView", "get_view"),
        "",
        *generate_view_return(layout, "ConstView", "get_const_view"),
        "",
        *generate_apply_arrays(layout),
        "",
        "    auto num() const -> size_type",
        "    {",
        f"        return {layout.components[0]}.Num();",
        "    }",
        "",
        "    auto is_empty() const -> bool",
        "    {",
        "        return num() == 0;",
        "    }",
    ]

    for fn in layout.functions:
        lines.append("")
        lines.extend(generate_array_function(layout, fn))

    lines.extend(
        [
            "};",
        ]
    )
    return "\n".join(lines)


def generate_header(header_filename: str, spec: HeaderSpec) -> str:
    generated_include = Path(header_filename).with_suffix(".generated.h").name
    chunks = [
        "// This file is autogenerated. Do not edit by hand.",
        "// Edit the generator script instead.",
        "// clang-format off",
        "",
        "#pragma once",
        "",
        '#include "CoreMinimal.h"',
        "#include <utility>",
        "",
        f'#include "{generated_include}"',
        "",
    ]
    chunks.extend(generate_view(layout) for layout in spec.layouts)
    for layout in spec.layouts:
        for value_type in spec.value_types:
            chunks.append(generate_storage_struct(layout, value_type))
    return "\n".join(chunks) + "\n".join(["", "", "// clang-format on", ""])


def write_header(header_filename: str, spec: HeaderSpec) -> Path:
    PUBLIC_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = PUBLIC_OUTPUT_DIR / header_filename
    output_path.write_text(
        generate_header(header_filename, spec), encoding="utf-8", newline="\n"
    )
    return output_path


def vector_spec() -> HeaderSpec:
    functions = default_functions()
    return HeaderSpec(
        layouts=[
            LayoutSpec(
                base_name="Vectors2",
                components=["xs", "ys"],
                functions=functions,
            ),
            LayoutSpec(
                base_name="Vectors3",
                components=["xs", "ys", "zs"],
                functions=functions,
            ),
        ],
        value_types=["float", "double", "int32", "uint32"],
    )


def rotator_spec() -> HeaderSpec:
    functions = default_functions()
    return HeaderSpec(
        layouts=[
            LayoutSpec(
                base_name="Rotators",
                components=["pitches", "yaws", "rolls"],
                functions=functions,
            ),
        ],
        value_types=["float", "double"],
    )


def main() -> None:
    for name, fn in (
        ("soa_vectors.h", vector_spec),
        ("soa_rotators.h", rotator_spec),
    ):
        output_path = write_header(name, fn())
        print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
