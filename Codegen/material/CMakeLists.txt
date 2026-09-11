add_library(material-synth-frontend
  "${PROJECT_SOURCE_DIR}/Source/SandboxEditor/material/MaterialFrontend.cpp"
  "${PROJECT_SOURCE_DIR}/Source/SandboxEditor/material/MaterialIR.cpp"
)
target_compile_features(material-synth-frontend PUBLIC cxx_std_23)
target_include_directories(material-synth-frontend PUBLIC
  "${PROJECT_SOURCE_DIR}/Source"
)
target_link_libraries(material-synth-frontend PUBLIC codegen-sexpr)

add_executable(material-synth-tests tests/material_tests.cpp)
target_link_libraries(material-synth-tests PRIVATE material-synth-frontend GTest::gtest_main)
target_compile_definitions(material-synth-tests PRIVATE
  SANDBOX_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

if(MSVC)
  target_compile_options(material-synth-frontend PRIVATE /W4 /WX)
  target_compile_options(material-synth-tests PRIVATE /W4 /WX)
else()
  target_compile_options(material-synth-frontend PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
  target_compile_options(material-synth-tests PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()

add_test(NAME MaterialSynthTests COMMAND material-synth-tests)
set_tests_properties(MaterialSynthTests PROPERTIES LABELS "unit;all;codegen")
