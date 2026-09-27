mod support;
use std::fs;
use std::io::Write;
use std::process::Stdio;
use support::*;

#[test]
fn integration_cli_requires_real_jobserver_context() {
    let repo = Repo::new();
    let output = repo.tool().arg("integrate").output().unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("lease"));
    let output = repo
        .tool()
        .arg("integrate")
        .env("NUKETHEBEES_JOBSERVER_LEASE", "made-up")
        .env("LOCALAPPDATA", &repo.root)
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("Cannot verify"));
}

#[test]
fn normal_git_rebase_recovery_needs_no_tool_provenance() {
    for recovery in ["--abort", "--skip", "--continue"] {
        let repo = Repo::new();
        fs::write(repo.feature.join("file.txt"), "feature\n").unwrap();
        repo.ok(&["add", "file.txt"]);
        repo.ok(&["commit", "-m", "feature change"]);
        fs::write(repo.dev.join("file.txt"), "dev\n").unwrap();
        repo.raw(&repo.dev, &["add", "file.txt"]);
        repo.raw(&repo.dev, &["commit", "-qm", "advance dev"]);
        let base = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
        let output = repo.tool().args(["git", "rebase", "dev"]).output().unwrap();
        assert!(!output.status.success());
        if recovery == "--continue" {
            fs::write(repo.feature.join("file.txt"), "resolved\n").unwrap();
            repo.ok(&["add", "file.txt"]);
        }
        repo.ok(&["rebase", recovery]);
        assert_eq!(
            repo.raw(&repo.feature, &["branch", "--show-current"]),
            "feature"
        );
        assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), base);
        assert!(
            repo.raw(&repo.feature, &["status", "--porcelain"])
                .is_empty()
        );
    }
}

#[test]
fn feature_autonomy_and_revision_inputs() {
    let repo = Repo::new();
    repo.ok(&["commit", "--allow-empty", "-m", "ordinary"]);
    repo.ok(&["commit", "--amend", "--allow-empty", "-m", "amended"]);
    repo.ok(&["reset", "--hard", "dev"]);
    fs::write(repo.feature.join("junk"), "junk").unwrap();
    repo.ok(&["clean", "-fd"]);
    assert!(!repo.feature.join("junk").exists());
    repo.ok(&["rebase", "dev"]);
    repo.ok(&["merge", "dev"]);
    repo.ok(&["branch", "temporary", "dev"]);
    repo.ok(&["branch", "-D", "temporary"]);
    repo.ok(&["worktree", "add", "-b", "from-dev", "from dev", "dev"]);
    repo.ok(&["worktree", "move", "from dev", "moved"]);
    repo.ok(&["worktree", "remove", "moved"]);
    repo.ok(&["switch", "--detach", "dev"]);
    repo.ok(&["switch", "feature"]);
    repo.ok(&[
        "commit",
        "--allow-empty",
        "-m",
        "--output=../message-is-not-a-path",
    ]);
    repo.ok(&["switch", "-c", "another", "dev"]);
    repo.ok(&["checkout", "-b", "third", "master"]);
    repo.ok(&["diff", "dev"]);
    repo.ok(&["log", "master..HEAD"]);
    repo.ok(&["merge-base", "dev", "HEAD"]);
    repo.ok(&["worktree", "add", "-b", "child", "child space"]);
    repo.ok(&["worktree", "remove", "child space"]);
    repo.ok(&["update-ref", "refs/heads/temporary", "HEAD"]);
    repo.ok(&["branch", "-D", "temporary"]);
}

#[test]
fn protected_ref_writes_and_attachments() {
    let repo = Repo::new();
    for args in [
        vec!["switch", "dev"],
        vec!["switch", "master"],
        vec!["switch", "--", "main"],
        vec!["checkout", "dev"],
        vec!["checkout", "master"],
        vec!["checkout", "master", "--"],
        vec!["switch", "-Cdev"],
        vec!["checkout", "-B", "main"],
        vec!["switch", "--create=master"],
        vec!["checkout", "--orphan", "dev"],
        vec!["branch", "-f", "dev", "HEAD"],
        vec!["branch", "-D", "master"],
        vec!["branch", "--delete", "main"],
        vec!["branch", "-M", "dev"],
        vec!["branch", "-m", "master", "renamed"],
        vec!["update-ref", "refs/heads/dev", "HEAD"],
        vec!["update-ref", "-d", "refs/heads/master"],
        vec!["update-ref", "--stdin"],
        vec!["worktree", "add", "child", "dev"],
        vec!["worktree", "add", "-B", "master", "child"],
        vec!["worktree", "add", "dev"],
        vec!["rebase", "HEAD", "dev"],
        vec!["rebase", "--root", "master"],
        vec!["rebase", "--update-refs", "dev"],
    ] {
        repo.blocked(&args);
    }
    repo.raw(&repo.feature, &["config", "rebase.updateRefs", "true"]);
    repo.blocked(&["rebase", "dev"]);
    repo.ok(&["rebase", "--no-update-refs", "dev"]);
}

