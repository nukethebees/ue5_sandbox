use crate::support::*;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
};

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct Source {
    pub root: PathBuf,
    pub commit: String,
    pub dirty: bool,
    pub status: String,
    pub diff_sha256: String,
    pub untracked_sha256: String,
    pub artifact_root: Option<PathBuf>,
}

pub fn make_git_command(root: &Path, args: &[&str]) -> Command {
    let mut command = Command::new("git");
    command
        .current_dir(root)
        .args([
            "--no-pager",
            "--literal-pathspecs",
            "-c",
            "core.protectNTFS=true",
            "-c",
            "core.protectHFS=true",
            "-c",
            "core.longpaths=true",
            "-c",
            "core.hooksPath=NUL",
            "-c",
            "gc.auto=0",
            "-c",
            "maintenance.auto=false",
            "-c",
            "submodule.recurse=false",
        ])
        .args(args)
        .env("GIT_TERMINAL_PROMPT", "0")
        .env("GCM_INTERACTIVE", "Never")
        .env("GIT_EDITOR", "false")
        .env("GIT_SEQUENCE_EDITOR", "false");
    command
}

pub fn run_git_capture(root: &Path, args: &[&str]) -> Result<String> {
    let output =
        require_process_success(capture_process_output(&mut make_git_command(root, args))?)?;
    Ok(String::from_utf8(output.stdout)?)
}

pub fn sha256_hex(bytes: impl AsRef<[u8]>) -> String {
    format!("{:x}", Sha256::digest(bytes.as_ref()))
}

pub fn capture_source_identity(root: &Path, artifact_root: Option<&Path>) -> Result<Source> {
    let modules = run_git_capture(
        root,
        &[
            "submodule",
            "foreach",
            "--quiet",
            "--recursive",
            "git status --porcelain=v1 --untracked-files=all --ignore-submodules=none",
        ],
    )?;
    if !modules.trim().is_empty() {
        return Err(format!(
            "Benchmark source contains dirty initialized submodules: '{}'.\n{modules}",
            root.display()
        )
        .into());
    }
    let commit = run_git_capture(root, &["rev-parse", "--verify", "HEAD^{commit}"])?
        .trim()
        .to_owned();
    let mut status = run_git_capture(
        root,
        &[
            "status",
            "--porcelain=v1",
            "--untracked-files=no",
            "--ignore-submodules=none",
        ],
    )?
    .trim()
    .to_owned();
    let diff = run_git_capture(
        root,
        &[
            "diff",
            "HEAD",
            "--binary",
            "--no-ext-diff",
            "--no-textconv",
            "--ignore-submodules=none",
        ],
    )?;
    let untracked = run_git_capture(root, &["ls-files", "--others", "--exclude-standard", "-z"])?;
    let mut paths: Vec<_> = untracked.split('\0').filter(|s| !s.is_empty()).collect();
    paths.sort();
    let mut content = Sha256::new();
    for path in paths {
        let full = root.join(path);
        if artifact_root.is_some_and(|excluded| full.starts_with(excluded)) {
            continue;
        }
        let bytes = if fs::symlink_metadata(&full)?.file_type().is_symlink() {
            fs::read_link(&full)?.to_string_lossy().as_bytes().to_vec()
        } else {
            fs::read(&full)?
        };
        content.update(path.as_bytes());
        content.update([0]);
        content.update(Sha256::digest(&bytes));
        status.push_str(&format!("\n?? {path}"));
    }
    Ok(Source {
        root: root.to_owned(),
        commit,
        dirty: !status.is_empty(),
        status,
        diff_sha256: sha256_hex(diff),
        untracked_sha256: format!("{:x}", content.finalize()),
        artifact_root: artifact_root.map(Path::to_path_buf),
    })
}

pub fn verify_source_unchanged(expected: &Source) -> Result<()> {
    if *expected != capture_source_identity(&expected.root, expected.artifact_root.as_deref())? {
        return Err(format!(
            "Source changed during benchmark preparation or measurement: '{}'. Start a new run.",
            expected.root.display()
        )
        .into());
    }
    Ok(())
}

