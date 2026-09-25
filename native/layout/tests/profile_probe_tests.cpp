#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <codegen/schema/schema_version.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace ioj::layout {
namespace {

TEST(ProfileProbe, CollectsExactExternalUsesWithoutLosingCvOrUnsupportedDiagnostics) {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::RegisteredTypeSchema registered{};
    registered.cpp_type = codegen::CppType{"sdk::ExternalValue"};
    manifest.types.emplace("external", registered);
    manifest.types.emplace("external_alias", registered);
    codegen::NormalModuleSchema module{};
    module.settings.name = "uses";
    module.settings.header = "uses.h";
    codegen::RecordSchema record{};
    record.name = "Uses";
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
        codegen::RecordMemberSchema member{};
        member.name = "value" + std::to_string(record.members.size());
        member.type = codegen::TypeRef{"@external", suffix, std::nullopt};
        record.members.push_back(member);
    }
    codegen::RecordMemberSchema unrelated{};
    unrelated.name = "unrelated";
    unrelated.type = codegen::TypeRef{"Other", "*", std::nullopt};
    record.members.push_back(unrelated);
    codegen::RecordMemberSchema alias{};
    alias.name = "alias";
    alias.type = codegen::TypeRef{"@external_alias", "***", std::nullopt};
    record.members.push_back(alias);
    module.declarations.emplace_back(record);
    manifest.modules.emplace_back(module);
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const selected{*types.find_registered("external")};
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

TEST(ProfileProbe, UnionsHeadersAcrossAlternateRegistrationsAndNestedDependencies) {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::RegisteredTypeSchema registered{};
    registered.cpp_type = codegen::CppType{"sdk::ExternalValue"};
    manifest.types.emplace("external_a", registered);
    registered.cpp_type.dependencies = {
        {"sdk::ExternalValue",
         "sdk/external_value.hpp",
         {{"support", std::nullopt, {{"config", "sdk/config.hpp", {}}}},
          {"duplicate", "sdk/external_value.hpp", {}}}}};
    manifest.types.emplace("external_b", registered);
    registered.cpp_type.dependencies = {{"extra", "sdk/Extra Detail.hpp", {}}};
    manifest.types.emplace("external_unused", registered);
    registered.cpp_type = codegen::CppType{"sdk::Unrelated", "unrelated.hpp"};
    manifest.types.emplace("unrelated", registered);
    codegen::NormalModuleSchema module{};
    module.settings.name = "probe";
    module.settings.header = "probe.h";
    codegen::RecordSchema record{};
    record.name = "Uses";
    codegen::RecordMemberSchema member{};
    member.name = "pointer";
    member.type = codegen::TypeRef{"@external_b", "*", std::nullopt};
    record.members.push_back(member);
    module.declarations.push_back(record);
    manifest.modules.emplace_back(module);
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const selected{*types.find_registered("external_a")};
    EXPECT_TRUE(std::get<lispb::schema::ExternalType>(types.type(selected).definition)
                    .cpp_type.dependencies.empty());
    ASSERT_EQ(types.type_uses().size(), 1);
    EXPECT_FALSE(types.type_uses().front().target.cpp_type.dependencies.empty());
    auto const request{external_probe_request(types, selected)};
    EXPECT_EQ(request.spellings,
              (std::vector<std::string>{"sdk::ExternalValue", "sdk::ExternalValue*"}));
    EXPECT_EQ(request.headers,
              (std::vector<std::string>{
                  "sdk/Extra Detail.hpp", "sdk/config.hpp", "sdk/external_value.hpp"}));
    EXPECT_TRUE(request.diagnostics.empty());
    for (auto const name : {"external_a", "external_b", "external_unused"}) {
        auto const repeated{external_probe_request(types, *types.find_registered(name))};
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
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema module{};
    module.settings.name = "probe";
    module.settings.header = "probe.h";
    codegen::RecordSchema record{};
    record.name = "Uses";
    codegen::RecordMemberSchema member{};
    member.name = "pointer";
    member.type = codegen::TypeRef{"std::uint16_t", "*", std::nullopt};
    record.members.push_back(member);
    module.declarations.push_back(record);
    manifest.modules.emplace_back(module);
    auto const types{lispb::schema::resolve_type_graph(manifest)};
    auto const& use{types.type_uses().front()};
    EXPECT_TRUE(std::get<lispb::schema::ExternalType>(types.type(use.target.type).definition)
                    .cpp_type.dependencies.empty());
    ASSERT_FALSE(use.target.cpp_type.dependencies.empty());
    auto const request{external_probe_request(types, use.target.type)};
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
    lispb::schema::TypeGraph const types;
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
