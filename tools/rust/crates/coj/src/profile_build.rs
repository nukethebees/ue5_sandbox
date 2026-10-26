use clap::Parser;
use serde::Deserialize;
use serde_json::json;
use std::collections::BTreeMap;
use std::ffi::OsString;
use std::fs::{self, File};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::Mutex;
use std::time::{Instant, SystemTime, UNIX_EPOCH};

#[derive(Parser)]
#[command(
    name = "coj profile-build",
    about = "Time a TOML command list and optionally analyze Clang traces"
)]
struct Arguments {
    /// Profile name under cmake/profiles, or a TOML file relative to the worktree.
    profile: PathBuf,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Profile {
    /// Directory containing Clang -ftime-trace JSON, with optional {output} substitution.
    trace_directory: Option<String>,
    #[serde(default)]
    environment: BTreeMap<String, String>,
    steps: Vec<Step>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Step {
    name: String,
    command: Vec<String>,
}

fn execute(
    root: &Path,
    command: &[String],
    environment: &BTreeMap<String, String>,
    log: &Path,
) -> Result<i32, String> {
    let log = Mutex::new(File::create(log).map_err(|e| e.to_string())?);
    let mut child = Command::new(&command[0])
        .args(&command[1..])
        .current_dir(root)
        .envs(environment)
        .env("CCACHE_DISABLE", "1")
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Cannot launch {}: {e}", command[0]))?;
    let stdout = child.stdout.take().unwrap();
    let stderr = child.stderr.take().unwrap();
    let (out, err) = std::thread::scope(|scope| {
        let out = scope.spawn(|| crate::tidy::tee(stdout, &log));
        let err = scope.spawn(|| crate::tidy::tee(stderr, &log));
        (out.join().unwrap(), err.join().unwrap())
    });
    let status = child.wait().map_err(|e| e.to_string())?;
    out.and(err)
        .map_err(|e| format!("Cannot write command log: {e}"))?;
    Ok(status.code().unwrap_or(1))
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match Arguments::try_parse_from(
        std::iter::once(OsString::from("coj profile-build")).chain(arguments.iter().cloned()),
    ) {
        Ok(args) => args,
        Err(error) => {
            let code = error.exit_code();
            let _ = error.print();
            return Ok(code);
        }
    };
    let root = crate::worktree_root()?;
    let profile_path = if args.profile.extension().is_some() {
        root.join(&args.profile)
    } else {
        root.join("cmake/profiles")
            .join(&args.profile)
            .with_extension("toml")
    };
    let source = fs::read_to_string(&profile_path)
        .map_err(|e| format!("Read {}: {e}", profile_path.display()))?;
    let mut profile: Profile =
        toml::from_str(&source).map_err(|e| format!("Invalid profile: {e}"))?;
    if profile.steps.is_empty()
        || profile
            .steps
            .iter()
            .any(|step| step.command.first().is_none_or(|s| s.trim().is_empty()))
    {
        return Err("A profile needs steps with nonempty commands".into());
    }

    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| e.to_string())?
        .as_millis();
    let output = root
        .join(".local/build-profiles")
        .join(format!("{timestamp}-{}", std::process::id()));
    fs::create_dir_all(&output).map_err(|e| e.to_string())?;
    fs::write(output.join("profile.toml"), source).map_err(|e| e.to_string())?;
    println!(
        "Build profile: {}\nResults: {}",
        profile_path.display(),
        output.display()
    );
    println!("ccache is bypassed for this run; its stored cache is retained.");

    let expand =
        |value: &str| value.replace("{output}", &output.to_string_lossy().replace('\\', "/"));
    for step in &mut profile.steps {
        step.command = step.command.iter().map(|arg| expand(arg)).collect();
    }
    for value in profile.environment.values_mut() {
        *value = expand(value);
    }
    profile
        .environment
        .insert("CCACHE_DISABLE".into(), "1".into());
    if let Some(directory) = &profile.trace_directory {
        let directory = root.join(expand(directory));
        fs::create_dir_all(&directory).map_err(|e| e.to_string())?;
        let directory = directory.to_string_lossy().into_owned();
        let capture = output.join("clang.bin").to_string_lossy().into_owned();
        profile.steps.insert(
            0,
            Step {
                name: "Start Clang capture".into(),
                command: vec![
                    "ClangBuildAnalyzer".into(),
                    "--start".into(),
                    directory.clone(),
                ],
            },
        );
        profile.steps.extend([
            Step {
                name: "Collect Clang traces".into(),
                command: vec![
                    "ClangBuildAnalyzer".into(),
                    "--stop".into(),
                    directory,
                    capture.clone(),
                ],
            },
            Step {
                name: "Analyze Clang traces".into(),
                command: vec!["ClangBuildAnalyzer".into(), "--analyze".into(), capture],
            },
        ]);
    }

    let git_info = |arguments: &[&str]| -> Result<String, String> {
        let result = Command::new("git")
            .args(arguments)
            .current_dir(&root)
            .output()
            .map_err(|e| e.to_string())?;
        if !result.status.success() {
            return Err(String::from_utf8_lossy(&result.stderr).into_owned());
        }
        Ok(String::from_utf8_lossy(&result.stdout)
            .trim_end()
            .to_owned())
    };
    let revision = git_info(&["rev-parse", "HEAD"])?;
    let worktree_status = git_info(&["status", "--short"])?;
    let mut records = Vec::new();
    let mut report = format!(
        "# Build profile\n\nProfile: `{}`\n\nRevision: `{revision}`\n\nccache: bypassed. Times are wall-clock seconds; traced builds include instrumentation overhead.\n\n| Step | Seconds | Exit code | Log |\n|---|---:|---:|---|\n",
        profile_path.display()
    );
    for (index, step) in profile.steps.iter().enumerate() {
        let log_name = format!("{index:02}.log");
        println!("\n{}: {:?}", step.name, step.command);
        let start = Instant::now();
        let result = execute(
            &root,
            &step.command,
            &profile.environment,
            &output.join(&log_name),
        );
        let seconds = start.elapsed().as_secs_f64();
        let code = result.as_ref().copied().unwrap_or(1);
        records.push(
            json!({"name":step.name, "command":step.command, "seconds":seconds,
            "exit_code":code, "error":result.as_ref().err(), "log":log_name}),
        );
        report.push_str(&format!(
            "| {} | {seconds:.3} | {code} | [{log_name}]({log_name}) |\n",
            step.name.replace('|', "\\|")
        ));
        fs::write(output.join("report.md"), &report).map_err(|e| e.to_string())?;
        let summary = json!({"profile":profile_path, "revision":revision, "worktree_status":worktree_status,
            "environment":profile.environment, "steps":records});
        fs::write(
            output.join("results.json"),
            serde_json::to_vec_pretty(&summary).map_err(|e| e.to_string())?,
        )
        .map_err(|e| e.to_string())?;
        println!("{}: {seconds:.3}s (exit {code})", step.name);
        if let Err(error) = result {
            eprintln!("{error}");
        }
        if code != 0 {
            eprintln!("Stopped; logs and timings retained in {}", output.display());
            return Ok(code);
        }
    }
    println!("Report: {}", output.join("report.md").display());
    Ok(0)
}
