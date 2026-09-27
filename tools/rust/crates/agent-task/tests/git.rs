mod support;
use std::fs;
use support::Repo;

#[test]
fn grammar_is_closed_and_help_is_available_without_a_repository() {
    let temp = std::env::temp_dir();
    for args in [
        vec!["update-ref", "refs/heads/dev", "HEAD"],
        vec!["bisect", "reset", "master"],
        vec!["checkout", "feature"],
        vec!["fetch"],
        vec!["log"],
        vec!["unknown"],
        vec!["status", "--porcelain"],
        vec!["switch", "--force-crea=master"],
        vec!["branch", "--mov", "feature", "master"],
        vec!["rebase", "--roo"],
        vec!["rebase", "--update-ref", "dev"],
        vec!["rebase", "--update-refs", "dev"],
        vec!["rebase", "--rebase-merges", "dev"],
        vec!["rebase", "dev", "feature"],
        vec!["rebase", "--continue", "dev"],
        vec!["rebase", "--onto", "dev", "--abort"],
        vec!["switch", "-C", "feature"],
        vec!["switch", "--track", "origin/feature"],
        vec!["branch", "-f", "feature", "dev"],
        vec!["branch", "-m", "old", "new"],
        vec!["worktree", "move", "a", "b"],
        vec!["worktree", "add", "child", "dev"],
        vec!["worktree", "remove", "--force", "child"],
        vec!["clean", "-fx"],
        vec!["clean", "-nd"],
        vec!["commit", "-a", "-m", "message"],
        vec!["commit", "--author", "someone"],
        vec!["add", "--unknown"],
        vec!["stash", "pop", "stash@{1}"],
        vec!["reset", "--hard", "--soft", "dev"],
        vec!["merge", "--abort", "--continue"],
        vec!["-C", ".", "status"],
        vec!["--git-dir=.git", "status"],
        vec!["--work-tree=.", "status"],
        vec!["-c", "key=value", "status"],
    ] {
        let output = std::process::Command::new(env!("CARGO_BIN_EXE_agent-task"))
            .current_dir(&temp)
            .arg("git")
            .args(&args)
            .output()
            .unwrap();
        assert_eq!(output.status.code(), Some(2), "{args:?}: {output:?}");
        assert!(
            String::from_utf8_lossy(&output.stderr).contains("--help"),
            "{output:?}"
        );
    }
    for command in [
        "status",
        "add",
        "commit",
        "restore",
        "reset",
        "clean",
        "rm",
        "mv",
        "stash",
        "switch",
        "branch",
        "merge",
        "rebase",
        "cherry-pick",
        "revert",
        "worktree",
    ] {
        let output = std::process::Command::new(env!("CARGO_BIN_EXE_agent-task"))
            .current_dir(&temp)
            .args(["git", command, "--unsupported-option"])
            .output()
            .unwrap();
        assert_eq!(output.status.code(), Some(2), "{command}: {output:?}");
    }
    for args in [
        vec!["--help"],
        vec!["rebase", "--help"],
        vec!["switch", "--help"],
        vec!["worktree", "--help"],
    ] {
        let output = std::process::Command::new(env!("CARGO_BIN_EXE_agent-task"))
            .current_dir(&temp)
            .arg("git")
            .args(args)
            .output()
            .unwrap();
        assert!(output.status.success(), "{output:?}");
        assert!(String::from_utf8_lossy(&output.stdout).contains("Usage:"));
    }
}

#[test]
fn feature_commit_amend_reset_clean_and_branch_lifecycle() {
    let repo = Repo::new();
    fs::write(repo.feature.join("file.txt"), "changed\n").unwrap();
    repo.ok(&["add", "-u"]);
    repo.ok(&["commit", "-m", "change"]);
    repo.ok(&["commit", "--amend", "--no-edit"]);
    repo.ok(&["commit", "--amend", "-m", "amended"]);
    repo.ok(&["reset", "--soft", "HEAD^"]);
    repo.ok(&["reset", "--mixed", "HEAD"]);
    repo.ok(&["reset"]);
    repo.ok(&["reset", "--hard", "dev"]);
    assert_eq!(
        fs::read_to_string(repo.feature.join("file.txt")).unwrap(),
        "base\n"
    );
    fs::create_dir(repo.feature.join("untracked")).unwrap();
    fs::write(repo.feature.join("untracked/file"), "delete").unwrap();
    repo.ok(&["clean", "-n"]);
    assert!(repo.feature.join("untracked/file").exists());
    repo.ok(&["clean", "-fd"]);
    assert!(!repo.feature.join("untracked").exists());
    repo.ok(&["branch", "temporary", "dev"]);
    repo.ok(&["branch", "-d", "temporary"]);
    repo.ok(&["switch", "-c", "feature-from-dev", "dev"]);
    repo.ok(&["commit", "--allow-empty", "-m", "empty"]);
    repo.ok(&["branch", "-m", "renamed"]);
    repo.ok(&["switch", "feature"]);
    repo.ok(&["switch", "-"]);
    assert_eq!(
        repo.raw(&repo.feature, &["branch", "--show-current"]),
        "renamed"
    );
    repo.ok(&["switch", "--detach", "dev"]);
    repo.ok(&["branch", "-D", "renamed"]);
    repo.ok(&["switch", "feature"]);
}

