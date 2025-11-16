from dataclasses import dataclass
from pathlib import Path

@dataclass
class FileBuffer:
    path: Path
    content: str = ""

    def __iadd__(self, other: str) -> FileBuffer:
        if not isinstance(other, str):
            raise ValueError("Not a string")
        self.content += other
        return self

    def write(self):
        with open(self.path, "w") as file:
            file.write(self.content)