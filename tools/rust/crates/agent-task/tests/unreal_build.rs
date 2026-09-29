mod support;
use std::fs;
use support::Repo;

#[test]
fn requires_all_arguments_and_existing_files() {
    let repo = Repo::new();
    let output = repo.tool().arg("unreal-build").output().unwrap();
    assert_eq!(output.status.code(), Some(2));
    let output = repo
        .tool()
        .args([
            "unreal-build",
            "--build-script",
            "missing.bat",
            "--target",
            "SandboxEditor",
            "--platform",
            "Win64",
            "--configuration",
            "DebugGame",
            "--project",
            "missing.uproject",
            "--native-toolchain",
            "native",
        ])
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("build script file is missing"));
}

#[cfg(windows)]
#[test]
fn batch_launch_preserves_arguments_environment_and_full_exit_code() {
    for extension in ["bat", "cmd"] {
        let repo = Repo::new();
        let script = repo.feature.join(format!("build & script.{extension}"));
        let project = repo.feature.join("game & project.uproject");
        fs::write(&project, "{}").unwrap();
        fs::write(&script, "@echo off\r\n>arguments.txt echo %1\r\n>>arguments.txt echo %2\r\n>>arguments.txt echo %3\r\n>>arguments.txt echo %4\r\n>>arguments.txt echo %5\r\n>>arguments.txt echo %IOJ_NATIVE_TOOLCHAIN%\r\n>>arguments.txt echo %MSBUILDDISABLENODEREUSE%\r\nexit /b 259\r\n").unwrap();
        let output = repo
            .tool()
            .arg("unreal-build")
            .arg("--build-script")
            .arg(&script)
            .args([
                "--target",
                "SandboxEditor",
                "--platform",
                "Win64",
                "--configuration",
                "DebugGame",
                "--project",
            ])
            .arg(&project)
            .args(["--native-toolchain", "test-native"])
            .output()
            .unwrap();
        assert_eq!(output.status.code(), Some(259), "{output:?}");
        let text = fs::read_to_string(repo.feature.join("arguments.txt")).unwrap();
        let values = text
            .lines()
            .map(|s| s.trim_matches('"'))
            .collect::<Vec<_>>();
        assert_eq!(
            values,
            [
                "SandboxEditor",
                "Win64",
                "DebugGame",
                &format!("-Project={}", project.display()),
                "-WaitMutex",
                "test-native",
                "1"
            ]
        );
        fs::remove_file(&project).unwrap();
        let output = repo
            .tool()
            .arg("unreal-build")
            .arg("--build-script")
            .arg(&script)
            .args([
                "--target",
                "SandboxEditor",
                "--platform",
                "Win64",
                "--configuration",
                "DebugGame",
                "--project",
            ])
            .arg(&project)
            .args(["--native-toolchain", "native"])
            .output()
            .unwrap();
        assert!(!output.status.success());
        assert!(String::from_utf8_lossy(&output.stderr).contains("project file is missing"));
    }
}

#[test]
#[cfg(windows)]
fn launch_failure_is_diagnostic() {
    let repo = Repo::new();
    fs::write(repo.feature.join("build.cmd"), "@exit /b 0\r\n").unwrap();
    let output = repo
        .tool()
        .args([
            "unreal-build",
            "--build-script",
            "build.cmd",
            "--target",
            "Sandbox\nEditor",
            "--platform",
            "Win64",
            "--configuration",
            "Development",
            "--project",
            "file.txt",
            "--native-toolchain",
            "native",
        ])
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(
        String::from_utf8_lossy(&output.stderr).contains("Unable to start Unreal build script")
    );
}
