add_library(sandbox_warnings INTERFACE)
add_library(sandbox::warnings ALIAS sandbox_warnings)

add_library(sandbox_warnings_as_errors INTERFACE)
add_library(sandbox::warnings_as_errors ALIAS sandbox_warnings_as_errors)
target_link_libraries(sandbox_warnings_as_errors INTERFACE sandbox::warnings)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(SANDBOX_MSVC_FRONTEND TRUE)
  target_compile_options(sandbox_warnings INTERFACE /W4)
  target_compile_options(sandbox_warnings_as_errors INTERFACE /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  set(SANDBOX_MSVC_FRONTEND FALSE)
  target_compile_options(sandbox_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
  )
  target_compile_options(sandbox_warnings_as_errors INTERFACE -Werror)
else()
  message(FATAL_ERROR
    "No strict warning policy is defined for '${CMAKE_CXX_COMPILER_ID}' with "
    "the '${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}' frontend.")
endif()
