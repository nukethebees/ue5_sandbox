use jobserver_client::Client;
use serde_json::json;
use std::fs::{self, File, OpenOptions};
use std::io::{ErrorKind, Write};
use std::os::windows::fs::symlink_file;
use std::os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::thread::sleep;
use std::time::{Duration, Instant};
use windows_sys::Win32::Foundation::WAIT_OBJECT_0;
use windows_sys::Win32::System::Threading::{
    OpenProcess, PROCESS_SYNCHRONIZE, WaitForSingleObject,
};

const TASK_NAME: &str = "NukeTheBeesJobserver";

struct TemporaryFile(PathBuf);

impl TemporaryFile {
    fn create(path: PathBuf) -> Result<(Self, File), String> {
        let file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&path)
            .map_err(|e| format!("Create '{}': {e}", path.display()))?;
        Ok((Self(path), file))
    }
}

impl Drop for TemporaryFile {
    fn drop(&mut self) {
        if let Err(error) = fs::remove_file(&self.0) {
            if error.kind() != ErrorKind::NotFound {
                eprintln!("coj: warning: Remove '{}': {error}", self.0.display());
            }
        }
    }
}

fn validate_link(link: &Path, target: &Path, built_daemon: &Path) -> Result<(), String> {
    match fs::symlink_metadata(link) {
        Ok(metadata) => {
            if metadata.file_type().is_symlink()
                && fs::read_link(link).map_err(|e| e.to_string())? == target
            {
                return Ok(());
            }
            return Err(format!(
                "Cannot publish '{}': an existing file or link belongs to another target. Move it aside explicitly, then rerun the installer. Expected target: '{}'.",
                link.display(),
                target.display()
            ));
        }
        Err(error) if error.kind() == ErrorKind::NotFound => {}
        Err(error) => return Err(format!("Inspect '{}': {error}", link.display())),
    }

    let probe = link.with_file_name(format!(".jobserver-link-probe-{}", std::process::id()));
    symlink_file(built_daemon, &probe).map_err(|e| format!(
        "Cannot create tool symlinks in '{}'. Enable Windows Developer Mode or grant the 'Create symbolic links' privilege, and ensure the directory is writable: {e}", link.parent().unwrap().display()
    ))?;
    let _probe = TemporaryFile(probe);
    Ok(())
}

fn xml_text(text: &str) -> String {
    text.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&apos;")
}

fn task_xml(daemon: &Path) -> Result<String, String> {
    let user = xml_text(&jobserver_client::current_user_sid()?);
    let daemon = xml_text(
        daemon
            .to_str()
            .ok_or("Installed daemon path is not valid Unicode.")?,
    );
    Ok(format!(
        r#"<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>{user}</UserId></LogonTrigger></Triggers>
  <Principals><Principal id="CurrentUser"><UserId>{user}</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
  </Settings>
  <Actions Context="CurrentUser"><Exec><Command>{daemon}</Command></Exec></Actions>
</Task>
"#
    ))
}

fn run_task_command(command: &mut Command) -> Result<(), String> {
    let output = command
        .output()
        .map_err(|e| format!("Run schtasks.exe: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "Scheduled-task operation failed ({}):\n{}{}",
            output.status,
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        ));
    }
    Ok(())
}

fn clear_tickets(client: &Client) -> Result<(), String> {
    let board = client.request(json!({"type": "status"}))?;
    let tickets = board["tickets"]
        .as_array()
        .ok_or("Jobs-board response is missing tickets.")?;
    let mut count = 0;
    for ticket in tickets {
        let id = ticket["id"]
            .as_u64()
            .ok_or("Jobs-board response is missing ticket ID.")?;
        let cleared = client.request(json!({"type": "clear", "id": id}))
            .map_err(|e| format!("Could not force-close ticket {id}. Already force-closed {count} tickets; installation stopped: {e}"))?;
        for closed in cleared["tickets"]
            .as_array()
            .ok_or("Jobs-board response is missing cleared tickets.")?
        {
            print!("Force-closed ticket {}", crate::jobs::ticket_line(closed)?);
            count += 1;
        }
    }
    println!("Force-closed {count} tickets. Their processes were not stopped.");
    Ok(())
}

