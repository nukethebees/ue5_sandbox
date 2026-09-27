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

    def test_compile_options_do_not_require_an_executable_link(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox option probes ") as directory:
            fixture = Path(directory)
            (fixture / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                'project(OptionProbes LANGUAGES CXX)\n'
                f'include("{self.source_dir.as_posix()}/cmake/compiler_warnings/add_supported_interface_options.cmake")\n'
                'set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:ioj_missing_entry")\n'
                'add_library(valid INTERFACE)\n'
                'target_add_supported_options(valid COMPILE /W4)\n'
                'get_target_property(options valid INTERFACE_COMPILE_OPTIONS)\n'
                'if(NOT options STREQUAL "/W4")\n'
                '  message(FATAL_ERROR "Compile-only probe required a link")\n'
                'endif()\n'
                'add_library(invalid INTERFACE)\n'
                'target_add_supported_options(invalid COMPILE /clang:-fioj-invalid-option)\n'
                'get_target_property(options invalid INTERFACE_COMPILE_OPTIONS)\n'
                'if(options)\n'
                '  message(FATAL_ERROR "Unsupported compile option accepted")\n'
                'endif()\n'
                'set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\n'
                'add_library(linked INTERFACE)\n'
                'target_add_supported_options(linked LINK /DEBUG)\n'
                'get_target_property(options linked INTERFACE_LINK_OPTIONS)\n'
                'if(options)\n'
                '  message(FATAL_ERROR "Link probe did not exercise the linker")\n'
                'endif()\n', encoding="utf-8",
            )
            self.run_cmake("-S", str(fixture), "-B", str(fixture / "build"), "-G", "Ninja",
                           f"-DCMAKE_TOOLCHAIN_FILE={self.source_dir.as_posix()}/cmake/toolchains/windows-clang-cl.cmake")

    def test_lean_configuration_keeps_native_policy_and_required_generators(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox lean native ") as directory:
            build = Path(directory) / "build"
            self.run_cmake("--preset", "native-lean", "-S", str(self.source_dir), "-B", str(build),
                           "-DUE_CONFIGURATION=Shipping", "-DCMAKE_COMPILE_WARNING_AS_ERROR=OFF")
            cache = (build / "CMakeCache.txt").read_text()
            self.assertNotIn("SDL_STATIC:", cache)
            self.assertNotIn("NFD_BUILD_TESTS:", cache)
            self.assertRegex(cache, r"IOJ_ENABLE_PROFILING:BOOL=(ON|TRUE)")
            graph = (build / "build.ninja").read_text()
            self.assertIn("SandboxTracyClientStatic", graph)
            self.assertNotIn("Tools.slnx", graph)
            self.assertNotIn("cargo test", graph)
            commands = self.run_cmake("--build", str(build), "--target", "native-core", "--", "-t", "commands")
            self.assertNotIn(" /WX", commands)
            self.assertNotIn(" /O2", commands)
            self.assertNotIn("-DNDEBUG", commands)
            generated = self.run_cmake("--build", str(build), "--target", "generate-native-soa-fixture",
                                       "--", "-t", "commands")
            self.assertIn("lispb.exe", generated)
            self.assertIn("--depfile", generated)

    def test_negative_compile_requires_the_expected_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox compile rejection ") as directory:
            fixture = Path(directory)
            (fixture / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                'project(CompileRejection NONE)\n'
                'add_custom_target(rejected COMMAND "${CMAKE_COMMAND}" -E echo "specific rejection"\n'
                '  COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)\n'
                'add_custom_target(unrelated COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)\n'
                'add_custom_target(accepted COMMAND "${CMAKE_COMMAND}" -E echo "specific rejection" VERBATIM)\n',
                encoding="utf-8",
            )
            build = fixture / "build with spaces"
            self.run_cmake("-S", str(fixture), "-B", str(build), "-G", "Ninja")
            for target in ("rejected", "unrelated", "accepted", "missing-target"):
                result = subprocess.run(
                    [self.cmake, f"-DBUILD_DIR={build}", f"-DREJECT_TARGET={target}",
                     "-DEXPECTED_DIAGNOSTIC=specific rejection", "-P",
                     str(self.source_dir / "cmake/expect_compile_failure.cmake")],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode == 0, target == "rejected", result.stdout + result.stderr)

    presets = json.loads((PRESET_DIRECTORY / "native.json").read_text(encoding="utf-8"))
    unreal_presets = json.loads(
        (PRESET_DIRECTORY / "unreal.json").read_text(encoding="utf-8")
    )

    def test_asan_llvm_215376_workaround_option(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox asan workaround ") as directory:
            fixture = Path(directory)
            (fixture / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                'project(AsanWorkaround NONE)\n'
                'set(WIN32 TRUE)\n'
                'set(CMAKE_SIZEOF_VOID_P 8)\n'
                'set(IOJ_IS_CLANG_CL TRUE)\n'
                'set(IOJ_WITH_UNREAL FALSE)\n'
                'set(resource_directory "${CMAKE_BINARY_DIR}/resource")\n'
                'foreach(artifact IN ITEMS clang_rt.asan_dynamic-x86_64.dll\n'
                '    clang_rt.asan_dynamic-x86_64.lib clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)\n'
                '  file(WRITE "${resource_directory}/lib/windows/${artifact}" "fixture")\n'
                'endforeach()\n'
                # Stub only compiler discovery; exercise the actual sanitizer target configuration.
                'function(execute_process)\n'
                '  cmake_parse_arguments(query "" "OUTPUT_VARIABLE;RESULT_VARIABLE" "" ${ARGN})\n'
                '  set(${query_OUTPUT_VARIABLE} "${resource_directory}" PARENT_SCOPE)\n'
                '  set(${query_RESULT_VARIABLE} 0 PARENT_SCOPE)\n'
                'endfunction()\n'
                f'add_subdirectory("{self.source_dir.as_posix()}/cmake/sanitizers" sanitizers)\n'
                'get_property(option_type CACHE IOJ_ASAN_WORKAROUND_LLVM_215376 PROPERTY TYPE)\n'
                'if(NOT option_type STREQUAL "BOOL")\n'
                '  message(FATAL_ERROR "Workaround must be a BOOL cache option")\n'
                'endif()\n'
                'get_target_property(options ioj_asan INTERFACE_COMPILE_OPTIONS)\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/result.txt"\n'
                '  "${IOJ_ASAN_WORKAROUND_LLVM_215376}\\n${options}")\n',
                encoding="utf-8",
            )
            cases = (
                ("default", True, None, True),
                ("enabled", True, "ON", True),
                ("disabled", True, "OFF", False),
                ("asan_disabled", False, None, False),
            )
            for name, asan_enabled, setting, active in cases:
                with self.subTest(case=name):
                    build = fixture / name
                    arguments = [f"-DIOJ_ENABLE_ASAN={'ON' if asan_enabled else 'OFF'}"]
                    if setting is not None:
                        arguments.append(f"-DIOJ_ASAN_WORKAROUND_LLVM_215376={setting}")
                    output = self.run_cmake("-S", str(fixture), "-B", str(build), "-G", "Ninja", *arguments)
                    value, options = (build / "result.txt").read_text().split("\n", 1)
                    self.assertEqual(value, setting or "ON")
                    self.assertEqual("/fsanitize=address" in options, asan_enabled)
                    self.assertEqual("/clang:-fsanitize-address-use-after-return=never" in options, active)
                    self.assertEqual("LLVM #215376 workaround active" in output, active)
                    if asan_enabled:
                        runtime = build / "bin/clang_rt.asan_dynamic-x86_64.dll"
                        self.assertFalse(runtime.exists(), "Configure must not stage build artifacts")
                        self.run_cmake("--build", str(build), "--target", "stage-asan-runtime")
                        self.assertEqual(runtime.read_text(), "fixture")
                        runtime.unlink()
                        self.run_cmake("--build", str(build), "--target", "stage-asan-runtime")
                        self.assertEqual(runtime.read_text(), "fixture")

            # An explicit OFF must also remove the flag when reconfiguring an existing build.
            output = self.run_cmake("-S", str(fixture), "-B", str(fixture / "default"),
                                    "-DIOJ_ASAN_WORKAROUND_LLVM_215376=OFF")
            self.assertNotIn("LLVM #215376 workaround active", output)
            self.assertNotIn("use-after-return=never", (fixture / "default/result.txt").read_text())

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
                'if(DEFINED CACHE{LLVM_ROOT} OR NOT LLVM_ROOT STREQUAL expected_root)\n'
                '  message(FATAL_ERROR "LLVM_ROOT must be captured from the environment: ${LLVM_ROOT}")\n'
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
            for selection in ("environment", "path", "unsupported_cache"):
                with self.subTest(selection=selection):
                    environment = os.environ.copy()
                    environment.pop("LLVM_ROOT", None)
                    environment["PATH"] = str(other_root / "bin") + os.pathsep + environment["PATH"]
                    arguments: list[str] = []
                    if selection == "environment":
                        environment["LLVM_ROOT"] = str(fixture / "unused" / ".." / llvm_root.name)
                    else:
                        environment["PATH"] = str(llvm_root / "bin") + os.pathsep + environment["PATH"]
                        if selection == "unsupported_cache":
                            arguments.append(f"-DLLVM_ROOT:PATH={other_root.as_posix()}")
                    expected = llvm_root.as_posix() if selection == "environment" else ""
                    result = subprocess.run(
                        [self.cmake, *arguments, f"-Dexpected_root={expected}", "-P", str(script)],
                        env=environment, capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            # Incomplete and invalid roots must not fall back to a complete PATH install.
            (llvm_root / "bin" / ("llvm-lib" + suffix)).unlink()
            environment = os.environ.copy()
            environment["PATH"] = str(other_root / "bin") + os.pathsep + environment["PATH"]
            for invalid_root in (llvm_root, fixture / "missing LLVM", Path("OFF")):
                with self.subTest(invalid_root=invalid_root):
                    environment["LLVM_ROOT"] = str(invalid_root)
                    result = subprocess.run(
                        [self.cmake, f"-Dexpected_root={invalid_root.as_posix()}", "-Dprobe_name=llvm-lib",
                         "-P", str(script)],
                        env=environment, capture_output=True, text=True,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Could not find llvm_tool", result.stdout + result.stderr)

    def test_tidy_runner_tool_names_and_root_isolation(self) -> None:
        cases = (
            ("root_runner", "run-clang-tidy", None, True, True),
            ("root_python", "run-clang-tidy.py", None, True, True),
            ("missing", None, None, True, False),
            ("isolated", None, "run-clang-tidy.py", True, False),
            ("path_python", None, "run-clang-tidy.py", False, True),
            ("path_missing", None, None, False, False),
        )
        with tempfile.TemporaryDirectory(prefix="sandbox tidy discovery ") as directory:
            fixture = Path(directory)
            script = fixture / "verify.cmake"
            script.write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                # Keep the PATH-only cases independent of the machine's other LLVM installs.
                'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n'
                'set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)\n'
                f'include("{self.source_dir.as_posix()}/cmake/llvm_tools.cmake")\n'
                'ioj_find_llvm_tool(selected run-clang-tidy run-clang-tidy.py)\n'
                'if(NOT selected STREQUAL expected_tool)\n'
                '  message(FATAL_ERROR "Wrong tidy runner: ${selected}")\n'
                'endif()\n', encoding="utf-8",
            )
            for name, root_tool, path_tool, use_root, succeeds in cases:
                with self.subTest(case=name):
                    llvm_root = fixture / name / "LLVM with spaces"
                    root_bin = llvm_root / "bin"
                    path_bin = fixture / name / "PATH with spaces"
                    root_bin.mkdir(parents=True)
                    path_bin.mkdir()
                    for directory, tool in ((root_bin, root_tool), (path_bin, path_tool)):
                        if tool:
                            executable = directory / tool
                            executable.touch()
                            executable.chmod(0o755)
                    environment = os.environ.copy()
                    environment["LLVM_ROOT"] = str(llvm_root) if use_root else ""
                    environment["PATH"] = str(path_bin)
                    expected = (root_bin / root_tool if root_tool else
                                path_bin / path_tool if path_tool else fixture / "missing")
                    result = subprocess.run(
                        [self.cmake, f"-Dexpected_tool={expected.as_posix()}", "-P", str(script)],
                        env=environment, capture_output=True, text=True,
                    )
                    output = result.stdout + result.stderr
                    if succeeds:
                        self.assertEqual(result.returncode, 0, output)
                    else:
                        self.assertNotEqual(result.returncode, 0, output)
                        self.assertIn("Could not find llvm_tool", output)

    @unittest.skipUnless(shutil.which("pwsh"), "requires PowerShell")
    def test_llvm_build_paths_follow_powershell_location(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox llvm paths ") as directory:
            result = subprocess.run([
                "pwsh", "-NoProfile", "-File",
                str(self.source_dir / "tools/llvm/tests/Test-BuildPaths.ps1"),
                "-FixtureRoot", directory,
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipUnless(shutil.which("pwsh"), "requires PowerShell")
    def test_jobserver_installer_rejects_asan_before_mutation(self) -> None:
        cmake_source = (self.source_dir / "tools/jobserver/CMakeLists.txt").read_text()
        self.assertIn('-AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"', cmake_source)
        result = subprocess.run([
            "pwsh", "-NoProfile", "-File",
            str(self.source_dir / "tools/jobserver/tests/Test-InstallerAsanGuard.ps1"),
        ], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

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
                '  if(output STREQUAL "IOJ_RUN_CLANG_TIDY_EXECUTABLE" AND\n'
                '     NOT "${name};${ARGN}" STREQUAL "run-clang-tidy;run-clang-tidy.py")\n'
                '    message(FATAL_ERROR "Tidy configuration lost a runner candidate")\n'
                '  endif()\n'
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

        for name, preset in test_presets.items():
            if name.startswith("win-x64-"):
                with self.subTest(preset=name):
                    self.assertEqual(preset["filter"]["include"]["label"], "^all$")

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
  foreach(target IN ITEMS native-simulation native-simulation-tests native-simulation-soak-tests)
    get_target_property(runtime ${target} MSVC_RUNTIME_LIBRARY)
    get_target_property(warnings ${target} COMPILE_WARNING_AS_ERROR)
    if(NOT runtime STREQUAL "MultiThreadedDLL" OR
       (NOT warnings AND NOT DEFINED CMAKE_COMPILE_WARNING_AS_ERROR))
      message(FATAL_ERROR "${target} omitted first-party target policy")
    endif()
  endforeach()
  file(WRITE "${CMAKE_BINARY_DIR}/policy_fixture.cpp" "void policy_fixture() {}\n")
  add_library(policy-consumer OBJECT EXCLUDE_FROM_ALL "${CMAKE_BINARY_DIR}/policy_fixture.cpp")
  target_link_libraries(policy-consumer PRIVATE native-simulation)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-policy.txt" CONTENT
    "$<TARGET_PROPERTY:policy-consumer,COMPILE_OPTIONS>\n$<TARGET_PROPERTY:policy-consumer,COMPILE_DEFINITIONS>\n$<TARGET_PROPERTY:policy-consumer,COMPILE_FEATURES>"
    CONDITION "$<COMPILE_LANGUAGE:CXX>")
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
            consumer_policy = (build_directory / "consumer-policy.txt").read_text()
            self.assertIn("cxx_std_23", consumer_policy)
            self.assertIn("_ITERATOR_DEBUG_LEVEL=0", consumer_policy)
            self.assertIn("/permissive-", consumer_policy)
            self.assertNotRegex(consumer_policy, r"/W4|/WX|-Werror|-Wpedantic")

            msvc_build = Path(temporary_root) / "msvc build with spaces"
            self.run_cmake(
                "--preset", "win-x64-msvc-debug", "-S", str(self.source_dir), "-B", str(msvc_build),
                "-DIOJ_BUILD_DEVELOPER_TOOLS=OFF", "-DCMAKE_COMPILE_WARNING_AS_ERROR=OFF",
                f"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={policy_check.as_posix()}",
            )
            msvc_policy = (msvc_build / "consumer-policy.txt").read_text()
            for requirement in ("cxx_std_23", "_ITERATOR_DEBUG_LEVEL=0", "/permissive-", "/Zc:preprocessor"):
                self.assertIn(requirement, msvc_policy)
            self.assertNotRegex(msvc_policy, r"/W[0-4X]|/w4\d+|-Werror|-Wpedantic")
            commands = self.run_cmake("--build", str(msvc_build), "--target", "native-core", "policy-consumer",
                                      "--", "-t", "commands")
            consumer_command = next(line for line in commands.splitlines() if "policy_fixture.cpp" in line)
            for option in ("/permissive-", "/Zc:preprocessor"):
                self.assertEqual(consumer_command.count(option), 1, consumer_command)
            self.assertNotRegex(consumer_command, r"[/-]W4|[/-]WX|/w4\d+|-Werror|-Wpedantic")
            core_commands = [line for line in commands.splitlines()
                             if "/native/core/src/" in line.replace("\\", "/") and re.search(r" [/-]c ", line)]
            self.assertTrue(core_commands)
            for command in core_commands:
                for option in ("/permissive-", "/Zc:preprocessor", "/W4"):
                    self.assertEqual(command.count(option), 1, command)
                self.assertNotRegex(command, r"[/-]WX\b")

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

            rules = (build_directory / "CMakeFiles/rules.ninja").read_text(encoding="utf-8")
            graph = (build_directory / "build.ninja").read_text(encoding="utf-8")
            self.assertIn('--name "Compile C"', graph)
            self.assertIn('--name "Compile CXX"', graph)
            self.assertIn('--name "Link CXX"', rules)
            test_file = (build_directory / "native/core/CTestTestfile.cmake").read_text(encoding="utf-8")
            self.assertIn('"--kind" "test"', test_file)
            self.assertIn('"--shared" "machine" "--"', test_file)
            self.assertIn('native-core-tests.exe"', test_file)
            bootstrap = self.run_cmake("--build", str(build_directory), "--target", "install-jobserver",
                                      "--", "-t", "commands")
            self.assertNotIn("jobserver.exe run", bootstrap)
            self.assertNotIn('jobserver.exe" run', bootstrap)

            for report, executable in (
                ("native-core-vector-lerp-benchmark-report", "native/core/native-core-benchmarks.exe"),
                ("native-soa-production-report", "native/lispb/native_soa/native-soa-benchmarks.exe"),
                ("native-soa-candidate-report", "native/lispb/native_soa/native-soa-benchmarks.exe"),
                ("native-soa-laser-hit-append-report", "native/lispb/native_soa/native-soa-benchmarks.exe"),
                ("kernel-benchmark-report", "native/lispb/kernel/kernel-native-benchmarks.exe"),
            ):
                commands = self.run_cmake("--build", str(build_directory), "--target", report,
                                          "--", "-t", "commands").replace("\\", "/")
                self.assertIn(f'-- "{(build_directory / executable).as_posix()}"', commands)

            host_tool = (
                build_directory
                / "host-tools"
                / "NativeBinaryTools"
                / "Debug"
                / "NativeBinaryTools.exe"
            )
            self.assertIn(host_tool.as_posix(), dry_run.replace("\\", "/"))
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
        self.assertFalse(inventory("-L", "^all$").keys() & tools.keys())
        expected_tools = {"layout-planner-ui-tests", "image-lab-tests", "tracy-benchmark-compare-tests", "tracy-benchmark-compare-version", "PowerShell.Navigation"}
        expected_tools.update("Sandbox." + name for name in (
            "ArchitectureChecks", "BenchmarkTools", "CodeFormatTools",
            "GamePackageTools", "NativeBinaryTools", "UnrealBuildTools"))
        self.assertEqual(tools.keys(), expected_tools)
        self.assertEqual(inventory("-L", "^native-simulation$", "-LE", "soak|compile-contract").keys(),
                         {"native-simulation-tests"})
        self.assertEqual(inventory("-L", "^native-simulation$").keys(),
                         {"native-simulation-tests", "native-simulation-soak-tests"})
        self.assertIn("CMake.CSharpTests", inventory("-L", "^cmake$"))
        workflow = inventory("-R", "^CMake.NativeWorkflow$")["CMake.NativeWorkflow"]
        root_argument = next(arg for arg in workflow["command"] if arg.startswith("--llvm-root="))
        self.assertEqual(Path(root_argument.removeprefix("--llvm-root=")), Path(self.llvm_root))
        environment = os.environ.copy()
        environment["LLVM_ROOT"] = str(build / "different ambient LLVM")
        result = subprocess.run(
            [*workflow["command"],
             "NativeWorkflowTests.test_compile_options_do_not_require_an_executable_link"],
            env=environment, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
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
        environment = os.environ.copy()
        environment["LLVM_ROOT"] = self.llvm_root
        result = subprocess.run(
            [self.cmake, *arguments],
            env=environment,
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
