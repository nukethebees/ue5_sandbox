from dataclasses import dataclass
from enum import Enum
import textwrap
from typing import Optional


@dataclass
class ConstantInput:
    value: str
    name: Optional[str] = None


@dataclass
class Constant:
    value: str
    name: str


class ConstantType(Enum):
    FName = 1
    FString = 2
    TStringView = 3
    FText = 4


indent = "    "


class ConstantGenerator:
    name: str
    constants: list[Constant]
    constant_types: set[ConstantType]

    def __init__(
        self,
        name: str,
        constant_inputs: list[ConstantInput],
        constant_types: Optional[set[ConstantType]] = None,
    ) -> None:
        self.name = name
        self.constants = self.process_input_constants(constant_inputs)
        self.constant_types = (
            constant_types
            if constant_types is not None
            else set(x for x in ConstantType)
        )

    def process_input_constants(self, values: list[ConstantInput]) -> list[Constant]:
        out: list[Constant] = []

        for v in values:
            name: str = v.name if v.name is not None else v.value.replace(" ", "_")
            out.append(Constant(v.value, name))

        return out

    def create_fn_sig(self, name: str, r_type: ConstantType) -> str:
        rt: Optional[str] = None

        match r_type:
            case ConstantType.TStringView:
                rt = f"::{r_type.name}<TCHAR>"
            case ConstantType.FName:
                rt = f"::{r_type.name}"
            case _:
                rt = f"::{r_type.name} const&"

        return f"static auto {name}() -> {rt}"

    def create_fn(self, name: str, r_type: ConstantType) -> str:
        out = ""
        out += self.create_fn_sig(name, r_type)
        out += " {"

        v = "v"

        value = "???"
        match r_type:
            case ConstantType.FName:
                value = f'TEXT("{name}")'
            case ConstantType.FString:
                value = f'TEXT("{name}")'
            case ConstantType.TStringView:
                value = f'TEXT("{name}")'
            case ConstantType.FText:
                value = f'::FText::FromName(TEXT("{name}"))'
            case _:
                raise ValueError(f"Unhandled: {r_type}")

        out += f"\n{indent}static ::{r_type.name} const {v} {{{value}}};"
        out += f"\n{indent}return {v};"

        out += "\n}"

        return out

    def create_substruct(self, return_type: ConstantType) -> str:
        o = f"struct {return_type.name} {{"

        for c in self.constants:
            o += "\n" + textwrap.indent(self.create_fn(c.name, return_type), indent)
        o += "\n};"

        return o

    def generate_file(self) -> str:
        o: str = f"""#pragma once
        
#include "CoreMinimal.h"

struct {self.name} {{"""

        for ct in sorted(list(self.constant_types), key=lambda x: x.name):
            o += "\n" + textwrap.indent(self.create_substruct(ct), indent)

        o += "\n};\n"

        return o
