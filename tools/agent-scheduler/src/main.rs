use anyhow::{Result, bail};
use serde_json::json;

#[tokio::main]
async fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let request = match args.first().map(String::as_str) {
        Some("ticket") if args.len() >= 3 => {
            json!({"type":"ticket","mode":args[1],"name":args[2..].join(" ")})
        }
        Some("status" | "clear") if args.len() == 1 => json!({"type":args[0]}),
        _ => bail!("Usage: agent-scheduler ticket shared|exclusive NAME | status | clear"),
    };
    println!("{}", agent_scheduler::control(request).await?);
    Ok(())
}
