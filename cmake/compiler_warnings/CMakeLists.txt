include(add_supported_interface_options.cmake)

add_library(ioj_warning_level INTERFACE)
add_library(ioj::warning_level ALIAS ioj_warning_level)
add_library(ioj_codegen_warning_exceptions INTERFACE)
add_library(ioj::codegen_warning_exceptions ALIAS ioj_codegen_warning_exceptions)
add_library(ioj_clang_strict INTERFACE)
add_library(ioj::clang_strict ALIAS ioj_clang_strict)

if(IOJ_MSVC_FRONTEND)
  target_add_supported_options(ioj_warning_level COMPILE /W4 /wd4324)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_add_supported_options(ioj_codegen_warning_exceptions COMPILE
      -Wno-missing-designated-field-initializers
      -Wno-missing-field-initializers
    )
  endif()
else()
  target_add_supported_options(ioj_warning_level COMPILE
    -Wall
    -Wextra
    -Wpedantic
  )
  target_add_supported_options(ioj_codegen_warning_exceptions COMPILE
    -Wno-missing-designated-field-initializers
    -Wno-missing-field-initializers
  )
endif()

if(IOJ_IS_CLANG_CL)
  set(ioj_clang_additional_warnings
    -Wpedantic # Warn about language extensions outside the selected C++ standard.
    -Wcast-align # Warn when a cast increases the required pointer alignment.
    -Wcast-qual # Warn when a cast removes type qualifiers such as const.
    -Wcomma # Warn about suspicious uses of the comma operator.
    -Wconditional-uninitialized # Warn when a value may be uninitialized on some paths.
    -Wdeprecated-copy-dtor # Warn about deprecated implicit copies caused by a destructor.
    -Wformat=2 # Enable stricter format-string validation, including non-literal formats.
    -Wheader-hygiene # Warn when headers leak using directives into includers.
    -Wimplicit-fallthrough # Warn about unannotated switch fallthrough.
    -Wimplicit-float-conversion # Warn when an implicit floating-point conversion loses precision.
    -Wimplicit-int-conversion # Warn when an implicit integer conversion loses precision.
    -Wmissing-declarations # Warn about definitions without prior declarations.
    -Wmissing-noreturn # Warn when a function could be declared noreturn.
    -Wnon-virtual-dtor # Warn when a polymorphic type has a non-virtual destructor.
    -Wnull-dereference # Warn about statically evident null dereferences.
    -Wold-style-cast # Warn about C-style casts in C++ code.
    -Woverloaded-virtual # Warn when a declaration hides a base virtual overload.
    -Wshadow-uncaptured-local # Warn when a lambda declaration shadows an uncaptured local.
    -Wsuggest-destructor-override # Warn when a virtual destructor should use override.
    -Wsuggest-override # Warn when a virtual function should use override.
    -Wthread-safety # Enable attribute-driven thread-safety analysis.
    -Wundef # Warn when an undefined macro is evaluated by #if.
    -Wundefined-func-template # Warn when an instantiated function template lacks a definition.
    -Wunreachable-code-aggressive # Warn about unreachable statements, returns, and breaks.
    -Wunused-macros # Warn about macros unused in their defining translation unit.
    -Wunused-member-function # Warn about unused members in internal-linkage classes.
    -Wweak-vtables # Warn when a virtual table has no out-of-line key function.
    -Wzero-as-null-pointer-constant # Warn when literal zero is used as a null pointer.
  )
  foreach(ioj_clang_warning IN LISTS ioj_clang_additional_warnings)
    target_add_supported_options(ioj_clang_strict COMPILE
      "${ioj_clang_warning}"
    )
  endforeach()
endif()


add_library(ioj_warnings INTERFACE)
add_library(ioj::warnings ALIAS ioj_warnings)
target_link_libraries(ioj_warnings INTERFACE
  ioj::warning_level
  ioj::clang_strict
)

add_library(ioj_codegen_warnings INTERFACE)
add_library(ioj::codegen_warnings ALIAS ioj_codegen_warnings)
target_link_libraries(ioj_codegen_warnings INTERFACE
  ioj::warnings
  ioj::codegen_warning_exceptions
)

function(ioj_enable_warnings target)
  cmake_parse_arguments(PARSE_ARGV 1 warnings "CODEGEN" "" "")
  if(warnings_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "ioj_enable_warnings(${target}): ${warnings_UNPARSED_ARGUMENTS}")
  endif()
  if(warnings_CODEGEN)
    target_link_libraries(${target} PRIVATE ioj::codegen_warnings)
  else()
    target_link_libraries(${target} PRIVATE ioj::warnings)
  endif()
  # Explicit CMAKE_COMPILE_WARNING_AS_ERROR and --compile-no-warning-as-error win.
  if(NOT DEFINED CMAKE_COMPILE_WARNING_AS_ERROR)
    set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
  endif()
endfunction()
