set(SANDBOX_S7_DEFINITIONS
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
target_sources(sandbox-s7-third-party PRIVATE
  src/s7_sandbox.cpp
)
target_compile_features(sandbox-s7-third-party PRIVATE cxx_std_23)
target_compile_definitions(sandbox-s7-third-party PRIVATE ${SANDBOX_S7_DEFINITIONS})
target_include_directories(sandbox-s7-third-party PRIVATE ../ThirdParty/s7)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(sandbox-s7-third-party PRIVATE
    "-Wno-#pragma-messages"
    -Wno-constant-logical-operand
    -Wno-inconsistent-dllimport
  )
endif()
configure_sandbox_s7_abi(sandbox-s7-third-party)

add_library(sandbox-s7 STATIC)
target_sources(sandbox-s7 PRIVATE
  src/interpreter.cpp
  src/value.cpp
  $<TARGET_OBJECTS:sandbox-s7-third-party>
)
target_compile_features(sandbox-s7 PUBLIC cxx_std_23)
target_compile_definitions(sandbox-s7 PRIVATE ${SANDBOX_S7_DEFINITIONS})
target_include_directories(sandbox-s7
  PUBLIC include
  PRIVATE ../ThirdParty/s7
)
target_link_libraries(sandbox-s7 PRIVATE sandbox::warnings_as_errors)
configure_sandbox_s7_abi(sandbox-s7)

foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
  file(MAKE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/S7/${UE_PLATFORM}/${configuration}"
  )
endforeach()

set_target_properties(sandbox-s7 PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/S7/${UE_PLATFORM}/${UE_CONFIGURATION}"
  OUTPUT_NAME sandbox_s7
)

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "sandbox-s7 requires an MSVC-compatible ABI on Windows.")
endif()
