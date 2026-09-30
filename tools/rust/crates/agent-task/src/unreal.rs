use clap::{Parser, Subcommand, ValueEnum};
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Parser)]
#[command(name = "agent-task unreal")]
struct Arguments {
    #[command(subcommand)]
    operation: Operation,
}

#[derive(Subcommand)]
enum Operation {
    /// Generate IDE project files without configuring CMake.
    ProjectFiles {
        /// Engine root (defaults to UE_ROOT).
        #[arg(long)]
        ue_root: Option<PathBuf>,
        /// Read engine/toolchain settings from an existing CMake build instead.
        #[arg(long, conflicts_with = "ue_root")]
        build_dir: Option<PathBuf>,
        #[arg(long)]
        native_toolchain: Option<String>,
    },
    ResaveAssets(Build),
    ImportGameAudio(Build),
    GenerateScriptedLevelAssets(Build),
    GenerateSlateDslSmokeAsset(Build),
    GenerateLabMesh {
        #[arg(value_enum)]
        shape: Mesh,
        #[command(flatten)]
        build: Build,
    },
    GenerateUiGlowMaterial(Build),
    GenerateWorldSoftTargetAssets(Build),
    GenerateMigratedMaterials(Build),
    GenerateCelestialAnalyticMaterial(Build),
    GenerateSpaceDustMaterial(Build),
}

#[derive(Clone, ValueEnum)]
enum Mesh {
    Box,
    Cylinder,
    Sphere,
    Cone,
    HexFrame,
    HexTile,
    HoneycombPanel,
    Assemblies,
}

#[derive(clap::Args)]
struct Build {
    /// Configured Unreal build directory (defaults to out/build/debug-game).
    #[arg(long)]
    build_dir: Option<PathBuf>,
}

impl Build {
    fn directory(self, root: &Path) -> Result<PathBuf, String> {
        std::path::absolute(
            self.build_dir
                .unwrap_or_else(|| root.join("out/build/debug-game")),
        )
        .map_err(|error| error.to_string())
    }
}

#[derive(Parser)]
#[command(name = "agent-task editor")]
struct Editor {
    #[command(flatten)]
    build: Build,
    #[arg(long)]
    wait_for_debugger: bool,
}

#[derive(Parser)]
#[command(name = "agent-task run-staged")]
struct Staged {
    /// Configured packaging build directory (defaults to out/build/development).
    #[arg(long)]
    build_dir: Option<PathBuf>,
}

fn parse<T: Parser>(name: &str, arguments: &[OsString]) -> Result<T, i32> {
    T::try_parse_from(std::iter::once(OsString::from(name)).chain(arguments.iter().cloned()))
        .map_err(|error| {
            let code = error.exit_code();
            let _ = error.print();
            code
        })
}

fn settings(build: &Path) -> Result<serde_json::Value, String> {
    let path = build.join("unreal-paths.json");
    serde_json::from_slice(&fs::read(&path).map_err(|error| {
        format!(
            "Cannot read {}: {error}. Configure the Unreal build first.",
            path.display()
        )
    })?)
    .map_err(|error| format!("Invalid Unreal build paths: {error}"))
}

fn value<'a>(settings: &'a serde_json::Value, key: &str) -> Result<&'a str, String> {
    settings[key]
        .as_str()
        .filter(|s| !s.is_empty())
        .ok_or_else(|| format!("Missing Unreal build path '{key}'"))
}

fn build_targets(root: &Path, build: &Path, targets: &[&str]) -> Result<i32, String> {
    status(
        Command::new("cmake")
            .arg("--build")
            .arg(build)
            .arg("--target")
            .args(targets)
            .current_dir(root),
    )
}

fn status(command: &mut Command) -> Result<i32, String> {
    command
        .status()
        .map(|status| status.code().unwrap_or(1))
        .map_err(|error| {
            format!(
                "Cannot launch {}: {error}",
                command.get_program().to_string_lossy()
            )
        })
}

pub fn editor(arguments: &[OsString]) -> Result<i32, String> {
    let args = match parse::<Editor>("agent-task editor", arguments) {
        Ok(args) => args,
        Err(code) => return Ok(code),
    };
    let root = crate::worktree_root()?;
    let build = args.build.directory(&root)?;
    let code = build_targets(&root, &build, &["editor"])?;
    if code != 0 {
        return Ok(code);
    }
    let settings = settings(&build)?;
    let mut command = Command::new(value(&settings, "editor")?);
    command.arg(value(&settings, "project")?)
        .arg("-ini:EditorPerProjectUserSettings:[/Script/LiveCoding.LiveCodingSettings]:bEnabled=False")
        .current_dir(root);
    if args.wait_for_debugger {
        command.arg("-WaitForDebugger");
    }
    status(&mut command)
}

pub fn run_staged(arguments: &[OsString]) -> Result<i32, String> {
    let args = match parse::<Staged>("agent-task run-staged", arguments) {
        Ok(args) => args,
        Err(code) => return Ok(code),
    };
    let root = crate::worktree_root()?;
    let settings = settings(
        &args
            .build_dir
            .unwrap_or_else(|| root.join("out/build/development")),
    )?;
    status(
        Command::new(value(&settings, "staged_executable")?)
            .current_dir(value(&settings, "staged_directory")?),
    )
}

