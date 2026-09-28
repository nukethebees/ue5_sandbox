use anyhow::{Context, Result, bail};
use serde_json::Value;
use std::time::Duration;
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::windows::named_pipe::{ClientOptions, NamedPipeClient};
use windows_sys::Win32::Foundation::{CloseHandle, LocalFree};
use windows_sys::Win32::Security::Authorization::ConvertSidToStringSidW;
use windows_sys::Win32::Security::{GetTokenInformation, TOKEN_QUERY, TOKEN_USER, TokenUser};
use windows_sys::Win32::System::Threading::{GetCurrentProcess, OpenProcessToken};

pub const WATCHDOG: Duration = Duration::from_secs(20);

pub fn daemon_endpoint() -> Result<String> {
    // Match the existing C++ client's endpoint and use its test endpoint for isolated examples.
    if let Ok(endpoint) = std::env::var("NUKETHEBEES_JOBSERVER_TEST_PIPE") {
        return Ok(endpoint);
    }
    unsafe {
        let mut token = std::ptr::null_mut();
        if OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &mut token) == 0 {
            return Err(std::io::Error::last_os_error().into());
        }
        let result = (|| {
            let mut size = 0;
            GetTokenInformation(token, TokenUser, std::ptr::null_mut(), 0, &mut size);
            let mut storage = vec![0usize; (size as usize).div_ceil(size_of::<usize>())];
            if GetTokenInformation(
                token,
                TokenUser,
                storage.as_mut_ptr().cast(),
                size,
                &mut size,
            ) == 0
            {
                return Err(std::io::Error::last_os_error().into());
            }
            let user = &*storage.as_ptr().cast::<TOKEN_USER>();
            let mut sid = std::ptr::null_mut();
            if ConvertSidToStringSidW(user.User.Sid, &mut sid) == 0 {
                return Err(std::io::Error::last_os_error().into());
            }
            let mut len = 0;
            while *sid.add(len) != 0 {
                len += 1;
            }
            let sid_text = String::from_utf16_lossy(std::slice::from_raw_parts(sid, len));
            LocalFree(sid.cast());
            Ok(format!(r"\\.\pipe\NukeTheBees.Jobserver.{sid_text}"))
        })();
        CloseHandle(token);
        result
    }
}

pub async fn connect(endpoint: &str) -> Result<NamedPipeClient> {
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    loop {
        match ClientOptions::new().open(endpoint) {
            Ok(pipe) => return Ok(pipe),
            Err(error)
                if error.raw_os_error() == Some(231) && tokio::time::Instant::now() < deadline =>
            {
                tokio::time::sleep(Duration::from_millis(50)).await;
            }
            Err(error) => {
                return Err(error).with_context(|| {
                    format!("Cannot connect to {endpoint}; start the jobserver before Codex")
                });
            }
        }
    }
}

pub async fn read_frame(pipe: &mut (impl AsyncRead + Unpin)) -> Result<Value> {
    let size = pipe.read_u32_le().await? as usize;
    if size == 0 || size > 1024 * 1024 {
        bail!("Invalid frame length: {size}");
    }
    let mut bytes = vec![0; size];
    pipe.read_exact(&mut bytes).await?;
    Ok(serde_json::from_slice(&bytes)?)
}

pub async fn write_frame(pipe: &mut (impl AsyncWrite + Unpin), message: &Value) -> Result<()> {
    let bytes = serde_json::to_vec(message)?;
    tokio::time::timeout(Duration::from_secs(5), async {
        pipe.write_u32_le(bytes.len() as u32).await?;
        pipe.write_all(&bytes).await
    })
    .await
    .context("Jobserver write timed out")??;
    Ok(())
}
