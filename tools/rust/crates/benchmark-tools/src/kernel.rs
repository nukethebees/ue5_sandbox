use crate::cli::{KernelOptions, KernelWorkload};
use crate::support::*;
use std::{fs, path::Path, process::Command};

fn workload_filter(workload: KernelWorkload) -> &'static str {
    match workload {
        KernelWorkload::Representative => {
            "^(add_scaled/elementwise|dot_product/relaxed)/(flat|chunked16)/[^/]+/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
        }
        KernelWorkload::VectorLayout => {
            "^dot_product_3d/elementwise/(aos/(scalar|autovec-avx2|avx2|autovec-avx512|avx512)|(soa-flat|soa-chunked16)/(autovec-avx2|avx2|autovec-avx512|avx512))/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
        }
        KernelWorkload::Highway => {
            "^(add_scaled/elementwise|dot_product/relaxed|dot_product_3d/elementwise)/[^/]+/[^/]+/ordinary/(aligned|unaligned)/(17|32|4096|65536)/real_time$"
        }
        KernelWorkload::Full => "",
    }
}

pub fn generate_kernel_report(root: &Path, args: &KernelOptions) -> Result<()> {
    let workload = args.workload;
    let highway = workload == KernelWorkload::Highway;
    let repetitions = args.repetitions.unwrap_or(if highway { 3 } else { 7 });
    let min_time = args.min_time.unwrap_or(if highway { 0.02 } else { 0.05 });
    if !args.skip_build {
        run_process_inherited(root, "cmake", &["--preset", "native-benchmark"])?;
        run_process_inherited(root, "cmake", &["--build", "--preset", "kernel-benchmark"])?;
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
    if !workload_filter(workload).is_empty() {
        command.arg(format!("--benchmark_filter={}", workload_filter(workload)));
    }
    let status = command.status()?;
    if !status.success() {
        return Err(Box::new(ProcessFailure(status.code().unwrap_or(1))));
    }
    crate::plots::automatic(|| {
        if plots.exists() {
            fs::remove_dir_all(&plots)?;
        }
        crate::plots::plot_kernel(&read_json(&results)?, &plots, "scalar")
    });
    println!("Results written to {}", results.display());
    Ok(())
}
