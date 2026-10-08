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
target_sources(sandbox-s7-third-party
  PRIVATE
    s7_sandbox.cpp
)
target_link_libraries(sandbox-s7-third-party
  PRIVATE
    ioj::cxx_standard
    ioj::asan
    ioj::cxx_conformance
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
target_compile_definitions(sandbox-s7-third-party
  PUBLIC
    ${IOJ_S7_DEFINITIONS}
)
target_include_directories(sandbox-s7-third-party SYSTEM
  PUBLIC
    s7
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(sandbox-s7-third-party
    PRIVATE
      "-Wno-#pragma-messages"
      -Wno-constant-logical-operand
      -Wno-inconsistent-dllimport
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sandbox-s7-third-party
    PRIVATE
      /wd4273 # s7 redeclares the CRT's __p__environ without its dllimport annotation.
  )
endif()
ioj_apply_native_abi(sandbox-s7-third-party)


target_include_directories(sandbox-s7-third-party
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
