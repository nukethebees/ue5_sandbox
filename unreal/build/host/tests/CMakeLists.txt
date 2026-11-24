# Compile a second fixture against a real, same-size field reorder.
set(alternate_directory "${CMAKE_CURRENT_BINARY_DIR}/alternate")
set(resolution_header "ioj/settings/screen_resolution.h")
set(original_header "${PROJECT_SOURCE_DIR}/native/levels/lib/include/${resolution_header}")
file(READ "${original_header}" resolution_source)
string(REPLACE "width{};\n    std::int32_t height{};" "height{};\n    std::int32_t width{};"
  reordered_source "${resolution_source}")
if(reordered_source STREQUAL resolution_source)
  message(FATAL_ERROR "Update the fixture to reorder the current ScreenResolution fields")
endif()
set(alternate_header "${alternate_directory}/${resolution_header}")
file(CONFIGURE OUTPUT "${alternate_header}" CONTENT "${reordered_source}" @ONLY)
list(FIND host_layout_headers "${original_header}" resolution_index)
list(REMOVE_AT host_layout_headers ${resolution_index})
list(INSERT host_layout_headers ${resolution_index} "${alternate_header}")
sandbox_write_host_fingerprint("${alternate_directory}" ${host_layout_headers})

function(add_host_fixture target headers)
  add_library(${target} SHARED)
  target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/native/sandbox_host/tests/fixture.cpp")
  target_link_libraries(${target} PRIVATE native-levels user32)
  target_compile_definitions(${target} PRIVATE SANDBOX_HOST_EXPORTS)
  target_include_directories(${target} PRIVATE "${headers}" "${host_directory}/include")
  add_dependencies(SandboxHost ${target})
endfunction()
add_host_fixture(sandbox-host-fixture "${interface_directory}")
add_host_fixture(sandbox-host-reordered-fixture "${alternate_directory}")

foreach(scenario IN ITEMS menu player observer missing invalid cli lifecycle display settings profiler)
  add_test(NAME sandbox-host-${scenario}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_host.py"
      "${scenario}" "${IOJ_GAME_STAGE_EXECUTABLE}"
      "$<TARGET_FILE:sandbox-host-fixture>" "${PROJECT_SOURCE_DIR}/LevelScripts"
      "${SBX_LLVM_READOBJ}" "$<TARGET_FILE:sandbox-host-reordered-fixture>"
  )
  set_tests_properties(sandbox-host-${scenario} PROPERTIES LABELS sandbox-host TIMEOUT 150)
endforeach()
