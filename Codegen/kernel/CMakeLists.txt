add_library(kernel-codegen-lib
  src/compiler.cpp
  src/lowering.cpp
  src/manifest.cpp
  src/parser.cpp
  src/reference_evaluator.cpp
  src/renderer.cpp
  src/standard_renderer.cpp
  src/test_renderer.cpp
  src/unreal_renderer.cpp
)
target_compile_features(kernel-codegen-lib PUBLIC cxx_std_23)
target_include_directories(kernel-codegen-lib PUBLIC include)
target_link_libraries(kernel-codegen-lib PRIVATE
  codegen-lib
  codegen-sexpr
  nlohmann_json::nlohmann_json
)

add_executable(sandbox-kernelc src/main.cpp)
set_target_properties(sandbox-kernelc PROPERTIES OUTPUT_NAME kernelc)
target_link_libraries(sandbox-kernelc PRIVATE kernel-codegen-lib)

add_executable(kernel-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
target_include_directories(kernel-codegen-tests PRIVATE src)
target_link_libraries(kernel-codegen-tests PRIVATE
  codegen-lib
  kernel-codegen-lib
  codegen-sexpr
  GTest::gtest_main
)

if(MSVC)
  target_compile_options(kernel-codegen-lib PRIVATE /W4 /WX)
  target_compile_options(sandbox-kernelc PRIVATE /W4 /WX)
  target_compile_options(kernel-codegen-tests PRIVATE /W4 /WX)
else()
  target_compile_options(kernel-codegen-lib PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(sandbox-kernelc PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(kernel-codegen-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()

gtest_discover_tests(kernel-codegen-tests PROPERTIES LABELS "unit;all;codegen")

set(KERNEL_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(KERNEL_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(KERNEL_COMPILE_FILES
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.h"
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_COMPILE_FILES}
  COMMAND sandbox-kernelc
    --manifest "${KERNEL_COMPILE_FIXTURE}/manifest.json"
    --output-root "${KERNEL_COMPILE_OUTPUT}"
    --profile unreal
  DEPENDS
    sandbox-kernelc
    "${KERNEL_COMPILE_FIXTURE}/manifest.json"
    "${KERNEL_COMPILE_FIXTURE}/array_math.sbxkernel"
  VERBATIM
)
add_executable(kernel-generated-compile-tests
  tests/compile/main.cpp
  ${KERNEL_COMPILE_FILES}
)
target_compile_features(kernel-generated-compile-tests PRIVATE cxx_std_23)
target_compile_definitions(kernel-generated-compile-tests PRIVATE CODEGEN_CHECK_EXIT)
target_include_directories(kernel-generated-compile-tests PRIVATE
  "${KERNEL_COMPILE_OUTPUT}"
  "${CMAKE_SOURCE_DIR}/Codegen/tests/compile_fixture/stubs"
)
if(MSVC)
  target_compile_options(kernel-generated-compile-tests PRIVATE /W4 /WX)
else()
  target_compile_options(kernel-generated-compile-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()
add_dependencies(kernel-codegen-tests kernel-generated-compile-tests)
add_test(NAME KernelGeneratedCompileSmoke COMMAND kernel-generated-compile-tests)
set_tests_properties(KernelGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")
foreach(INVARIANT_CASE IN ITEMS unequal-lengths overlapping-output overlapping-in-place)
  add_test(
    NAME "KernelGeneratedInvariant.${INVARIANT_CASE}"
    COMMAND kernel-generated-compile-tests "${INVARIANT_CASE}"
  )
  set_tests_properties(
    "KernelGeneratedInvariant.${INVARIANT_CASE}"
    PROPERTIES
      LABELS "unit;all;codegen"
      WILL_FAIL TRUE
  )
endforeach()

set(KERNEL_PRODUCTION_ROOT
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SandboxCore"
)
set(KERNEL_PRODUCTION_MANIFEST "${KERNEL_PRODUCTION_ROOT}/Kernels/manifest.json")
set(KERNEL_PRODUCTION_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/array_math.sbxkernel")
set(KERNEL_CANDIDATE_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/candidate_math.sbxkernel")

set(KERNEL_STANDARD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/standard-generated")
set(KERNEL_STANDARD_FILES
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels_tests.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels_tests.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_STANDARD_FILES}
  COMMAND sandbox-kernelc
    --manifest "${KERNEL_PRODUCTION_MANIFEST}"
    --output-root "${KERNEL_STANDARD_OUTPUT}"
    --profile standard
  DEPENDS
    sandbox-kernelc
    "${KERNEL_PRODUCTION_MANIFEST}"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  COMMENT "Generating standard C++ kernel tests"
  VERBATIM
)
add_executable(kernel-generated-standard-tests
  tests/standard_anchor_tests.cpp
  ${KERNEL_STANDARD_FILES}
)
target_compile_features(kernel-generated-standard-tests PRIVATE cxx_std_23)
target_include_directories(kernel-generated-standard-tests PRIVATE "${KERNEL_STANDARD_OUTPUT}")
target_link_libraries(kernel-generated-standard-tests PRIVATE GTest::gtest_main)
if(MSVC)
  target_compile_options(kernel-generated-standard-tests PRIVATE /W4 /WX)
else()
  target_compile_options(kernel-generated-standard-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()
gtest_discover_tests(kernel-generated-standard-tests PROPERTIES LABELS "unit;all;codegen")
add_dependencies(kernel-codegen-tests kernel-generated-standard-tests)

add_custom_target(generate-kernel-code
  COMMAND sandbox-kernelc
    --manifest "${KERNEL_PRODUCTION_MANIFEST}"
    --output-root "${KERNEL_PRODUCTION_ROOT}"
    --profile unreal
  DEPENDS
    sandbox-kernelc
    "${KERNEL_PRODUCTION_MANIFEST}"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-kernel-code
  COMMAND sandbox-kernelc
    --manifest "${KERNEL_PRODUCTION_MANIFEST}"
    --output-root "${KERNEL_PRODUCTION_ROOT}"
    --profile unreal
    --check
  DEPENDS
    sandbox-kernelc
    "${KERNEL_PRODUCTION_MANIFEST}"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

add_dependencies(generate-code generate-kernel-code)
add_dependencies(check-generated-code check-generated-kernel-code)
