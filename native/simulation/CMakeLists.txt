include(target_policy.cmake)

add_library(native-simulation STATIC)

add_subdirectory(src)
add_subdirectory(include/ioj/sim/entities)
add_subdirectory(include/ioj/sim/missions)

set(native_simulation_targets native-simulation)
if(IOJ_WITH_UNREAL)
  # Tracy DLL imports are compiled into the simulation objects used by Unreal.
  add_library(native-simulation-unreal STATIC)
  get_target_property(native_simulation_sources native-simulation SOURCES)
  target_sources(native-simulation-unreal 
    PRIVATE 
      ${native_simulation_sources}
  )
  list(APPEND native_simulation_targets native-simulation-unreal)
endif()

foreach(native_simulation_target IN LISTS native_simulation_targets)
  ioj_apply_native_abi(${native_simulation_target})
  target_include_directories(${native_simulation_target} PUBLIC
    include
  )
  ioj_enable_warnings(${native_simulation_target})
  set(simulation_profiling_libraries)
  if(IOJ_WITH_TRACY)
    target_compile_definitions(${native_simulation_target} PRIVATE IOJ_WITH_TRACY)
    if(native_simulation_target STREQUAL "native-simulation-unreal")
      set(simulation_profiling_libraries TracyClientUnreal)
    else()
      set(simulation_profiling_libraries Tracy::TracyClient sandbox::profiling)
    endif()
  endif()

  target_link_libraries(${native_simulation_target}
    PUBLIC
      sandbox::native_defaults
      sandbox::handmade_math
      native-memory
      native-core
      native-levels
      "$<$<BOOL:${IOJ_WITH_TRACY}>:sandbox::profiling-api>"
    PRIVATE
      ioj::core_pch
      ${simulation_profiling_libraries}
      "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
  )
  set_target_properties(${native_simulation_target} PROPERTIES
    OUTPUT_NAME SandboxNativeSimulation
  )
endforeach()

if(IOJ_WITH_UNREAL)
  set_target_properties(native-simulation-unreal PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/Simulation/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_subdirectory(tests)
