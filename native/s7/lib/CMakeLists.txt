set(IOJ_S7_DEFINITIONS
  DISABLE_AUTOLOAD=1
  DISABLE_FILE_OUTPUT=1
  HAVE_COMPLEX_NUMBERS=0
  HAVE_COMPLEX_TRIG=0
  WITH_C_LOADER=0
  WITH_GMP=0
  WITH_MAIN=0
  WITH_SYSTEM_EXTRAS=0
)

add_library(sandbox-s7-third-party OBJECT)
set_target_properties(sandbox-s7-third-party PROPERTIES UNITY_BUILD OFF)
target_sources(sandbox-s7-third-party PRIVATE
  src/s7_sandbox.cpp
)
target_link_libraries(sandbox-s7-third-party
  PRIVATE
    ioj::cxx_standard
    ioj::asan
    ioj::cxx_conformance
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
target_compile_definitions(sandbox-s7-third-party PUBLIC ${IOJ_S7_DEFINITIONS})
target_include_directories(sandbox-s7-third-party SYSTEM PUBLIC ../ThirdParty/s7)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(sandbox-s7-third-party PRIVATE
    "-Wno-#pragma-messages"
    -Wno-constant-logical-operand
    -Wno-inconsistent-dllimport
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sandbox-s7-third-party PRIVATE
    /wd4273 # s7 redeclares the CRT's __p__environ without its dllimport annotation.
  )
endif()
ioj_apply_native_abi(sandbox-s7-third-party)

add_library(native-s7-data STATIC)
target_sources(native-s7-data PRIVATE
  src/ast.cpp
  src/sexpression_emitter.cpp)
target_include_directories(native-s7-data PUBLIC include)
target_link_libraries(native-s7-data PUBLIC native-common sandbox::native_defaults)
ioj_enable_warnings(native-s7-data)
ioj_apply_native_abi(native-s7-data)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-s7-data PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
    "${IOJ_NATIVE_OUTPUT_ROOT}/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}")
endif()

add_library(sandbox-s7 STATIC)
target_sources(sandbox-s7 PRIVATE
  src/interpreter.cpp
  src/ast_materialization.cpp
  src/s7_ownership.cpp
  src/sandbox_policy.cpp
  src/script_bindings.cpp
  src/script_files_win32.cpp
  src/script_loader.cpp
)
target_include_directories(sandbox-s7 PUBLIC include)

add_library(sandbox-s7-warning-exceptions INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sandbox-s7-warning-exceptions INTERFACE
    /wd5039 # The third-party s7 C callback typedefs cannot express noexcept.
  )
endif()
target_link_libraries(sandbox-s7
  PUBLIC
    sandbox::native_defaults
    native-s7-data
  PRIVATE
    sandbox-s7-third-party
    sandbox-s7-warning-exceptions
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
ioj_enable_warnings(sandbox-s7)
ioj_apply_native_abi(sandbox-s7)

set_target_properties(sandbox-s7 PROPERTIES
  OUTPUT_NAME sandbox_s7
)
if(IOJ_WITH_UNREAL)
  foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
    file(MAKE_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${configuration}"
    )
  endforeach()

  set_target_properties(sandbox-s7 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "sandbox-s7 requires an MSVC-compatible ABI on Windows.")
endif()
