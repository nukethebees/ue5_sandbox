use crate::{
    ismc::{self, Plan, Request},
    results::*,
    revision::*,
    support::*,
};
use serde_json::json;
use std::{fs, path::Path};

fn repository_fixture() -> ioj_test_support::TempDir {
    let directory = ioj_test_support::temp_dir("benchmark source ");
    let root = directory.path();
    run_git_capture(root, &["init"]).unwrap();
    run_git_capture(root, &["config", "user.name", "Benchmark test"]).unwrap();
    run_git_capture(root, &["config", "user.email", "benchmark@example.invalid"]).unwrap();
    fs::write(root.join(".gitignore"), ".local/\nout/\n").unwrap();
    fs::write(root.join("CMakeLists.txt"), "# fixture\n").unwrap();
    fs::write(root.join("source.txt"), "original").unwrap();
    run_git_capture(root, &["add", "."]).unwrap();
    run_git_capture(root, &["commit", "-m", "fixture"]).unwrap();
    directory
}

#[test]
fn fingerprint_tracks_dirty_and_untracked_files_but_excludes_artifacts() {
    let directory = repository_fixture();
    let root = directory.path();
    let artifacts = root.join("results");
    fs::create_dir(&artifacts).unwrap();
    fs::write(artifacts.join("capture.json"), "one").unwrap();
    let clean = capture_source_identity(root, Some(&artifacts)).unwrap();
    assert!(!clean.dirty);
    fs::write(artifacts.join("capture.json"), "two").unwrap();
    verify_source_unchanged(&clean).unwrap();
    fs::write(root.join("source.txt"), "edited").unwrap();
    assert!(verify_source_unchanged(&clean).is_err());
    let dirty = capture_source_identity(root, Some(&artifacts)).unwrap();
    assert!(dirty.dirty);
    fs::write(root.join("untracked.txt"), "one").unwrap();
    let untracked = capture_source_identity(root, Some(&artifacts)).unwrap();
    assert_ne!(dirty.untracked_sha256, untracked.untracked_sha256);
    fs::write(root.join("untracked.txt"), "two").unwrap();
    assert!(verify_source_unchanged(&untracked).is_err());
}

#[test]
fn owned_baselines_are_detached_and_cleaned_and_supplied_ones_retained() {
    let directory = repository_fixture();
    let root = directory.path();
    let revisions =
        Revisions::new(root, "HEAD", None, false, &root.join(".local/results")).unwrap();
    let baseline = revisions.baseline.root.clone();
    assert!(baseline.join(".git").is_file());
    assert!(
        run_git_capture(&baseline, &["branch", "--show-current"])
            .unwrap()
            .trim()
            .is_empty()
    );
    revisions.cleanup_worktrees(Ok(())).unwrap();
    assert!(!baseline.exists());

    let revisions = Revisions::new(root, "HEAD", None, true, &root.join(".local/results")).unwrap();
    let baseline = revisions.baseline.root.clone();
    revisions.cleanup_worktrees(Ok(())).unwrap();
    let supplied = Revisions::new(
        root,
        "HEAD",
        Some(&baseline.to_string_lossy()),
        false,
        &root.join(".local/results"),
    )
    .unwrap();
    assert!(!supplied.owned);
    assert!(
        supplied
            .cleanup_worktrees::<()>(Err("measurement failed".into()))
            .is_err()
    );
    assert!(baseline.exists());
    fs::write(baseline.join("source.txt"), "dirty").unwrap();
    assert!(
        Revisions::new(
            root,
            "HEAD",
            Some(&baseline.to_string_lossy()),
            false,
            &root.join(".local/results")
        )
        .is_err()
    );
    assert_eq!(
        fs::read_to_string(root.join("source.txt")).unwrap(),
        "original"
    );
    run_git_capture(
        root,
        &[
            "worktree",
            "remove",
            "--force",
            "--force",
            &baseline.to_string_lossy(),
        ],
    )
    .unwrap();
}

fn workload(root: &Path) -> Request {
    serde_json::from_value(json!({"editor":root.join("unused-editor.exe"),"width":1280,"height":720,"instances":40000,
        "updatePercent":100,"mode":"paired","visibility":"all","bounds":"calculated","customData":"none","shadows":false,
        "churn":false,"minInstances":1000,"halfCycleUpdates":120,"replacementPercent":5,"warmupUpdates":0,"warmupSeconds":1,
        "seconds":5,"trace":false,"cacheDirectory":root.join("cache")})).unwrap()
}

fn conditions(request: &Request) -> Conditions {
    let mut conditions = request.build_comparison_conditions();
    for &key in CONDITIONS {
        conditions.entry(key.into()).or_insert("1".into());
    }
    conditions.insert("rhi".into(), "D3D12".into());
    conditions
}

