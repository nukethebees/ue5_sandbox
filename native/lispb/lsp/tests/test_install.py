from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "editors"))
from install import BUSY_MARKER_ENV, WORKFLOW_CLI, install


class InstallerTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="Lispb installer ")
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        self.installer_temp = root / "Windows Temp"
        self.installer_temp.mkdir()
        self.enterContext(patch.dict(os.environ, {"TMP": str(self.installer_temp)}))
        self.packages = [root / "vscode.vsix", root / "visualstudio.vsix"]
        self.instance = {"installationPath": str(root / "VS 2026"), "instanceId": "123abc",
                         "displayName": "Visual Studio Community 2026"}
        self.installer = Path(self.instance["installationPath"]) / "Common7/IDE/VSIXInstaller.exe"
        self.installer.parent.mkdir(parents=True)
        self.installer.touch()
        self.code = str(root / "code.cmd")
        self.vswhere = str(root / "vswhere.exe")
        self.instances = [self.instance]
        self.calls = []
        self.fail = None
        self.failure_status = 1
        self.log_bytes = None
        self.output = StringIO()
        self.enterContext(redirect_stdout(self.output))
        self.enterContext(redirect_stderr(self.output))
        self.which = self.enterContext(patch("install.shutil.which", return_value=self.code))
        self.enterContext(patch("install.subprocess.run", side_effect=self.run_cli))

    def run_cli(self, args, *, check, stdout=None):
        self.assertTrue(check)
        self.calls.append(args)
        if args[0] == self.fail:
            if self.fail == self.installer:
                name = args[3].removeprefix("/logFile:")
                self.assertEqual(name, Path(name).name)
                self.log = self.installer_temp / name
                if self.log_bytes is not None:
                    self.log.write_bytes(self.log_bytes)
            raise subprocess.CalledProcessError(self.failure_status, args)
        result = json.dumps(self.instances).encode() if args[0] == self.vswhere else None
        return subprocess.CompletedProcess(args, 0, stdout=result)

    def test_install_into_selected_editors(self):
        self.assertTrue(install(self.vswhere, *self.packages))
        self.which.assert_called_once_with("code.cmd")
        log = self.calls[-1][3]
        self.assertTrue(log.startswith(f"/logFile:{self.packages[1].stem}."))
        self.assertTrue(log.endswith(".log"))
        self.assertEqual(self.calls, [
            [self.vswhere, "-latest", "-version", "[18.0,19.0)", "-products", "*",
             "-requires", "Microsoft.VisualStudio.Component.CoreEditor", "-format", "json", "-utf8"],
            [self.code, "--install-extension", self.packages[0], "--force"],
            [self.installer, "/quiet", "/instanceIds:123abc", log, self.packages[1]],
        ])

    def test_missing_installers_stop_before_either_installation(self):
        self.which.return_value = None
        with self.assertRaisesRegex(RuntimeError, "code.cmd"):
            install(self.vswhere, *self.packages)
        self.assertFalse(self.calls)

        self.which.return_value = self.code
        self.instances = []
        with self.assertRaisesRegex(RuntimeError, "Visual Studio 2026"):
            install(self.vswhere, *self.packages)
        self.instances = [self.instance]
        self.installer.unlink()
        with self.assertRaisesRegex(RuntimeError, "VSIXInstaller.exe"):
            install(self.vswhere, *self.packages)
        self.assertEqual([args[0] for args in self.calls], [self.vswhere, self.vswhere])

    def test_discovery_failure_names_the_tool_and_exit_status(self):
        self.fail = self.vswhere
        with self.assertRaisesRegex(RuntimeError, r"vswhere failed.*exit 1"):
            install(self.vswhere, *self.packages)
        self.assertEqual([args[0] for args in self.calls], [self.vswhere])

    def test_each_editor_reports_its_own_result(self):
        for failing, editor in ((self.code, "VS Code"), (self.installer, self.instance["displayName"])):
            with self.subTest(editor=editor):
                self.calls.clear()
                self.output.seek(0)
                self.output.truncate()
                self.fail = failing
                self.assertFalse(install(self.vswhere, *self.packages))
                self.assertEqual([args[0] for args in self.calls], [self.vswhere, self.code, self.installer])
                self.assertIn(f"{editor} installation failed", self.output.getvalue())
                self.assertIn("installation succeeded", self.output.getvalue())
                if failing == self.installer:
                    self.assertIn("VSIXInstaller log unavailable", self.output.getvalue())

    def test_visual_studio_reports_the_inner_exception_and_log_path(self):
        self.fail = self.installer
        self.failure_status = 2148734468
        reason = "Microsoft.VisualStudio.ExtensionManager.InvalidExtensionPackageException: The file is not a valid VSIX package."
        for encoding in ("utf-8-sig", "utf-16"):
            with self.subTest(encoding=encoding):
                self.output.seek(0)
                self.output.truncate()
                self.log_bytes = (f"21/11/2026 15:49:04 - System.Reflection.TargetInvocationException: "
                                  f"Exception has been thrown by the target of an invocation. ---> {reason}\r\n"
                                  "   at VSIXInstaller.App.Initialize()\r\n").encode(encoding)
                self.assertFalse(install(self.vswhere, *self.packages))
                output = self.output.getvalue()
                self.assertIn("exit 2148734468, 0x80131604", output)
                self.assertIn(reason, output)
                copy = self.packages[1].with_name(self.log.name)
                self.assertEqual(copy.read_bytes(), self.log_bytes)
                self.assertIn(f"VSIXInstaller log: {copy}", output)
                self.assertNotIn("TargetInvocationException", output)
                self.assertNotIn("App.Initialize", output)

    def test_busy_instance_reports_the_blocking_processes_and_recovery(self):
        self.fail = self.installer
        self.failure_status = 2004
        self.log_bytes = (
            b"21/11/2026 17:56:42 - VSIXInstaller.BlockingProcessesException: Close these processes:\n"
            b"- Example - Microsoft Visual Studio (ID 1234)\n"
            b"- vshost.exe (ID 5678)\n"
            b"   at VSIXInstaller.SilentBlocker.BlockingProcessBlocker()\n"
        )
        self.assertFalse(install(self.vswhere, *self.packages))
        output = self.output.getvalue()
        self.assertIn("exit 2004, 0x000007D4", output)
        self.assertIn("VSIXInstaller log: ", output)
        hint = output.splitlines()[-1]
        self.assertTrue(hint.startswith("Save your work and close"))
        self.assertTrue(hint.endswith("cmake --workflow --preset install-lispb-editors."))
        self.assertNotIn("\x1b", output)
        self.assertIn("Example - Microsoft Visual Studio (ID 1234)", output)
        self.assertIn("vshost.exe (ID 5678)", output)
        self.assertNotIn("at VSIXInstaller", output)

    def test_busy_instance_without_log_still_explains_how_to_recover(self):
        self.fail = self.installer
        self.failure_status = 2004
        self.enterContext(patch.object(self.output, "isatty", return_value=True))
        for no_color, term, colored in (("", "xterm", True), ("1", "xterm", False), ("", "dumb", False)):
            with self.subTest(no_color=no_color, term=term):
                self.output.seek(0)
                self.output.truncate()
                with patch.dict(os.environ, {"NO_COLOR": no_color, "TERM": term}):
                    self.assertFalse(install(self.vswhere, *self.packages))
                output = self.output.getvalue()
                self.assertIn("VSIXInstaller log unavailable", output)
                hint = output.splitlines()[-1]
                start = "\x1b[33m" if colored else ""
                reset = "\x1b[0m" if colored else ""
                self.assertTrue(hint.startswith(f"{start}Save your work"))
                self.assertTrue(hint.endswith(f"install-lispb-editors.{reset}"))

    def test_outer_command_receives_only_visual_studio_blockers(self):
        marker = self.installer_temp / "vs-busy"
        for failing, status, busy in ((self.code, 2004, False), (self.installer, 42, False),
                                      (self.installer, 2004, True)):
            with self.subTest(failing=failing, status=status):
                self.fail = failing
                self.failure_status = status
                self.assertFalse(install(self.vswhere, *self.packages, busy_marker=marker))
                self.assertEqual(marker.exists(), busy)
                self.assertNotIn("Save your work", self.output.getvalue())


