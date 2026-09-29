mod support;
use std::fs;
use std::process::Output;
use support::Repo;

fn fixture() -> Repo {
    let repo = Repo::new();
    fs::create_dir_all(repo.feature.join("src/generated")).unwrap();
    fs::write(
        repo.feature.join(".code-format.json"),
        r#"{"Roots":["src"],"Extensions":[".cpp"],"ExcludedComponents":["generated"]}"#,
    )
    .unwrap();
    fs::write(
        repo.feature.join(".clang-format"),
        "BasedOnStyle: LLVM\nIndentWidth: 4\n",
    )
    .unwrap();
    fs::write(repo.feature.join("src/base.cpp"), "int base;\n").unwrap();
    repo.raw(&repo.feature, &["add", "."]);
    repo.raw(&repo.feature, &["commit", "-qm", "format fixture"]);
    repo
}

fn format(repo: &Repo, args: &[&str]) -> Output {
    repo.tool().arg("format").args(args).output().unwrap()
}

fn write(repo: &Repo, path: &str) {
    fs::write(repo.feature.join(path), "int  foo( ){return  1;}\r\n").unwrap();
}

#[test]
fn changed_formats_modified_staged_and_untracked_files_from_nested_directory() {
    let repo = fixture();
    for path in [
        "src/base.cpp",
        "src/staged.cpp",
        "src/new file.cpp",
        "src/generated/skip.cpp",
        "outside.cpp",
        "src/skip.txt",
    ] {
        write(&repo, path);
    }
    repo.raw(&repo.feature, &["add", "src/staged.cpp"]);
    let output = repo
        .tool()
        .current_dir(repo.feature.join("src"))
        .args(["format", "--changed", "--jobs", "2", "--verbose"])
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
    for path in ["src/base.cpp", "src/staged.cpp", "src/new file.cpp"] {
        assert_eq!(
            fs::read_to_string(repo.feature.join(path)).unwrap(),
            "int foo() { return 1; }\n"
        );
    }
    for path in ["src/generated/skip.cpp", "outside.cpp", "src/skip.txt"] {
        assert!(
            fs::read_to_string(repo.feature.join(path))
                .unwrap()
                .contains("int  foo")
        );
    }
    assert!(String::from_utf8_lossy(&output.stdout).contains("Successfully formatted 3 files"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains(&repo.feature.display().to_string()));
}

#[test]
fn default_and_all_include_ignored_source_files() {
    for arguments in [vec![], vec!["--all"]] {
        let repo = fixture();
        fs::write(repo.feature.join(".gitignore"), "src/ignored.cpp\n").unwrap();
        write(&repo, "src/ignored.cpp");
        let output = format(&repo, &arguments);
        assert!(output.status.success(), "{output:?}");
        assert_eq!(
            fs::read_to_string(repo.feature.join("src/ignored.cpp")).unwrap(),
            "int foo() { return 1; }\n"
        );
    }
}

#[test]
fn staged_rejects_partial_staging_before_touching_any_files() {
    let repo = fixture();
    write(&repo, "src/base.cpp");
    write(&repo, "src/new.cpp");
    repo.raw(&repo.feature, &["add", "src"]);
    fs::write(repo.feature.join("src/base.cpp"), "int  unstaged;\n").unwrap();
    let index = repo.raw(&repo.feature, &["show", ":src/base.cpp"]);
    let output = format(&repo, &["--staged"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("unstaged edits"));
    assert_eq!(repo.raw(&repo.feature, &["show", ":src/base.cpp"]), index);
    assert!(
        fs::read_to_string(repo.feature.join("src/new.cpp"))
            .unwrap()
            .contains("int  foo")
    );
}

#[test]
fn staged_restages_formatted_files_without_staging_other_changes() {
    let repo = fixture();
    write(&repo, "src/staged.cpp");
    write(&repo, "src/untracked.cpp");
    repo.raw(&repo.feature, &["add", "src/staged.cpp"]);
    let output = repo
        .tool()
        .current_dir(repo.feature.join("src"))
        .args(["format", "--staged"])
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
    assert_eq!(
        repo.raw(&repo.feature, &["show", ":src/staged.cpp"]),
        "int foo() { return 1; }"
    );
    assert_eq!(repo.raw(&repo.feature, &["diff", "--name-only"]), "");
    assert_eq!(
        repo.raw(&repo.feature, &["diff", "--cached", "--name-only"]),
        "src/staged.cpp"
    );
}

#[test]
fn deleted_and_renamed_sources_are_selected_correctly() {
    let repo = fixture();
    repo.raw(&repo.feature, &["mv", "src/base.cpp", "src/renamed.cpp"]);
    let output = format(&repo, &["--staged", "--verbose"]);
    assert!(output.status.success(), "{output:?}");
    assert!(String::from_utf8_lossy(&output.stdout).contains("renamed.cpp"));
    repo.raw(&repo.feature, &["rm", "-f", "src/renamed.cpp"]);
    assert!(format(&repo, &["--staged"]).status.success());
}

#[test]
fn formatter_failure_reports_relative_path_and_does_not_restage() {
    let repo = fixture();
    write(&repo, "src/base.cpp");
    repo.raw(&repo.feature, &["add", "src/base.cpp"]);
    let index = repo.raw(&repo.feature, &["show", ":src/base.cpp"]);
    fs::write(
        repo.feature.join(".clang-format"),
        "InvalidOptionForTest: true\n",
    )
    .unwrap();
    let output = format(&repo, &["--staged"]);
    assert!(!output.status.success());
    let error = String::from_utf8_lossy(&output.stderr);
    assert!(error.contains("ERROR formatting src"), "{error}");
    assert!(error.contains("base.cpp"));
    assert_eq!(repo.raw(&repo.feature, &["show", ":src/base.cpp"]), index);
}

#[test]
fn invalid_policy_options_and_protected_staging_fail() {
    let repo = fixture();
    for args in [
        vec!["--jobs", "0"],
        vec!["--all", "--changed"],
        vec!["--jobs", "no"],
        vec!["--unknown"],
    ] {
        assert_eq!(format(&repo, &args).status.code(), Some(2));
    }
    fs::write(
        repo.feature.join(".code-format.json"),
        r#"{"Roots":["../dev"],"Extensions":[".cpp"],"ExcludedComponents":[]}"#,
    )
    .unwrap();
    assert!(!format(&repo, &[]).status.success());
    let output = repo
        .tool()
        .current_dir(&repo.dev)
        .args(["format", "--staged"])
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("protected"));
}

#[test]
fn repository_root_policy_handles_changed_files_and_extension_case() {
    let repo = fixture();
    fs::write(
        repo.feature.join(".code-format.json"),
        r#"{"Roots":["."],"Extensions":[".CPP"],"ExcludedComponents":["generated"]}"#,
    )
    .unwrap();
    write(&repo, "src/base.cpp");
    let output = format(&repo, &["--changed"]);
    assert!(output.status.success(), "{output:?}");
    assert_eq!(
        fs::read_to_string(repo.feature.join("src/base.cpp")).unwrap(),
        "int foo() { return 1; }\n"
    );
}
