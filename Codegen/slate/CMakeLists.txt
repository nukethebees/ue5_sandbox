add_library(slate-codegen-lib
  src/compiler.cpp
  src/lexer.cpp
  src/manifest.cpp
  src/parser.cpp
  src/renderer.cpp
)
target_compile_features(slate-codegen-lib PUBLIC cxx_std_23)
target_include_directories(slate-codegen-lib PUBLIC include)
target_link_libraries(slate-codegen-lib PRIVATE
  codegen-lib
  nlohmann_json::nlohmann_json
)

add_executable(sandbox-slatec src/main.cpp)
set_target_properties(sandbox-slatec PROPERTIES OUTPUT_NAME slatec)
target_link_libraries(sandbox-slatec PRIVATE slate-codegen-lib)

if(MSVC)
  target_compile_options(slate-codegen-lib PRIVATE /W4 /WX)
  target_compile_options(sandbox-slatec PRIVATE /W4 /WX)
else()
  target_compile_options(slate-codegen-lib PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(sandbox-slatec PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()

set(SLATE_CODEGEN_SAMPLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/samples")
set(SLATE_CODEGEN_SAMPLE_MANIFEST "${SLATE_CODEGEN_SAMPLE_DIR}/manifest.json")
set(SLATE_CODEGEN_SAMPLE_INPUT "${SLATE_CODEGEN_SAMPLE_DIR}/Radar3DShowcase.sbxslate")

add_custom_target(generate-slate-code
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SAMPLE_MANIFEST}"
  DEPENDS
    sandbox-slatec
    "${SLATE_CODEGEN_SAMPLE_MANIFEST}"
    "${SLATE_CODEGEN_SAMPLE_INPUT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating the experimental Slate DSL sample"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-slate-code
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SAMPLE_MANIFEST}" --check
  DEPENDS
    sandbox-slatec
    "${SLATE_CODEGEN_SAMPLE_MANIFEST}"
    "${SLATE_CODEGEN_SAMPLE_INPUT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking the experimental Slate DSL sample"
  USES_TERMINAL
  VERBATIM
)
