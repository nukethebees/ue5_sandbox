#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <string>

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        return 2;
    }
    if (std::string{argv[1]} == "generate") {
        auto const source{ioj::layout::profile_probe_source(
            std::array{std::string{"std::array<float, 3>"}}, std::array{std::string{"array"}})};
        if (!source) {
            std::cerr << source.error();
            return 1;
        }
        std::ofstream output{argv[2]};
        output << *source;
        return output ? 0 : 1;
    }
    auto const profile{ioj::layout::load_abi_profile(argv[2])};
    if (!profile) {
        std::cerr << profile.error().message;
        return 1;
    }
    auto const facts{profile->find("std::array<float, 3>")};
    return facts && facts->size_bytes == sizeof(std::array<float, 3>) &&
                   facts->alignment_bytes == alignof(std::array<float, 3>) &&
                   facts->origin == ioj::layout::FactOrigin::compiler_probe &&
                   profile->identity().platform == "fixture-platform" &&
                   profile->identity().build_configuration == "fixture-config"
             ? 0
             : 1;
}
