#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <codegen/ast/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_registry.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

namespace ioj::layout {
namespace {

TEST(ProfileProbe, CollectsExactExternalUsesWithoutLosingCvOrUnsupportedDiagnostics) {
    lispb::CompilerStorage storage;
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::TypeRegistry{storage.registered_types},
                                      storage.declarations,
                                      {}};
    manifest.schema_version = lispb::codegen::manifest_schema_version;
    lispb::codegen::CppTypeId registered{};
    registered = storage.cpp_types.intern("sdk::ExternalValue");
    manifest.types.add("external", registered);
    manifest.types.add("external_alias", registered);
    std::vector<lispb::codegen::RecordMemberId> members;
    for (auto const suffix : {"",
                              "*",
                              "**",
                              " const*",
                              "* const*",
                              "*",
                              "&",
                              "&&",
                              "(*)()",
                              " Owner::*",
                              "[3]",
                              "*[]"}) {
        members.push_back([&] {
            auto const new_record_members{storage.declarations.record_members.append()};
            storage.declarations.record_members
                .name[storage.declarations.record_members.index(new_record_members)] =
                storage.strings.intern("value" + std::to_string(members.size()));
            storage.declarations.record_members
                .type[storage.declarations.record_members.index(new_record_members)] =
                fixture_reference(storage, "@external", suffix);
            return new_record_members;
        }());
    }
    members.push_back([&] {
        auto const new_record_members{storage.declarations.record_members.append()};
        storage.declarations.record_members
            .name[storage.declarations.record_members.index(new_record_members)] =
            storage.strings.intern("unrelated");
        storage.declarations.record_members
            .type[storage.declarations.record_members.index(new_record_members)] =
            fixture_reference(storage, "Other", "*");
        return new_record_members;
    }());
    members.push_back([&] {
        auto const new_record_members{storage.declarations.record_members.append()};
        storage.declarations.record_members
            .name[storage.declarations.record_members.index(new_record_members)] =
            storage.strings.intern("alias");
        storage.declarations.record_members
            .type[storage.declarations.record_members.index(new_record_members)] =
            fixture_reference(storage, "@external_alias", "***");
        return new_record_members;
    }());
    manifest.modules.push_back(fixture_module(
        storage, "uses", {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Uses");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                storage.declarations.range(members);
            return new_records;
        }()}));
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const selected{*types.find_registered(storage.strings.find("external"))};
    auto const probe{external_probe_request(types, selected)};
    EXPECT_EQ(probe.spellings,
              (std::vector<std::string>{"sdk::ExternalValue",
                                        "sdk::ExternalValue const*",
                                        "sdk::ExternalValue*",
                                        "sdk::ExternalValue* const*",
                                        "sdk::ExternalValue**",
                                        "sdk::ExternalValue***"}));
    EXPECT_EQ(probe.diagnostics.size(), 6);
    for (auto const& diagnostic : probe.diagnostics) {
        EXPECT_TRUE(diagnostic.contains("unsupported complete-object spelling"));
    }
    EXPECT_TRUE(profile_probe_source(probe.spellings, {}));
    auto const inventory{external_dependencies(types)};
    auto const external{std::ranges::find_if(inventory, [&](auto const& item) {
        return std::ranges::find(item.types, selected) != item.types.end();
    })};
    ASSERT_NE(external, inventory.end());
    EXPECT_EQ(external->uses.size(), 13);
    auto const repeated{external_probe_request(types, selected)};
    EXPECT_EQ(repeated.spellings, probe.spellings);
    EXPECT_EQ(repeated.diagnostics, probe.diagnostics);
}

TEST(ProfileProbe, UnsupportedCompleteObjectSubjectsAreRejectedBeforeExport) {
    for (auto const spelling : {"External&",
                                "External&&",
                                "External(*)()",
                                "External Owner::*",
                                "External[3]",
                                "External*[]",
                                "void const",
                                "auto*"}) {
        auto const result{profile_probe_source(std::array{std::string{spelling}}, {})};
        ASSERT_FALSE(result) << spelling;
        EXPECT_TRUE(result.error().contains(spelling));
    }
}

