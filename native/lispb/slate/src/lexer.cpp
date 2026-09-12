#include "lexer.h"

#include <codegen/sexpr/lexer.h>

namespace slate_codegen::detail {

auto lex(std::string_view const path, std::string_view const source) -> std::vector<Token> {
    return codegen::sexpr::lex(path, source);
}

}
