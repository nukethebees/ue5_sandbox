from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Any
import unittest

from matrix import validate_preset_references

PRESET_DIRECTORY = Path(__file__).resolve().parent
TIDY_SCOPES = {
    "core": ("core", "profiling"),
    "simulation": ("simulation", "simulation_benchmark"),
    "layout": ("layout",),
    "lispb": ("lispb",),
    "memory": ("memory",),
    "level-authoring": ("level_authoring",),
    "s7": ("s7",),
    "image": ("image",),
    "mesh-gen": ("mesh_gen",),
}


class NativeWorkflowTests(unittest.TestCase):
    source_dir: Path
    cmake: str
    llvm_root: str

    presets = json.loads((PRESET_DIRECTORY / "native.json").read_text(encoding="utf-8"))
    unreal_presets = json.loads(
        (PRESET_DIRECTORY / "unreal.json").read_text(encoding="utf-8")
    )

    def test_llvm_root_selects_tools_without_development_packages(self) -> None:
        base = json.loads((PRESET_DIRECTORY / "base.json").read_text(encoding="utf-8"))
        self.assertNotIn("LLVM_ROOT", base["configurePresets"][0].get("cacheVariables", {}))
        with tempfile.TemporaryDirectory(prefix="sandbox llvm root ") as directory:
            fixture = Path(directory)
            llvm_root = fixture / "LLVM with spaces"
            (llvm_root / "bin").mkdir(parents=True)
            names = ("clang-cl", "clang", "clang-tidy", "clang-format", "clang-scan-deps", "run-clang-tidy",
                     "llvm-lib", "llvm-nm", "llvm-readobj")
            suffix = ".exe" if sys.platform == "win32" else ""
            for name in names:
                tool = llvm_root / "bin" / (name + suffix)
                tool.touch()
                tool.chmod(0o755)
            other_root = fixture / "Other LLVM"
            shutil.copytree(llvm_root, other_root)
            script = fixture / "verify.cmake"
            script.write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                f'include("{self.source_dir.as_posix()}/cmake/llvm_tools.cmake")\n'
                'get_property(cached_root CACHE LLVM_ROOT PROPERTY VALUE)\n'
                'if(NOT cached_root STREQUAL expected_root)\n'
                '  message(FATAL_ERROR "Wrong LLVM_ROOT cache: ${cached_root}")\n'
                'endif()\n'
                'if(DEFINED probe_name)\n'
                '  ioj_find_llvm_tool(selected "${probe_name}")\n'
                '  return()\n'
                'endif()\n'
                f'foreach(name IN ITEMS {" ".join(names)})\n'
                '  set(selected "old installation" CACHE FILEPATH "" FORCE)\n'
                '  ioj_find_llvm_tool(selected "${name}")\n'
                f'  if(NOT selected STREQUAL "{llvm_root.as_posix()}/bin/${{name}}{suffix}")\n'
                '    message(FATAL_ERROR "Wrong LLVM tool: ${selected}")\n'
                '  endif()\n'
                'endforeach()\n'
                'function(find_package)\n'
                '  message(FATAL_ERROR "Non-tidy configuration requested development packages")\n'
                'endfunction()\n'
                'set(IOJ_ENABLE_CLANG_TIDY OFF)\n'
                f'include("{self.source_dir.as_posix()}/cmake/clang_tidy/CMakeLists.txt")\n',
                encoding="utf-8",
            )
            for selection in ("explicit", "environment", "path", "empty"):
                with self.subTest(selection=selection):
                    environment = os.environ.copy()
                    environment.pop("LLVM_ROOT", None)
                    environment["PATH"] = str(other_root / "bin") + os.pathsep + environment["PATH"]
                    arguments: list[str] = []
                    if selection == "explicit":
                        environment["LLVM_ROOT"] = str(fixture / "wrong installation")
                        arguments.append(f"-DLLVM_ROOT:PATH={llvm_root.as_posix()}")
                    elif selection == "environment":
                        environment["LLVM_ROOT"] = llvm_root.as_posix()
                    else:
                        environment["PATH"] = str(llvm_root / "bin") + os.pathsep + environment["PATH"]
                        if selection == "empty":
                            environment["LLVM_ROOT"] = str(fixture / "wrong installation")
                            arguments.append("-DLLVM_ROOT:PATH=")
                    expected = "" if selection in ("path", "empty") else llvm_root.as_posix()
                    result = subprocess.run(
                        [self.cmake, *arguments, f"-Dexpected_root={expected}", "-P", str(script)],
                        env=environment, capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            # Each missing tool must fail even when another installation has it on PATH.
            environment = os.environ.copy()
            environment["PATH"] = str(other_root / "bin") + os.pathsep + environment["PATH"]
            for name in names:
                with self.subTest(missing_tool=name):
                    tool = llvm_root / "bin" / (name + suffix)
                    tool.unlink()
                    result = subprocess.run(
                        [self.cmake, f"-DLLVM_ROOT:PATH={llvm_root.as_posix()}",
                         f"-Dexpected_root={llvm_root.as_posix()}", f"-Dprobe_name={name}",
                         "-P", str(script)],
                        env=environment, capture_output=True, text=True,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Could not find llvm_tool", result.stdout + result.stderr)
                    tool.touch()
                    tool.chmod(0o755)

    @unittest.skipUnless(shutil.which("pwsh"), "requires PowerShell")
    def test_tidy_runner_forwards_arguments_and_exit_status(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox tidy runner ") as directory:
            fixture = Path(directory)
            runner = fixture / "run clang tidy.py"
            captured = fixture / "captured.json"
            runner.write_text(
                "import json, pathlib, sys\n"
                "pathlib.Path(__file__).with_name('captured.json').write_text(json.dumps(sys.argv[1:]))\n"
                "print('runner output')\n"
                "sys.exit(7)\n", encoding="utf-8",
            )
            tidy = fixture / "clang tidy.exe"
            log = fixture / "logs" / "tidy.log"
            result = subprocess.run([
                "pwsh", "-NoProfile", "-File", str(self.source_dir / "cmake/clang_tidy/run_clang_tidy.ps1"),
                "-PythonExecutable", sys.executable, "-RunClangTidyExecutable", str(runner),
                "-ClangTidyExecutable", str(tidy), "-LogFile", str(log),
                "-CompilationDatabase", str(fixture), "-Jobs", "2", "-SourceFilter", "simulation|benchmark",
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
            self.assertEqual(json.loads(captured.read_text()), [
                "-quiet", "-clang-tidy-binary", str(tidy),
                "-p", str(fixture), "-j", "2", "simulation|benchmark",
            ])
            self.assertIn("runner output", log.read_text(encoding="utf-8-sig"))
            runner_source = (self.source_dir / "cmake/clang_tidy/run_clang_tidy.ps1").read_text()
            self.assertNotIn("Plugin", runner_source)
            self.assertNotIn("-load", runner_source)

    def test_tidy_presets_share_one_configure_tree(self) -> None:
        validate_preset_references(self.presets)
        configure_name = "win-x64-clangcl-debug-tidy"
        configure_presets = self.presets["configurePresets"]
        self.assertEqual(
            [preset["name"] for preset in configure_presets if "tidy" in preset["name"]],
            [configure_name],
        )
        tidy_configuration = next(
            preset for preset in configure_presets if preset["name"] == configure_name
        )
        self.assertEqual(
            tidy_configuration["binaryDir"],
            "${sourceDir}/out/build/win-x64-clangcl-debug/clang-tidy",
        )
        self.assertEqual(
            tidy_configuration["cacheVariables"],
            {
                "IOJ_ENABLE_CLANG_TIDY": True,
                "CMAKE_DISABLE_PRECOMPILE_HEADERS": True,
                "CMAKE_CXX_SCAN_FOR_MODULES": False,
                "CMAKE_EXPORT_COMPILE_COMMANDS": True,
            },
        )
        build_presets = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}
        targets = {configure_name: "native-clang-tidy"}
        targets.update(
            (f"clang-tidy-{scope}", f"native-clang-tidy-{scope}")
            for scope in TIDY_SCOPES
        )
        self.assertEqual(
            {name for name in build_presets if "tidy" in name}, set(targets)
        )
        self.assertEqual({name for name in workflows if "tidy" in name}, set(targets))
        for name, target in targets.items():
            with self.subTest(preset=name):
                self.assertEqual(build_presets[name]["configurePreset"], configure_name)
                self.assertEqual(build_presets[name]["targets"], [target])
                self.assertEqual(
                    workflows[name]["steps"],
                    [
                        {"type": "configure", "name": configure_name},
                        {"type": "build", "name": name},
                    ],
                )

    def test_tidy_targets_and_source_filters(self) -> None:
        self.check_tidy_targets_and_source_filters(has_ioj=False)

    def test_tidy_detects_builtin_ioj_checks(self) -> None:
        self.check_tidy_targets_and_source_filters(has_ioj=True)

    def check_tidy_targets_and_source_filters(self, has_ioj: bool) -> None:
        # Exercise the real CMake targets, capturing their runner arguments without
        # requiring LLVM or configuring the native dependency graph.
        with tempfile.TemporaryDirectory(prefix="sandbox tidy workflow ") as root:
            fixture = Path(root)
            build = fixture / "build with spaces"
            (fixture / "capture.py").write_text(
                "import json, pathlib, sys\n"
                "args = sys.argv[1:]\n"
                "log = pathlib.Path(args[args.index('-LogFile') + 1])\n"
                "if log.stem in ('clang-tidy', 'clang-tidy-lispb'):\n"
                "    for name in ('generate-native-soa-fixture', 'kernel-native-generated-sources'):\n"
                "        assert (log.parent / (name + '.stamp')).is_file(), name\n"
                "log.with_suffix('.json').write_text(json.dumps(args), encoding='utf-8')\n",
                encoding="utf-8",
            )
            (fixture / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.28)\n"
                "project(TidyWorkflow NONE)\n"
                f'set(PROJECT_SOURCE_DIR "{self.source_dir.as_posix()}")\n'
                "set(IOJ_ENABLE_CLANG_TIDY TRUE)\n"
                "enable_testing()\n"
                "set(IOJ_IS_CLANG_CL TRUE)\n"
                f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
                'set(IOJ_CLANG_TIDY_EXECUTABLE "${CMAKE_COMMAND}")\n'
                'set(IOJ_RUN_CLANG_TIDY_EXECUTABLE "${CMAKE_COMMAND}")\n'
                'set(IOJ_POWERSHELL_EXECUTABLE "${CMAKE_COMMAND}")\n'
                "function(ioj_find_llvm_tool output name)\n"
                '  set(${output} "${CMAKE_COMMAND}" PARENT_SCOPE)\n'
                "endfunction()\n"
                "function(find_package)\n"
                '  message(FATAL_ERROR "Tidy workflow requested development packages")\n'
                "endfunction()\n"
                "function(execute_process)\n"
                '  cmake_parse_arguments(query "" "OUTPUT_VARIABLE;RESULT_VARIABLE" "" ${ARGN})\n'
                '  set(${query_OUTPUT_VARIABLE} "Enabled checks:\\n    modernize-use-nullptr\\n'
                + ('    ioj-loop-condition-call\\n' if has_ioj else '')
                + '" PARENT_SCOPE)\n'
                '  set(${query_RESULT_VARIABLE} 0 PARENT_SCOPE)\n'
                "endfunction()\n"
                "function(add_subdirectory directory)\n"
                '  message(FATAL_ERROR "Tidy workflow tried to build an extra target: ${directory}")\n'
                "endfunction()\n"
                "function(sandbox_jobserver_command output)\n"
                '  set(${output} "${Python3_EXECUTABLE}" '
                '"${CMAKE_CURRENT_SOURCE_DIR}/capture.py" PARENT_SCOPE)\n'
                "endfunction()\n"
                'include("${PROJECT_SOURCE_DIR}/cmake/clang_tidy/CMakeLists.txt")\n'
                "foreach(prerequisite IN ITEMS generate-native-soa-fixture kernel-native-generated-sources)\n"
                '  set(output "${CMAKE_BINARY_DIR}/${prerequisite}.stamp")\n'
                '  add_custom_command(OUTPUT "${output}"\n'
                '    COMMAND "${CMAKE_COMMAND}" -E touch "${output}" VERBATIM)\n'
                '  add_custom_target(${prerequisite} DEPENDS "${output}")\n'
                "endforeach()\n"
                "sandbox_configure_native_clang_tidy()\n"
                "sandbox_add_native_clang_tidy_target()\n"
                "get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)\n"
                "foreach(target IN LISTS targets)\n"
                '  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/${target}.deps"\n'
                '    CONTENT "$<TARGET_PROPERTY:${target},MANUALLY_ADDED_DEPENDENCIES>")\n'
                "endforeach()\n",
                encoding="utf-8",
            )
            configured = self.run_cmake("-S", str(fixture), "-B", str(build), "-G", "Ninja")
            self.assertIn("built-in IOJ checks" if has_ioj else "standard checks only", configured)
            registered_tests = (build / "CTestTestfile.cmake").read_text()
            self.assertEqual("ClangTidy.LoopConditionCall" in registered_tests, has_ioj)
            if has_ioj:
                self.assertIn("tools/llvm/clang_tidy/tests/test_loop_condition_call.py", registered_tests)
            targets = [
                target
                for preset in self.presets["buildPresets"]
                if "tidy" in preset["name"]
                for target in preset["targets"]
            ]
            prerequisites = {"generate-native-soa-fixture", "kernel-native-generated-sources"}
            for target in targets:
                dependencies = set(
                    (build / f"{target}.deps").read_text(encoding="utf-8").split(";")
                ) - {""}
                with self.subTest(target=target):
                    self.assertEqual(
                        dependencies,
                        prerequisites if target in ("native-clang-tidy", "native-clang-tidy-lispb") else set(),
                    )

            outputs = [build / f"{name}.stamp" for name in prerequisites]
            self.run_cmake("--build", str(build), "--target", "native-clang-tidy-core")
            self.assertTrue(all(not output.exists() for output in outputs))
            for target in ("native-clang-tidy-lispb", "native-clang-tidy"):
                with self.subTest(fresh_target=target):
                    self.run_cmake("--build", str(build), "--target", target)
                    self.assertTrue(all(output.is_file() for output in outputs))
                    for output in outputs:
                        os.utime(output, ns=(1_000_000_000, 1_000_000_000))
                    self.run_cmake("--build", str(build), "--target", target)
                    for output in outputs:
                        self.assertEqual(output.stat().st_mtime_ns, 1_000_000_000)
                        output.unlink()

            self.run_cmake("--build", str(build), "--target", *targets)
            filters: dict[str, re.Pattern[str]] = {}
            for name in ("clang-tidy", *(f"clang-tidy-{scope}" for scope in TIDY_SCOPES)):
                args = json.loads((build / f"{name}.json").read_text(encoding="utf-8"))
                self.assertEqual(Path(args[args.index("-CompilationDatabase") + 1]), build)
                self.assertEqual(args[args.index("-Jobs") + 1], "0")
                self.assertNotIn("-Plugin", args)
                self.assertNotIn("-load", args)
                self.assertEqual(
                    Path(args[args.index("-File") + 1]),
                    self.source_dir / "cmake/clang_tidy/run_clang_tidy.ps1",
                )
                filters[name] = re.compile(args[args.index("-SourceFilter") + 1])

        full_filter = filters.pop("clang-tidy")
        included = {
            f"{directory}/src/example.cpp": scope
            for scope, directories in TIDY_SCOPES.items()
            for directory in directories
        }
        # File names containing 'generated' alone have never been excluded.
        included["lispb/tests/generated_tests.cpp"] = "lispb"
        excluded = (
            "third_party/example/src/example.cpp",
            "sbx_mimalloc/src/example.cpp",
            "s7/lib/src/s7_sandbox.cpp",
            "lispb/kernel/tests/standard_anchor_tests.cpp",
            "simulation/src/lasers/phase_interface.cpp",
            "simulation/src/fighters/phase_interface.cpp",
            "core/src/generated/example.cpp",
            "core/src/generated_kernels/example.cpp",
            "core/tests/compile/example.cpp",
            "lispb/tests/compile_fixture/example.cpp",
            "core/tests/static_string_view_reject.cpp",
            "core/include/example.h",
        )
        native_root = self.source_dir / "native"
        for separator in ("/", "\\"):
            for relative, scope in included.items():
                path = (native_root / relative).as_posix().replace("/", separator)
                with self.subTest(path=path):
                    self.assertIsNotNone(full_filter.search(path))
                    self.assertEqual(
                        [name for name, pattern in filters.items() if pattern.search(path)],
                        [f"clang-tidy-{scope}"],
                    )
            for relative in excluded:
                path = (native_root / relative).as_posix().replace("/", separator)
                with self.subTest(path=path):
                    self.assertIsNone(full_filter.search(path))
                    self.assertFalse(any(pattern.search(path) for pattern in filters.values()))
            outside = (self.source_dir / "tools/example.cpp").as_posix().replace("/", separator)
            self.assertIsNone(full_filter.search(outside))
            unscoped = (native_root / "core_extra/src/example.cpp").as_posix().replace("/", separator)
            self.assertIsNotNone(full_filter.search(unscoped))
            self.assertFalse(any(pattern.search(unscoped) for pattern in filters.values()))

        for source in native_root.rglob("*.cpp"):
            path = source.as_posix()
            with self.subTest(source=path):
                selected = sum(bool(pattern.search(path)) for pattern in filters.values())
                self.assertEqual(selected, int(bool(full_filter.search(path))))

    def test_native_preset_uses_the_unreal_disabled_default(self) -> None:
        configure_presets = {
            preset["name"]: preset for preset in self.presets["configurePresets"]
        }

        native = configure_presets["native"]
        self.assertEqual(native["inherits"], "win-x64-clangcl-debug-unity")
        self.assertFalse(
            configure_presets["native-common"]["cacheVariables"]["IOJ_WITH_UNREAL"]
        )

        for path in PRESET_DIRECTORY.glob("*.json"):
            document = json.loads(path.read_text(encoding="utf-8"))
            for preset in document.get("configurePresets", []):
                with self.subTest(file=path.name, preset=preset["name"]):
                    self.assertFalse(
                        any(
                            key.startswith("SANDBOX_")
                            for key in preset.get("cacheVariables", {})
                        ),
                        "Project build cache keys must use the IOJ_ prefix",
                    )

    def test_native_workflows_build_and_run_native_only_validation(self) -> None:
        build_presets = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        test_presets = {preset["name"]: preset for preset in self.presets["testPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}

        self.assertEqual(build_presets["native"]["targets"], ["native-tests"])
        self.assertEqual(
            test_presets["native-tests"]["filter"]["include"]["label"], "^native$"
        )
        self.assertEqual(
            workflows["native-tests"]["steps"],
            [
                {"type": "configure", "name": "native"},
                {"type": "build", "name": "native"},
                {"type": "test", "name": "native-tests"},
            ],
        )

    def test_tool_workflow_is_explicit_and_normal_workflows_exclude_it(self) -> None:
        test_presets = {preset["name"]: preset for preset in self.presets["testPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}
        unreal_test_presets = {
            preset["name"]: preset for preset in self.unreal_presets["testPresets"]
        }

        self.assertEqual(
            test_presets["tool-tests"]["filter"]["include"]["label"],
            "^developer-tool$",
        )
        self.assertEqual(
            workflows["tool-tests"]["steps"],
            [
                {"type": "configure", "name": "native"},
                {"type": "build", "name": "tool-tests"},
                {"type": "test", "name": "tool-tests"},
            ],
        )
        self.assertEqual(
            unreal_test_presets["debug-game-tests"]["filter"]["include"]["label"],
            "^all$",
        )
        self.assertEqual(
            unreal_test_presets["debug-game-full-tests"]["filter"]["include"]["label"],
            "^(all|developer-tool)$",
        )
        self.assertEqual(unreal_test_presets["debug-game-unit-tests"]["filter"]["exclude"]["label"],
                         "^developer-tool$")

    def test_simulation_presets_separate_iteration_from_final_validation(self) -> None:
        builds = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        tests = {preset["name"]: preset for preset in self.presets["testPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}
        normal, soak, full = "native-simulation-tests", "native-simulation-soak-tests", "native-simulation-full-tests"
        self.assertEqual(builds[normal]["targets"], [normal])
        self.assertEqual(builds[soak]["targets"], [soak])
        self.assertEqual(builds[full]["targets"], [normal, soak])
        self.assertEqual(tests[normal]["filter"], {
            "include": {"label": "^native-simulation$"}, "exclude": {"label": "soak|compile-contract"}})
        self.assertEqual(tests[full]["filter"], {"include": {"label": "^native-simulation$"}})
        self.assertEqual(tests[soak]["filter"], {
            "include": {"label": "^native-simulation$", "name": "^native-simulation-soak-tests$"}})
        for name in (normal, soak, full):
            self.assertEqual(workflows[name]["steps"], [
                {"type": "configure", "name": "native"}, {"type": "build", "name": name},
                {"type": "test", "name": name}])

    def test_native_target_dry_run_has_no_unreal_dependency(self) -> None:
        generated_sources = self.create_generated_source_sentinels()
        with tempfile.TemporaryDirectory(
            prefix="sandbox native workflow "
        ) as temporary_root:
            build_directory = Path(temporary_root) / "build with spaces"
            policy_check = Path(temporary_root) / "check_simulation_policy.cmake"
            policy_check.write_text('''
function(check_simulation_policy)
  foreach(directory IN ITEMS native/simulation native/simulation/tests)
    get_property(targets DIRECTORY "${PROJECT_SOURCE_DIR}/${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
      get_target_property(runtime ${target} MSVC_RUNTIME_LIBRARY)
      get_target_property(definitions ${target} COMPILE_DEFINITIONS)
      get_target_property(options ${target} COMPILE_OPTIONS)
      if(NOT runtime STREQUAL "MultiThreadedDLL" OR NOT "_ITERATOR_DEBUG_LEVEL=0" IN_LIST definitions)
        message(FATAL_ERROR "${target} omitted simulation ABI policy")
      endif()
      if(IOJ_MSVC_FRONTEND AND NOT "/permissive-" IN_LIST options)
        message(FATAL_ERROR "${target} omitted simulation compiler policy")
      endif()
    endforeach()
  endforeach()
  file(WRITE "${CMAKE_BINARY_DIR}/policy_fixture.cpp" "void policy_fixture() {}\n")
  foreach(frontend IN ITEMS ON OFF)
    foreach(configuration IN ITEMS Debug Development Shipping Test)
      set(IOJ_MSVC_FRONTEND ${frontend})
      set(UE_CONFIGURATION ${configuration})
      set(target policy-${frontend}-${configuration})
      add_library(${target} OBJECT EXCLUDE_FROM_ALL "${CMAKE_BINARY_DIR}/policy_fixture.cpp")
      configure_native_simulation_target(${target})
      get_target_property(options ${target} COMPILE_OPTIONS)
      get_target_property(definitions ${target} COMPILE_DEFINITIONS)
      if(frontend)
        set(optimization /O2)
      else()
        set(optimization -O2)
      endif()
      if(configuration STREQUAL "Debug")
        if("${optimization}" IN_LIST options OR "NDEBUG" IN_LIST definitions)
          message(FATAL_ERROR "Debug simulation policy unexpectedly optimized")
        endif()
      elseif(NOT "${optimization}" IN_LIST options)
        message(FATAL_ERROR "${configuration} simulation policy omitted optimization")
      endif()
      if(configuration MATCHES "^(Shipping|Test)$" AND NOT "NDEBUG" IN_LIST definitions)
        message(FATAL_ERROR "${configuration} simulation policy omitted NDEBUG")
      endif()
    endforeach()
  endforeach()
endfunction()
cmake_language(DEFER CALL check_simulation_policy)
''', encoding="utf-8")
            self.run_cmake(
                "--preset", "native",
                "-S",
                str(self.source_dir),
                "-B",
                str(build_directory),
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Debug",
                "-DCMAKE_UNITY_BUILD=ON",
                "-DIOJ_WITH_UNREAL=OFF",
                f"-DLLVM_ROOT={self.llvm_root}",
                f"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={policy_check.as_posix()}",
            )

            cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
            self.assertIn("IOJ_WITH_UNREAL:BOOL=OFF", cache)
            compiler = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, re.MULTILINE)
            assert compiler is not None
            expected_compiler = (Path(self.llvm_root) / "bin/clang-cl.exe" if self.llvm_root
                                 else Path(shutil.which("clang-cl") or "clang-cl"))
            self.assertEqual(Path(compiler[1]), expected_compiler)
            if self.llvm_root:
                archiver = re.search(r"^CMAKE_AR:[^=]+=(.+)$", cache, re.MULTILINE)
                assert archiver is not None
                self.assertEqual(Path(archiver[1]), Path(self.llvm_root) / "bin/llvm-lib.exe")
                scanner = re.search(r"^CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS:[^=]+=(.+)$", cache, re.MULTILINE)
                assert scanner is not None
                self.assertEqual(Path(scanner[1]), Path(self.llvm_root) / "bin/clang-scan-deps.exe")
            for variable, name in (("SBX_LLVM_NM", "llvm-nm"), ("SBX_LLVM_READOBJ", "llvm-readobj")):
                selected = re.search(rf"^{variable}:[^=]+=(.+)$", cache, re.MULTILINE)
                assert selected is not None
                expected = (Path(self.llvm_root) / "bin" / f"{name}.exe" if self.llvm_root
                            else Path(shutil.which(name) or name))
                self.assertEqual(Path(selected[1]), expected)
            self.assertNotIn("LLVM_DIR:", cache)
            self.assertNotIn("Clang_DIR:", cache)

            dry_run = self.run_cmake(
                "--build",
                str(build_directory),
                "--target",
                "native-tests",
                "--",
                "-t",
                "commands",
            )
            self.assertNotRegex(dry_run, r"UnrealBuildTools|UnrealEditor|RunUBT")
            self.assertNotRegex(dry_run, r"(?i)(?:^|[\\/\s])unreal(?:[\\/\s]|$)")
            self.assertNotIn("Tools.slnx", dry_run)
            self.assertNotIn("dotnet.exe\" test", dry_run)
            self.assertNotIn("layout-planner-ui-tests", dry_run)
            self.assertNotIn("image-lab-tests", dry_run)
            self.assertIn("native-simulation-soak-tests", dry_run)
            self.check_test_inventory(build_directory)

            host_tool = (
                build_directory
                / "host-tools"
                / "NativeBinaryTools"
                / "Debug"
                / "NativeBinaryTools.exe"
            )
            self.assertIn(str(host_tool), dry_run)
            self.assertNotIn("tools/bin/NativeBinaryTools.exe", dry_run)

            build_ninja = (build_directory / "build.ninja").read_text(encoding="utf-8")
            verify_globs = build_directory / "CMakeFiles/VerifyGlobs.cmake"
            if verify_globs.exists():
                owners = re.findall(
                    r"^# .* at (.+):\d+ \(file\)$",
                    verify_globs.read_text(encoding="utf-8"),
                    re.MULTILINE,
                )
                self.assertTrue(owners)
                for owner in owners:
                    self.assertTrue(owner.startswith("native/third_party/"), owner)
            normalized_build_ninja = build_ninja.replace("\\", "/").replace("$:", ":")
            native_binary_tools_directory = self.source_dir / "tools" / "NativeBinaryTools"
            self.assertIn(
                (native_binary_tools_directory / "NativeBinaryTools.csproj").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "Program.cs").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (self.source_dir / "tools" / "Directory.Build.targets").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "obj").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "bin").as_posix(),
                normalized_build_ninja,
            )
            for generated_source in generated_sources:
                self.assertNotIn(generated_source.as_posix(), normalized_build_ninja)

    @unittest.skipUnless(sys.platform == "win32", "requires Windows Ninja semantics")
    def test_ninja_direct_build_regenerates_after_configure_input_changes(self) -> None:
        top_level_cmake = (self.source_dir / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("CMAKE_SUPPRESS_REGENERATION", top_level_cmake)

        with tempfile.TemporaryDirectory(prefix="sandbox ninja regeneration ") as temporary_root:
            fixture_directory = Path(temporary_root) / "fixture"
            fixture_directory.mkdir()
            cmake_lists = fixture_directory / "CMakeLists.txt"
            template = fixture_directory / "configured.txt.in"
            build_directory = fixture_directory / "build"
            template.write_text("@configured_value@\n", encoding="utf-8")
            cmake_lists.write_text(
                "\n".join(
                    (
                        "cmake_minimum_required(VERSION 3.25)",
                        "project(NinjaRegenerationFixture LANGUAGES NONE)",
                        "set(configured_value before)",
                        "configure_file(configured.txt.in configured.txt @ONLY)",
                        "add_custom_target(verify_configuration ALL DEPENDS configured.txt)",
                        "",
                    )
                ),
                encoding="utf-8",
            )

            self.run_cmake("-S", str(fixture_directory), "-B", str(build_directory), "-G", "Ninja")
            self.run_cmake("--build", str(build_directory), "--target", "verify_configuration")
            configured_file = build_directory / "configured.txt"
            self.assertEqual(configured_file.read_text(encoding="utf-8"), "before\n")

            updated_cmake_lists = cmake_lists.read_text(encoding="utf-8").replace(
                "configured_value before", "configured_value after"
            )
            cmake_lists.write_text(updated_cmake_lists, encoding="utf-8")
            timestamp = cmake_lists.stat()
            os.utime(
                cmake_lists,
                ns=(timestamp.st_atime_ns, timestamp.st_mtime_ns + 1_000_000_000),
            )

            self.run_cmake("--build", str(build_directory), "--target", "verify_configuration")
            self.assertEqual(configured_file.read_text(encoding="utf-8"), "after\n")

    def create_generated_source_sentinels(self) -> tuple[Path, ...]:
        native_binary_tools_directory = self.source_dir / "tools" / "NativeBinaryTools"
        generated_sources = (
            native_binary_tools_directory / "obj" / "cmake-native-workflow-test" / "Ignored.cs",
            native_binary_tools_directory / "bin" / "cmake-native-workflow-test" / "Ignored.cs",
        )
        for generated_source in generated_sources:
            generated_source.parent.mkdir(parents=True, exist_ok=True)
            generated_source.write_text("// CMake source-discovery sentinel.\n", encoding="utf-8")

        self.addCleanup(self.remove_generated_source_sentinels, generated_sources)
        return generated_sources

    def check_test_inventory(self, build: Path) -> None:
        def inventory(*filters: str) -> dict[str, dict[str, Any]]:
            result = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1", *filters],
                                    check=True, capture_output=True, text=True)
            return {test["name"]: test for test in json.loads(result.stdout)["tests"]}

        native = inventory("-L", "^native$")
        tools = inventory("-L", "^developer-tool$")
        self.assertTrue({"native-simulation-tests", "native-simulation-soak-tests"} <= native.keys())
        self.assertFalse(native.keys() & tools.keys())
        expected_tools = {"layout-planner-ui-tests", "image-lab-tests", "tracy-benchmark-compare-tests", "Sandbox.RustSetLiveCodingDisabled"}
        expected_tools.update("Sandbox." + name for name in (
            "AgentGit", "AgentGitInstaller", "ArchitectureChecks", "BenchmarkTools", "CodeFormatTools",
            "GamePackageTools", "GitTools", "NativeBinaryTools", "UnrealBuildTools"))
        self.assertEqual(tools.keys(), expected_tools)
        self.assertEqual(inventory("-L", "^native-simulation$", "-LE", "soak|compile-contract").keys(),
                         {"native-simulation-tests"})
        self.assertEqual(inventory("-L", "^native-simulation$").keys(),
                         {"native-simulation-tests", "native-simulation-soak-tests"})
        self.assertIn("CMake.CSharpTests", inventory("-L", "^cmake$"))
        self.assertNotIn("CodegenCliChecksGeneratedFixture", inventory())
        contracts = inventory("-L", "compile-contract")
        self.assertTrue(contracts)
        self.assertTrue(contracts.keys() <= native.keys())
        for test in contracts.values():
            self.assertTrue(any(prop["name"] == "RESOURCE_LOCK" for prop in test["properties"]))

    @staticmethod
    def remove_generated_source_sentinels(generated_sources: tuple[Path, ...]) -> None:
        for generated_source in generated_sources:
            generated_source.unlink(missing_ok=True)
            generated_source.parent.rmdir()

    def run_cmake(self, *arguments: str) -> str:
        result = subprocess.run(
            [self.cmake, *arguments],
            check=False,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--llvm-root", required=True)
    arguments, unittest_arguments = parser.parse_known_args()
    NativeWorkflowTests.source_dir = arguments.source_dir.resolve()
    NativeWorkflowTests.cmake = arguments.cmake
    NativeWorkflowTests.llvm_root = arguments.llvm_root
    unittest.main(argv=[sys.argv[0], *unittest_arguments])
