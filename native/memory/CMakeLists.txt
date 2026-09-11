add_library(native-memory STATIC
  src/block.cpp
  src/root_arena.cpp
)
target_compile_features(native-memory PUBLIC cxx_std_23)
target_include_directories(native-memory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(NATIVE_MEMORY_LIBRARY_DIRECTORY
  "${PROJECT_SOURCE_DIR}/Binaries/NativeMemory/${UE_PLATFORM}/${UE_CONFIGURATION}"
)
set_target_properties(native-memory PROPERTIES
  OUTPUT_NAME SandboxNativeMemory
  ARCHIVE_OUTPUT_DIRECTORY "${NATIVE_MEMORY_LIBRARY_DIRECTORY}"
)

add_executable(native-memory-tests tests/root_arena_tests.cpp)
target_link_libraries(native-memory-tests PRIVATE native-memory)
add_test(NAME native-memory-tests COMMAND native-memory-tests)
set_tests_properties(native-memory-tests PROPERTIES LABELS "unit;all;native-memory")

foreach(native_memory_target IN ITEMS native-memory native-memory-tests)
  set_property(TARGET ${native_memory_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${native_memory_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)

  if(MSVC)
    target_compile_options(${native_memory_target} PRIVATE /W4 /WX /permissive-)
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_memory_target} PRIVATE /O2)
    endif()
  else()
    target_compile_options(${native_memory_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_memory_target} PRIVATE -O2)
    endif()
  endif()

  if(UE_CONFIGURATION MATCHES "^(Shipping|Test)$")
    target_compile_definitions(${native_memory_target} PRIVATE NDEBUG)
  endif()
endforeach()