#[test]
fn paths_messages_restore_rm_mv_and_stash_preserve_values() {
    let repo = Repo::new();
    let path = "space ' quote λ.txt";
    let message = "--message with spaces, \"quotes\", λ\nand a second line";
    fs::write(repo.feature.join(path), "content\n").unwrap();
    repo.ok(&["add", "--", path]);
    repo.ok(&["commit", "-m", message]);
    assert_eq!(
        repo.raw(&repo.feature, &["log", "-1", "--format=%B"]),
        message
    );
    repo.ok(&["mv", path, "renamed λ.txt"]);
    repo.ok(&["restore", "--staged", "--", path, "renamed λ.txt"]);
    repo.ok(&["restore", "--source", "HEAD", "--", path]);
    assert!(repo.feature.join(path).exists());
    repo.ok(&["clean", "-f"]);
    fs::write(repo.feature.join(path), "stash me\n").unwrap();
    repo.ok(&["stash", "push", "-m", message]);
    repo.ok(&["stash", "list"]);
    repo.ok(&["stash", "apply"]);
    assert_eq!(
        fs::read_to_string(repo.feature.join(path)).unwrap(),
        "stash me\n"
    );
    repo.ok(&["restore", path]);
    repo.ok(&["stash", "pop"]);
    repo.ok(&["stash", "push"]);
    repo.ok(&["stash", "drop"]);
    repo.ok(&["rm", "--cached", "--", path]);
    assert!(repo.feature.join(path).exists());
    repo.ok(&["restore", "--staged", path]);
    repo.ok(&["rm", path]);
    assert!(!repo.feature.join(path).exists());
}

#[test]
fn protected_refs_are_inputs_only() {
    let repo = Repo::new();
    for name in ["dev", "main", "master"] {
        for args in [
            vec!["switch", name],
            vec!["switch", "-c", name],
            vec!["branch", name],
            vec!["branch", "-m", name],
            vec!["branch", "-d", name],
            vec!["branch", "-D", name],
            vec!["worktree", "add", "-b", name, "child"],
        ] {
            repo.blocked(&args);
        }
    }
    repo.ok(&["reset", "--hard", "dev"]);
    repo.ok(&["merge", "--no-edit", "dev"]);
    repo.ok(&["rebase", "dev"]);
    repo.ok(&["switch", "-c", "from-dev", "dev"]);
    // Previous-branch syntax must not accidentally reattach to protected refs.
    repo.raw(&repo.feature, &["switch", "main"]);
    repo.raw(&repo.feature, &["switch", "from-dev"]);
    repo.blocked(&["switch", "-"]);
    assert_eq!(
        repo.raw(&repo.feature, &["rev-parse", "main"]),
        repo.raw(&repo.feature, &["rev-parse", "dev"])
    );
}

#[test]
fn foreign_branches_cannot_be_attached_deleted_or_renamed() {
    let repo = Repo::new();
    let other = repo.root.join("other λ");
    repo.raw(
        &repo.feature,
        &["worktree", "add", "-b", "other", other.to_str().unwrap()],
    );
    let before = repo.raw(&other, &["rev-parse", "HEAD"]);
    for args in [
        vec!["switch", "other"],
        vec!["switch", "-c", "other"],
        vec!["branch", "-D", "other"],
        vec!["branch", "-d", "other"],
        vec!["branch", "-m", "other"],
        vec!["branch", "other"],
    ] {
        repo.blocked(&args);
    }
    repo.ok(&["commit", "--allow-empty", "-m", "own branch"]);
    repo.ok(&["reset", "--hard", "other"]);
    assert_eq!(repo.raw(&other, &["rev-parse", "HEAD"]), before);
    // Even a maintainer-forced duplicate checkout must not let the ordinary path move it.
    repo.raw(&other, &["switch", "--ignore-other-worktrees", "feature"]);
    repo.blocked(&["reset", "--hard", "dev"]);
    repo.blocked(&["branch", "-m", "new-name"]);
    repo.blocked(&["commit", "--allow-empty", "-m", "no"]);
}

