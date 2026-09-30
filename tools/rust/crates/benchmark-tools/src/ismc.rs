use crate::{
    results::{self, Capture, Conditions},
    revision::{self, Repetition, Revisions, Run, Source},
    support::*,
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
};

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Request {
    pub editor: PathBuf,
    pub width: u32,
    pub height: u32,
    pub instances: u32,
    pub update_percent: f64,
    pub mode: String,
    pub visibility: String,
    pub bounds: String,
    pub custom_data: String,
    pub shadows: bool,
    pub churn: bool,
    pub min_instances: u32,
    pub half_cycle_updates: u32,
    pub replacement_percent: f64,
    pub warmup_updates: u32,
    pub warmup_seconds: f64,
    pub seconds: f64,
    pub trace: bool,
    pub cache_directory: PathBuf,
}

impl Request {
    fn parse(args: &Args, root: &Path) -> Result<Self> {
        let fallback = PathBuf::from(std::env::var_os("UE_ROOT").unwrap_or_default())
            .join("Engine/Binaries/Win64/UnrealEditor-Cmd.exe");
        let editor = absolute(root, args.value("--editor", &fallback.to_string_lossy()))?;
        if !editor.is_file() {
            return Err(format!(
                "Unreal Editor executable does not exist: '{}'. Use --editor or UE_ROOT.",
                editor.display()
            )
            .into());
        }
        let instances = args.integer("--instances", 40000, 1, i32::MAX as u32)?;
        Ok(Self {
            editor,
            width: args.integer("--width", 1280, 1, 16384)?,
            height: args.integer("--height", 720, 1, 16384)?,
            instances,
            update_percent: args.float("--update-percent", 100.0, 0.0, 100.0)?,
            mode: args
                .choice("--mode", "paired", &["paired", "custom", "engine_ismc"])?
                .into(),
            visibility: args
                .choice("--visibility", "all", &["all", "half", "none"])?
                .into(),
            bounds: args
                .choice("--bounds", "calculated", &["calculated", "supplied"])?
                .into(),
            custom_data: args
                .choice("--custom-data", "none", &["none", "static", "animated"])?
                .into(),
            shadows: args.choice("--shadows", "0", &["0", "1"])? == "1",
            churn: args.choice("--churn", "0", &["0", "1"])? == "1",
            min_instances: args.integer("--min-instances", 1000.min(instances), 0, instances)?,
            half_cycle_updates: args.integer("--half-cycle-updates", 120, 1, i32::MAX as u32)?,
            replacement_percent: args.float("--replacement-percent", 5.0, 0.0, 100.0)?,
            warmup_updates: args.integer("--warmup-updates", 0, 0, i32::MAX as u32)?,
            warmup_seconds: args.float("--warmup-seconds", 1.0, 0.0, 3600.0)?,
            seconds: args.float("--seconds", 5.0, 0.01, 3600.0)?,
            trace: args.choice("--trace", "1", &["0", "1"])? == "1",
            cache_directory: root.join(".local/benchmarks/ddc"),
        })
    }
    pub fn conditions(&self) -> Conditions {
        let mut result: Conditions = [
            ("mode", self.mode.clone()),
            ("visibility", format!("{}_visible", self.visibility)),
            ("bounds", self.bounds.clone()),
            (
                "custom_data",
                if self.custom_data == "none" {
                    "no_custom_data".into()
                } else {
                    format!("{}_rgb", self.custom_data)
                },
            ),
            ("instances", self.instances.to_string()),
            ("update_percent", self.update_percent.to_string()),
            ("shadows", u8::from(self.shadows).to_string()),
            ("churn", u8::from(self.churn).to_string()),
            ("min_instances", self.min_instances.to_string()),
            ("half_cycle_updates", self.half_cycle_updates.to_string()),
            ("replacement_percent", self.replacement_percent.to_string()),
            ("warmup_updates", self.warmup_updates.to_string()),
            ("warmup_seconds", self.warmup_seconds.to_string()),
            ("measurement_seconds", self.seconds.to_string()),
            ("trace", u8::from(self.trace).to_string()),
            ("requested_width", self.width.to_string()),
            ("requested_height", self.height.to_string()),
            ("observed_width", self.width.to_string()),
            ("observed_height", self.height.to_string()),
        ]
        .into_iter()
        .map(|(k, v)| (k.into(), v))
        .collect();
        for (k, v) in [
            ("frame_limits_disabled", "1"),
            ("r.VSync", "0"),
            ("r.VSyncEditor", "0"),
            ("t.MaxFPS", "0"),
            ("r.Editor.Viewport.OverridePIEScreenPercentage", "0"),
            ("r.ScreenPercentage", "100"),
            ("r.DynamicRes.OperationMode", "0"),
        ] {
            result.insert(k.into(), v.into());
        }
        result
    }
    fn common_arguments(&self, root: &Path, log: &Path) -> Vec<String> {
        let mut args = vec![
            root.join("Sandbox.uproject").to_string_lossy().into_owned(),
            map(root).to_string_lossy().into_owned(),
        ];
        args.extend(
            [
                "-unattended",
                "-nop4",
                "-nosplash",
                "-nosound",
                "-stdout",
                "-FullStdOutLogOutput",
                "-RenderOffscreen",
                "-ddc=NoZenLocalFallback",
            ]
            .map(str::to_owned),
        );
        args.extend([
            format!("-LocalDataCachePath={}", self.cache_directory.display()),
            format!("-abslog={}", log.display()),
        ]);
        args
    }
    fn editor_arguments(&self, root: &Path, run: &Run) -> Vec<String> {
        let mut args = self.common_arguments(root, &run.path("unreal.log"));
        args.extend(["-ExecCmds=r.VSync 0,r.Editor.Viewport.OverridePIEScreenPercentage 0,r.ScreenPercentage 100,r.DynamicRes.OperationMode 0,Automation Now;RunTests SandboxISMC.RemoteBenchmark;Quit".into(),
            "-SandboxISMCBenchmarkEndPIE".into(),format!("-ResX={}",self.width),format!("-ResY={}",self.height),"-ForceRes".into(),"-windowed".into(),
            format!("-SandboxISMCBenchmarkOutput={}",run.directory.display()),format!("-SandboxISMCBenchmarkRunId={}",run.id())]);
        for (key, value) in [
            ("Width", self.width.to_string()),
            ("Height", self.height.to_string()),
            ("Instances", self.instances.to_string()),
            ("UpdatePercent", self.update_percent.to_string()),
            ("Mode", self.mode.clone()),
            ("Visibility", self.visibility.clone()),
            ("Bounds", self.bounds.clone()),
            ("CustomData", self.custom_data.clone()),
            ("Shadows", u8::from(self.shadows).to_string()),
            ("Churn", u8::from(self.churn).to_string()),
            ("MinInstances", self.min_instances.to_string()),
            ("HalfCycleUpdates", self.half_cycle_updates.to_string()),
            ("ReplacementPercent", self.replacement_percent.to_string()),
            ("WarmupUpdates", self.warmup_updates.to_string()),
            ("WarmupSeconds", self.warmup_seconds.to_string()),
            ("Seconds", self.seconds.to_string()),
            ("Trace", u8::from(self.trace).to_string()),
        ] {
            args.push(format!("-SandboxISMCBenchmark{key}={value}"));
        }
        args
    }
}
fn map(root: &Path) -> PathBuf {
    root.join("Plugins/SandboxISMC/Content/Lab/FT_SandboxISMCBenchmark.umap")
}

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Plan {
    pub output: PathBuf,
    pub candidate: Source,
    pub baseline: Source,
    pub sequence: Vec<Repetition>,
    pub ismc: Option<Request>,
    pub validation_only: bool,
}

