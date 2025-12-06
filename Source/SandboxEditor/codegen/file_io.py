from dataclasses import dataclass
from pathlib import Path
from typing import Optional

@dataclass
class FileBuffer:
    path: Path
    content: str = ""
    namespace: Optional[str] = None

    def __iadd__(self, other: str) -> FileBuffer:
        if not isinstance(other, str):
            raise ValueError("Not a string")
        self.content += other
        return self

    def get_namespace(self, ns: Optional[str] = None) -> str:
        if not ns:
            ns = self.namespace
        if not ns:
            raise ValueError("Namespace is None")
        return ns

    def namespace_start(self, ns: Optional[str] = None) -> None:
        ns = self.get_namespace(ns)
        self.content += f"\n\nnamespace {ns} {{"
    def namespace_end(self, ns: Optional[str] = None) -> None:
        ns = self.get_namespace(ns)
        self.content += f"\n}} // namespace {ns}"

    def write(self):
        with open(self.path, "w") as file:
            file.write(self.content)