set(host_directory "${PROJECT_SOURCE_DIR}/native/sandbox_host")
add_executable(embed-host-identity)
target_sources(embed-host-identity PRIVATE
  "${host_directory}/embed_build_identity.cpp"
  "${host_directory}/build_identity.cpp"
)
target_link_libraries(embed-host-identity PRIVATE native-core sandbox::native_defaults)

function(add_player_launcher target binary dependency)
  set(include_directory "${CMAKE_CURRENT_BINARY_DIR}/${target}-include")
  set(identity "${include_directory}/SandboxGameBuildId.h")
  add_custom_target(${target}-identity
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${include_directory}"
    COMMAND embed-host-identity "${binary}" "${identity}.tmp"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${identity}.tmp" "${identity}"
    BYPRODUCTS "${identity}"
    DEPENDS embed-host-identity ${dependency}
    VERBATIM
  )
  add_executable(${target})
  add_dependencies(${target} ${target}-identity)
  target_include_directories(${target} PRIVATE "${include_directory}" "${host_directory}/include")
  set(agility_sdk "${UE_ROOT_ABSOLUTE}/Engine/Source/ThirdParty/Windows/AgilitySDK")
  target_include_directories(${target} SYSTEM
    PRIVATE
      "${agility_sdk}/Include"
      "${agility_sdk}/1.618.5/Include"
  )
  target_compile_definitions(${target} PRIVATE D3D12_CORE_ENABLED=1 SDL_MAIN_HANDLED)
  target_sources(${target}
    PRIVATE
      "${host_directory}/main.cpp"
      "${host_directory}/cli_args.cpp"
      "${host_directory}/application.cpp"
      "${host_directory}/engine.cpp"
      "${host_directory}/build_identity.cpp"
      "${host_directory}/windows/engine.cpp"
      "${host_directory}/library.cpp"
      "${host_directory}/window.cpp"
      "${host_directory}/windows/window.cpp"
      "${host_directory}/windows/display.cpp"
      "${host_directory}/windows/system.cpp"
  )
  target_link_libraries(${target}
    PRIVATE
      CLI11::CLI11
      SDL3::SDL3-static
      comctl32
      native-levels
      native-core
      shell32
      ole32
      uuid
      user32
  )
  # FSandboxCoreModule owns the game process's profiler lifetime.
  ioj_copy_runtime_dependencies(${target})
  target_compile_definitions(${target} PRIVATE SANDBOX_GAME_DLL=L"${game_dll_name}")
endfunction()

if(UE_CONFIGURATION STREQUAL "Development")
  set(game_dll_name "Sandbox.dll")
else()
  set(game_dll_name "Sandbox-Win64-${UE_CONFIGURATION}.dll")
endif()
add_player_launcher(SandboxHost "${PROJECT_SOURCE_DIR}/Binaries/Win64/${game_dll_name}" game)
set(IOJ_GAME_DLL_NAME "${game_dll_name}" PARENT_SCOPE)
