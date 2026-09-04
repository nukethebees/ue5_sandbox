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

set(SLATE_CODEGEN_RADAR_DIR
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Source/SbxUIExperiments/Private/Radar3D"
)
set(SLATE_CODEGEN_RADAR_MANIFEST "${SLATE_CODEGEN_RADAR_DIR}/manifest.json")
set(SLATE_CODEGEN_RADAR_INPUT "${SLATE_CODEGEN_RADAR_DIR}/Radar3DShowcase.sbxslate")
set(SLATE_CODEGEN_SCATTER_DIR
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Source/SbxUIExperiments/Private/Scatter3D"
)
set(SLATE_CODEGEN_SCATTER_MANIFEST "${SLATE_CODEGEN_SCATTER_DIR}/manifest.json")
set(SLATE_CODEGEN_SCATTER_INPUT "${SLATE_CODEGEN_SCATTER_DIR}/Scatter3DShowcase.sbxslate")
set(SLATE_CODEGEN_SMOKE_DIR
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Source/SbxUIExperiments/Private/SlateDslSmoke"
)
set(SLATE_CODEGEN_SMOKE_MANIFEST "${SLATE_CODEGEN_SMOKE_DIR}/manifest.json")
set(SLATE_CODEGEN_SMOKE_INPUT "${SLATE_CODEGEN_SMOKE_DIR}/SlateDslSmokeWidget.sbxslate")

add_custom_target(generate-slate-code
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_RADAR_MANIFEST}"
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SCATTER_MANIFEST}"
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SMOKE_MANIFEST}"
  DEPENDS
    sandbox-slatec
    "${SLATE_CODEGEN_RADAR_MANIFEST}"
    "${SLATE_CODEGEN_RADAR_INPUT}"
    "${SLATE_CODEGEN_SCATTER_MANIFEST}"
    "${SLATE_CODEGEN_SCATTER_INPUT}"
    "${SLATE_CODEGEN_SMOKE_MANIFEST}"
    "${SLATE_CODEGEN_SMOKE_INPUT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-slate-code
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_RADAR_MANIFEST}" --check
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SCATTER_MANIFEST}" --check
  COMMAND sandbox-slatec --manifest "${SLATE_CODEGEN_SMOKE_MANIFEST}" --check
  DEPENDS
    sandbox-slatec
    "${SLATE_CODEGEN_RADAR_MANIFEST}"
    "${SLATE_CODEGEN_RADAR_INPUT}"
    "${SLATE_CODEGEN_SCATTER_MANIFEST}"
    "${SLATE_CODEGEN_SCATTER_INPUT}"
    "${SLATE_CODEGEN_SMOKE_MANIFEST}"
    "${SLATE_CODEGEN_SMOKE_INPUT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)
