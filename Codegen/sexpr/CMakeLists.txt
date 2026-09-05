add_library(codegen-sexpr
  src/lexer.cpp
)
target_compile_features(codegen-sexpr PUBLIC cxx_std_23)
target_include_directories(codegen-sexpr PUBLIC include)

if(MSVC)
  target_compile_options(codegen-sexpr PRIVATE /W4 /WX)
else()
  target_compile_options(codegen-sexpr PRIVATE ${SANDBOX_CODEGEN_WARNINGS})
endif()