TEST(ProfileProbe, UnionsHeadersAcrossAlternateRegistrationsAndSharedDependencies) {
    lispb::CompilerStorage storage;
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::TypeRegistry{storage.registered_types},
                                      storage.declarations,
                                      {}};
    manifest.schema_version = lispb::codegen::manifest_schema_version;
    lispb::codegen::CppTypeId registered{};
    registered = storage.cpp_types.intern("sdk::ExternalValue");
    manifest.types.add("external_a", registered);
    registered = storage.cpp_types.intern(
        "sdk::ExternalValue",
        {storage.cpp_types.dependency("sdk::ExternalValue", "sdk/external_value.hpp"),
         storage.cpp_types.dependency("config", "sdk/config.hpp"),
         storage.cpp_types.dependency("duplicate", "sdk/external_value.hpp")});
    manifest.types.add("external_b", registered);
    registered = storage.cpp_types.intern(
        "sdk::ExternalValue", {storage.cpp_types.dependency("extra", "sdk/Extra Detail.hpp")});
    manifest.types.add("external_unused", registered);
    registered = storage.cpp_types.intern(
        "sdk::Unrelated", {storage.cpp_types.dependency("sdk::Unrelated", "unrelated.hpp")});
    manifest.types.add("unrelated", registered);
    auto const member{[&] {
        auto const new_record_members{storage.declarations.record_members.append()};
        storage.declarations.record_members
            .name[storage.declarations.record_members.index(new_record_members)] =
            storage.strings.intern("pointer");
        storage.declarations.record_members
            .type[storage.declarations.record_members.index(new_record_members)] =
            fixture_reference(storage, "@external_b", "*");
        return new_record_members;
    }()};
    manifest.modules.push_back(fixture_module(
        storage, "probe", {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Uses");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                storage.declarations.range<lispb::codegen::RecordMemberId>({member});
            return new_records;
        }()}));
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const selected{*types.find_registered(storage.strings.find("external_a"))};
    EXPECT_TRUE(storage.cpp_types
                    .dependencies(
                        types.definitions().externals.cpp_type[types.definitions().externals.index(
                            types.definition(selected))])
                    .empty());
    ASSERT_EQ(types.type_uses().size(), 1);
    EXPECT_FALSE(storage.cpp_types
                     .dependencies(types.occurrences().cpp_type[types.occurrences().index(
                         types.target(types.type_uses().front()))])
                     .empty());
    auto const request{external_probe_request(types, selected)};
    EXPECT_EQ(request.spellings,
              (std::vector<std::string>{"sdk::ExternalValue", "sdk::ExternalValue*"}));
    EXPECT_EQ(request.headers,
              (std::vector<std::string>{
                  "sdk/Extra Detail.hpp", "sdk/config.hpp", "sdk/external_value.hpp"}));
    EXPECT_TRUE(request.diagnostics.empty());
    for (auto const name : {"external_a", "external_b", "external_unused"}) {
        auto const repeated{
            external_probe_request(types, *types.find_registered(storage.strings.find(name)))};
        EXPECT_EQ(repeated.spellings, request.spellings);
        EXPECT_EQ(repeated.headers, request.headers);
        EXPECT_EQ(repeated.diagnostics, request.diagnostics);
    }
    auto headers{request.headers};
    headers.push_back("sdk/external_value.hpp"); // A repeated manually supplied header.
    auto const source{profile_probe_source(request.spellings, headers)};
    ASSERT_TRUE(source);
    for (auto const& header : request.headers) {
        auto const include{"#include \"" + header + "\""};
        auto const first{source->find(include)};
        ASSERT_NE(first, std::string::npos);
        EXPECT_EQ(source->find(include, first + include.size()), std::string::npos);
    }
}