#[test]
fn rebase_disables_updates_to_other_refs_and_supports_onto() {
    let repo = Repo::new();
    repo.ok(&["commit", "--allow-empty", "-m", "first"]);
    repo.ok(&["branch", "saved"]);
    let saved = repo.raw(&repo.feature, &["rev-parse", "saved"]);
    repo.ok(&["commit", "--allow-empty", "-m", "second"]);
    repo.raw(&repo.dev, &["commit", "--allow-empty", "-m", "advance dev"]);
    repo.raw(&repo.feature, &["config", "rebase.updateRefs", "true"]);
    repo.ok(&["rebase", "dev"]);
    assert_eq!(repo.raw(&repo.feature, &["rev-parse", "saved"]), saved);
    assert_ne!(repo.raw(&repo.feature, &["rev-parse", "HEAD^"]), saved);
    repo.ok(&["rebase", "--onto", "main", "dev"]);
}

#[test]
fn rebase_conflict_recovery_continues_aborts_and_skips() {
    for action in ["--continue", "--abort", "--skip"] {
        let repo = Repo::new();
        fs::write(repo.feature.join("file.txt"), "feature\n").unwrap();
        repo.ok(&["add", "-A"]);
        repo.ok(&["commit", "-m", "feature"]);
        let original = repo.raw(&repo.feature, &["rev-parse", "HEAD"]);
        fs::write(repo.dev.join("file.txt"), "dev\n").unwrap();
        repo.raw(&repo.dev, &["commit", "-am", "dev"]);
        let output = repo.tool().args(["git", "rebase", "dev"]).output().unwrap();
        assert!(!output.status.success());
        if action == "--continue" {
            fs::write(repo.feature.join("file.txt"), "resolved\n").unwrap();
            repo.ok(&["add", "file.txt"]);
        }
        repo.ok(&["rebase", action]);
        assert_eq!(
            repo.raw(&repo.feature, &["branch", "--show-current"]),
            "feature"
        );
        assert!(
            repo.raw(&repo.feature, &["status", "--porcelain"])
                .is_empty()
        );
        if action == "--abort" {
            assert_eq!(repo.raw(&repo.feature, &["rev-parse", "HEAD"]), original);
        }
    }
}

#[test]
fn worktrees_are_limited_to_child_destinations() {
    let repo = Repo::new();
    repo.ok(&["worktree", "list"]);
    repo.ok(&["worktree", "add", "-b", "child-feature", "child λ", "dev"]);
    assert_eq!(
        repo.raw(&repo.feature.join("child λ"), &["branch", "--show-current"]),
        "child-feature"
    );
    repo.ok(&["worktree", "remove", "child λ"]);
    assert!(!repo.feature.join("child λ").exists());
    repo.blocked(&["worktree", "add", "-b", "outside", "../outside", "dev"]);
    repo.blocked(&["worktree", "remove", "../dev"]);
    repo.blocked(&["worktree", "remove", "."]);
}

#[test]
fn protected_worktree_allows_inspection_only_and_environment_redirects_fail() {
    let repo = Repo::new();
    for args in [
        vec!["status", "--short"],
        vec!["branch"],
        vec!["stash", "list"],
        vec!["worktree", "list"],
    ] {
        let output = repo
            .tool()
            .current_dir(&repo.dev)
            .arg("git")
            .args(args)
            .output()
            .unwrap();
        assert!(output.status.success(), "{output:?}");
    }
    for args in [
        vec!["switch", "-c", "no"],
        vec!["reset", "--hard"],
        vec!["commit", "--allow-empty", "-m", "no"],
        vec!["add", "-A"],
    ] {
        let output = repo
            .tool()
            .current_dir(&repo.dev)
            .arg("git")
            .args(args)
            .output()
            .unwrap();
        assert!(!output.status.success());
        assert!(String::from_utf8_lossy(&output.stderr).contains("protected"));
    }
    for name in [
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_COMMON_DIR",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_NAMESPACE",
        "GIT_CONFIG",
        "GIT_CONFIG_COUNT",
        "GIT_CONFIG_PARAMETERS",
    ] {
        let output = repo
            .tool()
            .env(name, "redirect")
            .args(["git", "status"])
            .output()
            .unwrap();
        assert!(!output.status.success());
        assert!(String::from_utf8_lossy(&output.stderr).contains(name));
    }
}