pub struct Run {
    pub directory: PathBuf,
    pub manifest: Value,
}
impl Run {
    pub fn new(
        root: &Path,
        benchmark: &str,
        parent: &Path,
        configuration: Value,
        label: &str,
        latest: bool,
    ) -> Result<Self> {
        let parent = resolve_absolute_path(root, parent)?;
        fs::create_dir_all(&parent)?;
        let stamp = chrono::Utc::now();
        let id = format!(
            "{}-{}-{}",
            stamp.format("%Y%m%dT%H%M%S%.9fZ"),
            std::process::id(),
            NEXT_ID.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
        );
        let directory = parent.join(&id);
        fs::create_dir(&directory)?;
        let run = Self {
            directory,
            manifest: json!({"schemaVersion":1,"toolSchemaVersion":1,"runId":id,"benchmark":benchmark,"label":label,
            "purpose":"measurement","createdUtc":stamp.to_rfc3339(),"status":"preparing","configuration":configuration,"provenance":null,
            "comparability":null,"artifacts":{},"expectedArtifacts":[],"failure":null}),
        };
        run.write_manifest()?;
        if latest {
            write_text(
                &parent.join("latest.txt"),
                format!("{}\n", run.directory.display()),
            )?;
        }
        Ok(run)
    }
    pub fn path(&self, name: &str) -> PathBuf {
        self.directory.join(name)
    }
    pub fn id(&self) -> &str {
        self.manifest["runId"].as_str().unwrap()
    }
    pub fn write_manifest(&self) -> Result<()> {
        write_json(&self.path("manifest.json"), &self.manifest)
    }
    pub fn expect_artifact(&mut self, name: &str) -> Result<()> {
        self.manifest["artifacts"][name] = json!(self.path(name));
        self.manifest["expectedArtifacts"]
            .as_array_mut()
            .unwrap()
            .push(json!(name));
        self.write_manifest()
    }
    pub fn validate_artifacts(&self) -> Result<()> {
        for name in self.manifest["expectedArtifacts"].as_array().unwrap() {
            let path = self.path(name.as_str().unwrap());
            if !path.is_file() || fs::metadata(&path)?.len() == 0 {
                return Err(format!(
                    "Expected artifact was not produced or is empty: '{}'.",
                    path.display()
                )
                .into());
            }
        }
        Ok(())
    }
    pub fn set_status(&mut self, status: &str) -> Result<()> {
        self.manifest["status"] = json!(status);
        self.write_manifest()
    }
    pub fn finish_run<T>(&mut self, result: Result<T>) -> Result<T> {
        match result {
            Ok(value) => {
                self.set_status("complete")?;
                Ok(value)
            }
            Err(error) => {
                self.manifest["failure"] = json!(error.to_string());
                self.set_status("failed")?;
                Err(error)
            }
        }
    }
}
static NEXT_ID: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct Repetition {
    pub sequence: u32,
    pub repetition: u32,
    pub side: String,
    pub warmup: bool,
}
pub fn balanced_repetitions(repetitions: u32, warmups: u32) -> Vec<Repetition> {
    let mut result = Vec::new();
    for (warmup, count) in [(true, warmups), (false, repetitions)] {
        for repetition in 1..=count {
            let sides = if repetition % 2 == 1 {
                ["baseline", "candidate"]
            } else {
                ["candidate", "baseline"]
            };
            for side in sides {
                result.push(Repetition {
                    sequence: result.len() as u32 + 1,
                    repetition,
                    side: side.into(),
                    warmup,
                });
            }
        }
    }
    result
}

pub fn validate_owned_path(path: &Path, parent: &Path) -> Result<()> {
    let path = std::path::absolute(path)?;
    let parent = std::path::absolute(parent)?;
    if path == parent
        || !path.starts_with(&parent)
        || path
            .components()
            .any(|c| c == std::path::Component::ParentDir)
    {
        return Err(format!(
            "Refusing benchmark worktree outside '{}'.",
            parent.display()
        )
        .into());
    }
    for ancestor in path.ancestors() {
        if let Ok(metadata) = fs::symlink_metadata(ancestor) {
            #[cfg(windows)]
            let linked = {
                use std::os::windows::fs::MetadataExt;
                metadata.file_attributes() & 0x400 != 0
            };
            #[cfg(not(windows))]
            let linked = metadata.file_type().is_symlink();
            if linked {
                return Err(format!(
                    "Benchmark worktree path traverses a link: '{}'.",
                    ancestor.display()
                )
                .into());
            }
        }
    }
    Ok(())
}

