use clap::{Parser, ValueEnum};
use std::{
    ffi::OsString,
    fs,
    path::{Path, PathBuf},
    process::Command,
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

const MAPS: [&str; 2] = [
    "/SpaceGame/Levels/MainMenu",
    "/SpaceGame/Levels/GameRuntime",
];

const ASSET_DIRECTORIES: [&str; 5] = [
    "Plugins/SpaceGame/Content/UI",
    "Plugins/SpaceGame/Content/Input",
    "Plugins/SandboxShaders/Content/GpuStarfield",
    "Plugins/SandboxShaders/Content/Generated/Materials",
    "Plugins/SandboxShaders/Content/CelestialBackdrop",
];

#[derive(Clone, Copy, ValueEnum)]
enum Configuration {
    Debug,
    #[value(name = "DebugGame", alias = "debug-game")]
    DebugGame,
    Development,
    Test,
    Shipping,
}

#[derive(Parser)]
struct Request {
    #[arg(long)]
    project_root: PathBuf,
    #[arg(long)]
    package_root: PathBuf,
    #[arg(long)]
    unreal_pak: PathBuf,
    #[arg(long)]
    verification_directory: PathBuf,
    #[arg(long, value_enum, ignore_case = true)]
    configuration: Configuration,
}

fn files(directory: &Path, extension: &str, recursive: bool) -> Result<Vec<PathBuf>> {
    let mut result = Vec::new();

    for entry in
        fs::read_dir(directory).map_err(|error| format!("{}: {error}", directory.display()))?
    {
        let entry = entry?;
        let path = entry.path();
        if recursive && entry.file_type()?.is_dir() {
            result.extend(files(&path, extension, true)?);
        } else if path
            .extension()
            .is_some_and(|ext| ext.eq_ignore_ascii_case(extension))
        {
            result.push(path);
        }
    }

    result.sort();
    Ok(result)
}

fn unreal_path(project: &Path, asset: &Path) -> Result<String> {
    let content = asset
        .parent()
        .and_then(|parent| {
            parent.ancestors().find(|p| {
                p.file_name()
                    .is_some_and(|s| s.eq_ignore_ascii_case("Content"))
            })
        })
        .ok_or_else(|| {
            format!(
                "Asset is not below a Content directory: {}",
                asset.display()
            )
        })?;

    if asset.extension().is_none() {
        return Err(format!("Asset has no extension: {}", asset.display()).into());
    }

    let relative = asset
        .strip_prefix(content)?
        .with_extension("")
        .to_string_lossy()
        .replace('\\', "/");

    let root = if content
        .to_string_lossy()
        .eq_ignore_ascii_case(&project.join("Content").to_string_lossy())
    {
        "Game".to_owned()
    } else {
        files(
            content.parent().ok_or("Content directory has no parent")?,
            "uplugin",
            false,
        )?
        .first()
        .ok_or_else(|| {
            format!(
                "Could not find the plugin descriptor for '{}'.",
                asset.display()
            )
        })?
        .file_stem()
        .unwrap()
        .to_string_lossy()
        .into_owned()
    };

    Ok(format!("/{root}/{relative}"))
}

fn staged_path(package: &str) -> Result<String> {
    let (root, asset) = package
        .strip_prefix('/')
        .and_then(|s| s.split_once('/'))
        .filter(|(root, asset)| {
            !root.trim().is_empty() && !asset.trim().is_empty() && !asset.starts_with('/')
        })
        .ok_or_else(|| format!("Invalid Unreal package path: {package}"))?;

    Ok(if root == "Game" {
        format!("../../../Sandbox/Content/{asset}")
    } else {
        format!("../../../Sandbox/Plugins/{root}/Content/{asset}")
    })
}

fn contains(inventory: &str, path: &str) -> bool {
    inventory.contains(&path.replace('\\', "/").to_lowercase())
}

fn package_present(inventory: &str, package: &str) -> Result<bool> {
    Ok(contains(inventory, package) || contains(inventory, &staged_path(package)?))
}

fn require_package(inventory: &str, package: &str) -> Result<()> {
    if !package_present(inventory, package)? {
        return Err(
            format!("Required Unreal package is missing from the containers: {package}").into(),
        );
    }

    Ok(())
}

fn verify_inventory(project: &Path, text: &str) -> Result<()> {
    let inventory = text.replace('\\', "/").to_lowercase();
    if contains(&inventory, "LevelScripts/Settings/user-settings.scm") {
        return Err("Personal user-settings.scm was packaged".into());
    }

    for package in MAPS.into_iter().chain(["/Game/UI/DA_ui_data"]) {
        require_package(&inventory, package)?;
    }

    for directory in ["Content", "Plugins"] {
        for map in files(&project.join(directory), "umap", true)? {
            let package = unreal_path(project, &map)?;
            if !MAPS.contains(&package.as_str()) && package_present(&inventory, &package)? {
                return Err(format!("Unexpected project map was packaged: {package}").into());
            }
        }
    }

    let audio = project.join("Plugins/SpaceGame/Content/Audio/Generated");
    let mut directories: Vec<_> = ASSET_DIRECTORIES.iter().map(|s| project.join(s)).collect();
    if audio.is_dir() {
        directories.push(audio);
    }

    for directory in directories {
        for asset in files(&directory, "uasset", true)? {
            require_package(&inventory, &unreal_path(project, &asset)?)?;
        }
    }

    Ok(())
}

fn require_file(path: &Path) -> Result<()> {
    if !path.is_file() {
        return Err(format!("Required package file is missing: {}", path.display()).into());
    }

    Ok(())
}

fn run_unreal_pak(executable: &Path, args: &[OsString]) -> Result<String> {
    let output = Command::new(executable)
        .args(args)
        .output()
        .map_err(|error| {
            format!(
                "Unable to start UnrealPak '{}': {error}",
                executable.display()
            )
        })?;

    if !output.status.success() {
        return Err(format!(
            "UnrealPak failed with {}: {}",
            output.status,
            String::from_utf8_lossy(&output.stderr)
        )
        .into());
    }

    Ok(format!(
        "{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    ))
}

fn verify(
    request: &Request,
    mut unreal_pak: impl FnMut(&[OsString]) -> Result<String>,
) -> Result<usize> {
    let project = std::path::absolute(&request.project_root)?;
    let package = std::path::absolute(&request.package_root)?;
    let artifacts = std::path::absolute(&request.verification_directory)?;

    let binaries = package.join("Sandbox/Binaries/Win64");
    let binary = match request.configuration {
        Configuration::Debug => "Sandbox-Win64-Debug.dll",
        Configuration::DebugGame => "Sandbox-Win64-DebugGame.dll",
        Configuration::Development => "Sandbox.dll",
        Configuration::Test => "Sandbox-Win64-Test.dll",
        Configuration::Shipping => "Sandbox-Win64-Shipping.dll",
    };
    for legacy in [package.join("Sandbox.exe"), binaries.join(binary).with_extension("exe")] {
        if legacy.exists() {
            return Err(format!("Obsolete player launcher in archive: {}", legacy.display()).into());
        }
    }
    let debug = matches!(request.configuration, Configuration::Debug | Configuration::DebugGame);
    if debug || matches!(request.configuration, Configuration::Development) {
        require_file(&binaries.join("SandboxTracyClient.dll"))?;
    }
    for path in [
        binaries.join("SandboxHost.exe"),
        binaries.join(binary),
        binaries.join(if debug { "SandboxTBB12_debug.dll" } else { "SandboxTBB12.dll" }),
        package.join("Engine/Extras/Redist/en-us/vc_redist.x64.exe"),
        request.unreal_pak.clone(),
    ] {
        require_file(&path)?;
    }

    let pak_directory = package.join("Sandbox/Content/Paks");
    let paks = files(&pak_directory, "pak", false)?;
    if paks.is_empty() {
        return Err(format!(
            "No pak files were found under '{}'.",
            pak_directory.display()
        )
        .into());
    }
    if files(&pak_directory, "utoc", false)?.is_empty() {
        return Err(format!(
            "No IoStore containers were found under '{}'.",
            pak_directory.display()
        )
        .into());
    }

    fs::create_dir_all(&artifacts)?;
    let csv = artifacts.join("iostore.csv");

    // Discard stale output before checking this invocation's inventory.
    if csv.exists() {
        fs::remove_file(&csv)?;
    }

    let mut inventory = String::new();
    for pak in paks {
        inventory.push_str(&unreal_pak(&[pak.into_os_string(), "-List".into()])?);
        inventory.push('\n');
    }
    fs::write(artifacts.join("pak-files.txt"), &inventory)?;

    let mut containers = OsString::from("-ListContainer=");
    containers.push(pak_directory.join("*.utoc"));
    let mut csv_argument = OsString::from("-Csv=");
    csv_argument.push(&csv);
    unreal_pak(&[containers, csv_argument])?;
    require_file(&csv)?;
    inventory.push_str(&fs::read_to_string(csv)?);

    verify_inventory(&project, &inventory)?;
    let staged = package.join("Sandbox/LevelScripts");
    let personal = Path::new("Settings/user-settings.scm");
    if staged.join(personal).exists() {
        return Err("Personal user-settings.scm was staged".into());
    }
    let source = project.join("LevelScripts");
    let mut count = 0;
    for script in files(&source, "scm", true)? {
        let relative = script.strip_prefix(&source)?;
        if relative != personal {
            require_file(&staged.join(relative))?;
            count += 1;
        }
    }
    Ok(count)
}

fn main() {
    let request = Request::parse();
    match verify(&request, |args| run_unreal_pak(&request.unreal_pak, args)) {
        Ok(scripts) => println!(
            "Verified {scripts} level scripts, two maps, runtime-loaded assets, binaries, containers, and prerequisites."
        ),
        Err(error) => {
            eprintln!("game-package-tools: {error}");
            std::process::exit(1);
        }
    }
}

#[cfg(test)]
#[path = "../tests/unit/mod.rs"]
mod tests;