TEST(ProfileProbe, IncludesHeadersIntroducedByResolvedUses) {
    lispb::CompilerStorage storage;
    lispb::codegen::Manifest manifest{lispb::codegen::manifest_schema_version,
                                      lispb::codegen::TypeRegistry{storage.registered_types},
                                      storage.declarations,
                                      {}};
    manifest.schema_version = lispb::codegen::manifest_schema_version;
    auto const member{[&] {
        auto const new_record_members{storage.declarations.record_members.append()};
        storage.declarations.record_members
            .name[storage.declarations.record_members.index(new_record_members)] =
            storage.strings.intern("pointer");
        storage.declarations.record_members
            .type[storage.declarations.record_members.index(new_record_members)] =
            fixture_reference(storage, "std::uint16_t", "*");
        return new_record_members;
    }()};
    manifest.modules.push_back(fixture_module(
        storage, "probe", {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Uses");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                storage.declarations.range<lispb::codegen::RecordMemberId>({member});
            return new_records;
        }()}));
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const& use{types.type_uses().front()};
    EXPECT_TRUE(
        storage.cpp_types
            .dependencies(
                types.definitions()
                    .externals.cpp_type[types.definitions().externals.index(types.definition(
                        types.occurrences().type[types.occurrences().index(types.target(use))]))])
            .empty());
    ASSERT_FALSE(storage.cpp_types
                     .dependencies(
                         types.occurrences().cpp_type[types.occurrences().index(types.target(use))])
                     .empty());
    auto const request{external_probe_request(
        types, types.occurrences().type[types.occurrences().index(types.target(use))])};
    EXPECT_EQ(request.headers, (std::vector<std::string>{"cstdint"}));
    EXPECT_EQ(request.spellings, (std::vector<std::string>{"std::uint16_t", "std::uint16_t*"}));
}

TEST(ProfileProbe, RejectsInvalidHeaderMetadataAtSourceEmission) {
    for (auto const header :
         {"", "sdk/\"bad.hpp", "<sdk/value.hpp>", "sdk/value.hpp\n#error bad"}) {
        auto const source{profile_probe_source({}, std::array{std::string{header}})};
        ASSERT_FALSE(source) << header;
        EXPECT_TRUE(source.error().contains("Probe headers must be plain include paths"));
    }
}

TEST(ProfileProbe, ExactPointerFactsNeedNoPolicyAndOverrideAnExplicitPolicy) {
    auto profile{parse_abi_profile(R"(ioj-layout-profile 2
name "Measured target"
identity architecture "non-x86-test"
type "Opaque*" 16 8 non-integer unknown "exact target measurement" compiler-probe
type "void*" 8 8 non-integer unknown "generic target representation" compiler-probe
)")};
    ASSERT_TRUE(profile);
    ASSERT_FALSE(profile->object_pointer_representation());
    lispb::CompilerStorage storage;
    lispb::schema::TypeGraph const types{storage.declarations, storage.cpp_types};
    auto const exact{PhysicalFactsResolver{types, *profile}.resolve_spelling("Opaque*")};
    ASSERT_TRUE(exact.facts);
    EXPECT_EQ(exact.facts->size_bytes, 16);
    EXPECT_EQ(exact.facts->alignment_bytes, 8);
    EXPECT_EQ(exact.facts->origin, FactOrigin::compiler_probe);
    EXPECT_EQ(exact.facts->provenance, "exact target measurement");
    EXPECT_FALSE((PhysicalFactsResolver{types, *profile}.resolve_spelling("Opaque**").facts));
    profile->set_object_pointer_representation("void*");
    auto const preferred{PhysicalFactsResolver{types, *profile}.resolve_spelling("Opaque*")};
    ASSERT_TRUE(preferred.facts);
    EXPECT_EQ(preferred.facts->size_bytes, 16);
    EXPECT_EQ(preferred.facts->origin, FactOrigin::compiler_probe);
    AbiProfile const other{"other target"};
    EXPECT_FALSE((PhysicalFactsResolver{types, other}.resolve_spelling("Opaque*").facts));
}

} // namespace
} // namespace ioj::layout