pub struct Revisions {
    pub candidate: Source,
    pub baseline: Source,
    pub owned: bool,
    pub keep: bool,
}
impl Revisions {
    pub fn new(
        root: &Path,
        revision: &str,
        supplied: Option<&str>,
        keep: bool,
        artifacts: &Path,
    ) -> Result<Self> {
        let candidate = capture_source_identity(root, Some(artifacts))?;
        let commit = run_git_capture(
            root,
            &[
                "rev-parse",
                "--verify",
                "--end-of-options",
                &format!("{revision}^{{commit}}"),
            ],
        )?
        .trim()
        .to_owned();
        let parent = root.join(".local/benchmarks/wt");
        let owned = supplied.is_none();
        let path = if let Some(supplied) = supplied {
            resolve_absolute_path(root, supplied)?
        } else {
            validate_owned_path(&parent.join("0"), &parent)?;
            fs::create_dir_all(&parent)?;
            let mut slot = 0;
            loop {
                let path = parent.join(slot.to_string());
                match fs::create_dir(&path) {
                    Ok(()) => break path,
                    Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => slot += 1,
                    Err(error) => return Err(error.into()),
                }
            }
        };
        let prepare = (|| -> Result<Source> {
            if paths_equal(&path, root) {
                return Err("The baseline must be a separate worktree from the candidate.".into());
            }
            if owned {
                run_git_capture(
                    root,
                    &[
                        "worktree",
                        "add",
                        "--detach",
                        &path.to_string_lossy(),
                        &commit,
                    ],
                )?;
            } else {
                let top = run_git_capture(&path, &["rev-parse", "--show-toplevel"])?;
                if !paths_equal(Path::new(top.trim()), &path) {
                    return Err("Prepared baseline must be a worktree root.".into());
                }
            }
            let baseline = capture_source_identity(&path, None)?;
            if baseline.commit != commit || baseline.dirty {
                return Err("Baseline worktree must be clean and at the requested commit.".into());
            }
            Ok(baseline)
        })();
        match prepare {
            Ok(baseline) => Ok(Self {
                candidate,
                baseline,
                owned,
                keep,
            }),
            Err(error) => {
                if owned {
                    if path.join(".git").exists() {
                        run_git_capture(
                            root,
                            &[
                                "worktree",
                                "remove",
                                "--force",
                                "--force",
                                &path.to_string_lossy(),
                            ],
                        )?;
                    } else {
                        fs::remove_dir(&path)?;
                    }
                }
                Err(error)
            }
        }
    }
    pub fn cleanup_worktrees<T>(self, result: Result<T>) -> Result<T> {
        if self.owned && !self.keep {
            validate_owned_path(
                &self.baseline.root,
                &self.candidate.root.join(".local/benchmarks/wt"),
            )?;
            run_git_capture(
                &self.candidate.root,
                &[
                    "worktree",
                    "remove",
                    "--force",
                    "--force",
                    &self.baseline.root.to_string_lossy(),
                ],
            )?;
        }
        result
    }
}

fn paths_equal(a: &Path, b: &Path) -> bool {
    a.to_string_lossy()
        .replace('\\', "/")
        .eq_ignore_ascii_case(&b.to_string_lossy().replace('\\', "/"))
}

