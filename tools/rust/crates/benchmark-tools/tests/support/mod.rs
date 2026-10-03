use super::*;
#[test]
fn csv_quotes_and_json_noise() {
    assert_eq!(
        parse_csv_row("one,\"two,three\",\"say \"\"hello\"\"\"").unwrap(),
        ["one", "two,three", "say \"hello\""]
    );
    assert!(parse_csv_row("\"unclosed").is_err());
    assert_eq!(
        parse_json_lines("noise\n {\"ok\":1}\nmore").unwrap().len(),
        1
    );
    assert!(parse_json_lines("{broken").is_err());
}
