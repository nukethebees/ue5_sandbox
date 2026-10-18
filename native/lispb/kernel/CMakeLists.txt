add_library(kernel-codegen-lib
  src/binary_operator.cpp
  src/document.cpp
  src/expressions.cpp
  src/scalar_type.cpp
  src/compiler.cpp
  src/lowering.cpp
  src/parser.cpp
  src/reference_evaluator.cpp
  src/renderer.cpp
  src/portable_renderer.cpp
  src/test_renderer.cpp
  src/validation.cpp
)
set_target_properties(kernel-codegen-lib PROPERTIES UNITY_BUILD OFF)
target_include_directories(kernel-codegen-lib PUBLIC include)
target_link_libraries(kernel-codegen-lib
  PUBLIC
    sandbox::native_defaults
    lispb-output
    lispb-syntax
  PRIVATE
    codegen-lib
    ioj::codegen_pch
)
ioj_enable_warnings(kernel-codegen-lib CODEGEN)

add_executable(kernel-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
set_target_properties(kernel-codegen-tests PROPERTIES UNITY_BUILD OFF)
target_include_directories(kernel-codegen-tests PRIVATE src)
target_link_libraries(kernel-codegen-tests PRIVATE
  codegen-lib
  kernel-codegen-lib
  lispb-syntax
  GTest::gtest_main
  sandbox::native_defaults
  ioj::test_pch
)
ioj_enable_warnings(kernel-codegen-tests CODEGEN)

add_test(NAME kernel-codegen-tests COMMAND kernel-codegen-tests)
set_tests_properties(kernel-codegen-tests PROPERTIES LABELS "unit;all;native;codegen")

set(KERNEL_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(KERNEL_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(KERNEL_COMPILE_FILES
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.h"
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_COMPILE_FILES}
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-compile-fixture
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_COMPILE_FIXTURE}/array_math.lispb"
  VERBATIM
)
add_executable(kernel-generated-compile-tests
  tests/compile/main.cpp
  ${KERNEL_COMPILE_FILES}
)
set_target_properties(kernel-generated-compile-tests PROPERTIES UNITY_BUILD OFF)
target_compile_definitions(kernel-generated-compile-tests PRIVATE CODEGEN_CHECK_EXIT)
target_include_directories(kernel-generated-compile-tests PRIVATE
  "${KERNEL_COMPILE_OUTPUT}"
  "${CMAKE_SOURCE_DIR}/native/lispb/tests/compile_fixture/stubs"
)
target_link_libraries(kernel-generated-compile-tests PRIVATE sandbox::native_defaults)
ioj_enable_warnings(kernel-generated-compile-tests CODEGEN)
add_dependencies(kernel-codegen-tests kernel-generated-compile-tests)
add_test(NAME KernelGeneratedCompileSmoke COMMAND kernel-generated-compile-tests)
set_tests_properties(KernelGeneratedCompileSmoke PROPERTIES LABELS "unit;all;native;codegen")
foreach(INVARIANT_CASE IN ITEMS unequal-lengths overlapping-output overlapping-in-place)
  add_test(
    NAME "KernelGeneratedInvariant.${INVARIANT_CASE}"
    COMMAND kernel-generated-compile-tests "${INVARIANT_CASE}"
  )
  set_tests_properties(
    "KernelGeneratedInvariant.${INVARIANT_CASE}"
    PROPERTIES
      LABELS "unit;all;native;codegen"
      WILL_FAIL TRUE
  )
endforeach()

set(KERNEL_PRODUCTION_ROOT
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SandboxCore"
)
set(KERNEL_PRODUCTION_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/array_math.lispb")
set(KERNEL_NATIVE_VECTOR_LERP_DOCUMENT "${PROJECT_SOURCE_DIR}/native/core/Kernels/vector_lerp.lispb")
set(KERNEL_CANDIDATE_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/candidate_math.lispb")

set(KERNEL_STANDARD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/standard-generated")
set(KERNEL_STANDARD_FILES
  "${KERNEL_STANDARD_OUTPUT}/include/ioj/generated/array_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/src/generated/array_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/tests/generated/array_math_kernels_tests.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels_tests.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_STANDARD_FILES}
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-standard
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  COMMENT "Generating standard C++ kernel tests"
  VERBATIM
)
add_executable(kernel-generated-standard-tests
  tests/standard_anchor_tests.cpp
  ${KERNEL_STANDARD_FILES}
)
set_target_properties(kernel-generated-standard-tests PROPERTIES UNITY_BUILD OFF)
target_include_directories(kernel-generated-standard-tests PRIVATE
  "${KERNEL_STANDARD_OUTPUT}"
  "${KERNEL_STANDARD_OUTPUT}/include"
)
target_link_libraries(kernel-generated-standard-tests PRIVATE
  GTest::gtest_main
  sandbox::native_defaults
)
ioj_enable_warnings(kernel-generated-standard-tests CODEGEN)
add_test(NAME kernel-generated-standard-tests COMMAND kernel-generated-standard-tests)
set_tests_properties(kernel-generated-standard-tests PROPERTIES LABELS "unit;all;native;codegen")
add_dependencies(kernel-codegen-tests kernel-generated-standard-tests)

add_custom_target(generate-kernel-code
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-unreal
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-kernel-code
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-unreal
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(generate-native-core-kernels
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-native-core
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_NATIVE_VECTOR_LERP_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed native-core kernels"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-native-core-kernels
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-native-core
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_NATIVE_VECTOR_LERP_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed native-core kernels"
  USES_TERMINAL
  VERBATIM
)

add_dependencies(generate-code generate-kernel-code)
add_dependencies(generate-code generate-native-core-kernels)
add_dependencies(check-generated-code check-generated-kernel-code)
add_dependencies(check-generated-code check-generated-native-core-kernels)