fn stop_daemon(client: &Client, force: bool) -> Result<(), String> {
    let Some(pid) = client.server_process_id()? else {
        return Ok(());
    };
    // Hold the pipe server's process handle so shutdown cannot race PID reuse.
    let raw = unsafe { OpenProcess(PROCESS_SYNCHRONIZE, 0, pid) };
    if raw.is_null() {
        return Err(format!(
            "Open jobserver PID {pid}: {}",
            std::io::Error::last_os_error()
        ));
    }
    let daemon = unsafe { OwnedHandle::from_raw_handle(raw) };
    if force {
        clear_tickets(client)?;
    }
    client.request(json!({"type": "shutdown"})).map_err(|e| format!(
        "Could not shut down the installed board: {e}. Clear active tickets or retry with coj install jobserver --force; new tickets may have arrived. For a protocol upgrade, shut down the empty board with its matching client before replacing either component."
    ))?;
    if unsafe { WaitForSingleObject(daemon.as_raw_handle(), 15000) } != WAIT_OBJECT_0 {
        return Err("The installed jobserver did not exit within 15 seconds.".into());
    }
    Ok(())
}

pub(super) fn install(root: &Path, built_daemon: &Path, force: bool) -> Result<(), String> {
    let ioj_root = std::env::var_os("IOJ_ROOT")
        .map(PathBuf::from)
        .filter(|path| path.is_absolute())
        .ok_or("Set IOJ_ROOT to an absolute directory for shared tools and temporary files.")?;
    let bin = ioj_root.join("tools/jobserver/bin");
    let links = ioj_root.join("tools/bin");
    let temporary_directory = ioj_root
        .join("tmp")
        .join(root.file_name().unwrap_or_default());
    for directory in [&bin, &links, &temporary_directory] {
        fs::create_dir_all(directory)
            .map_err(|e| format!("Create '{}': {e}", directory.display()))?;
    }
    let installed_daemon = bin.join("jobserverd.exe");
    let link = links.join("jobserverd.exe");
    validate_link(&link, &installed_daemon, built_daemon)?;

    // Stage files before closing tickets or stopping the existing daemon.
    let (staged, mut executable) =
        TemporaryFile::create(bin.join(format!(".jobserverd-{}.exe", std::process::id())))?;
    std::io::copy(
        &mut File::open(built_daemon).map_err(|e| e.to_string())?,
        &mut executable,
    )
    .map_err(|e| format!("Stage jobserver executable: {e}"))?;
    drop(executable);
    let (task_file, mut file) = TemporaryFile::create(
        temporary_directory.join(format!("jobserver-task-{}.xml", std::process::id())),
    )?;
    let xml = task_xml(&installed_daemon)?;
    let bytes: Vec<_> = std::iter::once(0xfeff)
        .chain(xml.encode_utf16())
        .flat_map(u16::to_le_bytes)
        .collect();
    file.write_all(&bytes)
        .map_err(|e| format!("Write scheduled-task definition: {e}"))?;
    drop(file);

    let client = Client::for_current_user()?;
    stop_daemon(&client, force)?;
    fs::rename(&staged.0, &installed_daemon)
        .map_err(|e| format!("Replace '{}': {e}", installed_daemon.display()))?;

    run_task_command(
        Command::new("schtasks.exe")
            .args(["/Create", "/TN", TASK_NAME, "/XML"])
            .arg(&task_file.0)
            .arg("/F"),
    )?;
    run_task_command(Command::new("schtasks.exe").args(["/Run", "/TN", TASK_NAME]))?;
    println!("Registered and started per-user scheduled task '{TASK_NAME}'.");

    let deadline = Instant::now() + Duration::from_secs(20);
    loop {
        match client.request(json!({"type": "ping"})) {
            Ok(reply) if reply["type"] == "pong" => break,
            result if Instant::now() >= deadline => {
                return Err(format!(
                    "The installed jobserver did not become responsive: {result:?}"
                ));
            }
            _ => sleep(Duration::from_millis(100)),
        }
    }
    if fs::symlink_metadata(&link).is_err() {
        symlink_file(&installed_daemon, &link).map_err(|e| {
            format!(
                "Jobserver installed, but publishing '{}' failed: {e}",
                link.display()
            )
        })?;
    }
    println!("Installed the per-user jobs board at '{}'.", bin.display());
    println!(
        "Tool links are in '{}'. Keep this directory on PATH.",
        links.display()
    );
    Ok(())
}
