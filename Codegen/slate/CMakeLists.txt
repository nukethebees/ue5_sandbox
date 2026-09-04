add_library(slate-codegen-lib
  src/compiler.cpp
  src/lexer.cpp
  src/manifest.cpp
  src/parser.cpp
  src/preprocessor.cpp
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

set(SLATE_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(SLATE_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(SLATE_COMPILE_HEADERS
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Widgets.slate.generated.h"
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Host.slate.generated.h"
)
add_custom_command(
  OUTPUT ${SLATE_COMPILE_HEADERS}
  COMMAND sandbox-slatec
    --manifest "${SLATE_COMPILE_FIXTURE}/manifest.json"
    --output-root "${SLATE_COMPILE_OUTPUT}"
  DEPENDS sandbox-slatec
    "${SLATE_COMPILE_FIXTURE}/manifest.json"
    "${SLATE_COMPILE_FIXTURE}/widgets.sbxslate"
  VERBATIM
)
add_executable(slate-generated-compile-tests
  tests/compile/main.cpp
  tests/compile/other.cpp
  ${SLATE_COMPILE_HEADERS}
)
set_target_properties(slate-generated-compile-tests PROPERTIES UNITY_BUILD OFF)
target_compile_features(slate-generated-compile-tests PRIVATE cxx_std_23)
target_include_directories(slate-generated-compile-tests PRIVATE "${SLATE_COMPILE_OUTPUT}")
if(MSVC)
  target_compile_options(slate-generated-compile-tests PRIVATE /W4 /WX)
else()
  target_compile_options(slate-generated-compile-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()
add_dependencies(slate-codegen-tests slate-generated-compile-tests)
add_test(NAME SlateGeneratedCompileSmoke COMMAND slate-generated-compile-tests)
set_tests_properties(SlateGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")

add_test(NAME SlateExpandCli
  COMMAND "${CMAKE_COMMAND}"
    "-DSLATEC=$<TARGET_FILE:sandbox-slatec>"
    "-DFIXTURE=${SLATE_COMPILE_FIXTURE}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expand_cli.cmake"
)
set_tests_properties(SlateExpandCli PROPERTIES LABELS "unit;all;codegen")

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
