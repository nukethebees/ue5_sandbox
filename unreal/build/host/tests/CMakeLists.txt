add_library(sandbox-host-fixture SHARED)
target_sources(sandbox-host-fixture PRIVATE "${host_directory}/tests/fixture.cpp")
target_link_libraries(sandbox-host-fixture PRIVATE native-levels user32)
target_compile_definitions(sandbox-host-fixture PRIVATE SANDBOX_HOST_EXPORTS)
target_include_directories(sandbox-host-fixture PRIVATE "${host_directory}/include")
target_link_options(sandbox-host-fixture PRIVATE /DEBUG)
add_player_launcher(SandboxHostFixture "$<TARGET_FILE:sandbox-host-fixture>" sandbox-host-fixture)
add_dependencies(SandboxHost SandboxHostFixture)

foreach(scenario IN ITEMS menu player observer missing invalid cli lifecycle display settings profiler)
  add_test(NAME sandbox-host-${scenario}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_host.py"
      "${scenario}" "${IOJ_GAME_STAGE_EXECUTABLE}"
      "$<TARGET_FILE:sandbox-host-fixture>" "${PROJECT_SOURCE_DIR}/LevelScripts"
      "${SBX_LLVM_READOBJ}" "$<TARGET_FILE:SandboxHostFixture>"
  )
  set_tests_properties(sandbox-host-${scenario} PROPERTIES LABELS sandbox-host TIMEOUT 150)
endforeach()
