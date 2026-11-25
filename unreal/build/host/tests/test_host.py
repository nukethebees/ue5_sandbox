import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(exe, args, cwd, case="", diagnostic=None):
    result = subprocess.run(
        [exe, *args, "--user-dir", cwd / "user data"],
        cwd=cwd, capture_output=True, timeout=125,
        env={**os.environ, "SANDBOX_FIXTURE_CASE": case},
    )
    output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    if diagnostic is None:
        assert result.returncode == 0, (result.returncode, output)
    else:
        assert result.returncode != 0 and diagnostic in output, (result.returncode, output)
    return output


def fixture_cases(case, exe, fixture, fixture_host, scripts, cwd):
    binaries = cwd / "fixture project" / "Binaries" / "Win64"
    binaries.mkdir(parents=True)
    shutil.copyfile(fixture_host, binaries / exe.name)
    shutil.copyfile(fixture, binaries / "Sandbox.dll")
    host = binaries / exe.name
    args = ["--editor-scripts", scripts]
    if case == "display":
        output = run(host, [*args, "--display-mode", "windowed", "--window-size", "1280x720"], cwd, "display")
        # Win32 can send multiple size notifications for the same geometry.
        sizes = [line.removeprefix("fixture:client ") for line in output.splitlines()
                 if line.startswith("fixture:client ")]
        changes = [size for index, size in enumerate(sizes) if not index or size != sizes[index - 1]]
        assert changes == ["1280x720"], output
        assert output.count("fixture:forward") == 1 and output.count("fixture:shutdown") == 1, output
        assert not (cwd / "user data/Config/user-settings.scm").exists()
        return
    if case == "cli":
        for size in ["0x10", "9x0", "1280", "-1x2", "2x3x4", "2147483648x1"]:
            output = run(host, [*args, "--display-mode", "windowed", "--window-size", size], cwd, "cli", "--window-size")
            assert "fixture:prepare" not in output, output

        for mode in [[], ["--display-mode", "headless"], ["--display-mode", "borderless"]]:
            run(host, [*args, *mode, "--window-size", "200x200"], cwd, "cli", "--window-size")
        run(host, [*args, "--display-mode", "fullscreen"], cwd, "cli", "--display-mode")

        output = run(host, [*args, "--display-mode", "windowed", "--exit-when-ready"], cwd, "cli")
        assert "fixture:client" in output and "Ready: MainMenu" in output, output
        return
    for mode, diagnostic, expected in [
        ("prepare", "fixture settings failed", ["prepare"]),
        ("init", "UELibrary_InitW failed (7)", ["prepare", "init", "detach"]),
        ("readiness", "playable world", ["prepare", "init", "ready", "shutdown", "detach"]),
        ("tick", "UELibrary_Tick failed (8)", ["prepare", "init", "ready", "tick", "shutdown", "detach"]),
        ("quit", None, ["prepare", "init", "ready", "tick", "shutdown", "detach"]),
    ]:
        output = run(host, [*args, "--display-mode", "headless"], cwd, mode, diagnostic)
        events = [line.removeprefix("fixture:") for line in output.splitlines() if line.startswith("fixture:")]
        assert events == expected, (mode, output)
    shutil.copyfile(exe, host)
    output = run(host, [*args, "--display-mode", "headless"], cwd, "mismatch", "different build identities")
    assert "fixture:prepare" not in output, output


def test_settings(exe, cwd):
    user = cwd / "user data 測試"
    preferences = user / "Config" / "user-settings.scm"
    preferences.parent.mkdir(parents=True)
    preferences.write_text(
        "(export preferences)\n"
        "(define defaults (module-ref (import-module \"Settings/user-defaults.scm\") 'preferences))\n"
        "(define preferences (copy-with defaults :window-mode 'fullscreen :resolution (resolution 800 600)))\n",
        encoding="utf-8",
    )
    for phase, args in [
        ("windowed", ["--display-mode", "windowed", "--window-size", "800x500"]),
        ("headless", ["--display-mode", "headless"]),
        ("borderless", []),
        ("relaunch", []),
        ("forced-borderless", ["--display-mode", "borderless"]),
    ]:
        env = {**os.environ, "SANDBOX_STAGED_SETTINGS_TEST": phase}
        result = subprocess.run([exe, "--user-dir", user, *args], cwd=cwd, env=env,
                                timeout=60, capture_output=True)
        log = next(user.rglob("Sandbox.log"), None)
        content = log.read_text(encoding="utf-8-sig", errors="replace") if log else ""
        assert result.returncode == 0 and "TEST COMPLETE. EXIT CODE: 0" in content and (
            "Result={Success}" in content and "Sandbox.Staged.Settings" in content
        ), (phase, result.returncode, result.stderr, content[-10000:])


def test_stage(case, exe, fixture, fixture_host, scripts, readobj, cwd):
    if case == "profiler":
        for binary in [exe, exe.with_name("Sandbox.dll")]:
            imports = subprocess.check_output([readobj, "--coff-imports", binary], text=True)
            assert ("SandboxTracyClient.dll" in imports) == (binary.suffix == ".dll"), binary
            for symbol in ["StartupProfiler", "ShutdownProfiler"]:
                assert (symbol in imports) == (binary.suffix == ".dll"), (binary, symbol)
        return
    if case == "settings":
        test_settings(exe, cwd)
        return
    if case in {"lifecycle", "cli", "display"}:
        fixture_cases(case, exe, fixture, fixture_host, scripts, cwd)
        return
    args = ["--exit-when-ready"]
    diagnostic = None
    if case == "player":
        args += ["--mission", "LevelScripts/TurretTrial_0/TurretTrial_0.scm"]
    elif case in {"observer", "invalid"}:
        root = cwd / "Missions 日本" / "LevelScripts"
        shutil.copytree(scripts, root)
        mission = root / "FleetOverview" / "FleetOverview.scm"
        if case == "invalid":
            mission.write_text("(export mission)\n(define mission #f)\n", encoding="utf-8")
            diagnostic = "Mission launch failed"
        args += ["--mission", mission]
    elif case == "missing":
        args += ["--mission", "absent.scm"]
        diagnostic = "Mission file does not exist"
    output = run(exe, args, cwd, diagnostic=diagnostic)
    if diagnostic is None:
        assert "Ready: " in output, output
        if case == "observer":
            assert f"Ready: {mission}" in output, output


if __name__ == "__main__":
    scenario, host, fixture_dll, level_scripts, readobj, fixture_exe = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="sandbox host ") as work:
        test_stage(scenario, Path(host), Path(fixture_dll), Path(fixture_exe), Path(level_scripts),
                   readobj, Path(work))
