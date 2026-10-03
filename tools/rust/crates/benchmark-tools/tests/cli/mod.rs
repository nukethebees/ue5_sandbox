use super::*;
use clap::{CommandFactory, error::ErrorKind};

#[test]
fn command_definitions_are_consistent_and_every_command_has_help() {
    Cli::command().debug_assert();
    for name in [
        "kernel-report",
        "native-simulation",
        "fighter-simulation",
        "frame-memory-level",
        "frame-memory-revision-ab",
        "compare",
        "tracy-report",
        "plot",
        "gpu-starfield",
        "sandbox-ismc",
        "sandbox-ismc-revision-ab",
        "sandbox-ismc-report",
        "spark",
        "level-telemetry",
        "heatmap",
        "radar-3d",
        "scatter-3d",
        "volume-heatmap-3d",
        "entity-overlay",
    ] {
        let error = Cli::try_parse_from(["benchmark-tools", name, "--help"])
            .err()
            .unwrap();
        assert_eq!(error.kind(), ErrorKind::DisplayHelp, "{name}: {error}");
    }
}

#[test]
fn rejects_unknown_duplicate_missing_and_invalid_options() {
    for arguments in [
        vec!["kernel-report", "--unknown=1"],
        vec!["kernel-report", "--min-time"],
        vec!["kernel-report", "--repetitions=1", "--repetitions=2"],
        vec!["kernel-report", "--skip-build", "--skip-build"],
        vec!["kernel-report", "--min-time=NaN"],
        vec!["kernel-report", "--min-time=inf"],
        vec!["kernel-report", "--min-time=-1"],
        vec!["kernel-report", "--min-time=181"],
        vec!["kernel-report", "--repetitions=0"],
        vec!["kernel-report", "--workload=unknown"],
        vec!["plot", "--kind=other", "--input=x", "--output-dir=y"],
        vec!["plot", "--kind=kernel"],
        vec!["compare"],
        vec!["tracy-report", "--trace=x", "--sort=other"],
        vec!["spark", "--capacity=0"],
        vec!["sandbox-ismc", "--shadows=2"],
    ] {
        assert!(
            Cli::try_parse_from(std::iter::once("benchmark-tools").chain(arguments.clone()))
                .is_err(),
            "{arguments:?}"
        );
    }
}

#[test]
fn conflicting_and_dependent_options_are_validated_by_clap() {
    for args in [
        vec!["compare", "--baseline=dev", "--order=AB", "--repetitions=1"],
        vec!["compare", "--baseline=dev", "--candidate-worktree=x"],
        vec!["compare", "--baseline=dev", "--skip-build"],
        vec![
            "compare",
            "--baseline=dev",
            "--prepare-only",
            "--skip-build",
            "--baseline-worktree=x",
        ],
        vec![
            "frame-memory-revision-ab",
            "--prepare-only",
            "--validate-only",
        ],
        vec![
            "native-simulation",
            "--level=x",
            "--seconds=1",
            "--fighter-stress-cap=10",
            "--fighter-stress-caps=10,20",
        ],
        vec![
            "sandbox-ismc-revision-ab",
            "--prepare-only",
            "--validate-only",
        ],
    ] {
        assert!(
            Cli::try_parse_from(std::iter::once("benchmark-tools").chain(args.clone())).is_err(),
            "{args:?}"
        );
    }
}

#[test]
fn defaults_and_closed_choices_are_typed() {
    let Command::KernelReport(kernel) = Cli::try_parse_from(["benchmark-tools", "kernel-report"])
        .unwrap()
        .command
    else {
        panic!()
    };
    assert_eq!(kernel.workload, KernelWorkload::Representative);
    assert!(kernel.repetitions.is_none());
    let Command::FighterSimulation(fighter) =
        Cli::try_parse_from(["benchmark-tools", "fighter-simulation"])
            .unwrap()
            .command
    else {
        panic!()
    };
    assert_eq!(fighter.fighter_caps, "2000,4000");
    assert_eq!(fighter.seconds, 10.0);
    let Command::SandboxIsmc(ismc) = Cli::try_parse_from([
        "benchmark-tools",
        "sandbox-ismc",
        "--mode=engine_ismc",
        "--trace=0",
        "--shadows=1",
    ])
    .unwrap()
    .command
    else {
        panic!()
    };
    assert_eq!(ismc.mode.as_str(), "engine_ismc");
    assert!(!ismc.trace);
    assert!(ismc.shadows);
    let Command::GpuStarfield(gpu) = Cli::try_parse_from([
        "benchmark-tools",
        "gpu-starfield",
        "--camera-modes=moving,stationary",
    ])
    .unwrap()
    .command
    else {
        panic!()
    };
    assert_eq!(
        gpu.camera_modes,
        [CameraMode::Moving, CameraMode::Stationary]
    );
}
