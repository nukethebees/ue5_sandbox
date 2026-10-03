use super::*;
use serde_json::json;

fn entry(time: f64) -> Value {
    json!({"run_name":"add_scaled/elementwise/flat/scalar/ordinary/aligned/32/real_time", "real_time": time, "time_unit":"ns"})
}

#[test]
fn raw_repetitions_override_aggregates_and_use_sample_deviation() {
    let mut aggregate = entry(900.0);
    aggregate["run_type"] = json!("aggregate");
    aggregate["aggregate_name"] = json!("median");
    let points = load(&json!({"benchmarks":[entry(3.0),entry(1.0),entry(2.0),aggregate]})).unwrap();
    assert_eq!(points.len(), 1);
    assert_eq!(points[0].time, 2.0);
    assert_eq!(points[0].deviation, 1.0);
}

#[test]
fn aggregate_units_mean_fallback_and_zero_deviation() {
    for (unit, scale) in [("ns", 1.0), ("us", 1e3), ("ms", 1e6), ("s", 1e9)] {
        let mut mean = entry(2.0);
        mean["run_type"] = json!("aggregate");
        mean["aggregate_name"] = json!("mean");
        mean["time_unit"] = json!(unit);
        let mut stddev = mean.clone();
        stddev["aggregate_name"] = json!("stddev");
        stddev["real_time"] = json!(0.0);
        let points = load(&json!({"benchmarks":[mean,stddev]})).unwrap();
        assert_eq!(points[0].time, 2.0 * scale);
        assert_eq!(points[0].deviation, 0.0);
    }
}

#[test]
fn failed_backends_do_not_discard_successful_measurements() {
    let failed = json!({"run_name":"unavailable backend", "error_occurred":true, "error_message":"AVX-512 unavailable"});
    assert_eq!(
        load(&json!({"benchmarks":[failed,entry(2.0)]}))
            .unwrap()
            .len(),
        1
    );
    assert!(load(&json!({"benchmarks":[failed]})).is_err());
}

#[test]
fn rejects_malformed_measurements() {
    for (field, value) in [
        ("run_name", json!("bad/name")),
        ("time_unit", json!("minutes")),
        ("real_time", json!(0)),
        ("real_time", json!(-1)),
        ("real_time", json!(true)),
        ("run_type", json!("unknown")),
    ] {
        let mut bad = entry(2.0);
        bad[field] = value;
        assert!(load(&json!({"benchmarks":[bad]})).is_err(), "{field}");
    }
    assert!(load(&json!({})).is_err());
    assert!(parse_key("add_scaled/elementwise/flat/scalar/ordinary/aligned/0/real_time").is_err());
}

#[test]
fn alignment_ratios_match_backend_and_count() {
    let mut aligned = entry(2.0);
    let mut unaligned = entry(6.0);
    unaligned["run_name"] =
        json!("add_scaled/elementwise/flat/scalar/ordinary/unaligned/32/real_time");
    aligned["run_name"] = json!("add_scaled/elementwise/flat/avx2/ordinary/aligned/32/real_time");
    let measurements = load(&json!({"benchmarks":[entry(2.0),aligned,unaligned]})).unwrap();
    let points = measurements.iter().collect::<Vec<_>>();
    let ratios = ratios(
        &points,
        |k| k.alignment == "aligned",
        |k| k.alignment == "unaligned",
        |k| k.backend.clone(),
        true,
    );
    assert_eq!(ratios.len(), 1);
    assert_eq!(ratios[0].label, "scalar");
    assert_eq!(ratios[0].points[0].y, 3.0);
}
