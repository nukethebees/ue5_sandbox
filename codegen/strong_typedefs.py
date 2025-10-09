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
        lines = []

        # Header guard and comment
        lines.append("#pragma once")
        lines.append("")
        lines.append("/**")
        lines.append(" * GENERATED CODE - DO NOT EDIT MANUALLY")
        lines.append(f" * Strong typedef wrapper for {spec.underlying_type}")
        lines.append(" * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button")
        lines.append(" */")
        lines.append("")

        # Includes
        lines.append("#include \"CoreMinimal.h\"")
        lines.append("")
        lines.append(f"#include \"{spec.name}.generated.h\"")
        lines.append("")

        # Struct declaration
        bp_type = "BlueprintType" if spec.config.is_bp_struct else ""
        if bp_type:
            lines.append(f"USTRUCT({bp_type})")
        else:
            lines.append("USTRUCT()")
        lines.append(f"struct {struct_name} {{")
        lines.append("    GENERATED_BODY()")
        lines.append("")

        # Private value member
        lines.append("  private:")
        if spec.config.is_bp_struct:
            lines.append("    UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess=\"true\"))")
        else:
            lines.append("    UPROPERTY()")
        lines.append(f"    {spec.underlying_type} value{{}};")
        lines.append("")

        # Public interface
        lines.append("  public:")

        # Constructor
        explicit_keyword = "explicit " if spec.config.explicit_construction else ""
        default_value = self._get_default_value(spec.underlying_type)
        lines.append(f"    {explicit_keyword}{struct_name}({spec.underlying_type} v = {default_value}) : value(v) {{}}")
        lines.append("")

        # Conversion operator
        if spec.config.explicit_conversion:
            lines.append(f"    explicit operator {spec.underlying_type}() const {{ return value; }}")
        else:
            lines.append(f"    operator {spec.underlying_type}() const {{ return value; }}")
        lines.append("")

        # Getter for explicit access
        lines.append(f"    {spec.underlying_type} get_value() const {{ return value; }}")
        lines.append("")

        # Generate operators
        for op_group in spec.ops:
            operator_lines = self._generate_operators(op_group, spec, struct_name)
            lines.extend(operator_lines)
            if operator_lines:
                lines.append("")

        # Generate member function forwarding
        if spec.members:
            lines.append("    // Forwarded member functions")
            for member in spec.members:
                lines.append(f"    template<typename... Args>")
                lines.append(f"    auto {member}(Args&&... args) -> decltype(auto) {{")
                lines.append(f"        return value.{member}(std::forward<Args>(args)...);")
                lines.append("    }")
                lines.append("")
                lines.append(f"    template<typename... Args>")
                lines.append(f"    auto {member}(Args&&... args) const -> decltype(auto) {{")
                lines.append(f"        return value.{member}(std::forward<Args>(args)...);")
                lines.append("    }")
                lines.append("")

        # Hash function
        if spec.config.hash_fn:
            lines.append("    // Hash support for TMap/TSet")
            lines.append(f"    friend uint32 GetTypeHash({struct_name} const& obj) {{")
            lines.append("        return GetTypeHash(obj.value);")
            lines.append("    }")
            lines.append("")

        # Close struct
        lines.append("};")
        lines.append("")

        return "\n".join(lines)

    def _generate_operators(self, op_group: str, spec: TypedefSpec, struct_name: str) -> List[str]:
        """Generate operators for a specific operator group."""
        lines = []
        lines.append(f"    // {op_group.capitalize()} operators")

        if op_group == "comparison":
            lines.extend(self._generate_comparison_ops(struct_name))
        elif op_group == "arithmetic":
            lines.extend(self._generate_arithmetic_ops(spec, struct_name))
        elif op_group == "modulo":
            lines.extend(self._generate_modulo_ops(spec, struct_name))
        elif op_group == "increment":
            lines.extend(self._generate_increment_ops(struct_name))
        elif op_group == "bitwise":
            lines.extend(self._generate_bitwise_ops(spec, struct_name))
        elif op_group == "boolean":
            lines.extend(self._generate_boolean_ops(struct_name))
        elif op_group == "dereference":
            lines.extend(self._generate_dereference_ops(spec))
        else:
            print(f"Warning: Unknown operator group '{op_group}'")

        return lines

    def _generate_comparison_ops(self, struct_name: str) -> List[str]:
        """Generate comparison operators."""
        return [
            f"    bool operator==({struct_name} const& rhs) const {{ return value == rhs.value; }}",
            f"    bool operator!=({struct_name} const& rhs) const {{ return value != rhs.value; }}",
            f"    bool operator<({struct_name} const& rhs) const {{ return value < rhs.value; }}",
            f"    bool operator<=({struct_name} const& rhs) const {{ return value <= rhs.value; }}",
            f"    bool operator>({struct_name} const& rhs) const {{ return value > rhs.value; }}",
            f"    bool operator>=({struct_name} const& rhs) const {{ return value >= rhs.value; }}",
        ]

    def _generate_arithmetic_ops(self, spec: TypedefSpec, struct_name: str) -> List[str]:
        """Generate arithmetic operators (excluding modulo)."""
        return [
            f"    {struct_name} operator+({struct_name} const& rhs) const {{ return {struct_name}{{value + rhs.value}}; }}",
            f"    {struct_name} operator-({struct_name} const& rhs) const {{ return {struct_name}{{value - rhs.value}}; }}",
            f"    {struct_name} operator*({struct_name} const& rhs) const {{ return {struct_name}{{value * rhs.value}}; }}",
            f"    {struct_name} operator/({struct_name} const& rhs) const {{ return {struct_name}{{value / rhs.value}}; }}",
            "",
            f"    {struct_name}& operator+=({struct_name} const& rhs) {{ value += rhs.value; return *this; }}",
            f"    {struct_name}& operator-=({struct_name} const& rhs) {{ value -= rhs.value; return *this; }}",
            f"    {struct_name}& operator*=({struct_name} const& rhs) {{ value *= rhs.value; return *this; }}",
            f"    {struct_name}& operator/=({struct_name} const& rhs) {{ value /= rhs.value; return *this; }}",
            "",
            f"    {struct_name} operator-() const {{ return {struct_name}{{-value}}; }}",
        ]

    def _generate_modulo_ops(self, spec: TypedefSpec, struct_name: str) -> List[str]:
        """Generate modulo operators (for integral types only)."""
        return [
            f"    {struct_name} operator%({struct_name} const& rhs) const {{ return {struct_name}{{value % rhs.value}}; }}",
            "",
            f"    {struct_name}& operator%=({struct_name} const& rhs) {{ value %= rhs.value; return *this; }}",
        ]

    def _generate_increment_ops(self, struct_name: str) -> List[str]:
        """Generate increment/decrement operators."""
        return [
            f"    {struct_name}& operator++() {{ ++value; return *this; }}",
            f"    {struct_name} operator++(int) {{ return {struct_name}{{value++}}; }}",
            f"    {struct_name}& operator--() {{ --value; return *this; }}",
            f"    {struct_name} operator--(int) {{ return {struct_name}{{value--}}; }}",
        ]

    def _generate_bitwise_ops(self, spec: TypedefSpec, struct_name: str) -> List[str]:
        """Generate bitwise operators."""
        return [
            f"    {struct_name} operator&({struct_name} const& rhs) const {{ return {struct_name}{{value & rhs.value}}; }}",
            f"    {struct_name} operator|({struct_name} const& rhs) const {{ return {struct_name}{{value | rhs.value}}; }}",
            f"    {struct_name} operator^({struct_name} const& rhs) const {{ return {struct_name}{{value ^ rhs.value}}; }}",
            f"    {struct_name} operator~() const {{ return {struct_name}{{~value}}; }}",
            f"    {struct_name} operator<<(int32 shift) const {{ return {struct_name}{{value << shift}}; }}",
            f"    {struct_name} operator>>(int32 shift) const {{ return {struct_name}{{value >> shift}}; }}",
            "",
            f"    {struct_name}& operator&=({struct_name} const& rhs) {{ value &= rhs.value; return *this; }}",
            f"    {struct_name}& operator|=({struct_name} const& rhs) {{ value |= rhs.value; return *this; }}",
            f"    {struct_name}& operator^=({struct_name} const& rhs) {{ value ^= rhs.value; return *this; }}",
            f"    {struct_name}& operator<<=(int32 shift) {{ value <<= shift; return *this; }}",
            f"    {struct_name}& operator>>=(int32 shift) {{ value >>= shift; return *this; }}",
        ]

    def _generate_boolean_ops(self, struct_name: str) -> List[str]:
        """Generate boolean conversion operator."""
        return [
            "    explicit operator bool() const { return static_cast<bool>(value); }",
        ]

    def _generate_dereference_ops(self, spec: TypedefSpec) -> List[str]:
        """Generate dereference operators for pointer types."""
        # Strip pointer/const from type to get pointee type
        pointee_type = spec.underlying_type.rstrip('*').strip()
        return [
            f"    {pointee_type}* operator->() const {{ return value; }}",
            f"    {pointee_type}& operator*() const {{ check(value); return *value; }}",
        ]

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
    project_root = script_dir.parent

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
