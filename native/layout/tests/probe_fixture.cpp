#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <lispb/strings.h>

#include <array>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

auto main(int argc, char** argv) -> int {
    ioj::layout::EditorSession editor;
    if (argc != 3) {
        return 2;
    }
    if (std::string{argv[1]} == "generate") {
        auto const document{editor.from_source(R"(
            (module probe :header "Probe.h" (record Uses
                (member value @external_sdk)
                (member pointer (type-ref @external_sdk :suffix "*"))
                (member deep (type-ref @external_sdk :suffix "**"))
                (member constant (type-ref @external_sdk :suffix " const*"))))
        )",
                                               R"(
            (type external :spelling "std::array<float, 3>")
            (type external_sdk :spelling "std::array<float, 3>" :header "array"))")};
        if (!document.valid()) {
            return 1;
        }
        auto const& graph{document.types()};
        auto const selected{graph.find_registered(graph.strings().find("external"))};
        auto const probe{ioj::layout::external_probe_request(graph, *selected)};
        auto const source{ioj::layout::profile_probe_source(probe.spellings, probe.headers)};
        if (!source) {
            std::cerr << source.error();
            return 1;
        }
        std::ofstream output{argv[2]};
        output << *source;
        return output ? 0 : 1;
    }
    std::ifstream input{argv[2]};
    std::string measured;
    auto const output{std::back_inserter(measured)};
    for (std::string line; std::getline(input, line);) {
        if (!line.starts_with("object-pointers ")) {
            std::format_to(output, "{}\n", line);
        }
    }
    auto const profile{ioj::layout::parse_abi_profile(measured)};
    if (!profile) {
        std::cerr << profile.error().message;
        return 1;
    }
    auto const facts{profile->find("std::array<float, 3>")};
    for (auto const& [spelling, size, alignment] :
         {std::tuple{"std::array<float, 3>*",
                     sizeof(std::array<float, 3>*),
                     alignof(std::array<float, 3>*)},
          std::tuple{"std::array<float, 3>**",
                     sizeof(std::array<float, 3>**),
                     alignof(std::array<float, 3>**)},
          std::tuple{"std::array<float, 3> const*",
                     sizeof(std::array<float, 3> const*),
                     alignof(std::array<float, 3> const*)}}) {
        auto const pointer{profile->find(spelling)};
        if (!pointer || pointer->size_bytes != size || pointer->alignment_bytes != alignment ||
            pointer->origin != ioj::layout::FactOrigin::compiler_probe) {
            std::cerr << "Exact pointer measurement missing without generic ABI policy: "
                      << spelling << '\n';
            return 1;
        }
    }
    return !profile->object_pointer_representation() && facts &&
                   facts->size_bytes == sizeof(std::array<float, 3>) &&
                   facts->alignment_bytes == alignof(std::array<float, 3>) &&
                   facts->origin == ioj::layout::FactOrigin::compiler_probe &&
                   profile->identity().platform == "fixture-platform" &&
                   profile->identity().architecture == "fixture-arch" &&
                   profile->identity().build_configuration == "fixture-config"
             ? 0
             : 1;
}
