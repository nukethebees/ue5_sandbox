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

if(IOJ_BUILD_DEVELOPER_TOOLS)
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

  set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(nativefiledialog-extended EXCLUDE_FROM_ALL)

  add_library(sandbox-imgui)
  target_sources(sandbox-imgui PRIVATE
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_sdlgpu3.cpp
  )
  target_compile_features(sandbox-imgui PUBLIC cxx_std_23)
  target_include_directories(sandbox-imgui SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
  )
  target_link_libraries(sandbox-imgui PUBLIC SDL3::SDL3-static ioj::asan)
  ioj_apply_native_abi(sandbox-imgui)
endif()

foreach(google_test_target IN ITEMS gtest gtest_main)
  target_link_libraries(${google_test_target} PRIVATE ioj::asan)
  ioj_apply_native_abi(${google_test_target})
endforeach()

set(IOJ_CPU_FEATURES_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/cpu_features")
add_subdirectory(cpu_features_integration)
unset(IOJ_CPU_FEATURES_SOURCE_DIRECTORY)
ioj_apply_native_abi(cpu_features)

set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_LIST_TARGETS OFF CACHE BOOL "" FORCE)
add_subdirectory(highway EXCLUDE_FROM_ALL)
ioj_apply_native_abi(hwy)

# Keep explicit comparison targets isolated from baseline callers and unity builds.
function(ioj_configure_highway_comparison target isa)
  set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
  target_compile_features(${target} PRIVATE cxx_std_23)
  target_compile_definitions(${target} PRIVATE HWY_COMPILE_ONLY_STATIC)
  target_link_libraries(${target} PRIVATE hwy sandbox::native_defaults ioj::fp_precise)
  if(isa STREQUAL "avx512")
    set(architecture skylake-avx512)
    target_link_libraries(${target} PRIVATE ioj::arch_avx512)
  else()
    set(architecture haswell)
    target_link_libraries(${target} PRIVATE ioj::arch_avx2)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
    target_compile_options(${target} PRIVATE "/clang:-march=${architecture}")
  elseif(NOT MSVC)
    target_compile_options(${target} PRIVATE "-march=${architecture}")
  endif()
  ioj_apply_native_abi(${target})
  ioj_enable_warnings(${target})
endfunction()

if(IOJ_WITH_UNREAL)
  target_link_libraries(cpu_features PRIVATE ioj::unreal_avx2)
  set_target_properties(cpu_features PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/CpuFeatures/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_DOXYGEN OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(benchmark)

foreach(google_benchmark_target IN ITEMS benchmark benchmark_main)
  target_link_libraries(${google_benchmark_target} PRIVATE "$<BUILD_LOCAL_INTERFACE:ioj::asan>")
  ioj_apply_native_abi(${google_benchmark_target})
endforeach()
