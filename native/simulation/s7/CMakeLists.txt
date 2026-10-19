add_library(native-simulation-s7 STATIC)

target_sources(native-simulation-s7
  PRIVATE
    lib/src/simulation_settings_reader.cpp
)

target_include_directories(native-simulation-s7
  PUBLIC
    lib/include
)

target_link_libraries(native-simulation-s7
  PUBLIC
    native-simulation
  PRIVATE
    native-level-authoring-s7
)
ioj_enable_warnings(native-simulation-s7)
ioj_apply_native_abi(native-simulation-s7)
set_target_properties(native-simulation-s7 PROPERTIES OUTPUT_NAME SandboxNativeSimulationS7)

if(IOJ_WITH_UNREAL)
  set_target_properties(native-simulation-s7 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/Simulation/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()
