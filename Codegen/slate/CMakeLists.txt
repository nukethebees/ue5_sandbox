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

add_executable(slate-codegen-tests tests/compiler_tests.cpp)
target_include_directories(slate-codegen-tests PRIVATE src)
target_link_libraries(slate-codegen-tests PRIVATE
  slate-codegen-lib
  GTest::gtest_main
)

if(MSVC)
  target_compile_options(slate-codegen-lib PRIVATE /W4 /WX)
  target_compile_options(sandbox-slatec PRIVATE /W4 /WX)
  target_compile_options(slate-codegen-tests PRIVATE /W4 /WX)
else()
  target_compile_options(slate-codegen-lib PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(sandbox-slatec PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(slate-codegen-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()

gtest_discover_tests(slate-codegen-tests PROPERTIES LABELS "unit;all;codegen")

set(SLATE_CODEGEN_SOURCE_ROOT
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Source/SbxUIExperiments/Private"
)
set(SLATE_CODEGEN_DOCUMENTS
  "Radar3D/Radar3DShowcase.sbxslate"
  "HeatmapRDG/HeatmapRDGShowcase.sbxslate"
  "Scatter3D/Scatter3DShowcase.sbxslate"
  "SlateDslSmoke/SlateDslSmokeWidget.sbxslate"
  "VolumeHeatmap3D/VolumeHeatmap3DShowcase.sbxslate"
  "Widgets/ExperimentPanels.sbxslate"
)

set(SLATE_CODEGEN_GENERATE_COMMANDS)
set(SLATE_CODEGEN_CHECK_COMMANDS)
set(SLATE_CODEGEN_DEPENDENCIES)
foreach(document IN LISTS SLATE_CODEGEN_DOCUMENTS)
  get_filename_component(document_directory "${document}" DIRECTORY)
  set(manifest "${SLATE_CODEGEN_SOURCE_ROOT}/${document_directory}/manifest.json")
  list(APPEND SLATE_CODEGEN_GENERATE_COMMANDS
    COMMAND sandbox-slatec --manifest "${manifest}"
  )
  list(APPEND SLATE_CODEGEN_CHECK_COMMANDS
    COMMAND sandbox-slatec --manifest "${manifest}" --check
  )
  list(APPEND SLATE_CODEGEN_DEPENDENCIES
    "${manifest}"
    "${SLATE_CODEGEN_SOURCE_ROOT}/${document}"
  )
endforeach()

add_custom_target(generate-slate-code
  ${SLATE_CODEGEN_GENERATE_COMMANDS}
  DEPENDS
    sandbox-slatec
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-slate-code
  ${SLATE_CODEGEN_CHECK_COMMANDS}
  DEPENDS
    sandbox-slatec
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)
