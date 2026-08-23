from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

from Codegen.cpp import (
    CppType,
    MemberFunctionOperation,
    Node,
    REMOVE_AT_SWAP,
    TypeLike,
    composed_type,
    cpp_type,
    type_spelling,
)

TARRAY_REMOVE_AT_SWAP = MemberFunctionOperation("RemoveAtSwap")

@dataclass(frozen=True)
class FixedSoAContainer:
    name: str

    def __post_init__(self) -> None:
        if not self.name.strip():
            raise ValueError("Fixed SOA container name must not be empty")


@dataclass(frozen=True)
class FixedSoAConfig:
    storage_name: str
    containers: tuple[FixedSoAContainer, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "containers", tuple(self.containers))
        if not self.storage_name.strip():
            raise ValueError("Fixed SOA storage name must not be empty")
        container_names = tuple(container.name for container in self.containers)
        if len(set(container_names)) != len(container_names):
            raise ValueError("Fixed SOA container names must not contain duplicates")


@dataclass(frozen=True)
class SoAMember:
    container_type: CppType
    view_type: CppType
    const_view_type: CppType
    name: str
    view_function: str | None
    const_view_function: str | None

    def __post_init__(self) -> None:
        object.__setattr__(self, "container_type", cpp_type(self.container_type))
        object.__setattr__(self, "view_type", cpp_type(self.view_type))
        object.__setattr__(self, "const_view_type", cpp_type(self.const_view_type))
        types = (self.container_type, self.view_type, self.const_view_type)
        if any(not type_spelling(value).strip() for value in types) or not self.name.strip():
            raise ValueError("SOA member types and name must not be empty")
        if (self.view_function is None) != (self.const_view_function is None):
            raise ValueError(
                "SOA member view functions must be specified together"
            )
        if self.view_function is not None:
            if self.const_view_function is None:
                raise ValueError("SOA member view functions must be specified together")
            if not self.view_function.strip() or not self.const_view_function.strip():
                raise ValueError("SOA member view functions must not be empty")

    def view_expression(self, use_const_view: bool) -> str:
        if self.view_function is not None:
            function = self.const_view_function if use_const_view else self.view_function
            return f"{self.name}.{function}(offset, count)"
        view_type = self.const_view_type if use_const_view else self.view_type
        return f"{type_spelling(view_type)}{{{self.name}}}.Slice(offset, count)"

    def unchecked_index_expression(self, index: str) -> str:
        if type_spelling(self.container_type).startswith("TArray<"):
            return f"{self.name}.GetData()[{index}]"
        return f"{self.name}[{index}]"


@dataclass(frozen=True)
class ArraySoAMember(SoAMember):
    element_type: CppType

    def __post_init__(self) -> None:
        super().__post_init__()
        object.__setattr__(self, "element_type", cpp_type(self.element_type))


@dataclass(frozen=True)
class NestedSoAMember(SoAMember):
    fixed_schema: SoAStruct | None = None


class SoAStorageOperation(Enum):
    RESET = "reset"
    RESERVE = "reserve"
    ADD_UNINITIALISED = "add_uninitialised"
    ADD_DEFAULTED = "add_defaulted"
    REMOVE_AT_SWAP = "remove_at_swap"
    SET_NUM = "set_num"
    COPY_ELEMENT = "copy_element"
    APPEND_FROM = "append_from"


OUT_OF_LINE_STORAGE_OPERATIONS = frozenset(
    (
        SoAStorageOperation.RESET,
        SoAStorageOperation.RESERVE,
        SoAStorageOperation.ADD_UNINITIALISED,
        SoAStorageOperation.ADD_DEFAULTED,
        SoAStorageOperation.SET_NUM,
    )
)


def tarray_member(
    name: str, value_type: TypeLike, allocator: TypeLike | None = None
) -> ArraySoAMember:
    allocator_suffix = f", {type_spelling(allocator)}" if allocator else ""
    element = type_spelling(value_type)
    contained_types = (value_type, allocator) if allocator else (value_type,)
    return ArraySoAMember(
        container_type=composed_type(
            f"TArray<{element}{allocator_suffix}>",
            *contained_types,
            header="Containers/Array.h",
            operations={REMOVE_AT_SWAP: TARRAY_REMOVE_AT_SWAP},
        ),
        view_type=composed_type(
            f"TArrayView<{element}>", value_type, header="Containers/ArrayView.h"
        ),
        const_view_type=composed_type(
            f"TConstArrayView<{element}>", value_type, header="Containers/ArrayView.h"
        ),
        name=name,
        view_function=None,
        const_view_function=None,
        element_type=cpp_type(value_type),
    )


