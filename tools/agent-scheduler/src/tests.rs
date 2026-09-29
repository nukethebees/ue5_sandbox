use super::*;
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio::io::DuplexStream;
use tokio::sync::oneshot;

async fn fixture() -> (Arc<Scheduler>, DuplexStream) {
    let (client, mut server) = tokio::io::duplex(8192);
    let opening = tokio::spawn(async move {
        let mut parser = PolicyParser::new();
        parser
            .parse(
                "test.rules",
                r#"prefix_rule(pattern=["rg"], decision="allow")"#,
            )
            .unwrap();
        Scheduler::open(parser.build(), client).await.unwrap()
    });
    assert_eq!(read_frame(&mut server).await.unwrap()["type"], "hello");
    write_frame(
        &mut server,
        &json!({"type":"hello_ack","protocol":{"major":3}}),
    )
    .await
    .unwrap();
    (opening.await.unwrap(), server)
}

#[tokio::test]
async fn exempt_without_ticket_and_expensive_without_ticket() {
    let (scheduler, _server) = fixture().await;
    assert_eq!(scheduler.run("rg needle", async { 42 }).await.unwrap(), 42);
    let result = scheduler
        .run("cmake --build out", async { panic!("must not execute") })
        .await;
    assert!(
        result
            .unwrap_err()
            .to_string()
            .contains("No scheduling ticket")
    );
    assert_eq!(scheduler.status().unwrap()["state"], "none");
}

#[tokio::test]
async fn queued_command_waits_and_completion_releases() {
    let (scheduler, mut server) = fixture().await;
    let (grant, wait) = oneshot::channel();
    let daemon = tokio::spawn(async move {
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "request");
        write_frame(&mut server, &json!({"type":"queued"}))
            .await
            .unwrap();
        wait.await.unwrap();
        write_frame(&mut server, &json!({"type":"granted"}))
            .await
            .unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "release");
        write_frame(&mut server, &json!({"type":"released"}))
            .await
            .unwrap();
        server
    });
    scheduler.ticket("shared", "compile").await.unwrap();
    assert!(scheduler.ticket("exclusive", "duplicate").await.is_err());
    let executed = Arc::new(AtomicUsize::new(0));
    let calls = executed.clone();
    let client = scheduler.clone();
    let command = tokio::spawn(async move {
        client
            .run("cmake --build out", async {
                calls.fetch_add(1, Ordering::SeqCst);
                17
            })
            .await
    });
    tokio::task::yield_now().await;
    assert_eq!(executed.load(Ordering::SeqCst), 0);
    grant.send(()).unwrap();
    assert_eq!(command.await.unwrap().unwrap(), 17);
    assert_eq!(executed.load(Ordering::SeqCst), 1);
    assert_eq!(scheduler.status().unwrap()["state"], "none");
    let _server = daemon.await.unwrap();
}

#[tokio::test]
async fn granted_ticket_is_single_and_errors_release_it() {
    let (scheduler, mut server) = fixture().await;
    let daemon = tokio::spawn(async move {
        assert_eq!(read_frame(&mut server).await.unwrap()["mode"], "exclusive");
        write_frame(&mut server, &json!({"type":"granted"}))
            .await
            .unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "release");
        write_frame(&mut server, &json!({"type":"released"}))
            .await
            .unwrap();
        server
    });
    scheduler.ticket("exclusive", "benchmark").await.unwrap();
    assert!(scheduler.ticket("shared", "duplicate").await.is_err());
    assert_eq!(scheduler.run("rg read", async { 1 }).await.unwrap(), 1);
    assert_eq!(scheduler.status().unwrap()["state"], "granted");
    let result: Result<(), &str> = scheduler
        .run("benchmark", async { Err("command failed") })
        .await
        .unwrap();
    assert_eq!(result.unwrap_err(), "command failed");
    assert_eq!(scheduler.status().unwrap()["state"], "none");
    let _server = daemon.await.unwrap();
}

#[tokio::test]
async fn internal_control_uses_the_existing_connection() {
    let (scheduler, mut server) = fixture().await;
    let daemon = tokio::spawn(async move {
        let request = read_frame(&mut server).await.unwrap();
        assert_eq!(
            request,
            json!({"type":"request","mode":"shared","name":"compile project"})
        );
        write_frame(&mut server, &json!({"type":"queued"}))
            .await
            .unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "release");
        write_frame(&mut server, &json!({"type":"released"}))
            .await
            .unwrap();
        server
    });
    assert!(
        scheduler
            .control(r#"agent-scheduler ticket shared "compile project""#)
            .await
            .unwrap()
            .is_some()
    );
    assert!(
        scheduler
            .control("agent-scheduler status")
            .await
            .unwrap()
            .unwrap()
            .contains("queued")
    );
    scheduler.control("agent-scheduler clear").await.unwrap();
    assert_eq!(scheduler.status().unwrap()["state"], "none");
    let _server = daemon.await.unwrap();
}

#[tokio::test]
async fn connection_loss_fails_closed_without_reconnect() {
    let (scheduler, server) = fixture().await;
    let mut changed = scheduler.state.subscribe();
    drop(server);
    changed.changed().await.unwrap();
    assert!(
        scheduler
            .run("cmake --build out", async {})
            .await
            .unwrap_err()
            .to_string()
            .contains("connection lost")
    );
}

#[tokio::test]
async fn cancellation_releases_the_logical_call() {
    let (scheduler, mut server) = fixture().await;
    let daemon = tokio::spawn(async move {
        read_frame(&mut server).await.unwrap();
        write_frame(&mut server, &json!({"type":"queued"}))
            .await
            .unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "release");
        write_frame(&mut server, &json!({"type":"released"}))
            .await
            .unwrap();
        server
    });
    scheduler.ticket("shared", "cancel me").await.unwrap();
    let client = scheduler.clone();
    let command =
        tokio::spawn(async move { client.run("compile", std::future::pending::<()>()).await });
    tokio::task::yield_now().await;
    command.abort();
    let _ = command.await;
    let _server = daemon.await.unwrap();
    let mut changed = scheduler.state.subscribe();
    while scheduler.healthy_state().unwrap() != State::None {
        changed.changed().await.unwrap();
    }
}
