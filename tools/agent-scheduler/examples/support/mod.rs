use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

pub async fn request(listener: &TcpListener) -> Result<(TcpStream, Value)> {
    let (mut socket, _) = listener.accept().await?;
    let mut header = Vec::new();
    while !header.ends_with(b"\r\n\r\n") {
        header.push(socket.read_u8().await?);
        ensure!(header.len() < 65536, "HTTP header too large");
    }
    let header = String::from_utf8(header)?;
    ensure!(
        header.starts_with("POST /v1/responses "),
        "Unexpected request: {header}"
    );
    let size: usize = header
        .lines()
        .find_map(|line| {
            let (name, value) = line.split_once(':')?;
            name.eq_ignore_ascii_case("content-length")
                .then(|| value.trim().parse().ok())
                .flatten()
        })
        .context("Missing Content-Length")?;
    ensure!(size < 16 * 1024 * 1024);
    let mut body = vec![0; size];
    socket.read_exact(&mut body).await?;
    Ok((socket, serde_json::from_slice(&body)?))
}

pub async fn respond(mut socket: TcpStream, index: usize, command: Option<Value>) -> Result<()> {
    let item = match command {
        Some(command) => {
            json!({"type":"function_call","call_id":format!("call_{index}"),"name":"exec_command","arguments":command.to_string()})
        }
        None => {
            json!({"type":"message","id":"done","role":"assistant","content":[{"type":"output_text","text":"SCHEDULER_FLOW_COMPLETE"}]})
        }
    };
    let events = [
        json!({"type":"response.created","response":{"id":format!("response_{index}")}}),
        json!({"type":"response.output_item.done","item":item}),
        json!({"type":"response.completed","response":{"id":format!("response_{index}"),"usage":{"input_tokens":0,"output_tokens":0,"total_tokens":0}}}),
    ];
    let response = events
        .iter()
        .map(|event| format!("data: {event}\n\n"))
        .collect::<String>();
    socket.write_all(format!("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{response}", response.len()).as_bytes()).await?;
    Ok(())
}

pub fn outputs(requests: &[Value]) -> String {
    requests
        .iter()
        .flat_map(|request| request["input"].as_array().into_iter().flatten())
        .filter(|item| item["type"] == "function_call_output")
        .map(|item| item["output"].to_string())
        .collect::<Vec<_>>()
        .join("\n")
}
