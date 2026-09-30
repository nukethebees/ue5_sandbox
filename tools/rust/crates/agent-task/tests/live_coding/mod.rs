use super::disable;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

static NEXT_DIRECTORY: AtomicUsize = AtomicUsize::new(0);

struct TemporaryDirectory {
    path: PathBuf,
}

impl TemporaryDirectory {
    fn new() -> Self {
        let sequence = NEXT_DIRECTORY.fetch_add(1, Ordering::Relaxed);
        let path = std::env::temp_dir().join(format!(
            "agent-task-live-coding-{}-{sequence}",
            std::process::id()
        ));
        fs::create_dir(&path).expect("temporary directory should be created");
        Self { path }
    }

    fn path(&self) -> &Path {
        &self.path
    }
}

impl Drop for TemporaryDirectory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.path);
    }
}

fn write_utf8(path: &Path, contents: &str) {
    fs::write(path, contents.as_bytes()).expect("input should be written");
}

#[test]
fn changes_live_coding_value_and_preserves_unrelated_content() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("EditorPerProjectUserSettings.ini");
    let input = "[Other]\r\nbEnabled=True\r\n\r\n[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled = True ; keep\r\nbPreloadProjectModules=True\r\n\r\n[After]\r\nValue=Keep\r\n";
    let expected = "[Other]\r\nbEnabled=True\r\n\r\n[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled = False ; keep\r\nbPreloadProjectModules=True\r\n\r\n[After]\r\nValue=Keep\r\n";
    write_utf8(&settings_path, input);

    assert!(disable(&settings_path).expect("disable should succeed"));
    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        expected
    );
}

#[test]
fn preserves_trailing_comments_and_content() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True#keep\nOther=Value\n",
    );

    disable(&settings_path).expect("disable should succeed");

    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False#keep\nOther=Value\n"
    );
}

#[test]
fn does_not_rewrite_already_disabled_file() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n[Other]\nbEnabled=True\n",
    );
    let before = fs::read(&settings_path).expect("input should be read");

    assert!(!disable(&settings_path).expect("disable should succeed"));
    assert_eq!(
        fs::read(settings_path).expect("output should be read"),
        before
    );
}

#[test]
fn preserves_utf16_little_endian_bom_and_non_ascii_text() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    let input = "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=True\r\nName=Ångström\r\n";
    let expected = "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=False\r\nName=Ångström\r\n";

    let mut bytes = vec![0xff, 0xfe];

    for unit in input.encode_utf16() {
        bytes.extend_from_slice(&unit.to_le_bytes());
    }

    fs::write(&settings_path, bytes).expect("input should be written");

    disable(&settings_path).expect("disable should succeed");

    let bytes = fs::read(&settings_path).expect("output should be read");
    assert!(bytes.starts_with(&[0xff, 0xfe]));
    let code_units = bytes[2..]
        .chunks_exact(2)
        .map(|pair| u16::from_le_bytes([pair[0], pair[1]]))
        .collect::<Vec<_>>();
    assert_eq!(
        String::from_utf16(&code_units).expect("output should decode"),
        expected
    );
}

#[test]
fn preserves_utf8_bom() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    fs::write(
        &settings_path,
        b"\xef\xbb\xbf[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n",
    )
    .expect("input should be written");

    disable(&settings_path).expect("disable should succeed");

    let bytes = fs::read(&settings_path).expect("output should be read");
    assert!(bytes.starts_with(&[0xef, 0xbb, 0xbf]));
    assert_eq!(
        std::str::from_utf8(&bytes[3..]).expect("output should decode"),
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n"
    );
}

#[test]
fn preserves_utf32_little_endian_bom() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    let input = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\nName=星\n";
    let expected = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\nName=星\n";

    let mut bytes = vec![0xff, 0xfe, 0x00, 0x00];

    for character in input.chars() {
        bytes.extend_from_slice(&u32::from(character).to_le_bytes());
    }

    fs::write(&settings_path, bytes).expect("input should be written");

    disable(&settings_path).expect("disable should succeed");

    let bytes = fs::read(&settings_path).expect("output should be read");
    assert!(bytes.starts_with(&[0xff, 0xfe, 0x00, 0x00]));
    let output = bytes[4..]
        .chunks_exact(4)
        .map(|quad| u32::from_le_bytes([quad[0], quad[1], quad[2], quad[3]]))
        .map(|code_point| char::from_u32(code_point).expect("output should decode"))
        .collect::<String>();
    assert_eq!(output, expected);
}

#[test]
fn does_not_create_missing_settings_file() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Missing.ini");

    assert!(!disable(&settings_path).expect("disable should succeed"));
    assert!(!settings_path.exists());
}

#[test]
fn preserves_mixed_line_endings() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[Other]\r\nbEnabled=True\n[/script/livecoding.livecodingsettings]\rbEnabled = tRuE ; keep\r\n[After]\nValue=Keep",
    );

    disable(&settings_path).expect("disable should succeed");

    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        "[Other]\r\nbEnabled=True\n[/script/livecoding.livecodingsettings]\rbEnabled = False ; keep\r\n[After]\nValue=Keep"
    );
}

#[test]
fn changes_case_insensitive_target_section_and_key() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[/SCRIPT/LIVECODING.LIVECODINGSETTINGS]\nBENABLED=True\n",
    );

    disable(&settings_path).expect("disable should succeed");

    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        "[/SCRIPT/LIVECODING.LIVECODINGSETTINGS]\nBENABLED=False\n"
    );
}

#[test]
fn does_not_change_similarly_named_keys() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabledExtra=True\nnotbEnabled=True\nbEnabled=True\n",
    );

    disable(&settings_path).expect("disable should succeed");

    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabledExtra=True\nnotbEnabled=True\nbEnabled=False\n"
    );
}

#[test]
fn does_not_rewrite_target_section_without_enabled() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[/Script/LiveCoding.LiveCodingSettings]\nOther=Value\n",
    );
    let before = fs::read(&settings_path).expect("input should be read");

    assert!(!disable(&settings_path).expect("disable should succeed"));
    assert_eq!(
        fs::read(settings_path).expect("output should be read"),
        before
    );
}

#[test]
fn changes_enabled_in_each_target_section_only() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path().join("Settings.ini");
    write_utf8(
        &settings_path,
        "[First]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n[Second]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n",
    );

    disable(&settings_path).expect("disable should succeed");

    assert_eq!(
        fs::read_to_string(settings_path).expect("output should be read"),
        "[First]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n[Second]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n"
    );
}
