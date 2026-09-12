find_package(GTest CONFIG REQUIRED)
find_program(SANDBOX_CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
if(SANDBOX_KERNEL_BENCHMARKS)
  find_package(benchmark CONFIG REQUIRED)
  find_package(CpuFeatures CONFIG REQUIRED)
endif()
if(SANDBOX_KERNEL_BENCHMARK_PLOTS)
  if(NOT SANDBOX_KERNEL_BENCHMARKS)
    message(FATAL_ERROR
      "SANDBOX_KERNEL_BENCHMARK_PLOTS requires SANDBOX_KERNEL_BENCHMARKS")
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(UV_EXECUTABLE NAMES uv REQUIRED)
endif()

add_subdirectory(sexpr)
add_subdirectory(material)

add_library(lispb-output
  src/generator.cpp
  src/format_generated.cpp
)
target_compile_features(lispb-output PUBLIC cxx_std_23)
target_compile_definitions(lispb-output
  PRIVATE SANDBOX_CLANG_FORMAT_EXECUTABLE="${SANDBOX_CLANG_FORMAT_EXECUTABLE}"
)
target_include_directories(lispb-output PUBLIC include)
target_link_libraries(lispb-output PRIVATE sandbox::codegen_warnings)

add_library(lispb-project src/project.cpp)
target_compile_features(lispb-project PUBLIC cxx_std_23)
target_include_directories(lispb-project PUBLIC include)
target_link_libraries(lispb-project PRIVATE lispb-syntax sandbox::codegen_warnings)

add_library(codegen-lib
  src/ast.cpp
  src/expr.cpp
  src/enum_lowering.cpp
  src/facade_lowering.cpp
  src/fixed_soa_lowering.cpp
  src/fixed_soa_layout.cpp
  src/fixed_soa_container_lowering.cpp
  src/fixed_soa_storage_lowering.cpp
  src/cpp_generator.cpp
  src/homogeneous_lowering.cpp
  src/homogeneous_storage_lowering.cpp
  src/homogeneous_view_lowering.cpp
  src/source_loader.cpp
  src/lowering_utils.cpp
  src/schema.cpp
  src/settings_lowering.cpp
  src/soa_lowering.cpp
  src/soa_member_resolution.cpp
  src/soa_storage_lowering.cpp
  src/single_allocation_soa_emission.cpp
  src/single_allocation_soa_lowering.cpp
  src/single_allocation_soa_model.cpp
  src/native_soa_lowering.cpp
  src/soa_view_lowering.cpp
  src/static_table_lowering.cpp
  src/validation.cpp
  src/vector_lowering.cpp
)
target_compile_features(codegen-lib PUBLIC cxx_std_23)
target_include_directories(codegen-lib PUBLIC include)
target_link_libraries(codegen-lib
  PUBLIC
    lispb-output
  PRIVATE
    lispb-syntax
    sandbox::codegen_warnings
)

add_executable(lispb
  src/main.cpp
  material/compiler.cpp
)
target_compile_features(lispb PRIVATE cxx_std_23)
target_link_libraries(lispb PRIVATE
  codegen-lib
  kernel-codegen-lib
  lispb-project
  material-gen
  slate-codegen-lib
  sandbox::codegen_warnings
)

set(CODEGEN_COMPILE_FIXTURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fixture")
set(CODEGEN_COMPILE_FIXTURE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated_compile_fixture")
set(CODEGEN_COMPILE_FIXTURE_HEADERS
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Generated.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Homogeneous.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Soa.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector1.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector2.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector3.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/InlineFacade.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SourceFacade.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PlainEnums.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/ReflectedEnums.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/StaticTables.h"
)
set(CODEGEN_COMPILE_FIXTURE_SOURCES
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Homogeneous.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Soa.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector1.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector2.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector3.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SourceFacade.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PlainEnums.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/ReflectedEnums.cpp"
)
add_custom_command(
  OUTPUT
    ${CODEGEN_COMPILE_FIXTURE_HEADERS}
    ${CODEGEN_COMPILE_FIXTURE_SOURCES}
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target cpp-compile-fixture
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/types.lispb"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/modules.lispb"
  COMMENT "Generating compile-tested codegen fixture"
  VERBATIM
)
add_executable(codegen-generated-compile-tests
  tests/generated_compile_smoke.cpp
  ${CODEGEN_COMPILE_FIXTURE_HEADERS}
  ${CODEGEN_COMPILE_FIXTURE_SOURCES}
)
target_compile_features(codegen-generated-compile-tests PRIVATE cxx_std_23)
target_include_directories(codegen-generated-compile-tests PRIVATE
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
  "${CODEGEN_COMPILE_FIXTURE_SOURCE}/stubs"
  "${PROJECT_SOURCE_DIR}/native/core/include"
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SandboxCore/Public"
)
target_link_libraries(codegen-generated-compile-tests PRIVATE sandbox::codegen_warnings)

add_executable(codegen-tests
  tests/ast_tests.cpp
  tests/expr_tests.cpp
  tests/fixed_soa_lowering_tests.cpp
  tests/single_allocation_soa_tests.cpp
  tests/generator_tests.cpp
  tests/homogeneous_lowering_tests.cpp
  tests/source_loader_tests.cpp
  tests/lowering_tests.cpp
  tests/static_table_lowering_tests.cpp
  tests/validation_tests.cpp
)
add_dependencies(codegen-tests
  codegen-generated-compile-tests
  lispb-syntax-tests
  material-gen-tests
)
target_link_libraries(codegen-tests PRIVATE
  codegen-lib
  lispb-project
  GTest::gtest_main
  sandbox::codegen_warnings
)
target_compile_definitions(codegen-tests PRIVATE
  SANDBOX_CODEGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(codegen-tests PROPERTIES LABELS "unit;all;codegen")
add_test(NAME CodegenGeneratedCompileSmoke COMMAND codegen-generated-compile-tests)
set_tests_properties(CodegenGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")
foreach(restriction IN ITEMS COPY DESTRUCTOR DEFAULT)
  set(reject_target "codegen-reject-single-${restriction}")
  add_library(${reject_target} OBJECT EXCLUDE_FROM_ALL tests/compile_fixture/rejected_leaf.cpp)
  add_dependencies(${reject_target} codegen-generated-compile-tests)
  target_compile_features(${reject_target} PRIVATE cxx_std_23)
  target_compile_definitions(${reject_target} PRIVATE "CODEGEN_BAD_${restriction}")
  target_include_directories(${reject_target} PRIVATE
    "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/stubs"
    "${PROJECT_SOURCE_DIR}/native/core/include"
    "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SandboxCore/Public"
  )
  target_link_libraries(${reject_target} PRIVATE sandbox::codegen_warnings)
  add_test(NAME "SingleAllocationRejects${restriction}"
    COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DREJECT_TARGET=${reject_target}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reject_single_allocation_leaf.cmake")
  set_tests_properties("SingleAllocationRejects${restriction}" PROPERTIES
    LABELS "unit;all;codegen" RESOURCE_LOCK codegen-negative-build)
endforeach()
add_test(NAME CodegenCliChecksGeneratedFixture
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target cpp-compile-fixture
)
set_tests_properties(CodegenCliChecksGeneratedFixture PROPERTIES LABELS "unit;all;codegen")
add_test(NAME CodegenCliRejectsUnknownArgument COMMAND lispb generate --unknown value)
set_tests_properties(CodegenCliRejectsUnknownArgument PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsMissingArgumentValue COMMAND lispb generate --target)
set_tests_properties(CodegenCliRejectsMissingArgumentValue PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsOptionAsArgumentValue
  COMMAND lispb generate --project --target
)
set_tests_properties(CodegenCliRejectsOptionAsArgumentValue PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsDuplicateArguments
  COMMAND lispb generate --target sandbox-code --target sandbox-code
)
set_tests_properties(CodegenCliRejectsDuplicateArguments PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliReportsMissingProject
  COMMAND lispb generate --project missing-project.lispb --target sandbox-code
)
set_tests_properties(CodegenCliReportsMissingProject PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)

add_custom_target(generate-code
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target sandbox-code
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-code
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target sandbox-code
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)

add_subdirectory(slate)
add_subdirectory(kernel)
if(SANDBOX_NATIVE_SOA_BENCHMARKS)
  add_subdirectory(native_soa)
endif()
