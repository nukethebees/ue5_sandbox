use super::*;
use clap::Parser;
#[test]
fn gpu_capture_trims_frames_and_checks_resolution() {
    let temp = ioj_test_support::temp_dir("benchmark gpu ");
    let path = temp.path().join("capture.csv");
    fs::write(&path,"tag,width,GameThreadTime,GPUTime\n[systemresolution.resx],1280,1,2\n[systemresolution.resy],720,2,3\nframe,,3,4\nframe,,4,5\nframe,,5,6\n").unwrap();
    let config = Configuration {
        width: 1280,
        height: 720,
        size_multiplier: 1.0,
    };
    let capture = read_starfield_capture(&path, &config, 100, 1, false, false, 1).unwrap();
    assert_eq!(capture.frame_count, 5);
    assert_eq!(capture.medians["game_thread_ms"], 3.0);
    assert!(
        read_starfield_capture(
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
    let options = GpuOptions::try_parse_from(std::iter::once("gpu-starfield").chain(args)).unwrap();
    let request = Request::parse_starfield_options(&root, &options).unwrap();
    assert_eq!(request.configurations.len(), 4);
    assert_eq!(request.counts, [10, 20]);
    let command =
        request.build_starfield_arguments(&request.configurations[0], Path::new("raw output"));
    assert!(command.contains(&"-GpuStarfieldBenchmarkCameraModes=stationary,moving".into()));
    assert!(command.contains(&"-GpuStarfieldBenchmarkOutput=raw output".into()));
    for bad in [
        "--counts=10,10",
        "--resolutions=bad",
        "--size-multipliers=101",
        "--camera-modes=orbiting",
    ] {
        let result = GpuOptions::try_parse_from([
            "gpu-starfield",
            "--editor=e",
            "--project=p",
            "--output=o",
            bad,
        ])
        .map_err(|error| error.to_string())
        .and_then(|options| {
            Request::parse_starfield_options(&root, &options).map_err(|error| error.to_string())
        });
        assert!(result.is_err());
    }
}
