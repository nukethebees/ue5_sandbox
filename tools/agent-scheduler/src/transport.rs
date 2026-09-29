use anyhow::{Context, Result, bail};
use serde_json::Value;
use std::time::Duration;
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::windows::named_pipe::{
    ClientOptions, NamedPipeClient, NamedPipeServer, ServerOptions,
};
use windows_sys::Win32::Foundation::{CloseHandle, LocalFree};
use windows_sys::Win32::Security::Authorization::{
    ConvertSidToStringSidW, ConvertStringSecurityDescriptorToSecurityDescriptorW, SDDL_REVISION_1,
};
use windows_sys::Win32::Security::{
    GetTokenInformation, SECURITY_ATTRIBUTES, TOKEN_GROUPS, TOKEN_INFORMATION_CLASS, TOKEN_QUERY,
    TOKEN_USER, TokenLogonSid, TokenUser,
};
use windows_sys::Win32::System::Threading::{GetCurrentProcess, OpenProcessToken};

pub const WATCHDOG: Duration = Duration::from_secs(20);

pub fn daemon_endpoint() -> Result<String> {
    // Match the existing C++ client's endpoint and use its test endpoint for isolated examples.
    if let Ok(endpoint) = std::env::var("NUKETHEBEES_JOBSERVER_TEST_PIPE") {
        return Ok(endpoint);
    }
    Ok(format!(
        r"\\.\pipe\NukeTheBees.Jobserver.{}",
        token_sid(TokenUser)?
    ))
}

fn token_sid(information: TOKEN_INFORMATION_CLASS) -> Result<String> {
    unsafe {
        let mut token = std::ptr::null_mut();
        if OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &mut token) == 0 {
            return Err(std::io::Error::last_os_error().into());
        }
        let result = (|| {
            let mut size = 0;
            GetTokenInformation(token, information, std::ptr::null_mut(), 0, &mut size);
            let mut storage = vec![0usize; (size as usize).div_ceil(size_of::<usize>())];
            if GetTokenInformation(
                token,
                information,
                storage.as_mut_ptr().cast(),
                size,
                &mut size,
            ) == 0
            {
                return Err(std::io::Error::last_os_error().into());
            }
            let sid_pointer = if information == TokenUser {
                (*storage.as_ptr().cast::<TOKEN_USER>()).User.Sid
            } else if information == TokenLogonSid {
                (*storage.as_ptr().cast::<TOKEN_GROUPS>()).Groups[0].Sid
            } else {
                bail!("Unsupported token SID information class");
            };
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

pub fn session_listener(endpoint: &str, first: bool) -> Result<NamedPipeServer> {
    // Codex's restricted token retains the logon SID in its restricting SID list.
    // Grant this local logon access so both sandboxed and ordinary helpers can connect.
    let descriptor: Vec<u16> = format!("D:P(A;;GA;;;{})\0", token_sid(TokenLogonSid)?)
        .encode_utf16()
        .collect();
    unsafe {
        let mut security = std::ptr::null_mut();
        if ConvertStringSecurityDescriptorToSecurityDescriptorW(
            descriptor.as_ptr(),
            SDDL_REVISION_1,
            &mut security,
            std::ptr::null_mut(),
        ) == 0
        {
            return Err(std::io::Error::last_os_error().into());
        }
        let mut attributes = SECURITY_ATTRIBUTES {
            nLength: size_of::<SECURITY_ATTRIBUTES>() as u32,
            lpSecurityDescriptor: security,
            bInheritHandle: 0,
        };
        let result = ServerOptions::new()
            .first_pipe_instance(first)
            .reject_remote_clients(true)
            .create_with_security_attributes_raw(
                endpoint,
                (&mut attributes as *mut SECURITY_ATTRIBUTES).cast(),
            );
        LocalFree(security);
        result.with_context(|| format!("Cannot create scheduler session pipe {endpoint}"))
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
                return Err(error).with_context(|| format!("Cannot connect to {endpoint}"));
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
