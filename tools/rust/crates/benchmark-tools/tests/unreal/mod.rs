use super::*;
use clap::Parser;

#[test]
fn moved_automation_keeps_defaults_and_failure_detection() {
    let benchmark = Benchmark::Spark(SparkOptions::try_parse_from(["spark"]).unwrap());
    let (spark, automation) = measurement_arguments(&benchmark);
    assert_eq!(benchmark.editor().timeout_seconds, 1200);
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
    let benchmark =
        Benchmark::Telemetry(TelemetryOptions::try_parse_from(["level-telemetry"]).unwrap());
    let (telemetry, _) = measurement_arguments(&benchmark);
    assert!(telemetry.contains(&"-SandboxTelemetryBenchmarkSamples=7".into()));
    for log in [
        "Found 0 automation tests based on x",
        "Test Completed. Result={Fail}",
        "Test Completed. Result={Error}",
        "TEST COMPLETE. EXIT CODE: -1",
    ] {
        assert!(validate_automation_log(log).is_err(), "{log}");
    }
    validate_automation_log("TEST COMPLETE. EXIT CODE: 0").unwrap();
}

#[test]
fn engine_failure_and_timeout_preserve_logs() {
    let directory = ioj_test_support::temp_dir("benchmark engine ");
    let log = directory.path().join("engine.log");
    let mut command = Command::new("python");
    command.args([
        "-c",
        "print('failed engine', flush=True); raise SystemExit(23)",
    ]);
    let error = require_process_success(
        run_editor_with_timeout(&mut command, &log, Instant::now() + Duration::from_secs(10))
            .unwrap(),
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
        run_editor_with_timeout(&mut command, &log, Instant::now() + Duration::from_secs(1))
            .unwrap_err();
    assert!(error.to_string().contains("timed out"));
    assert!(fs::read_to_string(log).unwrap().contains("hung engine"));
}