#[test]
fn supported_git_streams_and_exit_status_are_preserved() {
    let repo = Repo::new();
    for args in [
        vec!["status", "--short"],
        vec!["commit", "-m", "nothing staged"],
    ] {
        let direct = repo.git(&repo.feature).args(&args).output().unwrap();
        let wrapped = repo.tool().arg("git").args(&args).output().unwrap();
        assert_eq!(wrapped.status.code(), direct.status.code());
        assert_eq!(wrapped.stdout, direct.stdout);
        assert_eq!(wrapped.stderr, direct.stderr);
    }
    let output = repo.tool().arg("integrate").output().unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("lease"));
}

#[test]
fn merge_cherry_pick_and_revert_change_only_the_current_branch() {
    let repo = Repo::new();
    fs::write(repo.dev.join("incoming.txt"), "incoming\n").unwrap();
    repo.raw(&repo.dev, &["add", "incoming.txt"]);
    repo.raw(&repo.dev, &["commit", "-m", "incoming"]);
    let dev = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    repo.ok(&["cherry-pick", "dev"]);
    assert!(repo.feature.join("incoming.txt").exists());
    repo.ok(&["revert", "--no-edit", "HEAD"]);
    assert!(!repo.feature.join("incoming.txt").exists());
    repo.ok(&["reset", "--hard", "main"]);
    repo.ok(&["merge", "dev"]);
    assert!(repo.feature.join("incoming.txt").exists());
    assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), dev);
}

#[test]
fn merge_and_cherry_pick_conflicts_support_recovery() {
    for (command, actions) in [
        ("merge", vec!["--continue", "--abort"]),
        ("cherry-pick", vec!["--continue", "--abort", "--skip"]),
    ] {
        for action in actions {
            let repo = Repo::new();
            fs::write(repo.dev.join("file.txt"), "dev\n").unwrap();
            repo.raw(&repo.dev, &["commit", "-am", "dev change"]);
            fs::write(repo.feature.join("file.txt"), "feature\n").unwrap();
            repo.ok(&["add", "file.txt"]);
            repo.ok(&["commit", "-m", "feature change"]);
            let result = repo.tool().args(["git", command, "dev"]).output().unwrap();
            assert!(!result.status.success());
            if action == "--continue" {
                fs::write(repo.feature.join("file.txt"), "resolved\n").unwrap();
                repo.ok(&["add", "file.txt"]);
            }
            repo.ok(&[command, action]);
            assert_eq!(
                repo.raw(&repo.feature, &["branch", "--show-current"]),
                "feature"
            );
            assert!(
                repo.raw(&repo.feature, &["status", "--porcelain"])
                    .is_empty()
            );
        }
    }
}

#[test]
fn revert_conflict_recovery() {
    for action in ["--continue", "--abort"] {
        let repo = Repo::new();
        fs::write(repo.feature.join("file.txt"), "first\n").unwrap();
        repo.ok(&["add", "file.txt"]);
        repo.ok(&["commit", "-m", "first"]);
        fs::write(repo.feature.join("file.txt"), "second\n").unwrap();
        repo.ok(&["add", "file.txt"]);
        repo.ok(&["commit", "-m", "second"]);
        let result = repo
            .tool()
            .args(["git", "revert", "HEAD^"])
            .output()
            .unwrap();
        assert!(!result.status.success());
        if action == "--continue" {
            fs::write(repo.feature.join("file.txt"), "resolved\n").unwrap();
            repo.ok(&["add", "file.txt"]);
        }
        repo.ok(&["revert", action]);
        assert!(
            repo.raw(&repo.feature, &["status", "--porcelain"])
                .is_empty()
        );
    }
}

#[test]
fn interactive_add_inherits_input_and_pathspec_magic_is_literal() {
    use std::io::Write;
    use std::process::Stdio;
    let repo = Repo::new();
    fs::write(repo.feature.join("file.txt"), "interactive\n").unwrap();
    let mut child = repo
        .tool()
        .args(["git", "add", "-p"])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    child.stdin.take().unwrap().write_all(b"y\n").unwrap();
    let output = child.wait_with_output().unwrap();
    assert!(output.status.success(), "{output:?}");
    assert!(
        repo.raw(&repo.feature, &["diff", "--cached"])
            .contains("interactive")
    );
    fs::write(repo.feature.join("--odd[1].txt"), "literal\n").unwrap();
    repo.ok(&["add", "--", ":(literal)--odd[1].txt"]);
    assert!(
        repo.raw(&repo.feature, &["diff", "--cached", "--name-only"])
            .contains("--odd[1].txt")
    );
}
