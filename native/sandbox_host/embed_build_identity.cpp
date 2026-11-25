#include "build_identity.h"

#include <fstream>
#include <iostream>

auto wmain(int argc, wchar_t** argv) -> int {
    if (argc != 3) {
        return 1;
    }
    auto const identity{ioj::sandbox_host::read_build_identity(argv[1])};
    if (!identity) {
        std::cerr << identity.error() << '\n';
        return 1;
    }

    std::ofstream output{std::filesystem::path{argv[2]}};
    output << "#pragma once\n#define SANDBOX_GAME_BUILD_ID \"" << *identity << "\"\n";
    output.close();
    return output ? 0 : 1;
}
