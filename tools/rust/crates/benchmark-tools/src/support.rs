use serde::Serialize;
use serde_json::Value;
use std::{
    collections::BTreeMap,
    ffi::OsStr,
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

pub fn captured(command: &mut Command) -> Result<Output> {
    command.output().map_err(|error| {
        format!(
            "Unable to run {}: {error}",
            command.get_program().to_string_lossy()
        )
        .into()
    })
}

pub fn succeeded(output: Output) -> Result<Output> {
    if !output.status.success() {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        return Err(Box::new(ProcessFailure(output.status.code().unwrap_or(1))));
    }
    Ok(output)
}

pub fn run(
    root: &Path,
    executable: impl AsRef<OsStr>,
    args: &[impl AsRef<OsStr>],
) -> Result<Output> {
    succeeded(captured(
        Command::new(executable).args(args).current_dir(root),
    )?)
}

pub fn visible(root: &Path, executable: &str, args: &[&str]) -> Result<()> {
    let status = Command::new(executable)
        .args(args)
        .current_dir(root)
        .status()?;
    if !status.success() {
        return Err(Box::new(ProcessFailure(status.code().unwrap_or(1))));
    }
    Ok(())
}

pub fn repository(start: &Path) -> Result<PathBuf> {
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

pub fn absolute(root: &Path, path: impl AsRef<Path>) -> Result<PathBuf> {
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

pub fn json_lines(output: &str) -> Result<Vec<Value>> {
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

pub fn parse_csv(line: &str) -> Result<Vec<String>> {
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

pub fn number(value: &Value, pointer: &str) -> Result<f64> {
    value
        .pointer(pointer)
        .and_then(Value::as_f64)
        .filter(|n| n.is_finite())
        .ok_or_else(|| format!("Missing or invalid benchmark field: {pointer}").into())
}

pub struct Args(BTreeMap<String, String>);
impl Args {
    pub fn parse(args: &[String], values: &[&str], flags: &[&str]) -> Result<Self> {
        let mut parsed = BTreeMap::new();
        let mut args = args.iter();
        while let Some(arg) = args.next() {
            let (name, value) = if flags.contains(&arg.as_str()) {
                (arg.as_str(), "true")
            } else {
                let (name, value) = match arg.split_once('=') {
                    Some(pair) => pair,
                    None => (
                        arg.as_str(),
                        args.next()
                            .ok_or_else(|| format!("Missing value for '{arg}'."))?
                            .as_str(),
                    ),
                };
                if !values.contains(&name) || value.trim().is_empty() {
                    return Err(format!("Unknown or missing argument '{arg}'.").into());
                }
                (name, value)
            };
            if parsed.insert(name.to_owned(), value.to_owned()).is_some() {
                return Err(format!("Duplicate argument '{name}'.").into());
            }
        }
        Ok(Self(parsed))
    }
    pub fn flag(&self, name: &str) -> bool {
        self.0.contains_key(name)
    }
    pub fn value<'a>(&'a self, name: &str, fallback: &'a str) -> &'a str {
        self.0.get(name).map(String::as_str).unwrap_or(fallback)
    }
    pub fn required(&self, name: &str) -> Result<&str> {
        self.0
            .get(name)
            .map(String::as_str)
            .ok_or_else(|| format!("'{name}' is required.").into())
    }
    pub fn integer(&self, name: &str, fallback: u32, min: u32, max: u32) -> Result<u32> {
        let text = self.value(name, "").to_owned();
        if text.is_empty() {
            return Ok(fallback);
        }
        text.parse::<u32>()
            .ok()
            .filter(|n| (min..=max).contains(n) && text.bytes().all(|c| c.is_ascii_digit()))
            .ok_or_else(|| format!("'{name}' must be an integer from {min} to {max}.").into())
    }
    pub fn float(&self, name: &str, fallback: f64, min: f64, max: f64) -> Result<f64> {
        let text = self.value(name, "").to_owned();
        if text.is_empty() {
            return Ok(fallback);
        }
        text.parse::<f64>()
            .ok()
            .filter(|n| n.is_finite() && (min..=max).contains(n))
            .ok_or_else(|| {
                format!("'{name}' must be finite and in the range {min} to {max}.").into()
            })
    }
    pub fn choice<'a>(
        &'a self,
        name: &str,
        fallback: &'a str,
        choices: &[&str],
    ) -> Result<&'a str> {
        let value = self.value(name, fallback);
        if !choices.contains(&value) {
            return Err(format!("'{name}' must be one of {}.", choices.join(", ")).into());
        }
        Ok(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn arguments_reject_unknown_duplicate_missing_and_invalid_numbers() {
        for args in [
            vec!["--unknown", "1"],
            vec!["--size"],
            vec!["--size", "1", "--size=2"],
            vec!["--flag", "--flag"],
        ] {
            assert!(
                Args::parse(
                    &args.into_iter().map(str::to_owned).collect::<Vec<_>>(),
                    &["--size"],
                    &["--flag"]
                )
                .is_err()
            );
        }
        for value in ["NaN", "inf", "-1", "101"] {
            let args = Args::parse(&[format!("--size={value}")], &["--size"], &[]).unwrap();
            assert!(args.float("--size", 1.0, 0.0, 100.0).is_err());
        }
    }
    #[test]
    fn csv_quotes_and_json_noise() {
        assert_eq!(
            parse_csv("one,\"two,three\",\"say \"\"hello\"\"\"").unwrap(),
            ["one", "two,three", "say \"hello\""]
        );
        assert!(parse_csv("\"unclosed").is_err());
        assert_eq!(json_lines("noise\n {\"ok\":1}\nmore").unwrap().len(), 1);
        assert!(json_lines("{broken").is_err());
    }
}
