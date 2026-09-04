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

add_executable(slate-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
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

set(SLATE_CODEGEN_DOCUMENTS
  "Plugins/SandboxUI/Source/SandboxUI/Private/widgets/CoreWidgets.sbxslate"
  "Plugins/SandboxUI/Source/SandboxUIExamples/Private/SandboxUIShowcase.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Benchmarks/Heatmap/HeatmapBenchmarkSlate.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Radar3D/Radar3DShowcase.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/HeatmapRDG/HeatmapRDGShowcase.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Scatter3D/Scatter3DShowcase.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/SlateDslSmoke/SlateDslSmokeWidget.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/VolumeHeatmap3D/VolumeHeatmap3DShowcase.sbxslate"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Widgets/ExperimentPanels.sbxslate"
)

set(SLATE_CODEGEN_GENERATE_COMMANDS)
set(SLATE_CODEGEN_CHECK_COMMANDS)
set(SLATE_CODEGEN_DEPENDENCIES)
foreach(document IN LISTS SLATE_CODEGEN_DOCUMENTS)
  get_filename_component(document_directory "${document}" DIRECTORY)
  set(manifest "${PROJECT_SOURCE_DIR}/${document_directory}/manifest.json")
  list(APPEND SLATE_CODEGEN_GENERATE_COMMANDS
    COMMAND sandbox-slatec --manifest "${manifest}"
  )
  list(APPEND SLATE_CODEGEN_CHECK_COMMANDS
    COMMAND sandbox-slatec --manifest "${manifest}" --check
  )
  list(APPEND SLATE_CODEGEN_DEPENDENCIES
    "${manifest}"
    "${PROJECT_SOURCE_DIR}/${document}"
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
