#pragma once

#include <string>
#include <vector>

namespace codegen {

struct FixedSoaSchema {
    std::string storage_name;
    std::vector<std::string> containers;
};

} // namespace codegen