class WorkflowTests(unittest.TestCase):
    def test_cli_reports_blockers_after_ninja_exits(self):
        script = Path(__file__).resolve().parents[1] / "editors/install.py"
        with tempfile.TemporaryDirectory(prefix="Lispb workflow ") as directory:
            root = Path(directory)
            (root / "CMakePresets.json").write_text(json.dumps({
                "version": 6,
                "configurePresets": [{"name": "test", "generator": "Ninja", "binaryDir": "${sourceDir}/build"}],
                "buildPresets": [{"name": "test", "configurePreset": "test"}],
                "workflowPresets": [{"name": "install-lispb-editors", "steps": [
                    {"type": "configure", "name": "test"}, {"type": "build", "name": "test"},
                ]}],
            }))
            for action, busy in (("false", True), ("false", False), ("true", False)):
                with self.subTest(action=action, busy=busy):
                    mark = f'COMMAND "${{CMAKE_COMMAND}}" -E touch "$ENV{{{BUSY_MARKER_ENV}}}"' if busy else ""
                    (root / "CMakeLists.txt").write_text(f"""
cmake_minimum_required(VERSION 3.25)
project(test NONE)
add_custom_target(simulate_install ALL
  {mark}
  COMMAND "${{CMAKE_COMMAND}}" -E {action}
  VERBATIM
)
""")
                    result = subprocess.run([sys.executable, script, "--workflow"], cwd=root,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(result.returncode == 0, action == "true", result.stdout)
                    if busy:
                        footer, hint = result.stdout.split("ninja: build stopped:", 1)
                        self.assertNotIn("Save your work", footer)
                        self.assertEqual(hint.count("Save your work"), 1)
                        self.assertTrue(hint.rstrip().endswith(f"then rerun {WORKFLOW_CLI}."), hint)
                    else:
                        self.assertNotIn("Save your work", result.stdout)


if __name__ == "__main__":
    unittest.main()
