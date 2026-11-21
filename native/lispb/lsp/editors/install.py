from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


INSTALL_PRESET = "install-lispb-editors"
BUSY_MARKER_ENV = "LISPB_VSIX_BUSY_MARKER"
WORKFLOW_CLI = "python native/lispb/lsp/editors/install.py --workflow"


def print_busy_hint(retry_command):
    hint = (
        "Save your work and close the Visual Studio windows for this instance, "
        f"then rerun {retry_command}."
    )
    if (
        sys.stderr.isatty()
        and not os.getenv("NO_COLOR")
        and os.getenv("TERM") != "dumb"
    ):
        hint = f"\x1b[33m{hint}\x1b[0m"
    print(f"\n{hint}", file=sys.stderr)


def run_workflow():
    with tempfile.TemporaryDirectory(prefix="lispb-install-") as directory:
        marker = Path(directory) / "vs-busy"
        result = subprocess.run(
            ["cmake", "--workflow", "--preset", INSTALL_PRESET],
            env={**os.environ, BUSY_MARKER_ENV: str(marker)},
        )
        if result.returncode and marker.is_file():
            print_busy_hint(WORKFLOW_CLI)
        return result.returncode


def report_vsix_log(path, package_log):
    try:
        data = path.read_bytes()
    except OSError as error:
        print(
            f"VSIXInstaller log unavailable at {path}: {error.strerror}",
            file=sys.stderr,
        )
        return

    try:
        package_log.write_bytes(data)
        path = package_log
    except OSError as error:
        print(
            f"Could not copy the installer log to {package_log}: {error.strerror}",
            file=sys.stderr,
        )

    encoding = "utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
    detail = ""
    last_entry = ""
    blockers = []
    reading_blockers = False
    for line in data.decode(encoding, errors="replace").splitlines():
        if reading_blockers and line.startswith("- "):
            blockers.append(line)
            continue
        reading_blockers = False
        _, separator, message = line.partition(" - ")
        if separator and message.strip("- "):
            last_entry = message.strip()
        if "Exception:" in line:
            detail = (message if separator else line).rsplit("--->", 1)[-1].strip()
            blockers.clear()
            reading_blockers = "BlockingProcessesException:" in detail
    if detail or last_entry:
        print(detail or last_entry, file=sys.stderr)
    for blocker in blockers:
        print(blocker, file=sys.stderr)
    print(f"VSIXInstaller log: {path}", file=sys.stderr)


def discover_visual_studio(vswhere):
    try:
        discovery = subprocess.run(
            [
                vswhere,
                "-latest",
                "-version",
                "[18.0,19.0)",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.CoreEditor",
                "-format",
                "json",
                "-utf8",
            ],
            stdout=subprocess.PIPE,
            check=True,
        )
    except subprocess.CalledProcessError as error:
        raise RuntimeError(
            f"vswhere failed to locate Visual Studio (exit {error.returncode})."
        ) from error
    instances = json.loads(discovery.stdout)
    if not instances:
        raise RuntimeError("vswhere did not find a Visual Studio 2026 IDE.")
    instance = instances[0]
    installer = Path(instance["installationPath"]) / "Common7/IDE/VSIXInstaller.exe"
    if not installer.is_file():
        raise RuntimeError(
            f"Visual Studio extension installer was not found at {installer}."
        )

    return instance, installer


def run_installer(editor, command):
    print(f"Installing Lispb for {editor}...", flush=True)
    try:
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as error:
        status = error.returncode
        print(
            f"{editor} installation failed (exit {status}, 0x{status & 0xFFFFFFFF:08X}).",
            file=sys.stderr,
        )
        return status
    except OSError as error:
        print(
            f"Could not start {editor} installer at {command[0]}: {error}",
            file=sys.stderr,
        )
        return None

    print(f"{editor}: installation succeeded.", flush=True)
    return 0


def install_visual_studio(instance, installer, package, busy_marker):
    package = Path(package).resolve()
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    package_log = package.with_suffix(f".{stamp}.log")
    # VSIXInstaller resolves /logFile against the Windows temp directory (TMP first).
    temp_dir = os.environ.get("TMP") or os.environ.get("TEMP") or tempfile.gettempdir()
    log = Path(temp_dir) / package_log.name
    status = run_installer(
        instance["displayName"],
        [
            installer,
            "/quiet",
            f"/instanceIds:{instance['instanceId']}",
            f"/logFile:{log.name}",
            package,
        ],
    )
    if status is None:
        return False
    if status == 0:
        return True

    report_vsix_log(log, package_log)
    if status == 2004:
        if busy_marker:
            Path(busy_marker).touch()
        else:
            print_busy_hint(f"cmake --workflow --preset {INSTALL_PRESET}")
    return False


def install(vswhere, vscode_package, visualstudio_package, *, busy_marker=None):
    code = shutil.which("code.cmd")
    if not code:
        raise RuntimeError("VS Code CLI (code.cmd) was not found on PATH.")
    instance, installer = discover_visual_studio(vswhere)

    vscode = run_installer(
        "VS Code", [code, "--install-extension", vscode_package, "--force"]
    )
    visual_studio_ok = install_visual_studio(
        instance, installer, visualstudio_package, busy_marker
    )
    return vscode == 0 and visual_studio_ok


if __name__ == "__main__":
    if sys.argv[1:] == ["--workflow"]:
        try:
            sys.exit(run_workflow())
        except OSError as error:
            sys.exit(f"Could not run the Lispb installation workflow: {error}")
    try:
        ok = install(*sys.argv[1:], busy_marker=os.getenv(BUSY_MARKER_ENV))
    except (OSError, RuntimeError, ValueError) as error:
        sys.exit(f"Installation skipped for both editors: {error}")
    sys.exit(0 if ok else 1)
