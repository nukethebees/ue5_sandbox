use super::*;
use tokio::net::windows::named_pipe::ServerOptions;
use tokio::sync::oneshot;

async fn mock() -> (
    Arc<Scheduler>,
    tokio::net::windows::named_pipe::NamedPipeServer,
) {
    static NEXT_ID: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(1);
    let id = (u64::from(std::process::id()) << 32)
        | NEXT_ID.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
    let endpoint = format!(r"\\.\pipe\SchedulerAckTest.{id}");
    let mut server = ServerOptions::new()
        .first_pipe_instance(true)
        .create(&endpoint)
        .unwrap();
    let accept = async {
        server.connect().await.unwrap();
        assert_eq!(read_frame(&mut server).await.unwrap()["type"], "hello");
        write_frame(
            &mut server,
            &json!({"type":"hello_ack","protocol":{"major":2},"client":id}),
        )
        .await
        .unwrap();
    };
    let (client, ()) = tokio::join!(
        Scheduler::connect(PolicyParser::new().build(), &endpoint),
        accept
    );
    (client.unwrap(), server)
}

async fn expect(pipe: &mut tokio::net::windows::named_pipe::NamedPipeServer, kind: &str) -> Value {
    loop {
        let message = read_frame(pipe).await.unwrap();
        if message["type"] == "health" {
            continue;
        }
        assert_eq!(message["type"], kind);
        return message;
    }
}

#[tokio::test]
async fn next_ticket_waits_for_ack_and_respects_cancellation() {
    let (client, mut daemon) = mock().await;
    let (released_tx, released) = oneshot::channel();
    let (acknowledge, ack) = oneshot::channel();
    let (stop, stopped) = oneshot::channel();
    let server = tokio::spawn(async move {
        expect(&mut daemon, "acquire").await;
        write_frame(&mut daemon, &json!({"type":"granted","lease":1}))
            .await
            .unwrap();
        assert_eq!(expect(&mut daemon, "release").await["lease"], 1);
        released_tx.send(()).unwrap();
        ack.await.unwrap();
        write_frame(&mut daemon, &json!({"type":"released","lease":1}))
            .await
            .unwrap();
        expect(&mut daemon, "acquire").await;
        write_frame(&mut daemon, &json!({"type":"granted","lease":2}))
            .await
            .unwrap();
        stopped.await.unwrap();
    });
    let cancellation = CancellationToken::new();
    client
        .request_ticket("shared", "one", &cancellation)
        .await
        .unwrap();
    let invocation = Invocation::default();
    let observer = invocation
        .before_spawn(&client, "work", &cancellation)
        .await
        .unwrap();
    invocation.accept();
    observer.root_exited(Some(0));
    released.await.unwrap();
    let cancelled = CancellationToken::new();
    {
        let request = client.request_ticket("shared", "cancelled", &cancelled);
        tokio::pin!(request);
        tokio::select! { biased; result = &mut request => panic!("must await ack: {result:?}"), _ = std::future::ready(()) => {} }
        cancelled.cancel();
        assert!(request.await.unwrap_err().to_string().contains("cancelled"));
    }
    let request = client.request_ticket("shared", "two", &cancellation);
    tokio::pin!(request);
    tokio::select! { biased; result = &mut request => panic!("must await ack: {result:?}"), _ = std::future::ready(()) => {} }
    acknowledge.send(()).unwrap();
    assert_eq!(request.await.unwrap()["lease"], 2);
    // Dropping/releasing an older logical invocation cannot change ticket 2.
    observer.root_exited(Some(0));
    drop(invocation);
    assert_eq!(client.status().unwrap()["lease"], 2);
    stop.send(()).unwrap();
    server.await.unwrap();
}

#[tokio::test]
async fn daemon_loss_wakes_a_ticket_waiting_for_release_ack() {
    let (client, mut daemon) = mock().await;
    let (disconnect, disconnected) = oneshot::channel();
    let server = tokio::spawn(async move {
        expect(&mut daemon, "acquire").await;
        write_frame(&mut daemon, &json!({"type":"granted","lease":1}))
            .await
            .unwrap();
        expect(&mut daemon, "cancel").await;
        disconnected.await.unwrap();
    });
    let cancellation = CancellationToken::new();
    client
        .request_ticket("shared", "one", &cancellation)
        .await
        .unwrap();
    client.clear().unwrap();
    let next = client.request_ticket("shared", "two", &cancellation);
    tokio::pin!(next);
    tokio::select! { biased; result = &mut next => panic!("must await ack: {result:?}"), _ = std::future::ready(()) => {} }
    disconnect.send(()).unwrap();
    assert!(
        tokio::time::timeout(std::time::Duration::from_secs(2), next)
            .await
            .unwrap()
            .unwrap_err()
            .to_string()
            .contains("disconnected")
    );
    server.await.unwrap();
}
