#include "ReadableRecords.h"

#include <array>

namespace codegen_compile_fixture {
void rejected_array(KeyedBits& bits) {
    bits.set<groups::Sparse::Positive>(std::array{true});
}
}
