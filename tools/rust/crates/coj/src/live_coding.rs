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
    // Check UTF-32 first to disambiguate the shared UTF-16 prefix.
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

        // Preserve each line's original terminator.
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

    // Replace only the value; retain surrounding whitespace and comments.
    let updated_line = format!("{}False{}", &line[..value_start], &line[value_end..]);
    (updated_line != line).then_some(updated_line)
}

#[cfg(test)]
#[path = "../tests/live_coding/mod.rs"]
mod tests;
