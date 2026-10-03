use super::*;
#[test]
fn workload_validation_and_editor_arguments() {
    let temp = ioj_test_support::temp_dir("benchmark ismc ");
    let editor = temp.path().join("UnrealEditor-Cmd.exe");
    fs::write(&editor, "").unwrap();
    let args = Args::parse_command_line(
        &[
            "--editor".into(),
            editor.to_string_lossy().into_owned(),
            "--instances=10".into(),
        ],
        &["--editor", "--instances"],
        &[],
    )
    .unwrap();
    let request = Request::parse_ismc_options(&args, temp.path()).unwrap();
    assert_eq!(request.min_instances, 10);
    let run = Run::new(temp.path(), "test", temp.path(), json!({}), "", false).unwrap();
    let args = request.build_measurement_editor_arguments(temp.path(), &run);
    assert!(args.contains(&format!("-SandboxISMCBenchmarkRunId={}", run.id())));
    assert!(args.contains(&"-SandboxISMCBenchmarkWidth=1280".into()));
    assert!(args.contains(&"-SandboxISMCBenchmarkTrace=1".into()));
    assert_eq!(
        request.build_comparison_conditions()["visibility"],
        "all_visible"
    );
    assert_eq!(
        request.build_comparison_conditions()["custom_data"],
        "no_custom_data"
    );
    let args = Args::parse_command_line(
        &[
            "--editor".into(),
            editor.to_string_lossy().into_owned(),
            "--width=0".into(),
        ],
        &["--editor", "--width"],
        &[],
    )
    .unwrap();
    assert!(Request::parse_ismc_options(&args, temp.path()).is_err());
}
