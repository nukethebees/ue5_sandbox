use super::*;
fn capture(side: &str, repetition: u32, median: f64) -> Capture {
    Capture {
        run_id: format!("{side}{repetition}"),
        directory: String::new(),
        repetition: Repetition {
            sequence: repetition,
            repetition,
            side: side.into(),
            warmup: false,
        },
        conditions: Conditions::new(),
        schema_version: 1,
        metrics: vec![Metric {
            identity: Identity {
                metric: "time".into(),
                unit: "us".into(),
                dimensions: Conditions::new(),
            },
            summary: Summary::across([median]).unwrap(),
        }],
    }
}
#[test]
fn paired_deltas_are_not_the_difference_of_independent_medians() {
    let captures = vec![
        capture("baseline", 1, 1.0),
        capture("candidate", 1, 2.0),
        capture("baseline", 2, 100.0),
        capture("candidate", 2, 90.0),
        capture("baseline", 3, 3.0),
        capture("candidate", 3, 4.0),
    ];
    let result = compare(&captures).unwrap();
    assert!(result.comparable);
    assert_eq!(result.metrics[0].delta.median, 1.0);
    assert_eq!(result.metrics[0].delta.samples, 3);
    assert_eq!(Summary::across([1.0, 4.0, 2.0, 3.0]).unwrap().median, 2.5);
    assert_eq!(
        Summary::across((1..=20).map(|v| v as f64)).unwrap().p95,
        19.0
    );
}
#[test]
fn zero_baselines_and_missing_or_mismatched_pairs() {
    let mut captures = vec![capture("baseline", 1, 0.0), capture("candidate", 1, 1.0)];
    assert!(
        compare(&captures).unwrap().metrics[0]
            .delta_percent
            .is_none()
    );
    captures[1].metrics[0].identity.unit = "ms".into();
    assert!(!compare(&captures).unwrap().comparable);
    assert!(compare(&captures[..1]).is_err());
    assert!(Summary::across([f64::NAN]).is_err());
}
