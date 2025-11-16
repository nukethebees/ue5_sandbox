from dataclasses import dataclass
from enum import Enum

@dataclass
class UnrealType:
    class Tag(Enum):
        ENUM = 0,
        STRUCT = 1,
        CLASS = 2

    name: str
    tag: Tag

    def __str__(self) -> str:
        return self.typename()

    def rawname(self) -> str:
        return self.name
    def typename(self) -> str:
        match self.tag:
            case self.Tag.ENUM:
                return self.enumname()
            case self.Tag.STRUCT:
                return self.structname()
            case self.Tag.CLASS:
                return self.classname()
            case _:
                return "???"
            
        return self.name
    def enumname(self) -> str:
        return "E" + self.name
    def classname(self) -> str:
        return "U" + self.name
    def structname(self) -> str:
        return "F" + self.name
