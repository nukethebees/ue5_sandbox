#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <codegen/ast/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/type_graph.h>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        return 2;
    }
    if (std::string{argv[1]} == "generate") {
        lispb::CompilerStorage storage;
        lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                          lispb::codegen::TypeRegistry{storage.registered_types},
                                          storage.declarations,
                                          {}};
        manifest.schema_version = lispb::codegen::manifest_schema_version;
        lispb::codegen::CppTypeId registered{};
        registered = storage.cpp_types.intern("std::array<float, 3>");
        manifest.types.add("external", registered);
        registered = storage.cpp_types.intern(
            "std::array<float, 3>",
            {storage.cpp_types.dependency("std::array<float, 3>", "array")});
        manifest.types.add("external_sdk", registered);
        std::vector<lispb::codegen::RecordMemberId> members;
        for (auto const suffix : {"", "*", "**", " const*"}) {
            auto const reference{[&] {
                auto const new_type_refs{storage.declarations.type_refs.append()};
                storage.declarations.type_refs
                    .name[storage.declarations.type_refs.index(new_type_refs)] =
                    storage.strings.intern("@external_sdk");
                storage.declarations.type_refs
                    .suffix[storage.declarations.type_refs.index(new_type_refs)] =
                    storage.strings.add(suffix);
                return new_type_refs;
            }()};
            members.push_back([&] {
                auto const new_record_members{storage.declarations.record_members.append()};
                storage.declarations.record_members
                    .name[storage.declarations.record_members.index(new_record_members)] =
                    storage.strings.intern("value" + std::to_string(members.size()));
                storage.declarations.record_members
                    .type[storage.declarations.record_members.index(new_record_members)] =
                    reference;
                return new_record_members;
            }());
        }
        auto const record{[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Uses");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                storage.declarations.range(members);
            return new_records;
        }()};
        auto const settings{[&] {
            auto const new_module_settings_store{
                storage.declarations.module_settings_store.append()};
            storage.declarations.module_settings_store
                .name[storage.declarations.module_settings_store.index(new_module_settings_store)] =
                storage.strings.intern("probe");
            storage.declarations.module_settings_store
                .header[storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = storage.strings.add("probe.h");
            return new_module_settings_store;
        }()};
        manifest.modules.push_back([&] {
            auto const new_normal_modules{storage.declarations.normal_modules.append()};
            storage.declarations.normal_modules
                .settings[storage.declarations.normal_modules.index(new_normal_modules)] = settings;
            storage.declarations.normal_modules
                .declarations[storage.declarations.normal_modules.index(new_normal_modules)] =
                storage.declarations.range<lispb::codegen::DeclarationHandle>({record});
            return new_normal_modules;
        }());
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const selected{graph.find_registered(storage.strings.find("external"))};
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
    lispb::CompilerStorage storage;
    lispb::schema::TypeGraph const types{storage.declarations, storage.cpp_types};
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
