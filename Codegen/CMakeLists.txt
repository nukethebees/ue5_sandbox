find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(codegen-lib
  src/ast.cpp
  src/facade_lowering.cpp
  src/fixed_soa_lowering.cpp
  src/generator.cpp
  src/homogeneous_lowering.cpp
  src/json.cpp
  src/lowering_utils.cpp
  src/schema.cpp
  src/soa_lowering.cpp
  src/validation.cpp
  src/vector_lowering.cpp
)
target_compile_features(codegen-lib PUBLIC cxx_std_23)
target_include_directories(codegen-lib PUBLIC include)
target_link_libraries(codegen-lib PUBLIC nlohmann_json::nlohmann_json)

add_executable(sandbox-codegen src/main.cpp)
set_target_properties(sandbox-codegen PROPERTIES OUTPUT_NAME codegen)
target_link_libraries(sandbox-codegen PRIVATE codegen-lib)

add_executable(codegen-tests
  tests/ast_tests.cpp
  tests/generator_tests.cpp
  tests/json_tests.cpp
  tests/lowering_tests.cpp
  tests/validation_tests.cpp
)
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
endif()

include(GoogleTest)
gtest_discover_tests(codegen-tests PROPERTIES LABELS "unit;all;codegen")

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
