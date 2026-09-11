#pragma once

#include <string>

namespace codegen {

struct StaticAssert {
    std::string condition;
    std::string message;
};

} // namespace codegen
