use super::*;

struct Fixture {
    _root: ioj_test_support::TempDir,
    request: Request,
    inventory: String,
}

fn write(path: &Path) {
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(path, "").unwrap();
}

impl Fixture {
    fn new() -> Self {
        let root = ioj_test_support::temp_dir("package tools ");
        let request = Request {
            project_root: root.path().join("project root"),
            package_root: root.path().join("package root"),
            unreal_pak: root.path().join("tool with spaces.exe"),
            verification_directory: root.path().join("artifacts"),
            configuration: Configuration::Development,
        };
        // This file is only validated by verify; the fixture never launches it.
        write(&request.unreal_pak);
        for path in [
            "Sandbox.exe",
            "Sandbox/Binaries/Win64/Sandbox.exe",
            "Sandbox/Binaries/Win64/Sandbox-Win64-Shipping.exe",
            "Engine/Extras/Redist/en-us/vc_redist.x64.exe",
            "Sandbox/Content/Paks/game.pak",
            "Sandbox/Content/Paks/game.utoc",
        ] {
            write(&request.package_root.join(path));
        }
        for path in [
            "LevelScripts/Nested/Scenario.scm",
            "Content/UI/DA_ui_data.uasset",
            "Plugins/SpaceGame/SpaceGame.uplugin",
            "Plugins/SandboxShaders/SandboxShaders.uplugin",
            "Plugins/SpaceGame/Content/Levels/MainMenu.umap",
            "Plugins/SpaceGame/Content/Levels/GameRuntime.umap",
        ] {
            write(&request.project_root.join(path));
        }
        let mut inventory = String::from(
            "LevelScripts/Nested/Scenario.scm\n/SpaceGame/Levels/MainMenu\n/SpaceGame/Levels/GameRuntime\n/Game/UI/DA_ui_data",
        );
        for directory in ASSET_DIRECTORIES {
            let asset = request.project_root.join(directory).join("Required.uasset");
            write(&asset);
            inventory.push('\n');
            inventory.push_str(&unreal_path(&request.project_root, &asset).unwrap());
        }
        Self {
            _root: root,
            request,
            inventory,
        }
    }

    fn verify(&self) -> Result<usize> {
        verify(&self.request, |args| {
            if args[0].to_string_lossy().starts_with("-ListContainer=") {
                fs::write(
                    self.request.verification_directory.join("iostore.csv"),
                    &self.inventory,
                )?;
            }
            Ok(String::from("pak inventory"))
        })
    }
}

#[test]
fn complete_packages_and_inventory_artifacts() {
    let mut fixture = Fixture::new();
    for configuration in [Configuration::Development, Configuration::Shipping] {
        fixture.request.configuration = configuration;
        let mut calls = Vec::new();
        let scripts = verify(&fixture.request, |args| {
            calls.push(args.to_vec());
            if calls.len() == 2 {
                fs::write(
                    fixture.request.verification_directory.join("iostore.csv"),
                    &fixture.inventory,
                )?;
            }
            Ok(String::from("pak inventory"))
        })
        .unwrap();
        assert_eq!(scripts, 1);
        assert_eq!(
            calls[0],
            [
                fixture
                    .request
                    .package_root
                    .join("Sandbox/Content/Paks")
                    .join("game.pak")
                    .into_os_string(),
                "-List".into()
            ]
        );
        assert_eq!(
            calls[1][0].to_string_lossy(),
            format!(
                "-ListContainer={}",
                fixture
                    .request
                    .package_root
                    .join("Sandbox/Content/Paks")
                    .join("*.utoc")
                    .display()
            )
        );
        assert_eq!(
            calls[1][1].to_string_lossy(),
            format!(
                "-Csv={}",
                fixture
                    .request
                    .verification_directory
                    .join("iostore.csv")
                    .display()
            )
        );
        assert_eq!(
            fs::read_to_string(fixture.request.verification_directory.join("pak-files.txt"))
                .unwrap(),
            "pak inventory\n"
        );
    }
}

#[test]
fn missing_files_and_containers_are_rejected() {
    for (path, message) in [
        ("Sandbox.exe", "Required package file"),
        ("Sandbox/Content/Paks/game.pak", "No pak files"),
        ("Sandbox/Content/Paks/game.utoc", "No IoStore containers"),
    ] {
        let fixture = Fixture::new();
        fs::remove_file(fixture.request.package_root.join(path)).unwrap();
        assert!(fixture.verify().unwrap_err().to_string().contains(message));
    }
}

#[test]
fn required_scripts_maps_and_assets_are_checked() {
    let fixture = Fixture::new();
    for missing in fixture.inventory.lines() {
        let inventory = fixture.inventory.replace(missing, "");
        let error = verify_inventory(&fixture.request.project_root, &inventory)
            .unwrap_err()
            .to_string();
        assert!(error.contains(missing), "{error}");
    }
}

