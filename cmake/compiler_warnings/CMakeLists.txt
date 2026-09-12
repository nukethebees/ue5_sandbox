include(add_supported_interface_options.cmake)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(SANDBOX_MSVC_FRONTEND TRUE)
else()
  set(SANDBOX_MSVC_FRONTEND FALSE)
endif()
set(SANDBOX_MSVC_FRONTEND "${SANDBOX_MSVC_FRONTEND}" PARENT_SCOPE)

add_library(sandbox_warning_level INTERFACE)
add_library(sandbox_warning_errors INTERFACE)
add_library(sandbox_codegen_warning_exceptions INTERFACE)

if(SANDBOX_MSVC_FRONTEND)
  target_add_supported_options(sandbox_warning_level COMPILE /W4)
  target_add_supported_options(sandbox_warning_errors COMPILE /WX)
else()
  target_add_supported_options(sandbox_warning_level COMPILE
    -Wall
    -Wextra
    -Wpedantic
  )
  target_add_supported_options(sandbox_warning_errors COMPILE -Werror)
  target_add_supported_options(sandbox_codegen_warning_exceptions COMPILE
    -Wno-missing-designated-field-initializers
    -Wno-missing-field-initializers
  )
endif()

add_library(sandbox_warnings INTERFACE)
add_library(sandbox::warnings ALIAS sandbox_warnings)
target_link_libraries(sandbox_warnings INTERFACE sandbox_warning_level)

add_library(sandbox_warnings_as_errors INTERFACE)
add_library(sandbox::warnings_as_errors ALIAS sandbox_warnings_as_errors)
target_link_libraries(sandbox_warnings_as_errors INTERFACE
  sandbox_warnings
  sandbox_warning_errors
)

add_library(sandbox_codegen_warnings INTERFACE)
add_library(sandbox::codegen_warnings ALIAS sandbox_codegen_warnings)
target_link_libraries(sandbox_codegen_warnings INTERFACE
  sandbox_warnings_as_errors
  sandbox_codegen_warning_exceptions
)
