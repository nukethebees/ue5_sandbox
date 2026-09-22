use std::error::Error;
use std::fmt;
use std::fs;
use std::io;
use std::path::Path;

const TARGET_SECTION: &str = "/Script/LiveCoding.LiveCodingSettings";

#[derive(Debug)]
pub enum ToolError {
    Io(io::Error),
    InvalidEncoding(&'static str),
}

impl fmt::Display for ToolError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(error) => error.fmt(formatter),
            Self::InvalidEncoding(message) => formatter.write_str(message),
        }
    }
}

impl Error for ToolError {}

impl From<io::Error> for ToolError {
    fn from(error: io::Error) -> Self {
        Self::Io(error)
    }
}

#[derive(Clone, Copy)]
enum Encoding {
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be,
    Utf32Le,
    Utf32Be,
}

pub fn disable(settings_path: &Path) -> Result<bool, ToolError> {
    if !settings_path.exists() {
        return Ok(false);
    }

    let bytes = fs::read(settings_path)?;
    let (contents, encoding) = decode(&bytes)?;
    let Some(updated_contents) = disable_in_contents(&contents) else {
        return Ok(false);
    };

    fs::write(settings_path, encode(&updated_contents, encoding))?;
    Ok(true)
}

fn decode(bytes: &[u8]) -> Result<(String, Encoding), ToolError> {
    if let Some(contents) = bytes.strip_prefix(&[0xff, 0xfe, 0x00, 0x00]) {
        return decode_utf32(contents, true).map(|contents| (contents, Encoding::Utf32Le));
    }
    if let Some(contents) = bytes.strip_prefix(&[0x00, 0x00, 0xfe, 0xff]) {
        return decode_utf32(contents, false).map(|contents| (contents, Encoding::Utf32Be));
    }
    if let Some(contents) = bytes.strip_prefix(&[0xef, 0xbb, 0xbf]) {
        return decode_utf8(contents).map(|contents| (contents, Encoding::Utf8Bom));
    }
    if let Some(contents) = bytes.strip_prefix(&[0xff, 0xfe]) {
        return decode_utf16(contents, true).map(|contents| (contents, Encoding::Utf16Le));
    }
    if let Some(contents) = bytes.strip_prefix(&[0xfe, 0xff]) {
        return decode_utf16(contents, false).map(|contents| (contents, Encoding::Utf16Be));
    }

    decode_utf8(bytes).map(|contents| (contents, Encoding::Utf8))
}

fn decode_utf8(bytes: &[u8]) -> Result<String, ToolError> {
    String::from_utf8(bytes.to_vec())
        .map_err(|_| ToolError::InvalidEncoding("Invalid UTF-8 input."))
}

fn decode_utf16(bytes: &[u8], little_endian: bool) -> Result<String, ToolError> {
    if !bytes.len().is_multiple_of(2) {
        return Err(ToolError::InvalidEncoding("Invalid UTF-16 input."));
    }

    let code_units = bytes
        .chunks_exact(2)
        .map(|pair| {
            if little_endian {
                u16::from_le_bytes([pair[0], pair[1]])
            } else {
                u16::from_be_bytes([pair[0], pair[1]])
            }
        })
        .collect::<Vec<_>>();
    String::from_utf16(&code_units).map_err(|_| ToolError::InvalidEncoding("Invalid UTF-16 input."))
}

fn decode_utf32(bytes: &[u8], little_endian: bool) -> Result<String, ToolError> {
    if !bytes.len().is_multiple_of(4) {
        return Err(ToolError::InvalidEncoding("Invalid UTF-32 input."));
    }

    bytes
        .chunks_exact(4)
        .map(|quad| {
            let code_point = if little_endian {
                u32::from_le_bytes([quad[0], quad[1], quad[2], quad[3]])
            } else {
                u32::from_be_bytes([quad[0], quad[1], quad[2], quad[3]])
            };
            char::from_u32(code_point).ok_or(ToolError::InvalidEncoding("Invalid UTF-32 input."))
        })
        .collect()
}