#[test]
fn containment_and_redirects() {
    let repo = Repo::new();
    for args in [
        vec!["worktree", "add", "../outside"],
        vec!["worktree", "remove", "../dev"],
        vec!["worktree", "move", "../dev", "child"],
        vec!["-C", "..", "status"],
        vec!["--git-dir=.git", "status"],
        vec!["--work-tree", "..", "status"],
        vec!["diff", "--output=../outside"],
        vec!["archive", "HEAD", "-o", "../outside"],
        vec!["checkout-index", "--prefix=../outside/", "--all"],
        vec!["checkout-index", "--prefix", "../outside/", "--all"],
        vec!["apply", "--unsafe-paths", "patch.diff"],
        vec!["archive", "--remote=../outside", "HEAD"],
    ] {
        repo.blocked(&args);
    }
    for name in [
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_COMMON_DIR",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_NAMESPACE",
        "GIT_CONFIG_COUNT",
        "GIT_CONFIG_PARAMETERS",
    ] {
        let output = repo
            .tool()
            .args(["git", "status"])
            .env(name, "redirect")
            .output()
            .unwrap();
        assert!(!output.status.success(), "{name}");
        assert!(String::from_utf8_lossy(&output.stderr).contains(name));
    }
    fs::create_dir(repo.feature.join("nested")).unwrap();
    let output = repo
        .tool()
        .current_dir(repo.feature.join("nested"))
        .args(["git", "worktree", "add", "-b", "nested-child", "../inside"])
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
}

#[test]
fn argv_streams_and_exit_status_are_git_native() {
    let repo = Repo::new();
    let message = "spaces, \"quotes\", $() ` ; & \\ and unicode: λ";
    repo.ok(&["commit", "--allow-empty", "-m", message]);
    assert_eq!(
        repo.raw(&repo.feature, &["log", "-1", "--format=%s"]),
        message
    );
    fs::write(repo.feature.join("space [a].txt"), "data").unwrap();
    repo.ok(&["add", "--", "space [a].txt"]);
    let args = [
        "log",
        "--format=%h %s",
        "--all",
        "--",
        ":(literal)space [a].txt",
    ];
    let direct = repo.git(&repo.feature).args(args).output().unwrap();
    let wrapped = repo.tool().arg("git").args(args).output().unwrap();
    assert_eq!(direct.stdout, wrapped.stdout);
    assert_eq!(direct.stderr, wrapped.stderr);
    let args = ["rev-parse", "--verify", "missing-ref"];
    let direct = repo.git(&repo.feature).args(args).output().unwrap();
    let wrapped = repo.tool().arg("git").args(args).output().unwrap();
    assert_eq!(direct.status.code(), wrapped.status.code());
    assert_eq!(direct.stderr, wrapped.stderr);
    let mut child = repo
        .tool()
        .args(["git", "hash-object", "--stdin"])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    child.stdin.take().unwrap().write_all(b"hello\n").unwrap();
    let output = child.wait_with_output().unwrap();
    assert!(output.status.success());
    assert_eq!(
        String::from_utf8_lossy(&output.stdout).trim(),
        "ce013625030ba8dba906f756967f9e9ca394464a"
    );
}

#[test]
fn protected_current_worktree_is_read_only() {
    let repo = Repo::new();
    for args in [
        vec!["commit", "--allow-empty", "-m", "bad"],
        vec!["reset", "--hard"],
        vec!["switch", "feature"],
        vec!["clean", "-fd"],
        vec!["log", "--output=file.txt"],
    ] {
        let output = repo
            .tool()
            .current_dir(&repo.dev)
            .arg("git")
            .args(&args)
            .output()
            .unwrap();
        assert!(!output.status.success(), "{args:?}");
        assert!(String::from_utf8_lossy(&output.stderr).contains("protected"));
    }
    let output = repo
        .tool()
        .current_dir(&repo.dev)
        .args(["git", "status", "--short"])
        .output()
        .unwrap();
    assert!(output.status.success());
}
