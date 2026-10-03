use super::*;
use clap::Parser;
#[test]
fn workload_validation_and_editor_arguments() {
    let temp = ioj_test_support::temp_dir("benchmark ismc ");
    let editor = temp.path().join("UnrealEditor-Cmd.exe");
    fs::write(&editor, "").unwrap();
    let args = IsmcOptions::try_parse_from([
        "sandbox-ismc",
        "--editor",
        editor.to_str().unwrap(),
        "--instances=10",
    ])
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
    assert!(
        IsmcOptions::try_parse_from([
            "sandbox-ismc",
            "--editor",
            editor.to_str().unwrap(),
            "--width=0"
        ])
        .is_err()
    );
}
