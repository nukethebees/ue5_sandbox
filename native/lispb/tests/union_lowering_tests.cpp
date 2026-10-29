#include "compiler_test_context.h"
#include "test_utilities.h"
#include "text_expectations.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/schema_version.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <string>

namespace lispb::codegen {
using lispb::test_support::expect_contains_all;
namespace {

TEST(UnionLowering, GeneratesRawAlternativesAndFixedArrays) {
    CompilerTestContext context;
    auto& types{context.storage.cpp_types};
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module unions :header "Unions.h" :namespace project
            (union Payload :export-specifier PROJECT_API
                (member identifier std::uint32_t)
                (member values @value :count 3)
    (projections (cpp)))))",
        {{"value", types.add("Value", {types.dependency("Value", "Project/Value.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    expect_contains_all(output,
                        {"#include <array>",
                         "#include \"Project/Value.h\"",
                         "namespace project",
                         "union PROJECT_API Payload",
                         "std::uint32_t identifier;",
                         "std::array<Value, 3> values;"});
}

TEST(UnionLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module unions :header "Unions.h"
            (union Outer (member inner Inner)
    (projections (cpp)))
            (union Inner (member value float)
    (projections (cpp)))))")};

    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("union Inner"), output.find("union Outer"));
}

TEST(UnionLowering, GeneratesTaggedAggregateWithExplicitPayloadUnion) {
    CompilerTestContext context;
    auto& types{context.storage.cpp_types};
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module events :header "Events.h" :namespace events
            (enum EventKind std::uint8_t (value Spawn) (value Damage)
    (projections (cpp))))
        (module unions :header "Unions.h" :namespace events
            (union EventPayload
                (member spawn std::uint32_t :case Spawn)
                (member damage std::uint16_t :count 4 :case Damage)
                (projections (cpp)))
            (record Event :export-specifier PROJECT_API
                (member tag @event_kind)
                (member payload @EventPayload :tag tag)
                (projections (cpp)))))",
        {{"event_kind",
          types.add("events::EventKind", {types.dependency("events::EventKind", "Events.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 2U);
    auto const& output{files.back().content};
    expect_contains_all(output,
                        {"#include \"Events.h\"",
                         "struct PROJECT_API Event",
                         "EventKind tag;",
                         "union EventPayload",
                         "std::uint32_t spawn;",
                         "std::array<std::uint16_t, 4> damage;",
                         "EventPayload payload;"});
}

} // namespace
} // namespace lispb::codegen
