use super::*;
#[test]
fn gpu_capture_trims_frames_and_checks_resolution() {
    let temp = tempfile::tempdir().unwrap();
    let path = temp.path().join("capture.csv");
    fs::write(&path,"tag,width,GameThreadTime,GPUTime\n[systemresolution.resx],1280,1,2\n[systemresolution.resy],720,2,3\nframe,,3,4\nframe,,4,5\nframe,,5,6\n").unwrap();
    let config = Configuration {
        width: 1280,
        height: 720,
        size_multiplier: 1.0,
    };
    let capture = read_capture(&path, &config, 100, 1, false, false, 1).unwrap();
    assert_eq!(capture.frame_count, 5);
    assert_eq!(capture.medians["game_thread_ms"], 3.0);
    assert!(
        read_capture(
            &path,
            &Configuration {
                width: 640,
                ..config
            },
            100,
            1,
            false,
            false,
            1
        )
        .is_err()
    );
}
#[test]
fn workload_lists_and_unreal_arguments() {
    let root = std::env::current_dir().unwrap();
    let args = [
        "--editor=editor.exe",
        "--project=project.uproject",
        "--output=results",
        "--counts=10,20",
        "--resolutions=1280x720,1920x1080",
        "--size-multipliers=1,4",
    ];
    let request = Request::parse(&root, &args.map(str::to_owned)).unwrap();
    assert_eq!(request.configurations.len(), 4);
    assert_eq!(request.counts, [10, 20]);
    let command = request.arguments(&request.configurations[0], Path::new("raw output"));
    assert!(command.contains(&"-GpuStarfieldBenchmarkCameraModes=stationary,moving".into()));
    assert!(command.contains(&"-GpuStarfieldBenchmarkOutput=raw output".into()));
    for bad in [
        "--counts=10,10",
        "--resolutions=bad",
        "--size-multipliers=101",
        "--camera-modes=orbiting",
    ] {
        assert!(
            Request::parse(
                &root,
                &["--editor=e", "--project=p", "--output=o", bad].map(str::to_owned)
            )
            .is_err()
        );
    }
}