fn build(root: &Path, editor: &Path) -> Result<()> {
    let engine = editor
        .ancestors()
        .nth(4)
        .ok_or("Editor must be under Engine/Binaries/Win64")?;
    visible(
        root,
        "cmake",
        &[
            "--preset",
            "sandbox-ismc-benchmark",
            &format!("-DUE_ROOT={}", engine.display()),
        ],
    )?;
    visible(
        root,
        "cmake",
        &[
            "--build",
            "--preset",
            "sandbox-ismc-benchmark",
            "--target",
            "editor",
        ],
    )
}

fn require_protocol(root: &Path) -> Result<()> {
    let path = root
        .join("Plugins/SandboxISMC/Source/SandboxISMCLab/Private/SandboxISMCBenchmarkActor.cpp");
    if !fs::read_to_string(path)
        .unwrap_or_default()
        .contains("SandboxISMCBenchmarkRunId=")
    {
        return Err(format!("SandboxISMC in '{}' lacks the run identity and viewport result protocol. Choose a baseline with the current benchmark harness.",root.display()).into());
    }
    Ok(())
}

fn prepare_cache(context: &mut Run, source: &Source, request: &Request, side: &str) -> Result<()> {
    let mut run = Run::new(
        &source.root,
        "sandbox-ismc-cache",
        &context.path("preparation"),
        json!(request),
        "",
        false,
    )?;
    let mut args = request.common_arguments(&source.root, &run.path("unreal.log"));
    args.push("-ExecCmds=r.VSync 0,r.Editor.Viewport.OverridePIEScreenPercentage 0,r.ScreenPercentage 100,r.DynamicRes.OperationMode 0,Editor.AsyncAssetCompilationFinishAll,Automation Now;SoftQuit".into());
    run.manifest["purpose"] = json!("cache-preparation");
    run.manifest["provenance"] = json!({"source":source,"side":side,"arguments":args});
    for name in ["process.log", "unreal.log"] {
        run.expect(name)?;
    }
    context.manifest["artifacts"][format!("cache-{side}")] = json!(run.path("manifest.json"));
    context.publish()?;
    println!(
        "Preparing {side} shader/cache data: {}",
        run.directory.display()
    );
    let result = (|| {
        revision::verify_source(source)?;
        if !map(&source.root).is_file() {
            return Err("SandboxISMC cache preparation map is missing.".into());
        }
        let process = captured(
            Command::new(&request.editor)
                .args(args)
                .current_dir(&source.root),
        )?;
        write_text(
            &run.path("process.log"),
            format!(
                "{}{}",
                String::from_utf8_lossy(&process.stdout),
                String::from_utf8_lossy(&process.stderr)
            ),
        )?;
        succeeded(process)?;
        revision::verify_source(source)?;
        run.validate()
    })();
    run.finish(result)
}

