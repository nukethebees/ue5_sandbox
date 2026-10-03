use serde::{Serialize, de::DeserializeOwned};
use serde_json::Value;
use std::{
    fs,
    path::{Path, PathBuf},
    process::{Command, Output},
};

pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Debug)]
pub struct ProcessFailure(pub i32);
impl std::fmt::Display for ProcessFailure {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Process exited with code {}", self.0)
    }
}
impl std::error::Error for ProcessFailure {}

pub fn capture_process_output(command: &mut Command) -> Result<Output> {
    command.output().map_err(|error| {
        format!(
            "Unable to run {}: {error}",
            command.get_program().to_string_lossy()
        )
        .into()
    })
}

pub fn run_process_with_log(command: &mut Command, path: &Path) -> Result<Output> {
    let output = capture_process_output(command)?;

    // Preserve diagnostics before the caller checks exit status or result artifacts.
    write_text(
        path,
        format!(
            "{}{}",
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        ),
    )?;

    Ok(output)
}

pub fn require_process_success(output: Output) -> Result<Output> {
    if !output.status.success() {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        return Err(Box::new(ProcessFailure(output.status.code().unwrap_or(1))));
    }
    Ok(output)
}

pub fn run_process_inherited(root: &Path, executable: &str, args: &[&str]) -> Result<()> {
    let status = Command::new(executable)
        .args(args)
        .current_dir(root)
        .status()?;
    if !status.success() {
        return Err(Box::new(ProcessFailure(status.code().unwrap_or(1))));
    }
    Ok(())
}

pub fn find_repository_root(start: &Path) -> Result<PathBuf> {
    start
        .ancestors()
        .find(|p| p.join(".git").exists() && p.join("CMakeLists.txt").is_file())
        .map(Path::to_path_buf)
        .ok_or_else(|| {
            format!(
                "Could not locate repository root from '{}'.",
                start.display()
            )
            .into()
        })
}

pub fn resolve_absolute_path(root: &Path, path: impl AsRef<Path>) -> Result<PathBuf> {
    Ok(std::path::absolute(root.join(path))?)
}

pub fn write_text(path: &Path, text: impl AsRef<[u8]>) -> Result<()> {
    fs::create_dir_all(path.parent().ok_or("Output path has no parent")?)?;
    fs::write(path, text)?;
    Ok(())
}

pub fn write_json(path: &Path, value: &impl Serialize) -> Result<()> {
    write_text(path, serde_json::to_string_pretty(value)? + "\n")
}

pub fn read_json<T: DeserializeOwned>(path: &Path) -> Result<T> {
    Ok(serde_json::from_slice(&fs::read(path)?)?)
}

pub fn parse_json_lines(output: &str) -> Result<Vec<Value>> {
    output
        .lines()
        .map(str::trim_start)
        .filter(|line| line.starts_with('{'))
        .map(|line| serde_json::from_str(line).map_err(Into::into))
        .collect()
}

pub fn write_csv(path: &Path, rows: &[Vec<String>]) -> Result<()> {
    let text = rows
        .iter()
        .map(|row| {
            row.iter()
                .map(|s| {
                    if s.contains([',', '"', '\r', '\n']) {
                        format!("\"{}\"", s.replace('"', "\"\""))
                    } else {
                        s.clone()
                    }
                })
                .collect::<Vec<_>>()
                .join(",")
        })
        .collect::<Vec<_>>()
        .join("\n")
        + "\n";
    write_text(path, text)
}

pub fn parse_csv_row(line: &str) -> Result<Vec<String>> {
    let mut result = Vec::new();
    let mut field = String::new();
    let mut quoted = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '"' if quoted && chars.peek() == Some(&'"') => {
                field.push('"');
                chars.next();
            }
            '"' => quoted = !quoted,
            ',' if !quoted => result.push(std::mem::take(&mut field)),
            _ => field.push(c),
        }
    }
    if quoted {
        return Err("Benchmark CSV contains an unterminated quoted field.".into());
    }
    result.push(field);
    Ok(result)
}

pub fn read_finite_number(value: &Value, pointer: &str) -> Result<f64> {
    value
        .pointer(pointer)
        .and_then(Value::as_f64)
        .filter(|n| n.is_finite())
        .ok_or_else(|| format!("Missing or invalid benchmark field: {pointer}").into())
}

#[cfg(test)]
#[path = "../tests/support/mod.rs"]
mod tests;
