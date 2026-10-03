#[path = "support/mod.rs"]
mod support;
use std::{fs, path::Path, process::Command};
use support::Repo;

fn configured(root: &Path) -> std::path::PathBuf {
    fs::write(root.join("CMakeLists.txt"), r#"
cmake_minimum_required(VERSION 3.25)
project(Operations NONE)
foreach(target IN ITEMS editor compile-space-dust-material generate-native-soa-fixture kernel-native-generated-sources)
  add_custom_target(${target} COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/${target}.stamp")
endforeach()
"#).unwrap();
    let build = root.join("configured build");
    assert!(
        Command::new("cmake")
            .arg("-S")
            .arg(root)
            .arg("-B")
            .arg(&build)
            .args(["-G", "Ninja"])
            .output()
            .unwrap()
            .status
            .success()
    );
    build
}

#[test]
fn presets_run_from_nested_directory_without_cmake() {
    let repo = Repo::new();
    fs::create_dir_all(repo.feature.join("cmake/presets")).unwrap();
    fs::write(repo.feature.join("cmake/presets/generate.py"),
        "import pathlib,sys\npathlib.Path('checked' if '--check' in sys.argv else 'generated').touch()\n").unwrap();
    for args in [vec!["presets"], vec!["presets", "--check"]] {
        let output = repo
            .tool()
            .args(args)
            .current_dir(repo.feature.join("cmake"))
            .output()
            .unwrap();
        assert!(output.status.success(), "{output:?}");
    }
    assert!(repo.feature.join("checked").is_file());
    assert!(repo.feature.join("generated").is_file());
    assert!(!repo.feature.join("out").exists());
}

#[test]
fn benchmark_builds_revision_local_host_then_forwards_arguments_and_exit_code() {
    let repo = Repo::new();
    let root = &repo.feature;
    fs::write(root.join("CMakePresets.json"), r#"{
        "version":6,"configurePresets":[{"name":"native","generator":"Ninja","binaryDir":"${sourceDir}/out/build/native"}],
        "buildPresets":[{"name":"native","configurePreset":"native"}]
    }"#).unwrap();
    let executable = env!("CARGO_BIN_EXE_coj").replace('\\', "/");
    let suffix = if cfg!(windows) { ".exe" } else { "" };
    fs::write(root.join("CMakeLists.txt"), format!(r#"
cmake_minimum_required(VERSION 3.25)
project(BenchmarkHost NONE)
add_custom_target(benchmark-tools-host
  COMMAND "${{CMAKE_COMMAND}}" -E make_directory "${{CMAKE_BINARY_DIR}}/rust-tools/release"
  COMMAND "${{CMAKE_COMMAND}}" -E copy "{executable}" "${{CMAKE_BINARY_DIR}}/rust-tools/release/benchmark-tools{suffix}"
  COMMAND "${{CMAKE_COMMAND}}" -E touch "${{CMAKE_BINARY_DIR}}/built")
"#)).unwrap();
    let output = repo
        .tool()
        .args(["benchmark", "--version"])
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
    assert!(
        String::from_utf8_lossy(&output.stdout)
            .contains(concat!("coj ", env!("CARGO_PKG_VERSION")))
    );
    assert!(root.join("out/build/native/built").is_file());
    let output = repo
        .tool()
        .args(["benchmark", "invalid-operation"])
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(2));
}

#[test]
fn tidy_passes_policy_workers_and_builds_only_required_inputs() {
    let repo = Repo::new();
    let root = &repo.feature;
    let build = configured(root);
    fs::create_dir(root.join("native")).unwrap();
    fs::write(
        root.join(".clang-tidy-scopes.json"),
        include_str!("../../../../../.clang-tidy-scopes.json"),
    )
    .unwrap();
    fs::write(build.join("compile_commands.json"), "[]").unwrap();
    let llvm = root.join("LLVM Root");
    fs::create_dir_all(llvm.join("bin")).unwrap();
    fs::write(llvm.join("bin/clang-tidy.exe"), "fixture").unwrap();
    fs::write(
        llvm.join("bin/run-clang-tidy.py"),
        r#"
import pathlib,sys,re
args=sys.argv[1:]
build=pathlib.Path(args[args.index('-p')+1])
assert args[args.index('-j')+1]=='3'
assert pathlib.Path.cwd().name=='native'
assert re.search(args[-1],str(pathlib.Path.cwd()/'lispb/src/a.cpp'))
assert not re.search(args[-1],str(pathlib.Path.cwd()/'third_party/a.cpp'))
for name in ('generate-native-soa-fixture','kernel-native-generated-sources'):
    assert (build/(name+'.stamp')).exists()
print('runner log')
raise SystemExit(23)
"#,
    )
    .unwrap();
    let output = repo
        .tool()
        .args(["tidy", "--scope", "lispb", "--jobs", "3", "--build-dir"])
        .arg(&build)
        .env("LLVM_ROOT", &llvm)
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(23), "{output:?}");
    assert!(
        fs::read_to_string(build.join("clang-tidy-lispb.log"))
            .unwrap()
            .contains("runner log")
    );
    assert!(!build.join("editor.stamp").exists());
}

#[cfg(windows)]
#[test]
fn unreal_routes_builds_arguments_order_and_failure_without_engine() {
    let repo = Repo::new();
    let root = &repo.feature;
    let build = configured(root);
    let editor = root.join("fake editor.cmd");
    fs::write(
        &editor,
        format!(
            "@echo off\nif not exist \"{}\" exit /b 40\necho %*>>\"{}\"\nexit /b 0\n",
            build.join("editor.stamp").display(),
            root.join("invocations.txt").display()
        ),
    )
    .unwrap();
    fs::write(
        build.join("unreal-paths.json"),
        serde_json::to_vec(&serde_json::json!({
            "editor":editor,"editor_cmd":editor,"project":root.join("Sandbox.uproject"),
            "local_ddc":build.join("ddc"),"staged_executable":editor,"staged_directory":root,
        }))
        .unwrap(),
    )
    .unwrap();
    for arguments in [
        vec!["editor", "--wait-for-debugger"],
        vec!["unreal", "generate-world-soft-target-assets"],
        vec!["unreal", "generate-space-dust-material"],
        vec!["unreal", "generate-lab-mesh", "hex-frame"],
    ] {
        let result = repo
            .tool()
            .args(arguments)
            .arg("--build-dir")
            .arg(&build)
            .output()
            .unwrap();
        assert!(result.status.success(), "{result:?}");
    }
    let text = fs::read_to_string(root.join("invocations.txt")).unwrap();
    assert!(text.contains("bEnabled=False"));
    assert!(text.contains("-WaitForDebugger"));
    assert!(
        text.find("SoftTargetWorld.smat").unwrap()
            < text.find("-run=GenerateWorldSoftTargetAssets").unwrap()
    );
    assert!(text.contains("-run=GenerateSandboxMeshHexFrame"));
    assert!(text.contains("-ddc=NoZenLocalFallback"));
    assert!(build.join("compile-space-dust-material.stamp").is_file());
    fs::write(&editor, "@exit /b 29\n").unwrap();
    let result = repo
        .tool()
        .args(["run-staged", "--build-dir"])
        .arg(&build)
        .output()
        .unwrap();
    assert_eq!(result.status.code(), Some(29));
    fs::remove_file(build.join("editor.stamp")).unwrap();
    let result = repo
        .tool()
        .args(["run-staged", "--build-dir"])
        .arg(&build)
        .output()
        .unwrap();
    assert_eq!(result.status.code(), Some(29));
    assert!(!build.join("editor.stamp").exists());
}

#[cfg(windows)]
#[test]
fn project_files_support_both_engine_layouts_without_configure() {
    let repo = Repo::new();
    let root = &repo.feature;
    let engine = root.join("Engine Root");
    let scripts = engine.join("Engine/Build/BatchFiles");
    fs::create_dir_all(&scripts).unwrap();
    let script = format!(
        "@echo %* >\"{}\"\n@echo %IOJ_NATIVE_TOOLCHAIN% >>\"{}\"\n@exit /b 0\n",
        root.join("args.txt").display(),
        root.join("args.txt").display()
    );
    fs::write(scripts.join("Build.bat"), &script).unwrap();
    for source in [false, true] {
        if source {
            fs::write(scripts.join("GenerateProjectFiles.bat"), &script).unwrap();
        }
        let result = repo
            .tool()
            .args(["unreal", "project-files"])
            .env("UE_ROOT", &engine)
            .output()
            .unwrap();
        assert!(result.status.success(), "{result:?}");
        let text = fs::read_to_string(root.join("args.txt")).unwrap();
        assert_eq!(text.contains("-ProjectFiles"), !source);
        assert!(text.contains("-Game -WaitMutex"));
        assert!(text.contains("clang-cl"));
        assert!(!root.join("out").exists());
    }
}
