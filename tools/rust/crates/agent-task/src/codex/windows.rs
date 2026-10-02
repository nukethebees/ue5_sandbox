use super::Operation;
use std::collections::{BTreeMap, HashSet};
use std::ffi::{OsStr, OsString};
use std::fs;
use std::mem::{size_of, zeroed};
use std::os::windows::ffi::{OsStrExt, OsStringExt};
use std::os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle};
use std::os::windows::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::System::Console::*;
use windows_sys::Win32::System::Diagnostics::ToolHelp::*;
use windows_sys::Win32::System::JobObjects::*;
use windows_sys::Win32::System::SystemServices::JOB_OBJECT_QUERY;
use windows_sys::Win32::System::Threading::*;

const CANCELLED: u32 = 130;

unsafe extern "system" fn console_control(event: u32) -> i32 {
    // Let Codex handle interactive interrupts while the launcher keeps waiting.
    i32::from(event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT)
}

fn owned_handle(value: HANDLE, operation: &str) -> Result<OwnedHandle, String> {
    if value.is_null() || value == INVALID_HANDLE_VALUE {
        return Err(last_error(operation));
    }
    Ok(unsafe { OwnedHandle::from_raw_handle(value) })
}

fn last_error(operation: &str) -> String {
    format!("{operation}: {}", std::io::Error::last_os_error())
}

fn wide(value: &OsStr) -> Vec<u16> {
    value.encode_wide().chain(Some(0)).collect()
}

fn checked(result: i32, operation: &str) -> Result<(), String> {
    if result == 0 {
        Err(last_error(operation))
    } else {
        Ok(())
    }
}

fn job_name(root: &Path, name: &str) -> String {
    // Keep names stable across AgentTask releases and case variants of a Windows path.
    let mut hash = 0xcbf29ce484222325u64;
    for byte in root.to_string_lossy().to_lowercase().bytes() {
        hash = (hash ^ u64::from(byte)).wrapping_mul(0x100000001b3);
    }
    format!("Local\\NukeTheBees.Codex.{hash:016x}.{name}")
}

fn state_path(root: &Path, name: &str) -> PathBuf {
    root.join(".local/codex").join(format!("{name}.pid"))
}

struct Process {
    handle: OwnedHandle,
    pid: u32,
    image: PathBuf,
}

fn image_path(handle: HANDLE) -> Result<PathBuf, String> {
    let mut buffer = vec![0u16; 32768];
    let mut length = buffer.len() as u32;
    checked(
        unsafe { QueryFullProcessImageNameW(handle, 0, buffer.as_mut_ptr(), &mut length) },
        "Read process image",
    )?;
    Ok(PathBuf::from(OsString::from_wide(
        &buffer[..length as usize],
    )))
}

fn member(job: HANDLE, process: HANDLE) -> Result<bool, String> {
    let mut belongs = 0;
    checked(
        unsafe { IsProcessInJob(process, job, &mut belongs) },
        "Verify process job membership",
    )?;
    Ok(belongs != 0)
}

fn process_ids(job: HANDLE) -> Result<Vec<u32>, String> {
    let mut capacity = 64usize;
    loop {
        // Use pointer-sized storage to satisfy the trailing ULONG_PTR array's alignment.
        let bytes = size_of::<JOBOBJECT_BASIC_PROCESS_ID_LIST>() + capacity * size_of::<usize>();
        let mut buffer = vec![0usize; bytes.div_ceil(size_of::<usize>())];
        let result = unsafe {
            QueryInformationJobObject(
                job,
                JobObjectBasicProcessIdList,
                buffer.as_mut_ptr().cast(),
                (buffer.len() * size_of::<usize>()) as u32,
                null_mut(),
            )
        };
        if result == 0 {
            if unsafe { GetLastError() } == ERROR_MORE_DATA && capacity < 1_048_576 {
                capacity *= 2;
                continue;
            }
            return Err(last_error("List job processes"));
        }
        let list = buffer.as_ptr().cast::<JOBOBJECT_BASIC_PROCESS_ID_LIST>();
        let count = unsafe { (*list).NumberOfProcessIdsInList } as usize;
        let values = unsafe {
            std::slice::from_raw_parts(
                std::ptr::addr_of!((*list).ProcessIdList).cast::<usize>(),
                count,
            )
        };
        return Ok(values.iter().map(|pid| *pid as u32).collect());
    }
}

