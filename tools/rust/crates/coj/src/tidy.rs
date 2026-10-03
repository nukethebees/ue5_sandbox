use clap::Parser;
use std::ffi::OsString;
use std::fs::{self, File};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::Mutex;

#[derive(Parser)]
#[command(
    name = "coj tidy",
    about = "Run LLVM analysis using revision-local scopes"
)]
struct Arguments {
    #[arg(long, default_value = "native")]
    scope: String,
    #[arg(long)]
    build_dir: PathBuf,
    #[arg(long, short = 'j', default_value_t = 0)]
    jobs: u32,
}

fn llvm_tool(names: &[&str]) -> Result<PathBuf, String> {
    let directories = match std::env::var_os("LLVM_ROOT").filter(|root| !root.is_empty()) {
        Some(root) => vec![PathBuf::from(root).join("bin")],
        None => std::env::split_paths(&std::env::var_os("PATH").unwrap_or_default()).collect(),
    };
    for directory in directories {
        for name in names {
            for suffix in if cfg!(windows) {
                &[".exe", ""][..]
            } else {
                &[""][..]
            } {
                let path = directory.join(format!("{name}{suffix}"));
                if path.is_file() {
                    return std::path::absolute(path).map_err(|error| error.to_string());
                }
            }
        }
    }
    Err(format!(
        "Cannot find {} using LLVM_ROOT/bin or PATH",
        names.join(" / ")
    ))
}

fn regex_escape(text: &str) -> String {
    let mut result = String::new();
    for character in text.chars() {
        if "\\.^$|?*+()[]{}".contains(character) {
            result.push('\\');
        }
        result.push(character);
    }
    result
}

fn source_filter(root: &Path, scope: &str) -> Result<String, String> {
    let path = root.join(".clang-tidy-scopes.json");
    let policy: serde_json::Value = serde_json::from_slice(
        &fs::read(&path).map_err(|error| format!("Cannot read {}: {error}", path.display()))?,
    )
    .map_err(|error| format!("Invalid tidy scope policy: {error}"))?;
    let directories = policy["scopes"][scope]
        .as_array()
        .ok_or_else(|| format!("Unknown tidy scope '{scope}'"))?;
    let directories = directories
        .iter()
        .map(|value| {
            value
                .as_str()
                .map(regex_escape)
                .ok_or("Invalid tidy scope directory")
        })
        .collect::<Result<Vec<_>, _>>()?;
    let relative_filter = policy["source_filter"]
        .as_str()
        .ok_or("Missing tidy source_filter")?;
    let native = regex_escape(&root.join("native").to_string_lossy().replace('\\', "/"))
        .replace('/', "[/\\\\]");
    let scope_filter = if directories.is_empty() {
        String::new()
    } else {
        format!("(?=^{native}[/\\\\](?:{})[/\\\\])", directories.join("|"))
    };
    Ok(format!("{scope_filter}^{native}[/\\\\]{relative_filter}"))
}

fn tee(reader: impl std::io::Read, log: &Mutex<File>) -> std::io::Result<()> {
    let mut reader = BufReader::new(reader);
    let mut line = Vec::new();
    while reader.read_until(b'\n', &mut line)? != 0 {
        let mut log = log.lock().unwrap();
        log.write_all(&line)?;
        std::io::stdout().write_all(&line)?;
        line.clear();
    }
    Ok(())
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match Arguments::try_parse_from(
        std::iter::once(OsString::from("coj tidy")).chain(arguments.iter().cloned()),
    ) {
        Ok(args) => args,
        Err(error) => {
            let code = error.exit_code();
            let _ = error.print();
            return Ok(code);
        }
    };
    let root = crate::worktree_root()?;
    let filter = source_filter(&root, &args.scope)?;
    let build = std::path::absolute(&args.build_dir).map_err(|error| error.to_string())?;
    if !build.join("compile_commands.json").is_file() {
        return Err(format!(
            "No compile_commands.json in {}; configure the analysis build first",
            build.display()
        ));
    }
    let clang_tidy = llvm_tool(&["clang-tidy"])?;
    let runner = llvm_tool(&["run-clang-tidy", "run-clang-tidy.py"])?;
    if matches!(args.scope.as_str(), "native" | "lispb") {
        let status = Command::new("cmake")
            .arg("--build")
            .arg(&build)
            .args([
                "--target",
                "generate-native-soa-fixture",
                "kernel-native-generated-sources",
            ])
            .current_dir(&root)
            .status()
            .map_err(|error| error.to_string())?;
        if !status.success() {
            return Ok(status.code().unwrap_or(1));
        }
    }
    let name = if args.scope == "native" {
        "clang-tidy".to_owned()
    } else {
        format!("clang-tidy-{}", args.scope)
    };
    let log_path = build.join(format!("{name}.log"));
    println!("Analyzing {} (log: {})", args.scope, log_path.display());
    let log = Mutex::new(File::create(log_path).map_err(|error| error.to_string())?);
    let mut child = Command::new("python")
        .arg(runner)
        .args(["-quiet", "-clang-tidy-binary"])
        .arg(clang_tidy)
        .arg("-p")
        .arg(build)
        .arg("-j")
        .arg(args.jobs.to_string())
        .arg(filter)
        // Let run-clang-tidy inspect the native policy before per-file nested policies.
        .current_dir(root.join("native"))
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|error| format!("Cannot launch run-clang-tidy: {error}"))?;
    let stdout = child.stdout.take().unwrap();
    let stderr = child.stderr.take().unwrap();
    let (out, err) = std::thread::scope(|scope| {
        let out = scope.spawn(|| tee(stdout, &log));
        let err = scope.spawn(|| tee(stderr, &log));
        (out.join().unwrap(), err.join().unwrap())
    });
    let status = child.wait().map_err(|error| error.to_string())?;
    out.and(err)
        .map_err(|error| format!("Cannot write tidy log: {error}"))?;
    Ok(status.code().unwrap_or(1))
}
