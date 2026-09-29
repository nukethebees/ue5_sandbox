use anyhow::{Context, Result, bail};
use serde_json::Value;
use std::os::windows::io::AsRawHandle;
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::windows::named_pipe::{ClientOptions, NamedPipeClient};
use windows_sys::Win32::Foundation::{CloseHandle, LocalFree};
use windows_sys::Win32::Security::Authorization::ConvertSidToStringSidW;
use windows_sys::Win32::Security::{GetTokenInformation, TOKEN_QUERY, TOKEN_USER, TokenUser};
use windows_sys::Win32::System::Pipes::GetNamedPipeServerProcessId;
use windows_sys::Win32::System::Threading::{
    GetCurrentProcess, OpenProcess, OpenProcessToken, PROCESS_QUERY_LIMITED_INFORMATION,
    QueryFullProcessImageNameW,
};

pub fn daemon_endpoint() -> Result<String> {
    Ok(format!(r"\\.\pipe\NukeTheBees.Jobserver.{}", token_sid()?))
}

fn token_sid() -> Result<String> {
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
            let sid_pointer = (*storage.as_ptr().cast::<TOKEN_USER>()).User.Sid;
            let mut sid = std::ptr::null_mut();
            if ConvertSidToStringSidW(sid_pointer, &mut sid) == 0 {
                return Err(std::io::Error::last_os_error().into());
            }
            let mut len = 0;
            while *sid.add(len) != 0 {
                len += 1;
            }
            let sid_text = String::from_utf16_lossy(std::slice::from_raw_parts(sid, len));
            LocalFree(sid.cast());
            Ok(sid_text)
        })();
        CloseHandle(token);
        result
    }
}

pub async fn connect(endpoint: &str) -> Result<NamedPipeClient> {
    let pipe = ClientOptions::new().open(endpoint).with_context(|| {
        format!(
            "Canonical jobserver unavailable at {endpoint}; ask the maintainer to start/install it"
        )
    })?;
    let expected = std::path::PathBuf::from(
        std::env::var_os("LOCALAPPDATA").context("LOCALAPPDATA is missing")?,
    )
    .join("NukeTheBees/jobserver/bin/jobserverd.exe");
    let expected = std::fs::canonicalize(&expected)
        .with_context(|| format!("Canonical jobserver is missing: {}", expected.display()))?;
    unsafe {
        let mut pid = 0;
        if GetNamedPipeServerProcessId(pipe.as_raw_handle(), &mut pid) == 0 {
            bail!("Cannot identify the jobserver process");
        }
        let process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
        if process.is_null() {
            bail!("Cannot inspect the jobserver process");
        }
        let mut path = vec![0u16; 32768];
        let mut size = path.len() as u32;
        let queried = QueryFullProcessImageNameW(process, 0, path.as_mut_ptr(), &mut size);
        CloseHandle(process);
        if queried == 0 {
            bail!("Cannot read the jobserver executable path");
        }
        let actual = std::fs::canonicalize(String::from_utf16_lossy(&path[..size as usize]))?;
        if !actual
            .to_string_lossy()
            .eq_ignore_ascii_case(&expected.to_string_lossy())
        {
            bail!("Scheduler endpoint is not owned by the canonical installed jobserver");
        }
    }
    Ok(pipe)
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
    pipe.write_u32_le(bytes.len() as u32).await?;
    pipe.write_all(&bytes).await?;
    Ok(())
}
