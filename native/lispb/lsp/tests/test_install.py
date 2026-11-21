from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "editors"))
from install import install


class InstallerTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="Lispb installer ")
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
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
                self.log = Path(args[3].removeprefix("/logFile:"))
                if self.log_bytes is not None:
                    self.log.write_bytes(self.log_bytes)
                raise subprocess.CalledProcessError(2148734468, args)
            raise subprocess.CalledProcessError(1, args)
        result = json.dumps(self.instances).encode() if args[0] == self.vswhere else None
        return subprocess.CompletedProcess(args, 0, stdout=result)

    def test_install_into_selected_editors(self):
        self.assertTrue(install(self.vswhere, *self.packages))
        self.which.assert_called_once_with("code.cmd")
        log = self.calls[-1][3]
        self.assertTrue(log.startswith(f"/logFile:{self.packages[1].with_suffix('')}."))
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
                self.assertIn(f"VSIXInstaller log: {self.log}", output)
                self.assertNotIn("TargetInvocationException", output)
                self.assertNotIn("App.Initialize", output)


if __name__ == "__main__":
    unittest.main()