fn measure(plan: &Plan) -> Result<Vec<Capture>> {
    let request = plan
        .ismc
        .as_ref()
        .ok_or("SandboxISMC plan has no workload.")?;
    let mut captures = Vec::new();
    for repetition in &plan.sequence {
        let source = if repetition.side == "baseline" {
            &plan.baseline
        } else {
            &plan.candidate
        };
        let mut run = Run::new(
            &source.root,
            "sandbox-ismc",
            &plan.output.join("runs"),
            json!(request),
            "",
            false,
        )?;
        run.manifest["purpose"] = json!(if plan.validation_only {
            "validation"
        } else {
            "measurement"
        });
        let args = request.editor_arguments(&source.root, &run);
        run.manifest["provenance"] = json!({"source":source,"repetition":repetition,"editor":request.editor,"arguments":args});
        for name in ["metrics.csv", "result.json", "unreal.log", "process.log"] {
            run.expect(name)?;
        }
        if request.trace {
            run.expect("capture.utrace")?;
        }
        let result = (|| -> Result<Capture> {
            run.status("measuring")?;
            let process = captured(
                Command::new(&request.editor)
                    .args(&args)
                    .current_dir(&source.root),
            )?;
            write_text(
                &run.path("process.log"),
                format!(
                    "{}{}",
                    String::from_utf8_lossy(&process.stdout),
                    String::from_utf8_lossy(&process.stderr)
                ),
            )?;
            if !run.path("result.json").is_file() {
                return Err(format!(
                    "Unreal exited with {} without publishing result.json; see '{}'.",
                    process.status,
                    run.directory.display()
                )
                .into());
            }
            let terminal: Value = serde_json::from_slice(&fs::read(run.path("result.json"))?)?;
            if terminal["schemaVersion"] != 1 || terminal["runId"] != run.id() {
                return Err("SandboxISMC result schema/run identity mismatch.".into());
            }
            let conditions: Conditions = serde_json::from_value(terminal["conditions"].clone())?;
            run.manifest["comparability"] = json!(conditions);
            run.publish()?;
            if terminal["complete"] != true {
                return Err(format!("SandboxISMC did not complete: {}", terminal["error"]).into());
            }
            results::validate_conditions(&conditions)?;
            results::validate_request(&conditions, request)?;
            succeeded(process)?;
            run.validate()?;
            Ok(Capture {
                run_id: run.id().into(),
                directory: run.directory.to_string_lossy().into_owned(),
                repetition: repetition.clone(),
                metrics: results::read_metrics(&run.path("metrics.csv"), &conditions)?,
                conditions,
                schema_version: 1,
            })
        })();
        captures.push(run.finish(result)?);
        write_json(&plan.output.join("captures.json"), &captures)?;
    }
    Ok(captures)
}

