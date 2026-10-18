#include "document_test_support.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/external_dependency.hpp>
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/profile_probe.hpp>

#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <vector>

namespace ioj::layout {
namespace profile_probe_tests {

TEST(ProfileProbe, CollectsExactExternalUsesAndUnsupportedDiagnostics) {
    EditorSession editor;
    auto const document{editor.from_source(R"lisp(
        (module uses :header "Uses.h" (record Uses
            (member value @external)
            (member pointer (type-ref @external :suffix "*"))
            (member repeated (type-ref @external :suffix "*"))
            (member deep (type-ref @external :suffix "**"))
            (member constant (type-ref @external :suffix " const*"))
            (member inner_constant (type-ref @external :suffix "* const*"))
            (member reference (type-ref @external :suffix "&"))
            (member rvalue (type-ref @external :suffix "&&"))
            (member function (type-ref @external :suffix "(*)()"))
            (member member_pointer (type-ref @external :suffix " Owner::*"))
            (member array (type-ref @external :suffix "[3]"))
            (member unsized (type-ref @external :suffix "*[]"))
            (member unrelated (type-ref Other :suffix "*"))
            (member alias (type-ref @external_alias :suffix "***"))))
    )lisp",
                                           R"(
        (type external :spelling "sdk::ExternalValue")
        (type external_alias :spelling "sdk::ExternalValue"))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const& types{document.types()};
    auto const selected{*types.find_registered(types.strings().find("external"))};
    auto const probe{external_probe_request(types, selected)};
    EXPECT_EQ(probe.spellings,
              (std::vector<std::string>{"sdk::ExternalValue",
                                        "sdk::ExternalValue const*",
                                        "sdk::ExternalValue*",
                                        "sdk::ExternalValue* const*",
                                        "sdk::ExternalValue**",
                                        "sdk::ExternalValue***"}));
    ASSERT_EQ(probe.diagnostics.size(), 6);
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

class UnsupportedProbeTest : public testing::TestWithParam<char const*> {};
TEST_P(UnsupportedProbeTest, RejectsBeforeExport) {
    auto const result{profile_probe_source(std::array{std::string{GetParam()}}, {})};
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().contains(GetParam()));
}
INSTANTIATE_TEST_SUITE_P(CompleteObjectSpellings,
                         UnsupportedProbeTest,
                         testing::Values("External&",
                                         "External&&",
                                         "External(*)()",
                                         "External Owner::*",
                                         "External[3]",
                                         "External*[]",
                                         "void const",
                                         "auto*"));

TEST(ProfileProbe, UnionsHeadersAcrossAliasesAndDeduplicatesIncludes) {
    EditorSession editor;
    auto const document{editor.from_source(R"(
        (module probe :header "Probe.h" (record Uses
            (member pointer (type-ref @external_b :suffix "*"))))
    )",
                                           R"(
        (type external_a :spelling "sdk::ExternalValue")
        (type external_b :spelling "sdk::ExternalValue" :header "sdk/external_value.hpp")
        (type external_unused :spelling "sdk::ExternalValue" :header "sdk/Extra Detail.hpp")
        (type external_config :spelling "sdk::ExternalValue" :header "sdk/config.hpp")
        (type unrelated :spelling "sdk::Unrelated" :header "unrelated.hpp"))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const& types{document.types()};
    auto const selected{*types.find_registered(types.strings().find("external_a"))};
    auto const request{external_probe_request(types, selected)};
    EXPECT_EQ(request.spellings,
              (std::vector<std::string>{"sdk::ExternalValue", "sdk::ExternalValue*"}));
    EXPECT_EQ(request.headers,
              (std::vector<std::string>{
                  "sdk/Extra Detail.hpp", "sdk/config.hpp", "sdk/external_value.hpp"}));
    EXPECT_TRUE(request.diagnostics.empty());
    for (auto const name : {"external_a", "external_b", "external_unused"}) {
        auto const repeated{
            external_probe_request(types, *types.find_registered(types.strings().find(name)))};
        EXPECT_EQ(repeated.spellings, request.spellings);
        EXPECT_EQ(repeated.headers, request.headers);
    }
    auto headers{request.headers};
    headers.push_back("sdk/external_value.hpp");
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
    EditorSession editor;
    auto const document{editor.from_source(R"(
        (module probe :header "Probe.h"
            (record Uses (member pointer (type-ref std::uint16_t :suffix "*")))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const inventory{external_dependencies(document.types())};
    ASSERT_EQ(inventory.size(), 1);
    auto const request{external_probe_request(document.types(), inventory.front().types.front())};
    EXPECT_EQ(request.headers, (std::vector<std::string>{"cstdint"}));
    EXPECT_EQ(request.spellings, (std::vector<std::string>{"std::uint16_t", "std::uint16_t*"}));
}

class InvalidProbeHeaderTest : public testing::TestWithParam<char const*> {};
TEST_P(InvalidProbeHeaderTest, RejectsAtSourceEmission) {
    auto const source{profile_probe_source({}, std::array{std::string{GetParam()}})};
    ASSERT_FALSE(source);
    EXPECT_TRUE(source.error().contains("Probe headers must be plain include paths"));
}
INSTANTIATE_TEST_SUITE_P(
    Headers,
    InvalidProbeHeaderTest,
    testing::Values("", "sdk/\"bad.hpp", "<sdk/value.hpp>", "sdk/value.hpp\n#error bad"));

TEST(ProfileProbe, ExactPointerFactsOverrideGenericPolicy) {
    EditorSession editor;
    auto profile{parse_abi_profile(R"(ioj-layout-profile 2
name "Measured target"
identity architecture "non-x86-test"
type "Opaque*" 16 8 non-integer unknown "exact target measurement" compiler-probe
type "void*" 8 8 non-integer unknown "generic target representation" compiler-probe
)")};
    ASSERT_TRUE(profile);
    ASSERT_FALSE(profile->object_pointer_representation());
    auto const document{editor.from_source("")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const& types{document.types()};
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

}
}
