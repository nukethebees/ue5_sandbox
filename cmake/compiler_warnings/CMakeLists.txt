include(add_supported_interface_options.cmake)

add_library(sandbox_warning_level INTERFACE)
add_library(sandbox_warning_errors INTERFACE)
add_library(sandbox_codegen_warning_exceptions INTERFACE)
add_library(sandbox_msvc_strict INTERFACE)

if(SANDBOX_MSVC_FRONTEND)
  target_add_supported_options(sandbox_warning_level COMPILE /W4 /wd4324)
  target_add_supported_options(sandbox_warning_errors COMPILE /WX)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_add_supported_options(sandbox_codegen_warning_exceptions COMPILE
      /clang:-Wno-missing-designated-field-initializers
      /clang:-Wno-missing-field-initializers
    )
  endif()
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(sandbox_msvc_additional_warnings
    /w44191 # A function pointer conversion is unsafe.
    /w44242 # A conversion may lose data.
    /w44254 # An operator conversion may lose data.
    /w44263 # A member function does not override a base virtual function.
    /w44264 # A base virtual function is hidden and has no override.
    /w44265 # A class with virtual functions has a non-virtual destructor.
    /w44266 # A derived function hides a base virtual overload.
    /w44287 # An unsigned value is compared with a negative constant.
    /w44355 # this is used in a base member initializer.
    /w44388 # A comparison mixes signed and unsigned values.
    /w44545 # A function name before a comma is missing an argument list.
    /w44546 # A function call before a comma is missing an argument list.
    /w44547 # An operator before a comma has no effect.
    /w44548 # An expression before a comma has no effect.
    /w44549 # An operator before a comma probably has the wrong intent.
    /w44555 # An expression has no effect.
    /w44574 # A macro defined as zero may have been used with #ifdef by mistake.
    /w44577 # noexcept is used without an exception-handling mode.
    /w44608 # More than one union member is initialized.
    /w44619 # A warning pragma refers to an unknown warning number.
    /w44643 # Code forward-declares a name in namespace std.
    /w44668 # An undefined preprocessor symbol is replaced with zero.
    /w44749 # offsetof is used with a non-standard-layout type.
    /w44774 # A format argument is not a string literal.
    /w44777 # A variadic argument does not match its format specifier.
    /w44800 # A value is implicitly converted to bool.
    /w44826 # A conversion sign-extends a value unexpectedly.
    /w44905 # A wide string literal is cast to a narrow string pointer.
    /w44906 # A narrow string literal is cast to a wide string pointer.
    /w44928 # Copy initialization applies multiple user-defined conversions.
    /w44946 # reinterpret_cast is used between related classes.
    /w44986 # An exception specification differs from an earlier declaration.
    /w44987 # A deprecated dynamic exception specification is used.
    /w45029 # An alignment attribute is applied in an invalid context.
    /w45031 # Warning state is popped from a different source file.
    /w45032 # Warning state is pushed without a matching pop.
    /w45038 # Members are initialized in declaration order, not list order.
    /w45039 # A potentially throwing callback crosses an extern C boundary.
    /w45041 # A redundant out-of-line constexpr definition is used.
    /w45042 # inline is used on a block-scope function declaration.
    /w45204 # A polymorphic class has a non-virtual trivial destructor.
    /w45214 # An operation on a volatile value is deprecated.
    /w45215 # A volatile-qualified function parameter is deprecated.
    /w45216 # A volatile-qualified return type is deprecated.
    /w45217 # A volatile-qualified structured binding is deprecated.
    /w45220 # A volatile member changes generated special-member triviality.
    /w45233 # An explicitly captured lambda variable is unused.
    /w45240 # An attribute is ignored in its current position.
    /w45243 # Using an incomplete class may violate the ODR because of ABI limits.
    /w45249 # An enum bit-field cannot represent all named enumerators.
    /w45256 # A non-defining enum declaration is not standalone.
    /w45259 # An explicit specialization is missing template <>.
    /w45262 # A switch case falls through without an annotation.
    /w45263 # Moving a temporary prevents copy elision.
    /w45264 # A const variable is unused.
    /w45266 # A const qualifier on a return type has no effect.
    /w45267 # An implicitly generated copy operation is deprecated.
  )
  foreach(sandbox_msvc_warning IN LISTS sandbox_msvc_additional_warnings)
    target_add_supported_options(sandbox_msvc_strict COMPILE
      "${sandbox_msvc_warning}"
    )
  endforeach()

  set(sandbox_msvc_conformance_options
    /permissive-
    /Zc:__cplusplus
    /Zc:alignedNew
    /Zc:auto
    /Zc:char8_t
    /Zc:checkGwOdr
    /Zc:enumTypes
    /Zc:externC
    /Zc:externConstexpr
    /Zc:forScope
    /Zc:gotoScope
    /Zc:hiddenFriend
    /Zc:implicitNoexcept
    /Zc:inline
    /Zc:lambda
    /Zc:noexceptTypes
    /Zc:preprocessor
    /Zc:referenceBinding
    /Zc:rvalueCast
    /Zc:sizedDealloc
    /Zc:static_assert
    /Zc:strictStrings
    /Zc:templateScope
    /Zc:ternary
    /Zc:threadSafeInit
    /Zc:tlsGuards
    /Zc:u8EscapeEncoding
    /Zc:wchar_t
    /Zc:zeroSizeArrayNew
  )
  foreach(sandbox_msvc_conformance_option IN LISTS sandbox_msvc_conformance_options)
    target_add_supported_options(sandbox_msvc_strict COMPILE
      "${sandbox_msvc_conformance_option}"
    )
  endforeach()
endif()

add_library(sandbox_warnings INTERFACE)
add_library(sandbox::warnings ALIAS sandbox_warnings)
target_link_libraries(sandbox_warnings INTERFACE
  sandbox_warning_level
  sandbox_msvc_strict
)

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
