add_unreal_target(embedded-game SandboxEmbedded)
add_dependencies(embedded-game unreal-native-dependencies check-generated-code)

add_executable(SandboxHost)
set(agility_sdk "${UE_ROOT_ABSOLUTE}/Engine/Source/ThirdParty/Windows/AgilitySDK")
target_include_directories(SandboxHost SYSTEM
  PRIVATE
    "${agility_sdk}/Include"
    "${agility_sdk}/1.618.5/Include"
)
target_compile_definitions(SandboxHost PRIVATE D3D12_CORE_ENABLED=1)
target_sources(SandboxHost
  PRIVATE
    "${PROJECT_SOURCE_DIR}/native/sandbox_host/main.cpp"
)
target_link_libraries(SandboxHost
  PRIVATE
    CLI11::CLI11
    native-level-authoring-s7
    native-simulation
    shell32
    ole32
    uuid
    user32
)
ioj_stage_runtime_dependencies(SandboxHost)

sandbox_make_unreal_container_arguments(embedded_container_arguments
  "${IOJ_GAME_COOK_ROOT}" "${IOJ_GAME_STAGE_ROOT}")
add_unreal_build_cook_run_target(stage-embedded
  GAME_TARGET SandboxEmbedded
  COMMENT "Staging the embedded game and native host"
  ARGUMENTS -skipcook -stage -NoBootstrapExe ${embedded_container_arguments}
)
add_dependencies(stage-embedded SandboxHost embedded-game)
set(staged_host_directory "${IOJ_GAME_STAGE_DIRECTORY}/Sandbox/Binaries/Win64")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SandboxHost.path"
  CONTENT "${staged_host_directory}/$<TARGET_FILE_NAME:SandboxHost>\n")
add_custom_command(TARGET stage-embedded POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:SandboxHost>" "${staged_host_directory}/"
  COMMAND "${CMAKE_COMMAND}"
    "-Druntime_dlls=$<TARGET_RUNTIME_DLLS:SandboxHost>"
    "-Ddestination=${staged_host_directory}"
    -P "${PROJECT_SOURCE_DIR}/cmake/stage_runtime_dependencies.cmake"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/Saved"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_BINARY_DIR}/SandboxHost.path" "${PROJECT_SOURCE_DIR}/Saved/SandboxHost.path"
  VERBATIM
)
