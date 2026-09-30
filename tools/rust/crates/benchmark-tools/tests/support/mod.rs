use super::*;
#[test]
fn arguments_reject_unknown_duplicate_missing_and_invalid_numbers() {
    for args in [
        vec!["--unknown", "1"],
        vec!["--size"],
        vec!["--size", "1", "--size=2"],
        vec!["--flag", "--flag"],
    ] {
        assert!(
            Args::parse(
                &args.into_iter().map(str::to_owned).collect::<Vec<_>>(),
                &["--size"],
                &["--flag"]
            )
            .is_err()
        );
    }
    for value in ["NaN", "inf", "-1", "101"] {
        let args = Args::parse(&[format!("--size={value}")], &["--size"], &[]).unwrap();
        assert!(args.float("--size", 1.0, 0.0, 100.0).is_err());
    }
}
#[test]
fn csv_quotes_and_json_noise() {
    assert_eq!(
        parse_csv("one,\"two,three\",\"say \"\"hello\"\"\"").unwrap(),
        ["one", "two,three", "say \"hello\""]
    );
    assert!(parse_csv("\"unclosed").is_err());
    assert_eq!(json_lines("noise\n {\"ok\":1}\nmore").unwrap().len(), 1);
    assert!(json_lines("{broken").is_err());
}
