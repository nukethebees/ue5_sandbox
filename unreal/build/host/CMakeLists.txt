# UBT and CMake compile separate copies of the window and lifecycle interface.
set(player_headers game_host_api.h game_host.h)
set(settings_headers launch_display.h screen_resolution.h game_window_mode.h)
list(TRANSFORM player_headers PREPEND "${PROJECT_SOURCE_DIR}/native/sandbox_host/include/ioj/host/")
list(TRANSFORM settings_headers PREPEND "${PROJECT_SOURCE_DIR}/native/levels/lib/include/ioj/settings/")
set(host_layout_headers ${player_headers} ${settings_headers})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${host_layout_headers})

function(sandbox_write_host_fingerprint output_directory)
  set(layout_digests "")
  foreach(header IN LISTS ARGN)
    file(READ "${header}" declaration)
    string(SHA256 digest "${declaration}")
    string(APPEND layout_digests "${digest}")
  endforeach()
  string(SHA256 fingerprint "${layout_digests}")
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SandboxHostFingerprint.h.in"
    "${output_directory}/SandboxHostFingerprint.h" @ONLY)
endfunction()

set(interface_directory "${PROJECT_SOURCE_DIR}/Intermediate/SandboxHost")
sandbox_write_host_fingerprint("${interface_directory}" ${host_layout_headers})

add_executable(SandboxHost)
target_include_directories(SandboxHost PRIVATE "${interface_directory}" "${PROJECT_SOURCE_DIR}/native/sandbox_host/include")
set(agility_sdk "${UE_ROOT_ABSOLUTE}/Engine/Source/ThirdParty/Windows/AgilitySDK")
target_include_directories(SandboxHost SYSTEM
  PRIVATE
    "${agility_sdk}/Include"
    "${agility_sdk}/1.618.5/Include"
)
target_compile_definitions(SandboxHost PRIVATE D3D12_CORE_ENABLED=1)
set(host_directory "${PROJECT_SOURCE_DIR}/native/sandbox_host")
target_sources(SandboxHost
  PRIVATE
    "${host_directory}/main.cpp"
    "${host_directory}/cli_args.cpp"
    "${host_directory}/application.cpp"
    "${host_directory}/engine.cpp"
    "${host_directory}/windows/engine.cpp"
    "${host_directory}/windows/library.cpp"
    "${host_directory}/windows/window.cpp"
    "${host_directory}/windows/display.cpp"
    "${host_directory}/windows/system.cpp"
)
target_link_libraries(SandboxHost
  PRIVATE
    CLI11::CLI11
    native-levels
    native-core
    shell32
    ole32
    uuid
    user32
)
# FSandboxCoreModule owns the game process's profiler lifetime.
ioj_copy_runtime_dependencies(SandboxHost)

if(UE_CONFIGURATION STREQUAL "Development")
  set(game_dll_name "Sandbox.dll")
else()
  set(game_dll_name "Sandbox-Win64-${UE_CONFIGURATION}.dll")
endif()
target_compile_definitions(SandboxHost PRIVATE SANDBOX_GAME_DLL=L"${game_dll_name}")
set(IOJ_GAME_DLL_NAME "${game_dll_name}" PARENT_SCOPE)

if(UE_CONFIGURATION STREQUAL "Development")
  add_subdirectory(tests)
endif()
