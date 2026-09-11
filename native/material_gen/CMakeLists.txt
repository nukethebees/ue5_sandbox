set(MATERIAL_GEN_SOURCES
  src/MaterialFrontend.cpp
  src/MaterialIR.cpp
  src/SourceHash.cpp
)

add_library(material-gen STATIC ${MATERIAL_GEN_SOURCES})
target_compile_features(material-gen PUBLIC cxx_std_23)
target_include_directories(material-gen PUBLIC include)
target_link_libraries(material-gen PUBLIC codegen-sexpr)

add_executable(material-gen-tests tests/material_tests.cpp)
target_link_libraries(material-gen-tests PRIVATE material-gen GTest::gtest_main)
target_compile_definitions(material-gen-tests PRIVATE
  SANDBOX_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(material-gen PRIVATE sandbox::codegen_warnings)
target_link_libraries(material-gen-tests PRIVATE sandbox::codegen_warnings)

add_test(NAME MaterialGenTests COMMAND material-gen-tests)
set_tests_properties(MaterialGenTests PROPERTIES LABELS "unit;all;codegen")

if(WIN32 AND NOT SANDBOX_CODEGEN_ONLY)
  add_library(material-gen-unreal STATIC
    ${MATERIAL_GEN_SOURCES}
    "${PROJECT_SOURCE_DIR}/Codegen/sexpr/src/lexer.cpp"
    "${PROJECT_SOURCE_DIR}/Codegen/sexpr/src/reader.cpp"
  )
  target_compile_features(material-gen-unreal PUBLIC cxx_std_23)
  target_include_directories(material-gen-unreal PUBLIC
    include
    "${PROJECT_SOURCE_DIR}/Codegen/sexpr/include"
  )
  set_property(TARGET material-gen-unreal PROPERTY
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL
  )
  target_compile_definitions(material-gen-unreal PRIVATE NDEBUG _ITERATOR_DEBUG_LEVEL=0)
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(material-gen-unreal PRIVATE /U_DEBUG)
  else()
    target_compile_options(material-gen-unreal PRIVATE -U_DEBUG)
  endif()
  target_link_libraries(material-gen-unreal PRIVATE sandbox::codegen_warnings)
endif()
