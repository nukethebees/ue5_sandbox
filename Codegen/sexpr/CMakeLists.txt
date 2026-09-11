add_library(codegen-sexpr
  src/lexer.cpp
)
target_compile_features(codegen-sexpr PUBLIC cxx_std_23)
target_include_directories(codegen-sexpr PUBLIC include)
target_link_libraries(codegen-sexpr PRIVATE sandbox::codegen_warnings)
