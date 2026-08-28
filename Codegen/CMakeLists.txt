find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(codegen-lib
  src/ast.cpp
  src/facade_lowering.cpp
  src/fixed_soa_lowering.cpp
  src/fixed_soa_layout.cpp
  src/fixed_soa_container_lowering.cpp
  src/fixed_soa_storage_lowering.cpp
  src/generator.cpp
  src/homogeneous_lowering.cpp
  src/homogeneous_storage_lowering.cpp
  src/homogeneous_view_lowering.cpp
  src/json.cpp
  src/lowering_utils.cpp
  src/schema.cpp
  src/soa_lowering.cpp
  src/soa_member_resolution.cpp
  src/soa_storage_lowering.cpp
  src/soa_view_lowering.cpp
  src/validation.cpp
  src/vector_lowering.cpp
)
target_compile_features(codegen-lib PUBLIC cxx_std_23)
target_include_directories(codegen-lib PUBLIC include)
target_link_libraries(codegen-lib PUBLIC nlohmann_json::nlohmann_json)

add_executable(sandbox-codegen src/main.cpp)
set_target_properties(sandbox-codegen PROPERTIES OUTPUT_NAME codegen)
target_link_libraries(sandbox-codegen PRIVATE codegen-lib)

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
)
set(CODEGEN_COMPILE_FIXTURE_SOURCES
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Homogeneous.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Soa.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector1.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector2.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Vector3.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SourceFacade.cpp"
)
add_custom_command(
  OUTPUT
    ${CODEGEN_COMPILE_FIXTURE_HEADERS}
    ${CODEGEN_COMPILE_FIXTURE_SOURCES}
  COMMAND sandbox-codegen
    --manifest "${CODEGEN_COMPILE_FIXTURE_SOURCE}/manifest.json"
    --project-root "${CODEGEN_COMPILE_FIXTURE_SOURCE}"
    --output-root "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
  DEPENDS
    sandbox-codegen
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/manifest.json"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/types.json"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/modules.json"
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
)

add_executable(codegen-tests
  tests/ast_tests.cpp
  tests/fixed_soa_lowering_tests.cpp
  tests/generator_tests.cpp
  tests/homogeneous_lowering_tests.cpp
  tests/json_tests.cpp
  tests/lowering_tests.cpp
  tests/validation_tests.cpp
)
add_dependencies(codegen-tests codegen-generated-compile-tests)
target_link_libraries(codegen-tests PRIVATE
  codegen-lib
  GTest::gtest_main
)
target_compile_definitions(codegen-tests PRIVATE
  SANDBOX_CODEGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
  target_compile_options(codegen-lib PRIVATE /W4 /WX)
  target_compile_options(sandbox-codegen PRIVATE /W4 /WX)
  target_compile_options(codegen-tests PRIVATE /W4 /WX)
  target_compile_options(codegen-generated-compile-tests PRIVATE /W4 /WX)
else()
  set(SANDBOX_CODEGEN_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-missing-designated-field-initializers
    -Wno-missing-field-initializers
  )
  target_compile_options(codegen-lib PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(sandbox-codegen PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(codegen-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(codegen-generated-compile-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()

include(GoogleTest)
gtest_discover_tests(codegen-tests PROPERTIES LABELS "unit;all;codegen")
add_test(NAME CodegenGeneratedCompileSmoke COMMAND codegen-generated-compile-tests)
set_tests_properties(CodegenGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")
add_test(NAME CodegenCliChecksGeneratedFixture
  COMMAND sandbox-codegen
    --manifest "${CODEGEN_COMPILE_FIXTURE_SOURCE}/manifest.json"
    --project-root "${CODEGEN_COMPILE_FIXTURE_SOURCE}"
    --output-root "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
    --check
)
set_tests_properties(CodegenCliChecksGeneratedFixture PROPERTIES LABELS "unit;all;codegen")
add_test(NAME CodegenCliRejectsUnknownArgument COMMAND sandbox-codegen --unknown)
set_tests_properties(CodegenCliRejectsUnknownArgument PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsMissingArgumentValue COMMAND sandbox-codegen --manifest)
set_tests_properties(CodegenCliRejectsMissingArgumentValue PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsOptionAsArgumentValue
  COMMAND sandbox-codegen --manifest --check
)
set_tests_properties(CodegenCliRejectsOptionAsArgumentValue PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsDuplicateArguments
  COMMAND sandbox-codegen --check --check
)
set_tests_properties(CodegenCliRejectsDuplicateArguments PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliReportsMissingManifest
  COMMAND sandbox-codegen --manifest missing-codegen-manifest.json
)
set_tests_properties(CodegenCliReportsMissingManifest PROPERTIES
  LABELS "unit;all;codegen"
  WILL_FAIL TRUE
)

set(SANDBOX_CODEGEN_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/manifests/manifest.json")

add_custom_target(generate-code
  COMMAND sandbox-codegen
    --manifest "${SANDBOX_CODEGEN_MANIFEST}"
    --project-root "${PROJECT_SOURCE_DIR}"
  DEPENDS sandbox-codegen
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-code
  COMMAND sandbox-codegen
    --manifest "${SANDBOX_CODEGEN_MANIFEST}"
    --project-root "${PROJECT_SOURCE_DIR}"
    --check
  DEPENDS sandbox-codegen
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)
