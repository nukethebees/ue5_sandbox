use super::*;

#[test]
fn moved_automation_keeps_defaults_and_failure_detection() {
    let args = Args::parse(&[], &[], &[]).unwrap();
    let (spark, timeout, automation) = measurement("spark", &args).unwrap();
    assert_eq!(timeout, 1200);
    assert!(automation);
    for argument in [
        "-SandboxSparkBenchmarkSeconds=10",
        "-SandboxSparkBenchmarkCapacity=50000",
        "-SandboxSparkBenchmarkSparksPerHit=96",
        "-SandboxSparkBenchmarkImpactsPerFrame=100",
        "-SandboxSparkBenchmarkWarmupFrames=60",
        "-RenderOffscreen",
        "-FullStdOutLogOutput",
    ] {
        assert!(spark.iter().any(|value| value == argument));
    }
    let (telemetry, _, _) = measurement("level-telemetry", &args).unwrap();
    assert!(telemetry.contains(&"-SandboxTelemetryBenchmarkSamples=7".into()));
    for log in [
        "Found 0 automation tests based on x",
        "Test Completed. Result={Fail}",
        "Test Completed. Result={Error}",
        "TEST COMPLETE. EXIT CODE: -1",
    ] {
        assert!(automation_succeeded(log).is_err(), "{log}");
    }
    automation_succeeded("TEST COMPLETE. EXIT CODE: 0").unwrap();
}

#[test]
fn engine_failure_and_timeout_preserve_logs() {
    let directory = tempfile::tempdir().unwrap();
    let log = directory.path().join("engine.log");
    let mut command = Command::new("python");
    command.args([
        "-c",
        "print('failed engine', flush=True); raise SystemExit(23)",
    ]);
    let error = succeeded(
        run_logged(&mut command, &log, Instant::now() + Duration::from_secs(10)).unwrap(),
    )
    .unwrap_err();
    assert_eq!(error.downcast_ref::<ProcessFailure>().unwrap().0, 23);
    assert!(fs::read_to_string(&log).unwrap().contains("failed engine"));
    let mut command = Command::new("python");
    command.args([
        "-c",
        "import time; print('hung engine', flush=True); time.sleep(20)",
    ]);
    let error =
        run_logged(&mut command, &log, Instant::now() + Duration::from_secs(1)).unwrap_err();
    assert!(error.to_string().contains("timed out"));
    assert!(fs::read_to_string(log).unwrap().contains("hung engine"));
}
