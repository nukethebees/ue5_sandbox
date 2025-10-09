"""
Strong typedef code generator for Unreal Engine structs.
Generates type-safe wrapper structs with reflection support.
"""

import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List


@dataclass
class ConfigOptions:
    """Configuration options for generated typedefs."""
    hash_fn: bool = True
    is_bp_struct: bool = True
    explicit_conversion: bool = True
    explicit_construction: bool = True


@dataclass
class TypedefSpec:
    """Specification for a strong typedef to generate."""
    name: str                                           # "Ammo" -> FAmmoStrong
    underlying_type: str                                # "float", "uint32", etc.
    ops: List[str] = field(default_factory=list)        # ["comparison", "arithmetic"]
    members: List[str] = field(default_factory=list)    # ["clamp", "sqrt"]
    includes: List[str] = field(default_factory=list)   # ["Sandbox/interfaces/inventory/InventoryItem.h"]
    config: ConfigOptions = field(default_factory=ConfigOptions)


class StrongTypedefGenerator:
    """Generates strong typedef wrapper structs."""

    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)

    def generate_all(self, typedefs: List[TypedefSpec]) -> bool:
        """Generate all typedef files."""
        print(f"Generating {len(typedefs)} strong typedefs...")

        # Ensure output directory exists
        self.output_dir.mkdir(parents=True, exist_ok=True)

        success = True
        for typedef in typedefs:
            if not self._generate_typedef(typedef):
                success = False

        return success

    def _generate_typedef(self, spec: TypedefSpec) -> bool:
        """Generate a single typedef header file."""
        struct_name = f"F{spec.name}"
        output_path = self.output_dir / f"{spec.name}.h"

        print(f"  Generating {struct_name} -> {output_path}")

        content = self._generate_header_content(spec, struct_name)

        return self._write_file_if_different(output_path, content)

    def _generate_header_content(self, spec: TypedefSpec, struct_name: str) -> str:
        """Generate the complete header file content."""
        # Pre-compute conditional strings
        ustruct_specifier = f"USTRUCT(BlueprintType)" if spec.config.is_bp_struct else "USTRUCT()"

        uproperty_line = (
            "    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=\"true\"))" 
            if spec.config.is_bp_struct 
            else  "    UPROPERTY()")

        explicit_keyword = "explicit " if spec.config.explicit_construction else ""
        default_value = self._get_default_value(spec.underlying_type)

        conversion_operator = (f"    explicit operator {spec.underlying_type}() const {{ return value; }}"
                               if spec.config.explicit_conversion
                               else f"    operator {spec.underlying_type}() const {{ return value; }}")

        # Generate operators section
        operators_section = ""
        for op_group in spec.ops:
            operator_lines = self._generate_operators(op_group, spec, struct_name)
            if operator_lines:
                operators_section += "\n".join(operator_lines) + "\n\n"

        # Generate member function forwarding section
        members_section = ""
        if spec.members:
            members_parts = ["    // Forwarded member functions"]
            for member in spec.members:
                members_parts.append(f"""    template<typename... Args>
    auto {member}(Args&&... args) -> decltype(auto) {{
        return value.{member}(std::forward<Args>(args)...);
    }}

    template<typename... Args>
    auto {member}(Args&&... args) const -> decltype(auto) {{
        return value.{member}(std::forward<Args>(args)...);
    }}""")
            members_section = "\n".join(members_parts) + "\n\n"

        # Generate hash function section
        hash_section = ""
        if spec.config.hash_fn:
            hash_section = f"""    // Hash support for TMap/TSet
    friend uint32 GetTypeHash({struct_name} const& obj) {{
        return GetTypeHash(obj.value);
    }}

"""

        # Generate includes section
        includes_section = ""
        if spec.includes:
            includes_lines = [f'#include "{inc}"' for inc in spec.includes]
            includes_section = "\n" + "\n".join(includes_lines) + "\n"

        # Generate complete header using multiline f-string
        content = f"""#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for {spec.underlying_type}
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"
{includes_section}
#include "{spec.name}.generated.h"

{ustruct_specifier}
struct {struct_name} {{
    GENERATED_BODY()
  private:
{uproperty_line}
    {spec.underlying_type} value{{}};

  public:
    {explicit_keyword}{struct_name}({spec.underlying_type} v = {default_value}) : value(v) {{}}
    template <typename... Args>
        requires std::constructible_from<{spec.underlying_type}, Args...>
    {explicit_keyword}{struct_name}(Args&&... args) : value(std::forward<Args>(args)...) {{}}

{conversion_operator}

    {spec.underlying_type} get_value() const {{ return value; }}

{operators_section}{members_section}{hash_section}}};
"""

        return content

    def _generate_operators(self, op_group: str, spec: TypedefSpec, struct_name: str) -> List[str]:
        """Generate operators for a specific operator group."""
        comment = f"    // {op_group.capitalize()} operators"

        ops_content = ""
        if op_group == "comparison":
            ops_content = self._generate_comparison_ops(struct_name)
        elif op_group == "arithmetic":
            ops_content = self._generate_arithmetic_ops(spec, struct_name)
        elif op_group == "modulo":
            ops_content = self._generate_modulo_ops(spec, struct_name)
        elif op_group == "increment":
            ops_content = self._generate_increment_ops(struct_name)
        elif op_group == "bitwise":
            ops_content = self._generate_bitwise_ops(spec, struct_name)
        elif op_group == "boolean":
            ops_content = self._generate_boolean_ops(struct_name)
        elif op_group == "dereference":
            ops_content = self._generate_dereference_ops(spec)
        else:
            print(f"Warning: Unknown operator group '{op_group}'")
            return []

        return [comment] + ops_content.split('\n')

    def _generate_comparison_ops(self, struct_name: str) -> str:
        """Generate comparison operators."""
        return f"""    bool operator==({struct_name} const& rhs) const {{ return value == rhs.value; }}
    bool operator!=({struct_name} const& rhs) const {{ return value != rhs.value; }}
    bool operator<({struct_name} const& rhs) const {{ return value < rhs.value; }}
    bool operator<=({struct_name} const& rhs) const {{ return value <= rhs.value; }}
    bool operator>({struct_name} const& rhs) const {{ return value > rhs.value; }}
    bool operator>=({struct_name} const& rhs) const {{ return value >= rhs.value; }}"""

    def _generate_arithmetic_ops(self, spec: TypedefSpec, struct_name: str) -> str:
        """Generate arithmetic operators (excluding modulo)."""
        return f"""    {struct_name} operator+({struct_name} const& rhs) const {{ return {struct_name}{{value + rhs.value}}; }}
    {struct_name} operator-({struct_name} const& rhs) const {{ return {struct_name}{{value - rhs.value}}; }}
    {struct_name} operator*({struct_name} const& rhs) const {{ return {struct_name}{{value * rhs.value}}; }}
    {struct_name} operator/({struct_name} const& rhs) const {{ return {struct_name}{{value / rhs.value}}; }}

    {struct_name}& operator+=({struct_name} const& rhs) {{ value += rhs.value; return *this; }}
    {struct_name}& operator-=({struct_name} const& rhs) {{ value -= rhs.value; return *this; }}
    {struct_name}& operator*=({struct_name} const& rhs) {{ value *= rhs.value; return *this; }}
    {struct_name}& operator/=({struct_name} const& rhs) {{ value /= rhs.value; return *this; }}

    {struct_name} operator-() const {{ return {struct_name}{{-value}}; }}"""

    def _generate_modulo_ops(self, spec: TypedefSpec, struct_name: str) -> str:
        """Generate modulo operators (for integral types only)."""
        return f"""    {struct_name} operator%({struct_name} const& rhs) const {{ return {struct_name}{{value % rhs.value}}; }}

    {struct_name}& operator%=({struct_name} const& rhs) {{ value %= rhs.value; return *this; }}"""

    def _generate_increment_ops(self, struct_name: str) -> str:
        """Generate increment/decrement operators."""
        return f"""    {struct_name}& operator++() {{ ++value; return *this; }}
    {struct_name} operator++(int) {{ return {struct_name}{{value++}}; }}
    {struct_name}& operator--() {{ --value; return *this; }}
    {struct_name} operator--(int) {{ return {struct_name}{{value--}}; }}"""

    def _generate_bitwise_ops(self, spec: TypedefSpec, struct_name: str) -> str:
        """Generate bitwise operators."""
        return f"""    {struct_name} operator&({struct_name} const& rhs) const {{ return {struct_name}{{value & rhs.value}}; }}
    {struct_name} operator|({struct_name} const& rhs) const {{ return {struct_name}{{value | rhs.value}}; }}
    {struct_name} operator^({struct_name} const& rhs) const {{ return {struct_name}{{value ^ rhs.value}}; }}
    {struct_name} operator~() const {{ return {struct_name}{{~value}}; }}
    {struct_name} operator<<(int32 shift) const {{ return {struct_name}{{value << shift}}; }}
    {struct_name} operator>>(int32 shift) const {{ return {struct_name}{{value >> shift}}; }}

    {struct_name}& operator&=({struct_name} const& rhs) {{ value &= rhs.value; return *this; }}
    {struct_name}& operator|=({struct_name} const& rhs) {{ value |= rhs.value; return *this; }}
    {struct_name}& operator^=({struct_name} const& rhs) {{ value ^= rhs.value; return *this; }}
    {struct_name}& operator<<=(int32 shift) {{ value <<= shift; return *this; }}
    {struct_name}& operator>>=(int32 shift) {{ value >>= shift; return *this; }}"""

    def _generate_boolean_ops(self, struct_name: str) -> str:
        """Generate boolean conversion operator."""
        return "    explicit operator bool() const { return static_cast<bool>(value); }"

    def _generate_dereference_ops(self, spec: TypedefSpec) -> str:
        """Generate dereference operators for pointer types."""
        return """    auto* operator->() { return &value; }
    auto& operator*() { return *value; }
    auto* operator->() const { return &value; }
    auto& operator*() const { return *value; }"""

    def _get_default_value(self, type_name: str) -> str:
        """Get appropriate default value for a type."""
        if type_name.endswith('*'):
            return "nullptr"
        elif type_name in ["float", "double"]:
            return "0.0f"
        elif type_name in ["int32", "uint32", "int64", "uint64", "int8", "uint8", "int16", "uint16"]:
            return "0"
        elif type_name == "bool":
            return "false"
        else:
            return "{}"

    def _write_file_if_different(self, file_path: Path, content: str) -> bool:
        """Write file only if content is different from existing file."""
        if file_path.exists():
            existing_content = file_path.read_text(encoding='utf-8')
            if existing_content == content:
                print(f"    File unchanged, skipping: {file_path}")
                return True
            print(f"    File content changed, updating: {file_path}")
        else:
            print(f"    Creating new file: {file_path}")

        try:
            file_path.write_text(content, encoding='utf-8')
            return True
        except Exception as e:
            print(f"    ERROR: Failed to write file: {e}")
            return False


def main():
    """Main entry point for the generator."""
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent.parent  # codegen -> SandboxEditor -> Source -> Project root

    # Import typedef definitions
    sys.path.insert(0, str(script_dir))
    from typedef_definitions import TYPEDEFS

    # Output directory
    output_dir = project_root / "Source" / "Sandbox" / "generated" / "strong_typedefs"

    # Generate
    generator = StrongTypedefGenerator(str(output_dir))
    success = generator.generate_all(TYPEDEFS)

    if success:
        print("\nGeneration completed successfully!")

        # Run clang-format on generated files
        format_script = project_root / "format-cpp.py"
        if format_script.exists():
            print("\nRunning clang-format...")
            import subprocess
            result = subprocess.run(
                [sys.executable, str(format_script)],
                cwd=str(project_root),
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                print("Code formatting complete!")
            else:
                print(f"Warning: clang-format failed: {result.stderr}")

        return 0
    else:
        print("\nGeneration failed!")
        return 1


if __name__ == "__main__":
    sys.exit(main())
