#include <codegen/schema/schema_version.h>
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        return 2;
    }
    if (std::string{argv[1]} == "generate") {
        codegen::Manifest manifest{};
        manifest.schema_version = codegen::manifest_schema_version;
        codegen::RegisteredTypeSchema registered{};
        registered.cpp_type = codegen::CppType{"std::array<float, 3>"};
        manifest.types.emplace("external", registered);
        codegen::NormalModuleSchema module{};
        module.settings.name = "probe";
        module.settings.header = "probe.h";
        codegen::RecordSchema record{};
        record.name = "Uses";
        for (auto const suffix : {"", "*", "**", " const*"}) {
            codegen::RecordMemberSchema member{};
            member.name = "value" + std::to_string(record.members.size());
            member.type = codegen::TypeRef{"@external", suffix, std::nullopt};
            record.members.push_back(member);
        }
        module.declarations.push_back(record);
        manifest.modules.emplace_back(module);
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const selected{graph.find_registered("external")};
        auto const probe{ioj::layout::external_probe_types(graph, *selected)};
        auto const source{
            ioj::layout::profile_probe_source(probe.spellings, std::array{std::string{"array"}})};
        if (!source) {
            std::cerr << source.error();
            return 1;
        }
        std::ofstream output{argv[2]};
        output << *source;
        return output ? 0 : 1;
    }
    std::ifstream input{argv[2]};
    std::ostringstream measured;
    for (std::string line; std::getline(input, line);) {
        if (!line.starts_with("object-pointers ")) {
            measured << line << '\n';
        }
    }
    auto const profile{ioj::layout::parse_abi_profile(measured.str())};
    if (!profile) {
        std::cerr << profile.error().message;
        return 1;
    }
    auto const facts{profile->find("std::array<float, 3>")};
    lispb::schema::TypeGraph const types;
    ioj::layout::PhysicalFactsResolver resolver{types, *profile};
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
        auto const pointer{resolver.resolve_spelling(spelling)};
        if (!pointer.facts || pointer.facts->size_bytes != size ||
            pointer.facts->alignment_bytes != alignment ||
            pointer.facts->origin != ioj::layout::FactOrigin::compiler_probe) {
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