fn project_files(root: &Path, engine: &Path, toolchain: &str) -> Result<i32, String> {
    let directory = root.join(engine).join("Engine/Build/BatchFiles");
    let generate = directory.join("GenerateProjectFiles.bat");
    let mut command = if generate.is_file() {
        Command::new(generate)
    } else {
        let mut command = Command::new(directory.join("Build.bat"));
        command.arg("-ProjectFiles");
        command
    };
    status(
        command
            .arg(format!(
                "-Project={}",
                root.join("Sandbox.uproject").display()
            ))
            .args(["-Game", "-WaitMutex"])
            .env("IOJ_NATIVE_TOOLCHAIN", toolchain)
            .current_dir(root),
    )
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match parse::<Arguments>("agent-task unreal", arguments) {
        Ok(args) => args,
        Err(code) => return Ok(code),
    };
    let root = crate::worktree_root()?;
    if let Operation::ProjectFiles {
        ue_root,
        build_dir,
        native_toolchain,
    } = args.operation
    {
        let (engine, configured_toolchain) = if let Some(build) = build_dir {
            let settings = settings(&build)?;
            (
                PathBuf::from(value(&settings, "engine_root")?),
                value(&settings, "native_toolchain")?.to_owned(),
            )
        } else {
            let engine = ue_root
                .or_else(|| {
                    std::env::var_os("UE_ROOT")
                        .filter(|s| !s.is_empty())
                        .map(PathBuf::from)
                })
                .ok_or("Set UE_ROOT or pass --ue-root for project-file generation")?;
            (
                engine,
                std::env::var("IOJ_NATIVE_TOOLCHAIN").unwrap_or_else(|_| "clang-cl".into()),
            )
        };
        return project_files(
            &root,
            &engine,
            native_toolchain.as_deref().unwrap_or(&configured_toolchain),
        );
    }
    let (build, commandlet, artifact, prerequisite) = match args.operation {
        Operation::ResaveAssets(build) => (build, "ResavePackages", None, None),
        Operation::ImportGameAudio(build) => (build, "ImportGameAudio", None, None),
        Operation::GenerateScriptedLevelAssets(build) => {
            (build, "GenerateScriptedLevelAssets", None, None)
        }
        Operation::GenerateSlateDslSmokeAsset(build) => {
            (build, "GenerateSlateDslSmokeAsset", None, None)
        }
        Operation::GenerateLabMesh { shape, build } => (
            build,
            match shape {
                Mesh::Box => "GenerateSandboxMeshBox",
                Mesh::Cylinder => "GenerateSandboxMeshCylinder",
                Mesh::Sphere => "GenerateSandboxMeshSphere",
                Mesh::Cone => "GenerateSandboxMeshCone",
                Mesh::HexFrame => "GenerateSandboxMeshHexFrame",
                Mesh::HexTile => "GenerateSandboxMeshHexTile",
                Mesh::HoneycombPanel => "GenerateSandboxMeshHoneycombPanel",
                Mesh::Assemblies => "GenerateSandboxMeshAssemblies",
            },
            None,
            None,
        ),
        Operation::GenerateUiGlowMaterial(build) => {
            (build, "MaterialSynth", Some("UiGlowComposite"), None)
        }
        Operation::GenerateWorldSoftTargetAssets(build) => (
            build,
            "GenerateWorldSoftTargetAssets",
            Some("SoftTargetWorld"),
            None,
        ),
        Operation::GenerateMigratedMaterials(build) => (
            build,
            "MaterialSynth",
            Some("all"),
            Some("compile-migrated-materials"),
        ),
        Operation::GenerateCelestialAnalyticMaterial(build) => {
            (build, "MaterialSynth", Some("CelestialAnalytic"), None)
        }
        Operation::GenerateSpaceDustMaterial(build) => (
            build,
            "MaterialSynth",
            Some("SpaceDust"),
            Some("compile-space-dust-material"),
        ),
        Operation::ProjectFiles { .. } => unreachable!(),
    };
    let build = build.directory(&root)?;
    let mut targets = vec!["editor"];
    targets.extend(prerequisite);
    let code = build_targets(&root, &build, &targets)?;
    if code != 0 {
        return Ok(code);
    }
    let settings = settings(&build)?;
    let base = || -> Result<Command, String> {
        let mut command = Command::new(value(&settings, "editor_cmd")?);
        command.arg(value(&settings, "project")?).current_dir(&root);
        Ok(command)
    };
    if let Some(artifact) = artifact {
        let mut command = base()?;
        command.arg("-run=MaterialSynth");
        if artifact == "all" {
            command.arg(format!("-ArtifactDirectory={}", root.join("Intermediate/MaterialGen").display()))
                .arg("-ArtifactList=UiGlowComposite,RadarDisplay,EnergyShield,SpaceEnergyField,GlowingCross,VertexRipple,ConstructionSpawn,PlanetAtmosphere,PlanetSurface,TacticalScan,CelestialAnalytic")
                .arg("-GenerateAll");
        } else {
            command
                .arg(format!(
                    "-Artifact={}",
                    root.join(format!("Intermediate/MaterialGen/{artifact}.smat"))
                        .display()
                ))
                .arg("-Generate");
        }
        command.args([
            "-RenderOffscreen",
            "-AllowCommandletRendering",
            "-unattended",
            "-nop4",
            "-nosplash",
            "-nosound",
            "-stdout",
        ]);
        let code = status(&mut command)?;
        if code != 0 || commandlet != "GenerateWorldSoftTargetAssets" {
            return Ok(code);
        }
    }
    let mut command = base()?;
    command.arg(format!("-run={commandlet}"));
    if commandlet == "ResavePackages" {
        command.args(["-projectonly", "-fixupredirects", "-unattended"]);
    } else {
        command.args(["-unattended", "-nop4", "-nosplash", "-nosound", "-stdout"]);
        if commandlet == "GenerateWorldSoftTargetAssets" {
            command.args(["-RenderOffscreen", "-AllowCommandletRendering"]);
        } else {
            command
                .arg(format!(
                    "-LocalDataCachePath={}",
                    value(&settings, "local_ddc")?
                ))
                .args(["-ddc=NoZenLocalFallback", "-nullrhi"]);
        }
    }
    status(&mut command)
}
