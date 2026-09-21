#include <ioj/layout/abi_profile.hpp>

#include <iostream>

auto main() -> int {
    std::cout << ioj::layout::serialize_abi_profile(ioj::layout::AbiProfile::host_common());
    return std::cout ? 0 : 1;
}
