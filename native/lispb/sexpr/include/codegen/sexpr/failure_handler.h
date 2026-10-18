#pragma once

#include <string>

namespace lispb::codegen::sexpr {
class Sources;
struct SourceSpan;

using FailureHandler = void (*)(Sources const&, SourceSpan const&, std::string const&);

}
