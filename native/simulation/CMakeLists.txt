include(target_policy.cmake)

add_library(native-simulation STATIC)

add_subdirectory(src)
add_subdirectory(include/ioj/sim/entities)
add_subdirectory(include/ioj/sim/missions)

ioj_apply_native_abi(native-simulation)
target_include_directories(native-simulation
  PUBLIC
    include
)
ioj_enable_warnings(native-simulation)
if(IOJ_WITH_TRACY)
  target_compile_definitions(native-simulation
    PRIVATE
      IOJ_WITH_TRACY
  )
endif()

target_link_libraries(native-simulation
  PUBLIC
    sandbox::native_defaults
    native-memory
    native-core
    native-levels
    "$<$<BOOL:${IOJ_WITH_TRACY}>:sandbox::profiling-api>"
  PRIVATE
    ioj::core_pch
    nlohmann_json::nlohmann_json
    "$<$<BOOL:${IOJ_WITH_TRACY}>:sandbox::profiling>"
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
set_target_properties(native-simulation PROPERTIES
  OUTPUT_NAME SandboxNativeSimulation
)

if(IOJ_WITH_UNREAL)
  set_target_properties(native-simulation PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/Simulation/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_subdirectory(s7)
add_subdirectory(tests)
