#include <codegen/sexpr/reader.h>

#include <codegen/sexpr/lexer.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/syntax_parser.h>

namespace lispb::codegen::sexpr {

auto read_forms(Syntax& syntax, SourceFileId const file) -> HandleRange {
    if (auto const cached{syntax.cached_forms(file)}; cached.has_value()) {
        return *cached;
    }
    auto const tokens{lex(syntax, file)};
    auto const forms{SyntaxParser{syntax, tokens}.parse()};
    syntax.cache_forms(file, forms);
    return forms;
}

}
