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
  lispb-output
  lispb-syntax
  sandbox::codegen_warnings
)

add_executable(slate-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
target_include_directories(slate-codegen-tests PRIVATE src)
target_link_libraries(slate-codegen-tests PRIVATE
  slate-codegen-lib
  lispb-syntax
  GTest::gtest_main
  sandbox::codegen_warnings
)

gtest_discover_tests(slate-codegen-tests PROPERTIES LABELS "unit;all;codegen")

set(SLATE_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(SLATE_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(SLATE_COMPILE_HEADERS
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Widgets.slate.generated.h"
  "${SLATE_COMPILE_OUTPUT}/CompileSmoke/Host.slate.generated.h"
)
add_custom_command(
  OUTPUT ${SLATE_COMPILE_HEADERS}
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-compile-fixture
  DEPENDS lispb
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
target_compile_features(slate-generated-compile-tests PRIVATE cxx_std_23)
target_include_directories(slate-generated-compile-tests PRIVATE "${SLATE_COMPILE_OUTPUT}")
target_link_libraries(slate-generated-compile-tests PRIVATE sandbox::codegen_warnings)
add_dependencies(slate-codegen-tests slate-generated-compile-tests)
add_test(NAME SlateGeneratedCompileSmoke COMMAND slate-generated-compile-tests)
set_tests_properties(SlateGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")

add_test(NAME SlateExpandCli
  COMMAND "${CMAKE_COMMAND}"
    "-DSLATEC=$<TARGET_FILE:lispb>"
    "-DFIXTURE=${SLATE_COMPILE_FIXTURE}"
    "-DPROJECT_FILE=${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "-DBUILD_ROOT=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expand_cli.cmake"
)
set_tests_properties(SlateExpandCli PROPERTIES LABELS "unit;all;codegen")

set(SLATE_CODEGEN_DOCUMENTS
  "Plugins/SandboxUI/Source/SandboxUI/Private/widgets/CoreWidgets.lispb"
  "Plugins/SandboxUI/Source/SandboxUIExamples/Private/SandboxUIShowcase.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Benchmarks/Heatmap/HeatmapBenchmarkSlate.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Radar3D/Radar3DShowcase.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/HeatmapRDG/HeatmapRDGShowcase.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Scatter3D/Scatter3DShowcase.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/SlateDslSmoke/SlateDslSmokeWidget.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/VolumeHeatmap3D/VolumeHeatmap3DShowcase.lispb"
  "Plugins/SandboxUI/Source/SbxUIExperiments/Private/Widgets/ExperimentPanels.lispb"
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
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-production
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-slate-code
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target slate-production
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    ${SLATE_CODEGEN_DEPENDENCIES}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking experimental Slate DSL outputs"
  USES_TERMINAL
  VERBATIM
)
