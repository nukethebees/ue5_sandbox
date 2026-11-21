from datetime import datetime
import json
from pathlib import Path
import shutil
import subprocess
import sys


def report_vsix_log(path):
    try:
        data = path.read_bytes()
    except OSError as error:
        print(f"VSIXInstaller log unavailable at {path}: {error.strerror}", file=sys.stderr)
        return

    encoding = "utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
    detail = ""
    last_entry = ""
    for line in data.decode(encoding, errors="replace").splitlines():
        _, separator, message = line.partition(" - ")
        if separator and message.strip("- "):
            last_entry = message.strip()
        if "Exception:" in line:
            detail = (message if separator else line).rsplit("--->", 1)[-1].strip()
    if detail or last_entry:
        print(detail or last_entry, file=sys.stderr)
    print(f"VSIXInstaller log: {path}", file=sys.stderr)


def install(vswhere, vscode_package, visualstudio_package):
    code = shutil.which("code.cmd")
    if not code:
        raise RuntimeError("VS Code CLI (code.cmd) was not found on PATH.")

    try:
        discovery = subprocess.run(
            [vswhere, "-latest", "-version", "[18.0,19.0)", "-products", "*",
             "-requires", "Microsoft.VisualStudio.Component.CoreEditor", "-format", "json", "-utf8"],
            stdout=subprocess.PIPE, check=True,
        )
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"vswhere failed to locate Visual Studio (exit {error.returncode}).") from error
    instances = json.loads(discovery.stdout)
    if not instances:
        raise RuntimeError("vswhere did not find a Visual Studio 2026 IDE.")
    instance = instances[0]
    installer = Path(instance["installationPath"]) / "Common7/IDE/VSIXInstaller.exe"
    if not installer.is_file():
        raise RuntimeError(f"Visual Studio extension installer was not found at {installer}.")

    visualstudio_package = Path(visualstudio_package).resolve()
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    log = visualstudio_package.with_suffix(f".{stamp}.log")
    commands = (
        ("VS Code", [code, "--install-extension", vscode_package, "--force"]),
        (instance["displayName"], [installer, "/quiet", f"/instanceIds:{instance['instanceId']}",
                                   f"/logFile:{log}", visualstudio_package]),
    )
    success = True
    for editor, command in commands:
        print(f"Installing Lispb for {editor}...", flush=True)
        try:
            subprocess.run(command, check=True)
        except subprocess.CalledProcessError as error:
            status = error.returncode
            print(f"{editor} installation failed (exit {status}, 0x{status & 0xffffffff:08X}).",
                  file=sys.stderr)
            if command[0] == installer:
                report_vsix_log(log)
            success = False
        except OSError as error:
            print(f"Could not start {editor} installer at {command[0]}: {error}", file=sys.stderr)
            success = False
        else:
            print(f"{editor}: installation succeeded.", flush=True)
    return success


if __name__ == "__main__":
    try:
        ok = install(*sys.argv[1:])
    except (OSError, RuntimeError, ValueError) as error:
        sys.exit(f"Installation skipped for both editors: {error}")
    sys.exit(0 if ok else 1)
