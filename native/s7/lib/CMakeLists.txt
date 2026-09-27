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
target_compile_features(sandbox-s7-third-party PRIVATE cxx_std_23)
target_link_libraries(sandbox-s7-third-party PRIVATE ioj::asan ioj::cxx_conformance)
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

add_library(sandbox-s7 STATIC)
target_sources(sandbox-s7 PRIVATE
  src/interpreter.cpp
  src/value.cpp
)
target_compile_features(sandbox-s7 PUBLIC cxx_std_23)
target_include_directories(sandbox-s7 PUBLIC include)

add_library(sandbox-s7-warning-exceptions INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sandbox-s7-warning-exceptions INTERFACE
    /wd5039 # The third-party s7 C callback typedefs cannot express noexcept.
  )
endif()
target_link_libraries(sandbox-s7 PRIVATE
  sandbox-s7-third-party
  sandbox::native_defaults
  sandbox-s7-warning-exceptions
)
ioj_enable_warnings(sandbox-s7)
ioj_apply_native_abi(sandbox-s7)

set_target_properties(sandbox-s7 PROPERTIES
  OUTPUT_NAME sandbox_s7
)
if(IOJ_WITH_UNREAL)
  target_link_libraries(sandbox-s7-third-party PRIVATE ioj::unreal_avx2)
  target_link_libraries(sandbox-s7 PRIVATE ioj::unreal_avx2)
  foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
    file(MAKE_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${configuration}"
    )
  endforeach()

  set_target_properties(sandbox-s7 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "sandbox-s7 requires an MSVC-compatible ABI on Windows.")
endif()