fn processes(job: HANDLE) -> Result<Vec<Process>, String> {
    let mut result = Vec::new();
    for pid in process_ids(job)? {
        let access = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SYNCHRONIZE;
        let handle = unsafe { OpenProcess(access, 0, pid) };
        if handle.is_null() && unsafe { GetLastError() } == ERROR_INVALID_PARAMETER {
            continue;
        }
        let handle = owned_handle(handle, "Open job member")?;
        if !member(job, handle.as_raw_handle())?
            || unsafe { WaitForSingleObject(handle.as_raw_handle(), 0) } == WAIT_OBJECT_0
        {
            continue;
        }
        result.push(Process {
            image: image_path(handle.as_raw_handle())?,
            handle,
            pid,
        });
    }
    Ok(result)
}

fn parents() -> Result<BTreeMap<u32, u32>, String> {
    let snapshot = owned_handle(
        unsafe { CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) },
        "Snapshot process ancestry",
    )?;
    let mut entry: PROCESSENTRY32W = unsafe { zeroed() };
    entry.dwSize = size_of::<PROCESSENTRY32W>() as u32;
    checked(
        unsafe { Process32FirstW(snapshot.as_raw_handle(), &mut entry) },
        "Read process ancestry",
    )?;
    let mut result = BTreeMap::new();
    loop {
        result.insert(entry.th32ProcessID, entry.th32ParentProcessID);
        if unsafe { Process32NextW(snapshot.as_raw_handle(), &mut entry) } == 0 {
            if unsafe { GetLastError() } != ERROR_NO_MORE_FILES {
                return Err(last_error("Read process ancestry"));
            }
            return Ok(result);
        }
    }
}

fn open_job(root: &Path, name: &str) -> Result<OwnedHandle, String> {
    let name = wide(OsStr::new(&job_name(root, name)));
    owned_handle(
        unsafe { OpenJobObjectW(JOB_OBJECT_QUERY, 0, name.as_ptr()) },
        "Open session job",
    )
}

fn protected(codex: &Process, processes: &[Process]) -> Result<HashSet<u32>, String> {
    let parents = parents()?;
    let executable = codex.image.canonicalize().map_err(|e| e.to_string())?;
    let directory = executable
        .parent()
        .ok_or("Codex executable has no directory")?;
    let mut runtime = vec![codex.pid, std::process::id()];
    let mut console_hosts = Vec::new();
    for process in processes {
        let image = process.image.canonicalize().map_err(|e| e.to_string())?;
        let filename = image
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .to_ascii_lowercase();
        let sandbox_runner = filename.starts_with("codex-command-runner-")
            && filename.ends_with(".exe")
            && image
                .parent()
                .and_then(Path::file_name)
                .is_some_and(|name| name == ".sandbox-bin");
        // Codex ships runtime helpers alongside its executable; preserve those exact paths.
        // Console hosts and installed sandbox runners support live tool connections too.
        if image.parent() == Some(directory) || sandbox_runner {
            runtime.push(process.pid);
        }
        if matches!(filename.as_str(), "conhost.exe" | "openconsole.exe") {
            console_hosts.push(process.pid);
        }
    }
    // Preserve the caller and runtime ancestry once, including the launcher and sandbox brokers.
    let mut keep = HashSet::new();
    for mut pid in runtime {
        while pid != 0 && keep.insert(pid) {
            let Some(parent) = parents.get(&pid) else {
                break;
            };
            pid = *parent;
        }
    }
    // A console host can be parented to ordinary work. Preserve the host, not that work.
    keep.extend(console_hosts);
    Ok(keep)
}