pub fn initialize_submodules(
    target: &Path,
    source: Option<&Path>,
    paths: Option<&[&str]>,
) -> Result<()> {
    let mut args = vec!["submodule", "init"];
    if let Some(paths) = paths {
        args.push("--");
        args.extend(paths);
    }
    run_git_capture(target, &args)?;
    if !target.join(".gitmodules").is_file() {
        return Ok(());
    }
    let output = capture_process_output(&mut make_git_command(
        target,
        &[
            "config",
            "--null",
            "--file",
            ".gitmodules",
            "--get-regexp",
            "^submodule\\..*\\.path$",
        ],
    ))?;
    if output.status.code() == Some(1) {
        return Ok(());
    }
    let listing = String::from_utf8(require_process_success(output)?.stdout)?;
    for entry in listing.split('\0').filter(|s| !s.is_empty()) {
        let (key, relative) = entry
            .split_once('\n')
            .ok_or("Malformed submodule path configuration.")?;
        if paths.is_some_and(|paths| !paths.contains(&relative)) {
            continue;
        }
        let destination = target.join(relative);
        validate_owned_path(&destination, target)?;
        let local = source
            .map(|p| p.join(relative))
            .filter(|p| p.join(".git").exists());
        if let Some(local) = &local {
            validate_owned_path(local, source.unwrap())?;
            if !paths_equal(
                Path::new(run_git_capture(local, &["rev-parse", "--show-toplevel"])?.trim()),
                local,
            ) {
                return Err("Local submodule is not a repository root.".into());
            }
            let url = run_git_capture(
                target,
                &[
                    "config",
                    "--get",
                    &(key.trim_end_matches("path").to_owned() + "url"),
                ],
            )?;
            let tree = run_git_capture(target, &["ls-tree", "-z", "HEAD", "--", relative])?;
            let fields: Vec<_> = tree.splitn(4, [' ', '\t']).collect();
            if fields.len() != 4 || fields[0] != "160000" || fields[1] != "commit" {
                return Err(format!("Missing baseline gitlink for '{relative}'.").into());
            }
            let commit = fields[2];
            println!("Seeding submodule {relative} from {}", local.display());
            run_git_capture(
                target,
                &[
                    "clone",
                    "--no-checkout",
                    "--no-hardlinks",
                    "--",
                    &local.to_string_lossy(),
                    &destination.to_string_lossy(),
                ],
            )?;
            run_git_capture(&destination, &["remote", "set-url", "origin", url.trim()])?;
            run_git_capture(
                &destination,
                &[
                    "config",
                    "--local",
                    "lfs.storage",
                    &destination.join(".git/lfs").to_string_lossy(),
                ],
            )?;
            if !capture_process_output(&mut make_git_command(
                &destination,
                &["cat-file", "-e", &format!("{commit}^{{commit}}")],
            ))?
            .status
            .success()
            {
                run_git_capture(&destination, &["fetch", "--no-tags", "origin", commit])?;
            }
            copy_lfs_objects(local, &destination, commit)?;
            run_git_capture(&destination, &["checkout", "--detach", commit])?;
        } else {
            run_git_capture(
                target,
                &[
                    "submodule",
                    "update",
                    "--init",
                    "--checkout",
                    "--",
                    relative,
                ],
            )?;
        }
        initialize_submodules(&destination, local.as_deref(), None)?;
    }
    Ok(())
}

fn copy_lfs_objects(source: &Path, target: &Path, commit: &str) -> Result<()> {
    let listing: Value = serde_json::from_str(&run_git_capture(
        target,
        &["lfs", "ls-files", "--json", commit],
    )?)?;
    let Some(files) = listing["files"]
        .as_array()
        .filter(|files| !files.is_empty())
    else {
        return Ok(());
    };
    fn find_lfs_cache_directory(root: &Path) -> Result<PathBuf> {
        let env = run_git_capture(root, &["lfs", "env"])?;
        resolve_absolute_path(
            root,
            env.lines()
                .find_map(|line| line.strip_prefix("LocalMediaDir="))
                .ok_or("Could not locate LFS cache.")?,
        )
    }
    let source = find_lfs_cache_directory(source)?;
    let target = find_lfs_cache_directory(target)?;
    for file in files {
        let oid = file["oid"].as_str().ok_or("Missing LFS object identity.")?;
        if oid.len() != 64 || !oid.bytes().all(|c| c.is_ascii_hexdigit()) {
            return Err("Invalid LFS object identity.".into());
        }
        let relative = Path::new(&oid[..2]).join(&oid[2..4]).join(oid);
        let cached = source.join(&relative);
        let destination = target.join(relative);
        if cached.is_file() && !destination.exists() {
            fs::create_dir_all(destination.parent().unwrap())?;
            fs::copy(cached, destination)?;
        }
    }
    Ok(())
}

#[cfg(test)]
#[path = "../tests/revision/mod.rs"]
mod tests;
