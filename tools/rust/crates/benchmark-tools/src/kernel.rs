use crate::support::*;
use std::{fs, path::Path, process::Command};

fn filter(workload: &str) -> &'static str {
    match workload {
        "representative" => {
            "^(add_scaled/elementwise|dot_product/relaxed)/(flat|chunked16)/[^/]+/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
        }
        "vector-layout" => {
            "^dot_product_3d/elementwise/(aos/(scalar|autovec-avx2|avx2|autovec-avx512|avx512)|(soa-flat|soa-chunked16)/(autovec-avx2|avx2|autovec-avx512|avx512))/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
        }
        "highway" => {
            "^(add_scaled/elementwise|dot_product/relaxed|dot_product_3d/elementwise)/[^/]+/[^/]+/ordinary/(aligned|unaligned)/(17|32|4096|65536)/real_time$"
        }
        _ => "",
    }
}

pub fn execute(root: &Path, arguments: &[String]) -> Result<()> {
    let args = Args::parse(
        arguments,
        &["--workload", "--repetitions", "--min-time"],
        &["--skip-build"],
    )?;
    let workload = args.choice(
        "--workload",
        "representative",
        &["representative", "vector-layout", "full", "highway"],
    )?;
    let highway = workload == "highway";
    let repetitions = args.integer("--repetitions", if highway { 3 } else { 7 }, 1, 1000)?;
    let min_time = args.float(
        "--min-time",
        if highway { 0.02 } else { 0.05 },
        0.001,
        180.0,
    )?;
    if !args.flag("--skip-build") {
        visible(root, "cmake", &["--preset", "native-benchmark"])?;
        visible(root, "cmake", &["--build", "--preset", "kernel-benchmark"])?;
    }
    let directory = root.join(if highway {
        ".local/benchmarks/highway"
    } else {
        "out/benchmarks/kernel"
    });
    let results = directory.join(if highway {
        "kernels.json"
    } else {
        "results.json"
    });
    let plots = directory.join("plots");
    fs::create_dir_all(&directory)?;
    if !highway && plots.exists() {
        fs::remove_dir_all(&plots)?;
    }
    if results.exists() {
        fs::remove_file(&results)?;
    }
    let executable = root
        .join("out/build/native-benchmark/native/lispb/kernel")
        .join(if cfg!(windows) {
            "kernel-native-benchmarks.exe"
        } else {
            "kernel-native-benchmarks"
        });
    let mut command = Command::new(executable);
    command.current_dir(root).args([
        format!("--benchmark_repetitions={repetitions}"),
        format!("--benchmark_min_time={min_time}s"),
        "--benchmark_enable_random_interleaving=true".into(),
        "--benchmark_display_aggregates_only=true".into(),
        format!("--benchmark_out={}", results.display()),
        "--benchmark_out_format=json".into(),
    ]);
    if !filter(workload).is_empty() {
        command.arg(format!("--benchmark_filter={}", filter(workload)));
    }
    let status = command.status()?;
    if !status.success() {
        return Err(Box::new(ProcessFailure(status.code().unwrap_or(1))));
    }
    if !highway {
        fs::create_dir_all(&plots)?;
        let status = Command::new("uv")
            .args(["run", "--locked"])
            .arg(root.join("Scripts/plot-kernel-benchmarks.py"))
            .arg(results)
            .arg(format!("--output={}", plots.display()))
            .arg("--format=png")
            .env("UV_CACHE_DIR", root.join("out/uv-cache"))
            .env("MPLCONFIGDIR", root.join("out/matplotlib-cache"))
            .current_dir(root)
            .status()?;
        if !status.success() {
            return Err(Box::new(ProcessFailure(status.code().unwrap_or(1))));
        }
    }
    Ok(())
}