fn clean(root: &Path, name: &str, dry_run: bool) -> Result<(), String> {
    let job = open_job(root, name)?;
    // Only an actual member can use the self-cleanup capability; names alone confer no ownership.
    if !member(job.as_raw_handle(), unsafe { GetCurrentProcess() })? {
        return Err("Clean must run inside the named Codex job.".into());
    }
    let members = processes(job.as_raw_handle())?;
    let root_pid = fs::read_to_string(state_path(root, name))
        .map_err(|e| e.to_string())?
        .parse::<u32>()
        .map_err(|e| format!("Read Codex PID: {e}"))?;
    let codex = members
        .iter()
        .find(|p| p.pid == root_pid)
        .ok_or("Codex is no longer running in this job")?;
    let keep = protected(codex, &members)?;
    let mut stopped = 0;
    for process in &members {
        if keep.contains(&process.pid) {
            continue;
        }
        println!(
            "{} PID {} {}",
            if dry_run { "Would stop" } else { "Stopping" },
            process.pid,
            process.image.display()
        );
        if !dry_run {
            if unsafe { WaitForSingleObject(process.handle.as_raw_handle(), 0) } == WAIT_OBJECT_0 {
                continue;
            }
            let target = owned_handle(
                unsafe {
                    OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | PROCESS_SYNCHRONIZE,
                        0,
                        process.pid,
                    )
                },
                "Open owned child for termination",
            )?;
            if !member(job.as_raw_handle(), target.as_raw_handle())? {
                return Err(format!(
                    "PID {} is no longer a job member; refusing termination.",
                    process.pid
                ));
            }
            checked(
                unsafe { TerminateProcess(target.as_raw_handle(), CANCELLED) },
                "Terminate owned child",
            )?;
            if unsafe { WaitForSingleObject(process.handle.as_raw_handle(), 5000) } != WAIT_OBJECT_0
            {
                return Err(format!(
                    "PID {} did not exit within five seconds.",
                    process.pid
                ));
            }
        }
        stopped += 1;
    }
    println!(
        "{} {stopped} child processes; Codex and runtime helpers preserved.",
        if dry_run { "Selected" } else { "Stopped" }
    );
    Ok(())
}

fn launch_codex(root: &Path, name: &str) -> Result<i32, String> {
    let name_wide = wide(OsStr::new(&job_name(root, name)));
    let raw_job = unsafe { CreateJobObjectW(null(), name_wide.as_ptr()) };
    let existed = unsafe { GetLastError() } == ERROR_ALREADY_EXISTS;
    let job = owned_handle(raw_job, "Create Codex job")?;
    if existed {
        return Err(format!(
            "Session '{name}' is already running in this worktree."
        ));
    }

    // Join before spawning so Codex and its later tool processes inherit this job.
    checked(
        unsafe { AssignProcessToJobObject(job.as_raw_handle(), GetCurrentProcess()) },
        "Join Codex job",
    )?;
    checked(
        unsafe { SetConsoleCtrlHandler(Some(console_control), 1) },
        "Preserve launcher during console interrupts",
    )?;
    let path = state_path(root, name);
    fs::create_dir_all(path.parent().unwrap()).map_err(|e| e.to_string())?;
    let mut command = Command::new("codex.exe");
    command.arg("--no-daemon").current_dir(root);
    if unsafe { GetConsoleCP() } == 0 {
        command.creation_flags(CREATE_NO_WINDOW);
    }
    let mut child = command
        .spawn()
        .map_err(|e| format!("Launch codex.exe: {e}"))?;
    fs::write(&path, child.id().to_string()).map_err(|e| e.to_string())?;
    println!(
        "Codex session '{name}' (PID {}); shared daemon disabled.",
        child.id()
    );
    let status = child.wait().map_err(|e| e.to_string())?;
    fs::remove_file(path).map_err(|e| e.to_string())?;
    Ok(status.code().unwrap_or(1))
}

pub(super) fn execute_codex_command(operation: Operation) -> Result<i32, String> {
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    // Scope process ownership to the physical checkout, independent of Git environment overrides.
    let root = cwd
        .ancestors()
        .find(|path| path.join(".git").exists())
        .ok_or("Run agent-task codex inside a Git worktree.")?
        .canonicalize()
        .map_err(|e| e.to_string())?;
    match operation {
        Operation::Start { name } => launch_codex(&root, &name),
        Operation::Processes { name } => {
            let job = open_job(&root, &name)?;
            for process in processes(job.as_raw_handle())? {
                println!("{}\t{}", process.pid, process.image.display());
            }
            Ok(0)
        }
        Operation::Clean { name, dry_run } => {
            clean(&root, &name, dry_run)?;
            Ok(0)
        }
    }
}
