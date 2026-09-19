set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_SystemInclude ON CACHE BOOL "" FORCE)
add_subdirectory(nlohmann_json)

set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(cli11 EXCLUDE_FROM_ALL)

if(SANDBOX_LAYOUT_PLANNER)
  if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set(SDL_CPU_X64 ON CACHE BOOL "Detected architecture X64" FORCE)
  endif()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(sdl EXCLUDE_FROM_ALL)
  get_target_property(sdl_include_directories SDL3-static INTERFACE_INCLUDE_DIRECTORIES)
  set_property(TARGET SDL3-static PROPERTY
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${sdl_include_directories}")

  add_library(sandbox-imgui)
  target_sources(sandbox-imgui PRIVATE
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_sdlgpu3.cpp
  )
  target_compile_features(sandbox-imgui PUBLIC cxx_std_23)
  target_include_directories(sandbox-imgui SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
  )
  target_link_libraries(sandbox-imgui PUBLIC SDL3::SDL3-static)
  set_property(TARGET sandbox-imgui PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()

foreach(google_test_target IN ITEMS gtest gtest_main)
  set_property(TARGET ${google_test_target} PROPERTY
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${google_test_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endforeach()

set(SANDBOX_CPU_FEATURES_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/cpu_features")
add_subdirectory(cpu_features_integration)
unset(SANDBOX_CPU_FEATURES_SOURCE_DIRECTORY)
set_property(TARGET cpu_features PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

if(SANDBOX_WITH_UNREAL)
  target_link_libraries(cpu_features PRIVATE sandbox::unreal_avx2)
  set_target_properties(cpu_features PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/CpuFeatures/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

if(SANDBOX_KERNEL_BENCHMARKS OR SANDBOX_NATIVE_SOA_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_DOXYGEN OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
  add_subdirectory(benchmark)

  foreach(google_benchmark_target IN ITEMS benchmark benchmark_main)
    set_property(TARGET ${google_benchmark_target} PROPERTY
      MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    target_compile_definitions(${google_benchmark_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
  endforeach()
endif()
