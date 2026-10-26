add_library(slate-codegen-lib
  src/document.cpp
  src/compiler.cpp
  src/parser.cpp
  src/preprocessor.cpp
  src/renderer.cpp
)
target_include_directories(slate-codegen-lib PUBLIC include)
target_link_libraries(slate-codegen-lib
  PUBLIC
    sandbox::native_defaults
    lispb-output
    lispb-syntax
    codegen-lib
  PRIVATE
    ioj::codegen_pch
)
ioj_enable_warnings(slate-codegen-lib CODEGEN)

add_executable(slate-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
target_include_directories(slate-codegen-tests PRIVATE src)
target_link_libraries(slate-codegen-tests PRIVATE
  slate-codegen-lib
  lispb-syntax
  GTest::gtest_main
  sandbox::native_defaults
  ioj::test_pch
)
ioj_enable_warnings(slate-codegen-tests CODEGEN)

add_test(NAME slate-codegen-tests COMMAND slate-codegen-tests)
set_tests_properties(slate-codegen-tests PROPERTIES LABELS "unit;all;native;codegen")

set(SLATE_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(SLATE_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(SLATE_COMPILE_HEADERS
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Widgets.slate.generated.h"
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Host.slate.generated.h"
)
add_custom_command(
  OUTPUT ${SLATE_COMPILE_HEADERS}
  COMMAND "$<TARGET_FILE:lispb-executable>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-compile-fixture
  DEPENDS lispb-executable
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${SLATE_COMPILE_FIXTURE}/widgets.lispb"
  VERBATIM
)
add_executable(slate-generated-compile-tests
  tests/compile/main.cpp
  tests/compile/other.cpp
  ${SLATE_COMPILE_HEADERS}
)
set_target_properties(slate-generated-compile-tests PROPERTIES UNITY_BUILD OFF)
target_include_directories(slate-generated-compile-tests PRIVATE "${SLATE_COMPILE_OUTPUT}")
target_link_libraries(slate-generated-compile-tests PRIVATE sandbox::native_defaults)
ioj_enable_warnings(slate-generated-compile-tests CODEGEN)
add_dependencies(slate-codegen-tests slate-generated-compile-tests)
add_test(NAME SlateGeneratedCompileSmoke COMMAND slate-generated-compile-tests)
set_tests_properties(SlateGeneratedCompileSmoke PROPERTIES LABELS "unit;all;native;codegen")

add_test(NAME SlateExpandCli
  COMMAND "${CMAKE_COMMAND}"
    "-DSLATEC=$<TARGET_FILE:lispb-executable>"
    "-DFIXTURE=${SLATE_COMPILE_FIXTURE}"
    "-DPROJECT_FILE=${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "-DBUILD_ROOT=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expand_cli.cmake"
)
set_tests_properties(SlateExpandCli PROPERTIES LABELS "unit;all;native;codegen")

set(SLATE_CODEGEN_DOCUMENTS
  "Source/SandboxEditor/levels/S7LevelScriptEditor.lispb"
  "Source/SandboxEditor/levels/S7LevelAuthoringModeToolkit.lispb"
  "Plugins/SandboxUI/Source/SandboxUI/Private/widgets/CoreWidgets.lispb"
  "Plugins/SandboxExperiments/Source/SandboxUIExamples/Private/SandboxUIShowcase.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/Benchmarks/Heatmap/HeatmapBenchmarkSlate.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/Radar3D/Radar3DShowcase.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/HeatmapRDG/HeatmapRDGShowcase.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/Scatter3D/Scatter3DShowcase.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/SlateDslSmoke/SlateDslSmokeWidget.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/VolumeHeatmap3D/VolumeHeatmap3DShowcase.lispb"
  "Plugins/SandboxExperiments/Source/SbxUIExperiments/Private/Widgets/ExperimentPanels.lispb"
  "Plugins/SpaceGame/Source/SpaceGame/Private/ui/main_menu/SGameOptionsView.lispb"
  "Plugins/SpaceGame/Source/SpaceGameS7/Private/SScriptLevelSelectView.lispb"
)

set(SLATE_CODEGEN_DEPENDENCIES)
foreach(document IN LISTS SLATE_CODEGEN_DOCUMENTS)
  list(APPEND SLATE_CODEGEN_DEPENDENCIES
    "${PROJECT_SOURCE_DIR}/${document}"
  )
endforeach()

add_custom_target(generate-slate-code
  COMMAND "$<TARGET_FILE:lispb-executable>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-production
  DEPENDS
    lispb-executable
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-slate-code
  COMMAND "$<TARGET_FILE:lispb-executable>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-production
  DEPENDS
    lispb-executable
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)
