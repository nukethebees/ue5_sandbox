use super::*;
#[path = "../support/mod.rs"]
mod support;
use std::fs;
use support::Repo;

fn transaction(root: &Path, keep: bool) -> Result<(), String> {
    super::transaction(root, keep, |_| Ok(()))
}

#[test]
fn lease_is_reverified_before_promotion() {
    let repo = Repo::new();
    commit(&repo, &repo.feature, "feature.txt", "feature\n");
    let base = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    let feature = repo.raw(&repo.feature, &["rev-parse", "HEAD"]);
    let mut calls = 0;
    let error = super::transaction(&repo.feature, false, |_| {
        calls += 1;
        if calls == 2 {
            // The merge object exists at this boundary, but dev has not advanced.
            let unreachable = repo.raw(&repo.feature, &["fsck", "--unreachable", "--no-reflogs"]);
            let merge = unreachable
                .lines()
                .find_map(|line| line.strip_prefix("unreachable commit "))
                .unwrap();
            assert_eq!(
                repo.raw(&repo.feature, &["show", "-s", "--format=%P", merge]),
                format!("{base} {feature}")
            );
            assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), base);
            Err("integration lease expired".into())
        } else {
            Ok(())
        }
    })
    .unwrap_err();
    assert_eq!(calls, 2);
    assert!(error.contains("lease expired"));
    assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), base);
    assert_eq!(branch(&repo.feature).unwrap(), "feature");
}

#[test]
fn final_rebase_flattens_merges_despite_ambient_configuration() {
    let repo = Repo::new();
    repo.raw(&repo.feature, &["switch", "-c", "side"]);
    commit(&repo, &repo.feature, "side.txt", "side\n");
    repo.raw(&repo.feature, &["switch", "feature"]);
    commit(&repo, &repo.feature, "feature.txt", "feature\n");
    repo.raw(&repo.feature, &["merge", "--no-edit", "side"]);
    repo.raw(&repo.feature, &["config", "rebase.rebaseMerges", "true"]);
    repo.raw(&repo.feature, &["config", "rebase.updateRefs", "true"]);
    let side = repo.raw(&repo.feature, &["rev-parse", "side"]);
    commit(&repo, &repo.dev, "advance.txt", "advance\n");
    let base = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    transaction(&repo.feature, true).unwrap();
    assert_eq!(
        repo.raw(
            &repo.dev,
            &[
                "rev-list",
                "--count",
                "--merges",
                &format!("{base}..feature")
            ]
        ),
        "0"
    );
    assert_eq!(repo.raw(&repo.dev, &["rev-parse", "side"]), side);
}

fn commit(repo: &Repo, root: &Path, file: &str, content: &str) {
    fs::write(root.join(file), content).unwrap();
    repo.raw(root, &["add", file]);
    repo.raw(root, &["commit", "-qm", "change"]);
}

#[test]
fn lease_requires_matching_running_exclusive_integration_job() {
    let repo = Repo::new();
    let status = serde_json::json!({"jobs": [{"id":"lease", "state":"RUNNING", "kind":"integration",
        "worktree":repo.feature, "claims":[{"name":"integration/dev", "mode":"exclusive"}]}]});
    assert!(check_lease(&status, "lease", &repo.feature).is_ok());
    assert!(check_lease(&status, "missing", &repo.feature).is_err());
    assert!(check_lease(&status, "lease", &repo.dev).is_err());
    for (key, value) in [("state", "QUEUED"), ("kind", "build")] {
        let mut wrong = status.clone();
        wrong["jobs"][0][key] = value.into();
        assert!(check_lease(&wrong, "lease", &repo.feature).is_err());
    }
    for (key, value) in [("name", "integration/main"), ("mode", "shared")] {
        let mut wrong = status.clone();
        wrong["jobs"][0]["claims"][0][key] = value.into();
        assert!(check_lease(&wrong, "lease", &repo.feature).is_err());
    }
    assert!(check_lease(&serde_json::json!({}), "lease", &repo.feature).is_err());
}

#[test]
fn successful_integration_rebases_merges_refreshes_and_returns_home() {
    for keep in [false, true] {
        let repo = Repo::new();
        commit(&repo, &repo.feature, "feature.txt", "feature\n");
        let original = repo.raw(&repo.feature, &["rev-parse", "HEAD"]);
        commit(&repo, &repo.dev, "advance.txt", "advance\n");
        let base = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
        transaction(&repo.feature, keep).unwrap();
        let parents = repo.raw(&repo.dev, &["show", "-s", "--format=%P", "HEAD"]);
        let parents: Vec<_> = parents.split_whitespace().collect();
        assert_eq!(parents.len(), 2);
        assert_eq!(parents[0], base);
        assert_ne!(parents[1], original);
        assert_eq!(
            repo.raw(&repo.dev, &["rev-parse", &format!("{}^", parents[1])]),
            base
        );
        assert_eq!(
            repo.raw(&repo.dev, &["rev-parse", "HEAD^{tree}"]),
            repo.raw(
                &repo.dev,
                &["rev-parse", &format!("{}^{{tree}}", parents[1])]
            )
        );
        assert_eq!(
            repo.raw(&repo.feature, &["branch", "--show-current"]),
            "dev1"
        );
        assert_eq!(
            fs::read_to_string(repo.dev.join("feature.txt")).unwrap(),
            "feature\n"
        );
        assert!(repo.raw(&repo.dev, &["status", "--porcelain"]).is_empty());
        let exists = repo
            .git(&repo.dev)
            .args(["show-ref", "--verify", "--quiet", "refs/heads/feature"])
            .status()
            .unwrap()
            .success();
        assert_eq!(exists, keep);
    }
}

