from __future__ import annotations

from dataclasses import dataclass

from Codegen.nodes import Node, RenderContext


@dataclass(frozen=True)
class SoAMember:
    container_type: str
    view_type: str
    const_view_type: str
    name: str

    def __post_init__(self) -> None:
        values = (self.container_type, self.view_type, self.const_view_type, self.name)
        if any(not value.strip() for value in values):
            raise ValueError("SOA member types and name must not be empty")


def tarray_member(name: str, value_type: str, allocator: str | None = None) -> SoAMember:
    allocator_suffix = f", {allocator}" if allocator else ""
    return SoAMember(
        container_type=f"TArray<{value_type}{allocator_suffix}>",
        view_type=f"TArrayView<{value_type}>",
        const_view_type=f"TConstArrayView<{value_type}>",
        name=name,
    )


def soa_member(
    name: str,
    container_type: str,
    view_type: str | None = None,
    const_view_type: str | None = None,
) -> SoAMember:
    return SoAMember(
        container_type=container_type,
        view_type=view_type or f"{container_type}::View",
        const_view_type=const_view_type or f"{container_type}::ConstView",
        name=name,
    )


@dataclass(frozen=True)
class SoAStruct(Node):
    name: str
    view_name: str
    members: tuple[SoAMember, ...]
    storage_base: str = "ml::FSoAArrayMixin"
    view_base: str = "ml::FSoAViewMixin"

    def __post_init__(self) -> None:
        object.__setattr__(self, "members", tuple(self.members))
        if not self.members:
            raise ValueError(f"SOA struct {self.name!r} must contain at least one member")
        names = [member.name for member in self.members]
        duplicates = sorted({name for name in names if names.count(name) > 1})
        if duplicates:
            raise ValueError(
                f"SOA struct {self.name!r} has duplicate members: {', '.join(duplicates)}"
            )

    def _render_apply_arrays(self, indent: str) -> list[str]:
        lines = [
            f"{indent}template <typename TFunc>",
            f"{indent}auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {{",
            f"{indent}    return std::forward<TFunc>(func)(",
        ]
        final_index = len(self.members) - 1
        for index, member in enumerate(self.members):
            comma = "," if index != final_index else ""
            lines.append(f"{indent}        self.{member.name}{comma}")
        lines.extend([f"{indent}    );", f"{indent}}}"])
        return lines

    def _render_view(self) -> str:
        lines = [
            "template <bool is_const>",
            f"struct {self.view_name} : public {self.view_base} {{",
            f"    using View = {self.view_name}<false>;",
            f"    using ConstView = {self.view_name}<true>;",
            "",
        ]
        for member in self.members:
            declaration = (
                "    std::conditional_t<is_const, "
                f"{member.const_view_type}, {member.view_type}> {member.name};"
            )
            if len(declaration) <= 100:
                lines.append(declaration)
                continue
            lines.extend(
                [
                    "    std::conditional_t<is_const,",
                    f"                       {member.const_view_type},",
                    f"                       {member.view_type}>",
                    f"        {member.name};",
                ]
            )
        lines.append("")
        lines.extend(self._render_apply_arrays("    "))
        lines.append("};")
        return "\n".join(lines)

    def _render_storage(self) -> str:
        lines = [
            f"struct {self.name} : public {self.storage_base} {{",
            f"    using View = {self.view_name}<false>;",
            f"    using ConstView = {self.view_name}<true>;",
            "",
        ]
        for member in self.members:
            lines.append(f"    {member.container_type} {member.name};")
        lines.append("")
        lines.extend(self._render_apply_arrays("    "))
        lines.extend(
            [
                "",
                "    template <typename Self, typename Other, typename TFunc>",
                "        requires std::is_same_v<std::remove_cvref_t<Self>,",
                "                                std::remove_cvref_t<Other>>",
                "    auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func)",
                "        -> decltype(auto) {",
                "        return std::forward<TFunc>(func)(",
            ]
        )
        final_index = len(self.members) - 1
        for index, member in enumerate(self.members):
            lines.append(f"            self.{member.name},")
            comma = "," if index != final_index else ""
            lines.append(f"            other.{member.name}{comma}")
        lines.extend(["        );", "    }", "};"])
        return "\n".join(lines)

    def render(self, context: RenderContext) -> str:
        return context.apply_indent(f"{self._render_view()}\n\n{self._render_storage()}")