#[test]
fn csv_and_offline_report_enforce_comparability_and_sequence() {
    let directory = ioj_test_support::temp_dir("benchmark report ");
    let root = directory.path();
    let request = workload(root);
    let conditions = conditions(&request);
    validate_ismc_conditions(&conditions).unwrap();
    validate_ismc_request(&conditions, &request).unwrap();
    let csv = root.join("metrics.csv");
    let mut header = vec![
        "renderer",
        "metric",
        "unit",
        "samples",
        "min",
        "median",
        "p95",
        "max",
        "updated_instances",
    ]
    .into_iter()
    .map(str::to_owned)
    .collect::<Vec<_>>();
    header.extend(conditions.keys().cloned());
    let mut row = vec!["custom", "upload", "us", "10", "1", "2", "3", "4", "40000"]
        .into_iter()
        .map(str::to_owned)
        .collect::<Vec<_>>();
    row.extend(conditions.values().cloned());
    write_csv(&csv, &[header.clone(), row.clone()]).unwrap();
    let metrics = read_ismc_metrics(&csv, &conditions).unwrap();
    assert_eq!(metrics[0].summary.median, 2.0);
    let mut mismatched = conditions.clone();
    mismatched.insert("instances".into(), "100".into());
    assert!(read_ismc_metrics(&csv, &mismatched).is_err());
    write_csv(&csv, &[header.clone(), row.clone(), row.clone()]).unwrap();
    assert!(read_ismc_metrics(&csv, &conditions).is_err());
    row[3] = "0".into();
    write_csv(&csv, &[header, row]).unwrap();
    assert!(read_ismc_metrics(&csv, &conditions).is_err());

    let source = Source {
        root: root.into(),
        commit: "a".repeat(40),
        dirty: false,
        status: String::new(),
        diff_sha256: String::new(),
        untracked_sha256: String::new(),
        artifact_root: None,
    };
    let sequence = balanced_repetitions(2, 1);
    let plan = Plan {
        output: root.into(),
        candidate: source.clone(),
        baseline: source,
        sequence: sequence.clone(),
        ismc: Some(request),
        validation_only: false,
    };
    let manifest = json!({"schemaVersion":1,"benchmark":"sandbox-ismc-revision-ab","status":"complete","runId":"comparison","label":"<test>|label"});
    let mut captures = Vec::new();
    for repetition in sequence {
        let mut metrics = metrics.clone();
        let value = if repetition.side == "baseline" {
            2.0
        } else {
            3.0
        };
        metrics[0].summary = Summary::from_samples([value]).unwrap();
        captures.push(Capture {
            run_id: format!("run{}", repetition.sequence),
            directory: root.display().to_string(),
            repetition,
            conditions: conditions.clone(),
            metrics,
            schema_version: 1,
        });
    }
    for (name, value) in [
        ("manifest.json", manifest),
        ("measurement-plan.json", json!(plan)),
        ("sequence.json", json!(plan.sequence)),
        ("captures.json", json!(captures)),
    ] {
        write_json(&root.join(name), &value).unwrap();
    }
    let args = crate::cli::IsmcReportOptions {
        run_dir: root.to_owned(),
    };
    ismc::regenerate_comparison_reports(&args).unwrap();
    let output: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("comparison.json")).unwrap()).unwrap();
    assert_eq!(output["schemaVersion"], 2);
    assert_eq!(output["metrics"][0]["delta"]["samples"], 2);
    assert_eq!(output["metrics"][0]["delta"]["median"], 1.0);
    assert!(
        fs::read_to_string(root.join("comparison.md"))
            .unwrap()
            .contains("&lt;test&gt;\\|label")
    );
    captures
        .last_mut()
        .unwrap()
        .conditions
        .insert("rhi".into(), "different".into());
    write_json(&root.join("captures.json"), &captures).unwrap();
    assert!(ismc::regenerate_comparison_reports(&args).is_err());
    let output: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("comparison.json")).unwrap()).unwrap();
    assert_eq!(output["comparable"], false);
    assert!(output["metrics"].as_array().unwrap().is_empty());
    captures.pop();
    write_json(&root.join("captures.json"), &captures).unwrap();
    assert!(ismc::regenerate_comparison_reports(&args).is_err());
}

#[cfg(windows)]
#[test]
fn subprocess_failure_preserves_exit_code_and_captures_both_streams() {
    let directory = ioj_test_support::temp_dir("benchmark subprocess ");
    let log = directory.path().join("nested/process.log");
    let output = run_process_with_log(
        std::process::Command::new("cmd.exe").args([
            "/d",
            "/c",
            "echo output&echo diagnostic 1>&2&exit /b 17",
        ]),
        &log,
    )
    .unwrap();
    assert!(String::from_utf8_lossy(&output.stdout).contains("output"));
    assert!(String::from_utf8_lossy(&output.stderr).contains("diagnostic"));
    assert_eq!(
        fs::read_to_string(log).unwrap(),
        format!(
            "{}{}",
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )
    );
    let error = require_process_success(output).unwrap_err();
    assert_eq!(error.downcast_ref::<ProcessFailure>().unwrap().0, 17);
}
