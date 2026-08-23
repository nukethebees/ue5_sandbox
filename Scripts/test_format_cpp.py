import contextlib
import importlib.util
import io
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import override


SCRIPT_PATH = Path(__file__).with_name("format-cpp.py")
SPEC = importlib.util.spec_from_file_location("format_cpp", SCRIPT_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load formatter module from {SCRIPT_PATH}")
format_cpp = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(format_cpp)


class FormatCppSelectionTests(unittest.TestCase):
    @override
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.repository_root = Path(self.temporary_directory.name)
        self.source_directory = self.repository_root / "Source"
        self.source_directory.mkdir()
        self.extensions = format_cpp.get_file_extensions(False)
        self.run_git("init", "-q")
        self.run_git("config", "user.email", "format-test@example.com")
        self.run_git("config", "user.name", "Format Test")

    @override
    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def run_git(self, *arguments: str) -> None:
        subprocess.run(["git", *arguments], cwd=self.repository_root, check=True, capture_output=True)

    def write_file(self, relative_path: str, content: str = "int main() {}\n") -> Path:
        file_path = self.repository_root / relative_path
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.write_text(content, encoding="utf-8")
        return file_path

    def commit_all(self) -> None:
        self.run_git("add", "--all")
        self.run_git("commit", "-qm", "initial")

    def test_staged_files_exclude_unstaged_only_files(self) -> None:
        staged_file = self.write_file("Source/staged.cpp")
        unstaged_file = self.write_file("Source/unstaged.cpp")
        self.run_git("add", "Source/staged.cpp")

        files = format_cpp.select_staged_files(self.repository_root, [self.source_directory], self.extensions)

        self.assertEqual(files, [staged_file.resolve()])
        self.assertNotIn(unstaged_file.resolve(), files)

    def test_changed_files_include_working_tree_changes(self) -> None:
        changed_file = self.write_file("Source/changed.cpp")
        self.commit_all()
        changed_file.write_text("int main() { return 1; }\n", encoding="utf-8")

        files = format_cpp.select_changed_files(self.repository_root, [self.source_directory], self.extensions)

        self.assertEqual(files, [changed_file.resolve()])

    def test_deleted_and_unsupported_staged_files_are_ignored(self) -> None:
        deleted_file = self.write_file("Source/deleted.h")
        unsupported_file = self.write_file("Source/notes.txt")
        self.commit_all()
        deleted_file.unlink()
        unsupported_file.write_text("changed\n", encoding="utf-8")
        self.run_git("add", "--all")

        files = format_cpp.select_staged_files(self.repository_root, [self.source_directory], self.extensions)

        self.assertEqual(files, [])

    def test_all_files_retains_full_project_selection(self) -> None:
        cpp_file = self.write_file("Source/all.cpp")
        self.write_file("Source/notes.txt")

        files = format_cpp.select_all_files([self.source_directory], self.extensions)

        self.assertEqual(files, [cpp_file.resolve()])

    def test_conflicting_modes_are_rejected(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                format_cpp.create_parser().parse_args(["--staged", "--changed"])

    def test_repository_root_is_found_from_a_subdirectory(self) -> None:
        self.assertEqual(format_cpp.get_repository_root(self.source_directory), self.repository_root.resolve())

    def test_staged_files_with_unstaged_edits_are_detected(self) -> None:
        mixed_file = self.write_file("Source/mixed.cpp")
        self.commit_all()
        mixed_file.write_text("int main() { return 1; }\n", encoding="utf-8")
        self.run_git("add", "Source/mixed.cpp")
        mixed_file.write_text("int main() { return 2; }\n", encoding="utf-8")
        selected_files = format_cpp.select_staged_files(self.repository_root, [self.source_directory], self.extensions)

        mixed_files = format_cpp.select_unstaged_files(self.repository_root, selected_files)

        self.assertEqual(mixed_files, [mixed_file.resolve()])


if __name__ == "__main__":
    unittest.main()
