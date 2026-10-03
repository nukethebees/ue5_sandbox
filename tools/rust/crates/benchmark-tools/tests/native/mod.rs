use super::*;
use serde_json::json;
#[test]
fn fighter_caps_are_unique_positive_u32() {
    assert_eq!(parse_fighter_caps("2000, 4000").unwrap(), [2000, 4000]);
    for value in ["", "1,1", "0", "-1", "4294967296"] {
        assert!(parse_fighter_caps(value).is_err());
    }
}
#[test]
fn frame_validation_rejects_incomplete_or_empty_workloads() {
    let mut result = json!({"level":{"id":"batch-benchmark"},"workload":{"requested_ticks":1200,"completed_ticks":1200,"advance_calls":1200,"game_speed":100},"memory":{"frame_overflow_count":0,"frame_peak_claimed_bytes":128},"final_state":{"peak_fighters":100}});
    validate_frame_memory_result(&result).unwrap();
    result["workload"]["completed_ticks"] = json!(1199);
    assert!(validate_frame_memory_result(&result).is_err());
    assert!(validate_frame_memory_result(&json!({})).is_err());
}
#[test]
fn fighter_validation_requires_saturation_and_timing() {
    let mut result = json!({"level":{"id":"fighter-scheduling-benchmark"},"workload":{"measured_ticks":600},
        "fighter_stress":{"enabled":true,"configured_cap":2000,"steady_state_fighters":2000,"minimum_measured_fighters":2000,"maximum_measured_fighters":2000,"fighter_spawns_during_measurement":0,"task_counts":{"attack":2000},"lasers_spawned_during_measurement":20},
        "memory":{"frame_overflow_count":0},"timing":{"elapsed_seconds":10,"mean_tick_microseconds":1,"median_tick_microseconds":1,"p95_tick_microseconds":1,"p99_tick_microseconds":1,"ticks_per_second":60,"realtime_factor":1}});
    validate_fighter_result(&result, 2000, 600).unwrap();
    result["fighter_stress"]["minimum_measured_fighters"] = json!(1999);
    assert!(validate_fighter_result(&result, 2000, 600).is_err());
    assert!(validate_fighter_result(&json!({}), 2000, 600).is_err());
}
