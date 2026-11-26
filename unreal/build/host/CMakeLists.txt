set(host_directory "${PROJECT_SOURCE_DIR}/native/player")
add_executable(IojPlayer)
set_target_properties(IojPlayer PROPERTIES OUTPUT_NAME ioj)
target_include_directories(IojPlayer
  PRIVATE
    "${PROJECT_SOURCE_DIR}/native"
    "${host_directory}/include"
)
set(agility_sdk "${UE_ROOT_ABSOLUTE}/Engine/Source/ThirdParty/Windows/AgilitySDK")
target_include_directories(IojPlayer SYSTEM
  PRIVATE
    "${agility_sdk}/Include"
    "${agility_sdk}/1.618.5/Include"
)
target_compile_definitions(IojPlayer
  PRIVATE
    D3D12_CORE_ENABLED=1
    SDL_MAIN_HANDLED
    IOJ_SHIPPING=$<STREQUAL:${UE_CONFIGURATION},Shipping>
)
target_sources(IojPlayer
  PRIVATE
    "${host_directory}/main.cpp"
    "${host_directory}/cli_args.cpp"
    "${host_directory}/application.cpp"
    "${host_directory}/engine.cpp"
    "${host_directory}/windows/engine.cpp"
    "${host_directory}/window.cpp"
    "${host_directory}/windows/window.cpp"
    "${host_directory}/display.cpp"
    "${host_directory}/windows/system.cpp"
    "${host_directory}/windows/capabilities.cpp"
)
target_link_libraries(IojPlayer
  PRIVATE
    "${game_import_library}"
    CLI11::CLI11
    SDL3::SDL3-static
    advapi32
    comctl32
    native-level-authoring-s7
    native-profiles
    native-simulation
    sbx-mimalloc
    native-levels
    native-core
    shell32
    ole32
    uuid
    user32
)
# FSandboxCoreModule owns the game process's profiler lifetime.
ioj_copy_runtime_dependencies(IojPlayer)

add_dependencies(IojPlayer game)