pub fn execute(root: &Path, args: &[String], comparison: bool) -> Result<()> {
    let parsed = Args::parse(
        args,
        &[
            "--editor",
            "--width",
            "--height",
            "--instances",
            "--update-percent",
            "--mode",
            "--visibility",
            "--bounds",
            "--custom-data",
            "--shadows",
            "--churn",
            "--min-instances",
            "--half-cycle-updates",
            "--replacement-percent",
            "--warmup-updates",
            "--warmup-seconds",
            "--seconds",
            "--trace",
            "--output-dir",
            "--baseline",
            "--baseline-worktree",
            "--repetitions",
            "--warmup-runs",
            "--label",
        ],
        &[
            "--skip-build",
            "--keep-baseline-worktree",
            "--prepare-only",
            "--validate-only",
        ],
    )?;
    let mut settings = Request::parse(&parsed, root)?;
    let prepare = parsed.flag("--prepare-only");
    let validate = parsed.flag("--validate-only");
    if prepare && validate {
        return Err("--prepare-only and --validate-only are mutually exclusive.".into());
    }
    let mut repetitions =
        parsed.integer("--repetitions", if comparison { 2 } else { 1 }, 1, 100)?;
    let mut warmups = parsed.integer("--warmup-runs", 0, 0, 10)?;
    if !comparison
        && (prepare
            || validate
            || parsed.flag("--baseline")
            || parsed.flag("--baseline-worktree")
            || repetitions != 1
            || warmups != 0)
    {
        return Err("Revision and repetition options require sandbox-ismc-revision-ab.".into());
    }
    if validate {
        settings.warmup_updates = 0;
        settings.warmup_seconds = 0.25;
        settings.seconds = 0.5;
        repetitions = 1;
        warmups = 0;
        println!("Validation only: one short A/B pair; no performance conclusions.");
    }
    let supplied = parsed.value("--baseline-worktree", "");
    if comparison && parsed.flag("--skip-build") && supplied.is_empty() {
        return Err("--skip-build requires --baseline-worktree for revision comparisons.".into());
    }
    let command = if comparison {
        "sandbox-ismc-revision-ab"
    } else {
        "sandbox-ismc"
    };
    let parent = format!(".local/benchmarks/{command}");
    let mut context = Run::new(
        root,
        command,
        Path::new(parsed.value("--output-dir", &parent)),
        json!(settings),
        parsed.value("--label", ""),
        true,
    )?;
    context.manifest["purpose"] = json!(if prepare {
        "preparation"
    } else if validate {
        "validation"
    } else {
        "measurement"
    });
    context.publish()?;
    println!("Artifacts: {}", context.directory.display());
    let result = (|| -> Result<()> {
        let mut revisions = if comparison {
            Some(Revisions::new(
                root,
                parsed.required("--baseline")?,
                if supplied.is_empty() {
                    None
                } else {
                    Some(supplied)
                },
                parsed.flag("--keep-baseline-worktree"),
                &context.directory,
            )?)
        } else {
            None
        };
        let operation = (|| -> Result<()> {
            let candidate = if let Some(revisions) = &revisions {
                revisions.candidate.clone()
            } else {
                revision::source(root, Some(&context.directory))?
            };
            let baseline = revisions
                .as_ref()
                .map(|r| r.baseline.clone())
                .unwrap_or_else(|| candidate.clone());
            context.manifest["provenance"] = json!({"candidate":candidate,"baseline":if comparison{Some(&baseline)}else{None},"baselineOwned":revisions.as_ref().map(|r|r.owned),
                "orchestrator":std::env::current_exe()?,"effectiveArguments":args});
            context.publish()?;
            require_protocol(&candidate.root)?;
            if comparison {
                require_protocol(&baseline.root)?;
            }
            if !parsed.flag("--skip-build") {
                build(&candidate.root, &settings.editor)?;
                if let Some(revisions) = &revisions {
                    if revisions.owned {
                        revision::initialize_submodules(
                            &baseline.root,
                            Some(&candidate.root),
                            None,
                        )?;
                    }
                    build(&baseline.root, &settings.editor)?;
                }
            }
            revision::verify_source(&candidate)?;
            revision::verify_source(&baseline)?;
            if prepare {
                context.expect("preparation.json")?;
                write_json(
                    &context.path("preparation.json"),
                    &json!({"candidate":candidate,"baseline":baseline,"baselineOwned":revisions.as_ref().unwrap().owned,
                    "retainedBaselinePath":baseline.root,"workload":settings}),
                )?;
                context.status("prepared")?;
                revisions.as_mut().unwrap().keep = true;
                println!("Prepared baseline retained: {}", baseline.root.display());
                return Ok(());
            }
            prepare_cache(&mut context, &candidate, &settings, "candidate")?;
            if comparison {
                prepare_cache(&mut context, &baseline, &settings, "baseline")?;
            }
            let sequence = if comparison {
                revision::balanced(repetitions, warmups)
            } else {
                vec![Repetition {
                    sequence: 1,
                    repetition: 1,
                    side: "candidate".into(),
                    warmup: false,
                }]
            };
            let plan = Plan {
                output: context.directory.clone(),
                candidate,
                baseline,
                sequence,
                ismc: Some(settings),
                validation_only: validate,
            };
            for name in ["sequence.json", "measurement-plan.json"] {
                context.manifest["artifacts"][name] = json!(context.path(name));
            }
            write_json(&context.path("sequence.json"), &plan.sequence)?;
            write_json(&context.path("measurement-plan.json"), &plan)?;
            context.expect("captures.json")?;
            context.status("measuring")?;
            revision::verify_source(&plan.candidate)?;
            revision::verify_source(&plan.baseline)?;
            let captures = measure(&plan)?;
            revision::verify_source(&plan.candidate)?;
            revision::verify_source(&plan.baseline)?;
            context.validate()?;
            context.manifest["comparability"] = json!(captures[0].conditions);
            if comparison {
                let comparable =
                    results::reports(&context.directory, &context.manifest, &plan, &captures)?;
                for name in ["comparison.json", "comparison.csv", "comparison.md"] {
                    context.expect(name)?;
                }
                if !comparable {
                    context.manifest["failure"] =
                        json!("Comparison conditions or metric identities differ.");
                    context.status("incomparable")?;
                    return Err("Comparison is incomparable; see comparison.json.".into());
                }
            }
            context.status("complete")
        })();
        if let Some(revisions) = revisions {
            revisions.finish(operation)
        } else {
            operation
        }
    })();
    if let Err(error) = &result {
        if context.manifest["status"] != "incomparable" {
            context.manifest["failure"] = json!(error.to_string());
            context.status("failed")?;
        }
    }
    result
}

