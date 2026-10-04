use std::fs::{File, OpenOptions};
use std::os::windows::ffi::OsStrExt;
use std::os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle};
use std::path::{Path, PathBuf};
use std::ptr::null_mut;
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::Security::Authorization::ConvertSidToStringSidW;
use windows_sys::Win32::Security::*;
use windows_sys::Win32::System::Pipes::{GetNamedPipeServerProcessId, WaitNamedPipeW};
use windows_sys::Win32::System::Threading::{GetCurrentProcess, OpenProcessToken};

pub(super) fn default_endpoint() -> Result<PathBuf, String> {
    Ok(PathBuf::from(format!(
        r"\\.\pipe\NukeTheBees.Jobserver.{}",
        current_user_sid()?
    )))
}

pub(super) fn current_user_sid() -> Result<String, String> {
    let mut raw = null_mut();
    if unsafe { OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &mut raw) } == 0 {
        return Err(format!(
            "Read current user token: {}",
            std::io::Error::last_os_error()
        ));
    }
    let token = unsafe { OwnedHandle::from_raw_handle(raw) };
    let mut size = 0;
    unsafe { GetTokenInformation(token.as_raw_handle(), TokenUser, null_mut(), 0, &mut size) };
    if size == 0 {
        return Err(format!(
            "Read user token size: {}",
            std::io::Error::last_os_error()
        ));
    }
    // Keep TOKEN_USER and its trailing SID storage pointer-aligned.
    let mut storage = vec![0usize; (size as usize).div_ceil(std::mem::size_of::<usize>())];
    if unsafe {
        GetTokenInformation(
            token.as_raw_handle(),
            TokenUser,
            storage.as_mut_ptr().cast(),
            size,
            &mut size,
        )
    } == 0
    {
        return Err(format!(
            "Read current user: {}",
            std::io::Error::last_os_error()
        ));
    }
    let user = unsafe { &*storage.as_ptr().cast::<TOKEN_USER>() };
    let mut sid = null_mut();
    if unsafe { ConvertSidToStringSidW(user.User.Sid, &mut sid) } == 0 {
        return Err(format!(
            "Format current user SID: {}",
            std::io::Error::last_os_error()
        ));
    }
    let mut length = 0;
    while unsafe { *sid.add(length) } != 0 {
        length += 1;
    }
    let text = String::from_utf16_lossy(unsafe { std::slice::from_raw_parts(sid, length) });
    unsafe { LocalFree(sid.cast()) };
    Ok(text)
}

pub(super) fn server_process_id(endpoint: &Path) -> Result<Option<u32>, String> {
    let pipe = match connect(endpoint) {
        Ok(pipe) => pipe,
        Err(error) if error.raw_os_error() == Some(ERROR_FILE_NOT_FOUND as i32) => return Ok(None),
        Err(error) => return Err(format!("Connect to installed jobs board: {error}")),
    };
    let mut pid = 0;
    if unsafe { GetNamedPipeServerProcessId(pipe.as_raw_handle(), &mut pid) } == 0 {
        return Err(format!(
            "Read jobs-board process ID: {}",
            std::io::Error::last_os_error()
        ));
    }
    // Complete a normal request so probing the process does not log a broken connection.
    super::exchange(pipe, &super::encode(serde_json::json!({"type": "ping"}))?)?;
    Ok(Some(pid))
}

pub(super) fn connect(endpoint: &Path) -> std::io::Result<File> {
    let wide: Vec<_> = endpoint.as_os_str().encode_wide().chain(Some(0)).collect();
    loop {
        match OpenOptions::new().read(true).write(true).open(endpoint) {
            Ok(pipe) => return Ok(pipe),
            Err(error) => {
                if error.raw_os_error() == Some(ERROR_PIPE_BUSY as i32)
                    && unsafe { WaitNamedPipeW(wide.as_ptr(), 5000) } != 0
                {
                    continue;
                }
                return Err(error);
            }
        }
    }
}