def soa_member(
    name: str,
    container_type: TypeLike,
    view_type: TypeLike | None = None,
    const_view_type: TypeLike | None = None,
    *,
    fixed_schema: SoAStruct | None = None,
) -> NestedSoAMember:
    container_spelling = type_spelling(container_type)
    return NestedSoAMember(
        container_type=cpp_type(container_type),
        view_type=cpp_type(
            view_type
            or composed_type(f"{container_spelling}::View", container_type)
        ),
        const_view_type=cpp_type(
            const_view_type
            or composed_type(f"{container_spelling}::ConstView", container_type)
        ),
        name=name,
        view_function="get_view",
        const_view_function="get_const_view",
        fixed_schema=fixed_schema,
    )


@dataclass(frozen=True)
class SoAStructNames:
    name: str
    view_name: str
    const_view_name: str

    def __init__(
        self,
        name: str,
        view_name: str | None = None,
        const_view_name: str | None = None,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "view_name", view_name if view_name is not None else f"{name}View")
        object.__setattr__(
            self,
            "const_view_name",
            const_view_name if const_view_name is not None else f"{name}ConstView",
        )
        if not self.name.strip():
            raise ValueError("SOA struct name must not be empty")
        if not self.view_name.strip() or not self.const_view_name.strip():
            raise ValueError("SOA view struct names must not be empty")


@dataclass(frozen=True)
class SoAStruct:
    names: SoAStructNames
    members: tuple[ArraySoAMember | NestedSoAMember, ...]
    storage_export_specifier: str | None = None
    storage_operations: tuple[SoAStorageOperation, ...] = ()
    nodes: tuple[Node, ...] = ()
    source_nodes: tuple[Node, ...] = ()
    equivalent_type: TypeLike | None = None
    copy_element_memberwise: bool = False
    fixed: FixedSoAConfig | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "members", tuple(self.members))
        object.__setattr__(self, "storage_operations", tuple(self.storage_operations))
        object.__setattr__(self, "nodes", tuple(self.nodes))
        object.__setattr__(self, "source_nodes", tuple(self.source_nodes))
        if not self.members:
            raise ValueError(
                f"SOA struct {self.names.name!r} must contain at least one member"
            )
        member_names = tuple(member.name for member in self.members)
        if len(set(member_names)) != len(member_names):
            raise ValueError("SOA structs must not contain duplicate members")
        if len(set(self.storage_operations)) != len(self.storage_operations):
            raise ValueError("SOA storage operations must not contain duplicates")
        if self.fixed is not None:
            for member in self.members:
                if isinstance(member, ArraySoAMember):
                    continue
                if (
                    member.fixed_schema is not None
                    and member.fixed_schema.fixed is not None
                ):
                    continue
                raise ValueError(
                    f"Fixed SOA storage {self.fixed.storage_name!r} cannot represent "
                    f"member {member.name!r} without an element type or fixed schema"
                )


@dataclass(frozen=True)
class HomogeneousSoAValueType:
    cpp_type: TypeLike
    suffix: str
    equivalent_type: TypeLike | None = None
    input_types: tuple[TypeLike, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "input_types", tuple(self.input_types))
        if not type_spelling(self.cpp_type).strip() or not self.suffix.strip():
            raise ValueError("Homogeneous SOA value type and suffix must not be empty")
        if any(not type_spelling(value).strip() for value in self.input_types):
            raise ValueError("Homogeneous SOA input types must not be empty")


@dataclass(frozen=True)
class HomogeneousSoALayout:
    name: str
    components: tuple[str, ...]
    value_types: tuple[HomogeneousSoAValueType, ...]
    storage_export_specifier: str | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "components", tuple(self.components))
        object.__setattr__(self, "value_types", tuple(self.value_types))
        if not self.name.strip():
            raise ValueError("Homogeneous SOA layout name must not be empty")
        if not self.components or any(
            not component.strip() for component in self.components
        ):
            raise ValueError("Homogeneous SOA components must not be empty")
        if len(set(self.components)) != len(self.components):
            raise ValueError("Homogeneous SOA components must not contain duplicates")
        if not self.value_types:
            raise ValueError("Homogeneous SOA layout must contain value types")
        equivalent_types = tuple(value.equivalent_type for value in self.value_types)
        if any(equivalent_types) and not all(equivalent_types):
            raise ValueError(
                "Homogeneous SOA layouts must define equivalent types for every value type"
            )

    @property
    def view_name(self) -> str:
        return f"T{self.name}View"

    def storage_name(self, value_type: HomogeneousSoAValueType) -> str:
        return f"F{self.name}{value_type.suffix}"

    @property
    def has_equivalent_type(self) -> bool:
        return all(value.equivalent_type is not None for value in self.value_types)

    @property
    def equivalent_type_trait_name(self) -> str:
        return f"T{self.name}EquivalentType"
