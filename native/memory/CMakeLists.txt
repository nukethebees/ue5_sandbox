add_library(native-memory STATIC)
target_sources(native-memory PRIVATE
  src/backing.cpp
  src/block.cpp
  src/root_arena.cpp
)
target_compile_features(native-memory PUBLIC cxx_std_23)
target_link_libraries(native-memory PUBLIC ioj::cxx_conformance)
target_include_directories(native-memory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(native-memory PRIVATE sandbox::native_defaults)
ioj_enable_warnings(native-memory)
if(IOJ_WITH_TRACY)
  target_compile_definitions(native-memory PRIVATE IOJ_WITH_TRACY)
  target_link_libraries(native-memory PRIVATE sandbox::profiling)
endif()

set_target_properties(native-memory PROPERTIES
  OUTPUT_NAME SandboxNativeMemory
)
if(IOJ_WITH_UNREAL)
  target_link_libraries(native-memory PRIVATE ioj::unreal_avx2)
  set_target_properties(native-memory PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/NativeMemory/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_executable(native-memory-tests)
target_sources(native-memory-tests PRIVATE tests/root_arena_tests.cpp)
target_link_libraries(native-memory-tests PRIVATE
  native-memory
  sandbox::native_defaults
)
ioj_enable_warnings(native-memory-tests)
add_test(NAME native-memory-tests COMMAND native-memory-tests)
set_tests_properties(native-memory-tests PROPERTIES LABELS "unit;all;native;native-memory")

ioj_apply_native_abi(native-memory)
ioj_apply_native_abi(native-memory-tests)
