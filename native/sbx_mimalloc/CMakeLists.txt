find_program(SBX_LLVM_NM NAMES llvm-nm REQUIRED)
find_program(SBX_LLVM_READOBJ NAMES llvm-readobj REQUIRED)

set(SBX_MIMALLOC_ROOT "${PROJECT_SOURCE_DIR}/native/third_party/mimalloc")
set(SBX_MIMALLOC_SOURCE "${SBX_MIMALLOC_ROOT}/src/static.c")
set(SBX_MIMALLOC_PREFIX_HEADER
  "${CMAKE_CURRENT_BINARY_DIR}/generated/sbx_mimalloc_prefix.h"
)
set(SBX_MIMALLOC_SYMBOL_SCRIPT
  "${PROJECT_SOURCE_DIR}/Scripts/sbx_mimalloc_symbols.py"
)

set_source_files_properties("${SBX_MIMALLOC_SOURCE}" PROPERTIES LANGUAGE CXX)

function(configure_sbx_mimalloc_object target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${SBX_MIMALLOC_ROOT}/include")
  target_compile_definitions(${target} PRIVATE
    MI_BUILD_RELEASE=1
    MI_DEBUG=0
    MI_STATIC_LIB=1
    MI_WIN_NOREDIRECT=1
    _ITERATOR_DEBUG_LEVEL=0
  )
  set_property(TARGET ${target} PROPERTY CXX_SCAN_FOR_MODULES OFF)
  set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

add_library(sbx-mimalloc-prefix-probe OBJECT "${SBX_MIMALLOC_SOURCE}")
configure_sbx_mimalloc_object(sbx-mimalloc-prefix-probe)

add_custom_command(
  OUTPUT "${SBX_MIMALLOC_PREFIX_HEADER}"
  COMMAND "${Python3_EXECUTABLE}" "${SBX_MIMALLOC_SYMBOL_SCRIPT}" generate
    --nm "${SBX_LLVM_NM}"
    --readobj "${SBX_LLVM_READOBJ}"
    --output "${SBX_MIMALLOC_PREFIX_HEADER}"
    $<TARGET_OBJECTS:sbx-mimalloc-prefix-probe>
  DEPENDS $<TARGET_OBJECTS:sbx-mimalloc-prefix-probe> "${SBX_MIMALLOC_SYMBOL_SCRIPT}"
  COMMAND_EXPAND_LISTS
  VERBATIM
)
add_custom_target(sbx-mimalloc-prefix-header DEPENDS "${SBX_MIMALLOC_PREFIX_HEADER}")

add_library(sbx-mimalloc-implementation OBJECT "${SBX_MIMALLOC_SOURCE}")
configure_sbx_mimalloc_object(sbx-mimalloc-implementation)
add_dependencies(sbx-mimalloc-implementation sbx-mimalloc-prefix-header)
if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(sbx-mimalloc-implementation PRIVATE
    /Zl
    "/FI${SBX_MIMALLOC_PREFIX_HEADER}"
  )
else()
  target_compile_options(sbx-mimalloc-implementation PRIVATE
    -fno-autolink
    -include "${SBX_MIMALLOC_PREFIX_HEADER}"
  )
endif()

set(SBX_MIMALLOC_AUDIT_STAMP "${CMAKE_CURRENT_BINARY_DIR}/sbx-mimalloc-symbol-audit.stamp")
add_custom_command(
  OUTPUT "${SBX_MIMALLOC_AUDIT_STAMP}"
  COMMAND "${Python3_EXECUTABLE}" "${SBX_MIMALLOC_SYMBOL_SCRIPT}" verify
    --nm "${SBX_LLVM_NM}"
    --readobj "${SBX_LLVM_READOBJ}"
    $<TARGET_OBJECTS:sbx-mimalloc-implementation>
  COMMAND "${CMAKE_COMMAND}" -E touch "${SBX_MIMALLOC_AUDIT_STAMP}"
  DEPENDS sbx-mimalloc-implementation "${SBX_MIMALLOC_SYMBOL_SCRIPT}"
  COMMAND_EXPAND_LISTS
  VERBATIM
)
add_custom_target(sbx-mimalloc-symbol-audit DEPENDS "${SBX_MIMALLOC_AUDIT_STAMP}")

add_library(sbx-mimalloc STATIC)
add_library(sandbox::mimalloc ALIAS sbx-mimalloc)
target_sources(sbx-mimalloc PRIVATE
  src/memory.cpp
  $<TARGET_OBJECTS:sbx-mimalloc-implementation>
)
target_compile_features(sbx-mimalloc PUBLIC cxx_std_17)
target_include_directories(sbx-mimalloc
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${SBX_MIMALLOC_ROOT}/include"
)
target_link_libraries(sbx-mimalloc
  PUBLIC psapi shell32 user32 advapi32 bcrypt
  PRIVATE sandbox::warnings_as_errors
)
set_property(TARGET sbx-mimalloc PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_definitions(sbx-mimalloc PRIVATE _ITERATOR_DEBUG_LEVEL=0)
add_dependencies(sbx-mimalloc sbx-mimalloc-symbol-audit)

set_target_properties(sbx-mimalloc PROPERTIES
  OUTPUT_NAME SandboxMimalloc
  ARCHIVE_OUTPUT_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/SbxMimalloc/${UE_PLATFORM}/${UE_CONFIGURATION}"
)

add_executable(sbx-mimalloc-tests tests/memory_tests.cpp)
target_link_libraries(sbx-mimalloc-tests PRIVATE
  sandbox::mimalloc
  sandbox::warnings_as_errors
)
set_property(TARGET sbx-mimalloc-tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_definitions(sbx-mimalloc-tests PRIVATE _ITERATOR_DEBUG_LEVEL=0)
add_test(NAME sbx-mimalloc-tests COMMAND sbx-mimalloc-tests)
set_tests_properties(sbx-mimalloc-tests PROPERTIES LABELS "unit;all;sbx-mimalloc")
