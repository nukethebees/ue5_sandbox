use std::path::Path;
use std::process::ExitCode;

fn main() -> ExitCode {
    let arguments = std::env::args().skip(1).collect::<Vec<_>>();
    if arguments.len() != 2 || arguments[0] != "--settings-path" || arguments[1].trim().is_empty() {
        eprintln!("Usage: set-live-coding-disabled --settings-path <path>");
        return ExitCode::from(2);
    }

    let settings_path = Path::new(&arguments[1]);
    match set_live_coding_disabled::disable(settings_path) {
        Ok(changed) => {
            if changed {
                match std::path::absolute(settings_path) {
                    Ok(path) => println!("Disabled Live Coding in '{}'.", path.display()),
                    Err(error) => {
                        eprintln!("set-live-coding-disabled: {error}");
                        return ExitCode::from(1);
                    }
                }
            }
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("set-live-coding-disabled: {error}");
            ExitCode::from(1)
        }
    }
}
