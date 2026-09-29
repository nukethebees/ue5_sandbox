//! In-memory peer for the pinned Codex retry regression; absent from production builds.
use super::*;
use tokio::io::DuplexStream;
use tokio::task::JoinHandle;

pub async fn single_ticket() -> (Arc<Scheduler>, JoinHandle<DuplexStream>) {
    let (client, mut server) = tokio::io::duplex(8192);
    let daemon = tokio::spawn(async move {
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "hello");
        write_frame(
            &mut server,
            &json!({"type":"hello_ack","protocol":{"major":3}}),
        )
        .await
        .unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "request");
        write_frame(&mut server, &json!({"type":"granted"}))
            .await
            .unwrap();
        // Any retry-visible request/release or other protocol message fails this assertion.
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "release");
        write_frame(&mut server, &json!({"type":"released"}))
            .await
            .unwrap();
        server
    });
    let scheduler = Scheduler::open(PolicyParser::new().build(), client)
        .await
        .unwrap();
    scheduler
        .ticket("shared", "Codex sandbox retry")
        .await
        .unwrap();
    (scheduler, daemon)
}