fn encode(contents: &str, encoding: Encoding) -> Vec<u8> {
    match encoding {
        Encoding::Utf8 => contents.as_bytes().to_vec(),
        Encoding::Utf8Bom => {
            let mut bytes = vec![0xef, 0xbb, 0xbf];
            bytes.extend_from_slice(contents.as_bytes());
            bytes
        }
        Encoding::Utf16Le => encode_utf16(contents, true),
        Encoding::Utf16Be => encode_utf16(contents, false),
        Encoding::Utf32Le => encode_utf32(contents, true),
        Encoding::Utf32Be => encode_utf32(contents, false),
    }
}

fn encode_utf16(contents: &str, little_endian: bool) -> Vec<u8> {
    let mut bytes = if little_endian {
        vec![0xff, 0xfe]
    } else {
        vec![0xfe, 0xff]
    };

    for code_unit in contents.encode_utf16() {
        let encoded = if little_endian {
            code_unit.to_le_bytes()
        } else {
            code_unit.to_be_bytes()
        };
        bytes.extend_from_slice(&encoded);
    }

    bytes
}

fn encode_utf32(contents: &str, little_endian: bool) -> Vec<u8> {
    let mut bytes = if little_endian {
        vec![0xff, 0xfe, 0x00, 0x00]
    } else {
        vec![0x00, 0x00, 0xfe, 0xff]
    };

    for character in contents.chars() {
        let encoded = if little_endian {
            u32::from(character).to_le_bytes()
        } else {
            u32::from(character).to_be_bytes()
        };
        bytes.extend_from_slice(&encoded);
    }

    bytes
}

fn disable_in_contents(contents: &str) -> Option<String> {
    let mut updated_contents = String::with_capacity(contents.len());
    let mut line_start = 0;
    let mut in_target_section = false;
    let mut changed = false;

    while line_start < contents.len() {
        let remaining = &contents[line_start..];
        let line_end = remaining
            .find(['\r', '\n'])
            .map_or(contents.len(), |index| line_start + index);
        let line = &contents[line_start..line_end];

        if let Some(is_target_section) = is_section(line) {
            in_target_section = is_target_section;
            updated_contents.push_str(line);
        } else if in_target_section {
            if let Some(updated_line) = disable_enabled_value(line) {
                updated_contents.push_str(&updated_line);
                changed = true;
            } else {
                updated_contents.push_str(line);
            }
        } else {
            updated_contents.push_str(line);
        }

        if line_end == contents.len() {
            break;
        }

        if contents[line_end..].starts_with("\r\n") {
            updated_contents.push_str("\r\n");
            line_start = line_end + 2;
        } else {
            updated_contents.push_str(&contents[line_end..line_end + 1]);
            line_start = line_end + 1;
        }
    }

    changed.then_some(updated_contents)
}

fn is_section(line: &str) -> Option<bool> {
    let trimmed = line.trim();
    if !trimmed.starts_with('[') || !trimmed.ends_with(']') {
        return None;
    }

    let section = &trimmed[1..trimmed.len() - 1];
    if section.is_empty() || section.contains(']') {
        return None;
    }

    Some(section.eq_ignore_ascii_case(TARGET_SECTION))
}

fn disable_enabled_value(line: &str) -> Option<String> {
    let without_leading_whitespace = line.trim_start_matches(char::is_whitespace);
    let key = without_leading_whitespace.get(..8)?;
    if !key.eq_ignore_ascii_case("bEnabled") {
        return None;
    }

    let after_key = without_leading_whitespace[8..].trim_start_matches(char::is_whitespace);
    let after_equals = after_key
        .strip_prefix('=')?
        .trim_start_matches(char::is_whitespace);
    let value_length = after_equals
        .find(|character: char| character == ';' || character == '#' || character.is_whitespace())
        .unwrap_or(after_equals.len());
    if value_length == 0 {
        return None;
    }

    let value_start = line.len() - after_equals.len();
    let value_end = value_start + value_length;
    let updated_line = format!("{}False{}", &line[..value_start], &line[value_end..]);
    (updated_line != line).then_some(updated_line)
}

#[cfg(test)]
mod tests {
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
                "set-live-coding-disabled-{}-{sequence}",
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
        let expected =
            "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=False\r\nName=Ångström\r\n";
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
}
