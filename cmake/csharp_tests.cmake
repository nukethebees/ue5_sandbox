find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
set(sandbox_csharp_configuration "${CMAKE_BUILD_TYPE}")
set(sandbox_csharp_artifacts "${CMAKE_BINARY_DIR}/csharp-tests")
set(sandbox_csharp_metadata "${CMAKE_BINARY_DIR}/csharp-tests.json")
set(sandbox_csharp_runner "${PROJECT_SOURCE_DIR}/cmake/csharp_tests.py")
set(sandbox_csharp_entries "")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/tools/Tools.slnx"
  "${PROJECT_SOURCE_DIR}/.integration-gates.json"
  "${sandbox_csharp_runner}")

add_custom_target(csharp-tests-build
  COMMAND "${DOTNET_EXECUTABLE}" build "${PROJECT_SOURCE_DIR}/tools/Tools.slnx"
    --configuration "${sandbox_csharp_configuration}"
    --artifacts-path "${sandbox_csharp_artifacts}" --nologo -m:1
    -p:IsStandaloneTool=false -p:SandboxCMakeHostToolBuild=true
  COMMAND "${Python3_EXECUTABLE}" "${sandbox_csharp_runner}"
    record "${sandbox_csharp_metadata}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  VERBATIM
)

function(sandbox_add_csharp_test name project labels)
  set(input_directories ${ARGN})
  foreach(input_directory IN LISTS input_directories)
    file(GLOB project_files "${PROJECT_SOURCE_DIR}/${input_directory}/*.csproj")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${project_files})
  endforeach()
  list(TRANSFORM input_directories PREPEND "\"")
  list(TRANSFORM input_directories APPEND "\"")
  list(JOIN input_directories ", " inputs_json)
  set(entry "{\"name\": \"${name}\", \"project\": \"${project}\", \"inputs\": [${inputs_json}]}")
  set(sandbox_csharp_entries ${sandbox_csharp_entries} "${entry}" PARENT_SCOPE)

  add_custom_target(csharp-${name}-build
    COMMAND "${DOTNET_EXECUTABLE}" build "${PROJECT_SOURCE_DIR}/${project}"
      --configuration "${sandbox_csharp_configuration}"
      --artifacts-path "${sandbox_csharp_artifacts}" --nologo
      -p:IsStandaloneTool=false -p:SandboxCMakeHostToolBuild=true
    COMMAND "${Python3_EXECUTABLE}" "${sandbox_csharp_runner}"
      record "${sandbox_csharp_metadata}" "${name}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
  )
  add_test(NAME Sandbox.${name}
    COMMAND "${Python3_EXECUTABLE}" "${sandbox_csharp_runner}"
      test "${sandbox_csharp_metadata}" "${name}")
  set_tests_properties(Sandbox.${name} PROPERTIES
    LABELS "developer-tool;csharp;${labels}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  )
endfunction()

# Explicit transitive source ownership keeps no-build tests honest after shared edits.
sandbox_add_csharp_test(AgentGit "tools/AgentGit.Tests/AgentGit.Tests.csproj"
  "agent-git;integration;subprocess" tools/AgentGit.Tests tools/AgentGit tools/GitSupport)
sandbox_add_csharp_test(AgentGitInstaller "tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj"
  "installer;integration;subprocess" tools/AgentGitInstaller.Tests tools/AgentGitInstaller tools/AgentGit tools/GitSupport)
sandbox_add_csharp_test(ArchitectureChecks "tools/ArchitectureChecks.Tests/ArchitectureChecks.Tests.csproj"
  "architecture;integration;subprocess" tools/ArchitectureChecks.Tests tools/ArchitectureChecks)
sandbox_add_csharp_test(BenchmarkTools "tools/BenchmarkTools.Tests/BenchmarkTools.Tests.csproj"
  "benchmark;unit" tools/BenchmarkTools.Tests tools/BenchmarkTools)
sandbox_add_csharp_test(CodeFormatTools "tools/CodeFormatTools.Tests/CodeFormatTools.Tests.csproj"
  "formatting;integration;subprocess" tools/CodeFormatTools.Tests tools/CodeFormatTools)
sandbox_add_csharp_test(GamePackageTools "tools/GamePackageTools.Tests/GamePackageTools.Tests.csproj"
  "game-package;unit" tools/GamePackageTools.Tests tools/GamePackageTools)
sandbox_add_csharp_test(GitTools "tools/GitTools.Tests/GitTools.Tests.csproj"
  "git;integration;subprocess" tools/GitTools.Tests tools/GitTools tools/GitSupport)
sandbox_add_csharp_test(NativeBinaryTools "tools/NativeBinaryTools.Tests/NativeBinaryTools.Tests.csproj"
  "native-binary;integration;subprocess" tools/NativeBinaryTools.Tests tools/NativeBinaryTools)
sandbox_add_csharp_test(UnrealBuildTools "tools/UnrealBuildTools.Tests/UnrealBuildTools.Tests.csproj"
  "unreal-build;integration;subprocess" tools/UnrealBuildTools.Tests tools/UnrealBuildTools)

list(JOIN sandbox_csharp_entries ",\n    " sandbox_csharp_entries_json)
file(CONFIGURE OUTPUT "${sandbox_csharp_metadata}" CONTENT
"{
  \"root\": \"${PROJECT_SOURCE_DIR}\",
  \"dotnet\": \"${DOTNET_EXECUTABLE}\",
  \"configuration\": \"${sandbox_csharp_configuration}\",
  \"artifacts\": \"${sandbox_csharp_artifacts}\",
  \"projects\": [
    ${sandbox_csharp_entries_json}
  ]
}
")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${sandbox_csharp_runner}" validate "${sandbox_csharp_metadata}"
  COMMAND_ERROR_IS_FATAL ANY
)
