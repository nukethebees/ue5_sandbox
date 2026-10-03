use serde_json::Value;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

#[cfg(windows)]
mod windows;

pub const PROTOCOL_VERSION: u32 = 5;
const MAXIMUM_PAYLOAD_SIZE: usize = 1024 * 1024;

pub struct Client {
    endpoint: PathBuf,
}

impl Client {
    pub fn new(endpoint: impl Into<PathBuf>) -> Self {
        Self {
            endpoint: endpoint.into(),
        }
    }

    pub fn for_current_user() -> Result<Self, String> {
        #[cfg(windows)]
        {
            Ok(Self::new(windows::default_endpoint()?))
        }
        #[cfg(not(windows))]
        {
            Err("The jobs board requires Windows.".into())
        }
    }

    pub fn request(&self, mut message: Value) -> Result<Value, String> {
        message["protocol"] = PROTOCOL_VERSION.into();
        let payload = serde_json::to_vec(&message).map_err(|e| e.to_string())?;
        if payload.len() > MAXIMUM_PAYLOAD_SIZE {
            return Err("Jobs-board request exceeds one MiB.".into());
        }
        let mut pipe = connect(&self.endpoint)?;
        pipe.write_all(&(payload.len() as u32).to_le_bytes())
            .and_then(|()| pipe.write_all(&payload))
            .map_err(|e| format!("Write jobs-board request: {e}"))?;

        let mut header = [0; 4];
        pipe.read_exact(&mut header)
            .map_err(|e| format!("Read jobs-board header: {e}"))?;
        let size = u32::from_le_bytes(header) as usize;
        if size > MAXIMUM_PAYLOAD_SIZE {
            return Err("Jobs-board response exceeds one MiB.".into());
        }
        let mut payload = vec![0; size];
        pipe.read_exact(&mut payload)
            .map_err(|e| format!("Read jobs-board response: {e}"))?;
        let reply: Value = serde_json::from_slice(&payload)
            .map_err(|e| format!("Invalid jobs-board response: {e}"))?;
        if !reply.is_object() || !reply["type"].is_string() {
            return Err("Invalid jobs-board response.".into());
        }
        if reply["type"] == "error" {
            return Err(format!(
                "{}: {}",
                reply["code"].as_str().unwrap_or("error"),
                reply["message"].as_str().unwrap_or("Request failed")
            ));
        }
        Ok(reply)
    }
}

fn connect(endpoint: &Path) -> Result<std::fs::File, String> {
    #[cfg(windows)]
    {
        windows::connect(endpoint)
    }
    #[cfg(not(windows))]
    {
        let _ = endpoint;
        Err("The jobs board requires Windows.".into())
    }
}
