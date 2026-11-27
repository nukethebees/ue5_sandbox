#include "ReadableRecords.h"

auto unmapped_key(codegen_compile_fixture::KeyedBits bits) -> bool {
    return bits.get<codegen_compile_fixture::groups::Action::Timer>();
}