#[test]
fn conflict_aborts_rebase_and_leaves_dev_and_feature_unchanged() {
    let repo = Repo::new();
    commit(&repo, &repo.feature, "file.txt", "feature\n");
    commit(&repo, &repo.dev, "file.txt", "dev\n");
    let dev = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    let feature = repo.raw(&repo.feature, &["rev-parse", "HEAD"]);
    let error = transaction(&repo.feature, false).unwrap_err();
    assert!(error.contains("outside the queue"), "{error}");
    assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), dev);
    assert_eq!(repo.raw(&repo.feature, &["rev-parse", "HEAD"]), feature);
    assert_eq!(
        repo.raw(&repo.feature, &["branch", "--show-current"]),
        "feature"
    );
    clean(&repo.feature).unwrap();
}

#[test]
fn preflight_requires_feature_clean_index_worktrees_and_no_operation() {
    let repo = Repo::new();
    assert!(
        transaction(&repo.dev, false)
            .unwrap_err()
            .contains("feature branch")
    );
    repo.raw(&repo.feature, &["switch", "--detach"]);
    assert!(
        transaction(&repo.feature, false)
            .unwrap_err()
            .contains("feature branch")
    );
    repo.raw(&repo.feature, &["switch", "dev1"]);
    assert!(
        transaction(&repo.feature, false)
            .unwrap_err()
            .contains("feature branch")
    );
    repo.raw(&repo.feature, &["switch", "feature"]);
    for root in [&repo.feature, &repo.dev] {
        fs::write(root.join("dirty.txt"), "dirty").unwrap();
        assert!(
            transaction(&repo.feature, false)
                .unwrap_err()
                .contains("clean")
        );
        repo.raw(root, &["add", "dirty.txt"]);
        assert!(
            transaction(&repo.feature, false)
                .unwrap_err()
                .contains("clean")
        );
        repo.raw(root, &["reset", "--hard"]);
        let admin = PathBuf::from(repo.raw(root, &["rev-parse", "--absolute-git-dir"]));
        fs::create_dir(admin.join("sequencer")).unwrap();
        assert!(
            transaction(&repo.feature, false)
                .unwrap_err()
                .contains("in-progress")
        );
        fs::remove_dir(admin.join("sequencer")).unwrap();
    }
}

#[test]
fn atomic_update_rejects_moved_dev_without_retry() {
    let repo = Repo::new();
    let pinned = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    commit(&repo, &repo.feature, "feature.txt", "feature\n");
    let feature = repo.raw(&repo.feature, &["rev-parse", "HEAD"]);
    let tree = repo.raw(&repo.feature, &["rev-parse", "HEAD^{tree}"]);
    let merge = repo.raw(
        &repo.feature,
        &[
            "commit-tree",
            &tree,
            "-p",
            &pinned,
            "-p",
            &feature,
            "-m",
            "merge",
        ],
    );
    commit(&repo, &repo.dev, "advance.txt", "advance\n");
    let moved = repo.raw(&repo.dev, &["rev-parse", "HEAD"]);
    let error = compare_and_swap(&repo.dev, &pinned, &merge).unwrap_err();
    assert!(error.contains("Atomic promotion rejected"));
    assert_eq!(repo.raw(&repo.dev, &["rev-parse", "HEAD"]), moved);
}

#[test]
fn nonpersistent_worktree_detaches_for_deletion_and_keeps_when_requested() {
    for keep in [false, true] {
        let repo = Repo::new();
        let temporary = repo.root.join("temporary");
        repo.raw(
            &repo.dev,
            &[
                "worktree",
                "add",
                "-b",
                "other-feature",
                temporary.to_str().unwrap(),
            ],
        );
        commit(&repo, &temporary, "feature.txt", "feature\n");
        transaction(&temporary, keep).unwrap();
        assert_eq!(
            branch(&temporary).unwrap(),
            if keep { "other-feature" } else { "" }
        );
        let exists = repo
            .git(&repo.dev)
            .args([
                "show-ref",
                "--verify",
                "--quiet",
                "refs/heads/other-feature",
            ])
            .status()
            .unwrap()
            .success();
        assert_eq!(exists, keep);
    }
}

#[test]
fn cleanup_failure_reports_promotion_and_preserves_feature() {
    let repo = Repo::new();
    let other = repo.root.join("home-holder");
    repo.raw(
        &repo.dev,
        &["worktree", "add", other.to_str().unwrap(), "dev1"],
    );
    commit(&repo, &repo.feature, "feature.txt", "feature\n");
    let error = transaction(&repo.feature, false).unwrap_err();
    assert!(error.contains("dev already contains merge"), "{error}");
    assert_eq!(branch(&repo.feature).unwrap(), "feature");
    assert!(repo.dev.join("feature.txt").exists());
}
