add_library(native-simulation STATIC)

add_subdirectory(src)
add_subdirectory(include/ioj/sim/entities)
add_subdirectory(include/ioj/sim/missions)

set(native_simulation_targets native-simulation)
if(SANDBOX_WITH_UNREAL)
  # Tracy DLL imports are compiled into the simulation objects used by Unreal.
  add_library(native-simulation-unreal STATIC)
  get_target_property(native_simulation_sources native-simulation SOURCES)
  target_sources(native-simulation-unreal PRIVATE ${native_simulation_sources})
  list(APPEND native_simulation_targets native-simulation-unreal)
endif()

foreach(native_simulation_target IN LISTS native_simulation_targets)
  target_compile_features(${native_simulation_target} PUBLIC cxx_std_23)
  target_include_directories(${native_simulation_target} PUBLIC
    include
    "${PROJECT_SOURCE_DIR}/native/core/include"
    "${PROJECT_SOURCE_DIR}/native/lispb/native_soa/include"
  )
  target_link_libraries(${native_simulation_target}
    PUBLIC sandbox::handmade_math native-memory native-core
    PRIVATE sandbox::warnings_as_errors
  )
  if(SANDBOX_WITH_TRACY)
    target_compile_definitions(${native_simulation_target} PRIVATE SANDBOX_WITH_TRACY)
    target_link_libraries(${native_simulation_target} PUBLIC sandbox::profiling-api)
    if(native_simulation_target STREQUAL "native-simulation-unreal")
      target_link_libraries(${native_simulation_target} PRIVATE TracyClientUnreal)
    else()
      target_link_libraries(${native_simulation_target} PRIVATE
        Tracy::TracyClient
        sandbox::profiling
      )
    endif()
  endif()

  set_target_properties(${native_simulation_target} PROPERTIES
    OUTPUT_NAME SandboxNativeSimulation
  )
  if(SANDBOX_WITH_UNREAL)
    target_link_libraries(${native_simulation_target} PRIVATE sandbox::unreal_avx2)
  endif()
endforeach()

if(SANDBOX_WITH_UNREAL)
  set_target_properties(native-simulation-unreal PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/Simulation/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_subdirectory(tests)

list(APPEND native_simulation_targets native-simulation-tests)
foreach(native_simulation_target IN LISTS native_simulation_targets)
  set_property(TARGET ${native_simulation_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${native_simulation_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)

  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(${native_simulation_target} PRIVATE /permissive-)
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_simulation_target} PRIVATE /O2)
    endif()
  else()
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_simulation_target} PRIVATE -O2)
    endif()
  endif()

  if(UE_CONFIGURATION MATCHES "^(Shipping|Test)$")
    target_compile_definitions(${native_simulation_target} PRIVATE NDEBUG)
  endif()
endforeach()
