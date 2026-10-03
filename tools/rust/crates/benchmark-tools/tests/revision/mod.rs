use super::*;
#[test]
fn balanced_order_preserves_pairs_and_excludes_warmups_from_numbering() {
    let sequence = balanced_repetitions(3, 1);
    assert_eq!(
        sequence.iter().map(|r| r.side.as_str()).collect::<Vec<_>>(),
        [
            "baseline",
            "candidate",
            "baseline",
            "candidate",
            "candidate",
            "baseline",
            "baseline",
            "candidate"
        ]
    );
    assert!(sequence[..2].iter().all(|r| r.warmup));
    assert_eq!(sequence[2].repetition, 1);
    assert_eq!(sequence[7].sequence, 8);
}
#[test]
fn owned_paths_cannot_escape_or_equal_the_parent() {
    let temp = ioj_test_support::temp_dir("benchmark paths ");
    assert!(validate_owned_path(&temp.path().join("wt/0"), &temp.path().join("wt")).is_ok());
    assert!(validate_owned_path(temp.path(), temp.path()).is_err());
    assert!(
        validate_owned_path(&temp.path().join("wt/../outside"), &temp.path().join("wt")).is_err()
    );
}
#[test]
fn manifest_requires_nonempty_expected_artifacts_and_keeps_failure() {
    let temp = ioj_test_support::temp_dir("benchmark manifest ");
    let mut run = Run::new(temp.path(), "test", temp.path(), json!({}), "", true).unwrap();
    run.expect_artifact("result.json").unwrap();
    assert!(run.validate_artifacts().is_err());
    write_text(&run.path("result.json"), "{}").unwrap();
    run.validate_artifacts().unwrap();
    assert!(
        run.finish_run::<()>(Err("failed measurement".into()))
            .is_err()
    );
    let manifest: Value =
        serde_json::from_slice(&fs::read(run.path("manifest.json")).unwrap()).unwrap();
    assert_eq!(manifest["status"], "failed");
    assert_eq!(manifest["failure"], "failed measurement");
}
