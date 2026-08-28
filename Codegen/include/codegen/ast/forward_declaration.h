#pragma once

#include <string>

namespace codegen {

struct ForwardDeclaration {
    std::string name;
    std::string kind{"struct"};
};

} // namespace codegen
