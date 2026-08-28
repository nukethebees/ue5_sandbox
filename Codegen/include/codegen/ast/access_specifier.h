#pragma once

#include <string>

namespace codegen {

struct AccessSpecifier {
    enum class Indentation {
        outdented,
        normal,
    };

    std::string access;
    Indentation indentation{Indentation::outdented};
};

} // namespace codegen
