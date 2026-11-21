add_executable(lispb-lsp EXCLUDE_FROM_ALL)
target_sources(lispb-lsp
  PRIVATE
    main.cpp
    analysis.cpp
    highlighting.cpp
    protocol.cpp
)
target_link_libraries(lispb-lsp
  PRIVATE
    lispb-target-compiler
    nlohmann_json::nlohmann_json
    sandbox::native_defaults
)
ioj_enable_warnings(lispb-lsp CODEGEN)

add_executable(lispb-protocol-tests EXCLUDE_FROM_ALL)
target_sources(lispb-protocol-tests
  PRIVATE
    tests/protocol_tests.cpp
    protocol.cpp
)
target_link_libraries(lispb-protocol-tests
  PRIVATE
    lispb-syntax
    nlohmann_json::nlohmann_json
    GTest::gtest_main
    sandbox::native_defaults
)
add_test(NAME LispbFileUris COMMAND lispb-protocol-tests)
set_tests_properties(LispbFileUris PROPERTIES LABELS "tool;lispb-lsp")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME LispbLanguageServer
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/session.py" "$<TARGET_FILE:lispb-lsp>"
)
set_tests_properties(LispbLanguageServer PROPERTIES LABELS "tool;lispb-lsp" TIMEOUT 90)