pub fn report(args: &[String]) -> Result<()> {
    let args = Args::parse(args, &["--run-dir"], &[])?;
    let directory = absolute(&std::env::current_dir()?, args.required("--run-dir")?)?;
    let manifest: Value = serde_json::from_slice(&fs::read(directory.join("manifest.json"))?)?;
    if manifest["schemaVersion"] != 1
        || manifest["benchmark"] != "sandbox-ismc-revision-ab"
        || !["complete", "incomparable"].contains(&manifest["status"].as_str().unwrap_or(""))
    {
        return Err("Run directory is not a completed SandboxISMC revision comparison.".into());
    }
    let plan: Plan = serde_json::from_slice(&fs::read(directory.join("measurement-plan.json"))?)?;
    let sequence: Vec<Repetition> =
        serde_json::from_slice(&fs::read(directory.join("sequence.json"))?)?;
    if sequence != plan.sequence {
        return Err("Comparison plan and sequence disagree.".into());
    }
    let captures: Vec<Capture> =
        serde_json::from_slice(&fs::read(directory.join("captures.json"))?)?;
    if !results::reports(&directory, &manifest, &plan, &captures)? {
        return Err("Comparison is incomparable.".into());
    }
    println!("Reports regenerated: {}", directory.display());
    Ok(())
}

#[cfg(test)]
#[path = "../tests/ismc/mod.rs"]
mod tests;
