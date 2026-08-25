#pragma once

#include <stdexcept>

namespace codegen {

class ManifestError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace codegen
