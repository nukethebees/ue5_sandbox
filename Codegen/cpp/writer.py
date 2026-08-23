from __future__ import annotations

from collections.abc import Generator, Iterable
from contextlib import contextmanager


class CppWriter:
    """Small deterministic writer for C++ that is awkward to express as nodes."""

    def __init__(self, indent_text: str = "    ") -> None:
        if not indent_text:
            raise ValueError("Indent text must not be empty")
        self._indent_text = indent_text
        self._indent_level = 0
        self._lines: list[str] = []

    def line(self, text: str = "") -> None:
        if "\n" in text:
            raise ValueError("CppWriter.line accepts exactly one line")
        prefix = self._indent_text * self._indent_level if text else ""
        self._lines.append(f"{prefix}{text}")

    def lines(self, lines: Iterable[str]) -> None:
        for line in lines:
            self.line(line)

    @contextmanager
    def indented(self) -> Generator[None, None, None]:
        self._indent_level += 1
        try:
            yield
        finally:
            self._indent_level -= 1

    @contextmanager
    def block(self, declaration: str, closing: str = "}") -> Generator[None, None, None]:
        self.line(f"{declaration} {{")
        with self.indented():
            yield
        self.line(closing)

    def render(self) -> str:
        if self._indent_level != 0:
            raise ValueError("Cannot render while an indentation context is active")
        return "\n".join(self._lines)