#[test]
fn staged_paths_case_and_separators_are_accepted() {
    let fixture = Fixture::new();
    let inventory = fixture
        .inventory
        .lines()
        .map(|line| {
            if line.starts_with('/') {
                staged_path(line).unwrap()
            } else {
                line.to_owned()
            }
        })
        .collect::<Vec<_>>()
        .join("\n")
        .to_uppercase()
        .replace('/', "\\");
    assert_eq!(
        verify_inventory(&fixture.request.project_root, &inventory).unwrap(),
        1
    );
}

#[test]
fn unexpected_maps_and_generated_audio() {
    let fixture = Fixture::new();
    write(
        &fixture
            .request
            .project_root
            .join("Content/Levels/Unexpected.umap"),
    );
    let inventory = format!("{}\n/Game/Levels/Unexpected", fixture.inventory);
    assert!(
        verify_inventory(&fixture.request.project_root, &inventory)
            .unwrap_err()
            .to_string()
            .contains("Unexpected project map")
    );
    fixture.verify().unwrap();
    write(
        &fixture
            .request
            .project_root
            .join("Plugins/SpaceGame/Content/Audio/Generated/Audio.uasset"),
    );
    assert!(
        fixture
            .verify()
            .unwrap_err()
            .to_string()
            .contains("/SpaceGame/Audio/Generated/Audio")
    );
    verify_inventory(
        &fixture.request.project_root,
        &format!("{}\n/SpaceGame/Audio/Generated/Audio", fixture.inventory),
    )
    .unwrap();
}

#[test]
fn stale_csv_cannot_mask_missing_output_or_tool_failure() {
    let fixture = Fixture::new();
    let csv = fixture.request.verification_directory.join("iostore.csv");
    write(&csv);
    let error = verify(&fixture.request, |_| Ok(String::new()))
        .unwrap_err()
        .to_string();
    assert!(error.contains("iostore.csv"));
    assert!(!csv.exists());
    assert!(
        verify(&fixture.request, |_| Err("tool failure".into()))
            .unwrap_err()
            .to_string()
            .contains("tool failure")
    );
}

#[test]
fn plugin_mount_uses_descriptor_name_and_invalid_paths_fail() {
    let fixture = Fixture::new();
    let asset = fixture
        .request
        .project_root
        .join("Plugins/OtherDirectory/Content/UI/Asset.uasset");
    write(&asset);
    assert!(unreal_path(&fixture.request.project_root, &asset).is_err());
    write(
        &fixture
            .request
            .project_root
            .join("Plugins/OtherDirectory/ActualPlugin.uplugin"),
    );
    assert_eq!(
        unreal_path(&fixture.request.project_root, &asset).unwrap(),
        "/ActualPlugin/UI/Asset"
    );
    assert!(
        unreal_path(
            &fixture.request.project_root,
            &fixture.request.project_root.join("Outside.uasset")
        )
        .is_err()
    );
    for path in ["Game/UI/Test", "/Game", "//Asset", "/Game//Asset", "/Game/"] {
        assert!(staged_path(path).is_err(), "{path}");
    }
}

#[test]
fn cli_requires_all_paths_and_known_configuration() {
    let args = [
        "game-package-tools",
        "--project-root",
        "project",
        "--package-root",
        "package",
        "--unreal-pak",
        "tool",
        "--verification-directory",
        "artifacts",
        "--configuration",
        "Shipping",
    ];
    assert!(Request::try_parse_from(args).is_ok());
    assert!(Request::try_parse_from(&args[..9]).is_err());
    assert!(Request::try_parse_from(args.into_iter().chain(["--configuration", "Debug"])).is_err());
}

#[test]
fn launch_failure_has_executable_context() {
    let fixture = Fixture::new();
    let missing = fixture.request.project_root.join("missing tool.exe");
    let error = run_unreal_pak(&missing, &[]).unwrap_err().to_string();
    assert!(error.contains("Unable to start UnrealPak"));
    assert!(error.contains("missing tool.exe"));
}

#[cfg(windows)]
#[test]
fn unreal_pak_output_includes_stderr_and_reports_nonzero_exit() {
    let executable = PathBuf::from(std::env::var_os("COMSPEC").unwrap());
    let args = ["/d", "/c", "echo listing&echo diagnostic 1>&2"].map(OsString::from);
    let output = run_unreal_pak(&executable, &args).unwrap();
    assert!(output.contains("listing"));
    assert!(output.contains("diagnostic"));
    let args = ["/d", "/c", "echo pak failure 1>&2&exit /b 7"].map(OsString::from);
    let error = run_unreal_pak(&executable, &args).unwrap_err().to_string();
    assert!(error.contains("7"));
    assert!(error.contains("pak failure"));
}
