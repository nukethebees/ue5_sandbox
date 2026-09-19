using System.Globalization;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class NativeSimulationBenchmarkCommandTests
{
    [TestMethod]
    public void TryParse_accepts_required_arguments_and_defaults()
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "LevelScripts/BenchmarkFleet_10.scm", "--seconds", "20"],
            out var request,
            out var error);

        Assert.IsTrue(parsed, error);
        Assert.IsNotNull(request);
        Assert.AreEqual(20.0, request.Seconds);
        Assert.AreEqual(1U, request.GameSpeed);
        Assert.AreEqual(5.0, request.WarmupSeconds);
        Assert.AreEqual(60.0, request.SaturationTimeoutSeconds);
        Assert.AreEqual("native-simulation-benchmark", request.BuildPreset);
        Assert.IsFalse(request.SkipBuild);
    }

    [DataTestMethod]
    [DataRow(new string[] { "--seconds", "20" })]
    [DataRow(new string[] { "--level", "level.scm" })]
    public void TryParse_rejects_missing_required_arguments(string[] arguments)
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(arguments, out _, out var error);

        Assert.IsFalse(parsed);
        StringAssert.Contains(error, "required");
    }

    [TestMethod]
    public void TryParse_parses_single_fighter_stress_cap()
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "level.scm", "--seconds", "1", "--fighter-stress-cap", "2000"],
            out var request,
            out var error);

        Assert.IsTrue(parsed, error);
        Assert.AreEqual(2000U, request!.FighterStressCap);
        Assert.AreEqual(0, request.FighterStressCaps.Count);
    }

    [DataTestMethod]
    [DataRow("0")]
    [DataRow("-1")]
    [DataRow("1000,2000")]
    [DataRow("not-a-cap")]
    public void TryParse_rejects_invalid_single_fighter_stress_cap(string cap)
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "level.scm", "--seconds", "1", "--fighter-stress-cap", cap],
            out _,
            out var error);

        Assert.IsFalse(parsed);
        StringAssert.Contains(error, "fighter-stress-cap");
    }

    [TestMethod]
    public void TryParse_parses_multiple_fighter_stress_caps()
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "level.scm", "--seconds", "1", "--fighter-stress-caps", "1000, 2000,4000"],
            out var request,
            out var error);

        Assert.IsTrue(parsed, error);
        CollectionAssert.AreEqual(new uint[] { 1000, 2000, 4000 }, request!.FighterStressCaps.ToArray());
    }

    [DataTestMethod]
    [DataRow("2000,2000")]
    [DataRow("0")]
    [DataRow("-1")]
    [DataRow("not-a-cap")]
    [DataRow("2000,")]
    public void TryParse_rejects_invalid_fighter_stress_caps(string caps)
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "level.scm", "--seconds", "1", "--fighter-stress-caps", caps],
            out _,
            out var error);

        Assert.IsFalse(parsed);
        StringAssert.Contains(error, "fighter-stress-caps");
    }

    [TestMethod]
    public void TryParse_rejects_mutually_exclusive_fighter_stress_options()
    {
        var parsed = NativeSimulationBenchmarkCommand.TryParse(
            ["--level", "level.scm", "--seconds", "1", "--fighter-stress-cap", "1000", "--fighter-stress-caps", "2000"],
            out _,
            out var error);

        Assert.IsFalse(parsed);
        StringAssert.Contains(error, "mutually exclusive");
    }

    [TestMethod]
    public void ToBenchmarkArguments_uses_invariant_culture_and_propagates_stress_options()
    {
        var previous_culture = CultureInfo.CurrentCulture;
        try
        {
            CultureInfo.CurrentCulture = new CultureInfo("fr-FR");
            var request = new NativeSimulationBenchmarkRequest(
                @"C:\benchmarks with spaces\level.scm",
                1.25,
                100,
                true,
                null,
                [1000, 2000],
                5.5,
                60.25,
                "benchmark preset",
                false);

            CollectionAssert.AreEqual(
                new[]
                {
                    "--level", @"C:\benchmarks with spaces\level.scm", "--seconds", "1.25", "--game-speed", "100",
                    "--telemetry", "--fighter-stress-caps", "1000", "2000", "--warmup-seconds", "5.5",
                    "--saturation-timeout-seconds", "60.25",
                },
                request.ToBenchmarkArguments().ToArray());
        }
        finally
        {
            CultureInfo.CurrentCulture = previous_culture;
        }
    }

    [TestMethod]
    public void ToCommandArguments_only_includes_warmup_and_saturation_for_fighter_stress()
    {
        var request = new NativeSimulationBenchmarkRequest(
            "level.scm",
            1.0,
            1,
            false,
            null,
            [],
            5.0,
            60.0,
            "native-simulation-benchmark",
            false);

        CollectionAssert.DoesNotContain(request.ToCommandArguments(include_skip_build: false).ToList(), "--warmup-seconds");
        CollectionAssert.DoesNotContain(request.ToCommandArguments(include_skip_build: false).ToList(), "--saturation-timeout-seconds");
    }
}
