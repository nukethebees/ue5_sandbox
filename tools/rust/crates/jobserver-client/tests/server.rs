#![cfg(windows)]

use jobserver_client::Client;
use serde_json::{Value, json};
use std::os::windows::process::CommandExt;
use std::process::{Child, Command};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};

static NEXT: AtomicUsize = AtomicUsize::new(0);

struct Fixture {
    client: Client,
    server: Child,
}

impl Fixture {
    fn new() -> Self {
        let endpoint = format!(
            r"\\.\pipe\NukeTheBees.JobsBoardTest.{}.{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        );
        let server = Command::new(
            std::env::var_os("IOJ_JOBSERVER_TEST_DAEMON")
                .expect("Run through CTest jobserver-client-integration"),
        )
        .arg(&endpoint)
        .creation_flags(0x08000000)
        .spawn()
        .unwrap();
        let mut fixture = Self {
            client: Client::new(endpoint),
            server,
        };
        let deadline = Instant::now() + Duration::from_secs(5);
        while fixture.client.request(json!({"type":"ping"})).is_err() {
            assert!(
                fixture.server.try_wait().unwrap().is_none(),
                "Test daemon exited during startup"
            );
            assert!(Instant::now() < deadline, "Test daemon did not start");
            std::thread::sleep(Duration::from_millis(10));
        }
        fixture
    }

    fn ticket(&self, mode: &str, name: &str) -> Value {
        self.client.request(json!({"type":"request", "mode":mode, "name":name, "owner":"dev5", "worktree":"c:/work/dev5"})).unwrap()
    }

    fn command(&self, kind: &str, id: &Value) -> Result<Value, String> {
        self.client.request(json!({"type":kind, "id":id}))
    }
}

impl Drop for Fixture {
    fn drop(&mut self) {
        // Kill only this isolated fixture, including when an assertion unwinds.
        let _ = self.server.kill();
        let _ = self.server.wait();
    }
}

#[test]
#[ignore = "requires the CMake-built isolated test daemon; run via CTest"]
fn request_check_start_end_cancel_and_status_round_trip() {
    let mut fixture = Fixture::new();
    assert_eq!(
        fixture.client.server_process_id().unwrap(),
        Some(fixture.server.id())
    );
    let a = fixture.ticket("shared", "build tools");
    assert_eq!(a["state"], "Ready");
    assert_eq!(a["owner"], "dev5");
    assert_eq!(a["worktree"], "c:/work/dev5");
    assert_eq!(
        fixture.command("check", &a["id"]).unwrap()["state"],
        "Ready"
    );
    assert_eq!(
        fixture.command("start", &a["id"]).unwrap()["state"],
        "Running"
    );
    let b = fixture.ticket("exclusive", "benchmark");
    assert_eq!(b["state"], "Queued");
    let c = fixture.ticket("shared", "tests");
    let status = fixture.client.request(json!({"type":"status"})).unwrap();
    let tickets = status["tickets"].as_array().unwrap();
    assert_eq!(tickets.len(), 3);
    assert_eq!(
        tickets.iter().map(|t| &t["id"]).collect::<Vec<_>>(),
        [&a["id"], &b["id"], &c["id"]]
    );
    assert!(fixture.command("start", &b["id"]).is_err());
    assert_eq!(fixture.command("end", &a["id"]).unwrap()["state"], "Done");
    assert!(fixture.command("check", &a["id"]).is_err());
    assert_eq!(
        fixture.command("check", &b["id"]).unwrap()["state"],
        "Ready"
    );
    assert_eq!(
        fixture.command("check", &c["id"]).unwrap()["state"],
        "Queued"
    );
    assert_eq!(
        fixture.command("start", &b["id"]).unwrap()["state"],
        "Running"
    );
    assert!(fixture.command("cancel", &b["id"]).is_err());
    assert!(fixture.command("start", &b["id"]).is_err());
    assert_eq!(fixture.command("end", &b["id"]).unwrap()["state"], "Done");
    assert_eq!(
        fixture.command("check", &c["id"]).unwrap()["state"],
        "Ready"
    );
    assert_eq!(
        fixture.command("cancel", &c["id"]).unwrap()["state"],
        "Cancelled"
    );
    assert!(fixture.command("check", &c["id"]).is_err());

    let running = fixture.ticket("exclusive", "stale run");
    fixture.command("start", &running["id"]).unwrap();
    let queued = fixture.ticket("shared", "waiting");
    assert_eq!(
        fixture.command("clear", &running["id"]).unwrap()["tickets"][0]["id"],
        running["id"]
    );
    assert_eq!(
        fixture.command("check", &queued["id"]).unwrap()["state"],
        "Ready"
    );
    let elsewhere = fixture.client.request(json!({"type":"request", "mode":"shared", "name":"other checkout", "owner":"dev5", "worktree":"c:/work/other"})).unwrap();
    let cleared = fixture
        .client
        .request(json!({"type":"clear", "owner":"dev5", "worktree":"c:/work/dev5"}))
        .unwrap();
    assert_eq!(cleared["tickets"].as_array().unwrap().len(), 1);
    assert!(fixture.command("check", &elsewhere["id"]).is_ok());
    fixture
        .client
        .request(json!({"type":"clear", "owner":"dev5"}))
        .unwrap();
    assert!(
        fixture.client.request(json!({"type":"status"})).unwrap()["tickets"]
            .as_array()
            .unwrap()
            .is_empty()
    );
    assert_eq!(
        fixture.client.request(json!({"type":"shutdown"})).unwrap()["type"],
        "accepted"
    );
    fixture.server.wait().unwrap();
    assert_eq!(fixture.client.server_process_id().unwrap(), None);
}

#[test]
#[ignore = "requires the CMake-built isolated test daemon; run via CTest"]
fn invalid_messages_return_errors_and_leave_board_usable() {
    let fixture = Fixture::new();
    for request in [
        json!({"type":"request", "mode":"invalid", "name":"x"}),
        json!({"type":"check", "id":"one"}),
        json!({"type":"check", "id":0}),
        json!({"type":"request"}),
        json!({"type":"clear", "owner":""}),
        json!({"type":"clear", "owner":"dev5", "id":1}),
    ] {
        assert!(fixture.client.request(request).is_err());
    }
    assert!(fixture.command("check", &json!(999)).is_err());
    fixture.ticket("shared", "build");
    assert!(fixture.client.request(json!({"type":"shutdown"})).is_err());
    assert_eq!(
        fixture.client.request(json!({"type":"status"})).unwrap()["tickets"]
            .as_array()
            .unwrap()
            .len(),
        1
    );
}
