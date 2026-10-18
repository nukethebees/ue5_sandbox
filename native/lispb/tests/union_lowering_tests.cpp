#include "compiler_test_context.h"
#include "test_utilities.h"

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
namespace {

TEST(UnionLowering, GeneratesRawAlternativesAndFixedArrays) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module unions :header "Unions.h" :namespace project
            (union Payload :export-specifier PROJECT_API
                (alternative identifier std::uint32_t)
                (alternative values @value :count 3))))",
        {{"value",
          context.storage.cpp_types.add(
              "Value", {context.storage.cpp_types.dependency("Value", "Project/Value.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("#include <array>"), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("namespace project"), std::string::npos);
    EXPECT_NE(output.find("union PROJECT_API Payload"), std::string::npos);
    EXPECT_NE(output.find("std::uint32_t identifier;"), std::string::npos);
    EXPECT_NE(output.find("std::array<Value, 3> values;"), std::string::npos);
}

TEST(UnionLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module unions :header "Unions.h"
            (union Outer (alternative inner Inner))
            (union Inner (alternative value float))))")};

    auto const files{render_test_manifest(context, manifest)};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("union Inner"), output.find("union Outer"));
}

TEST(UnionLowering, GeneratesTaggedAggregateWithExplicitPayloadUnion) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module events :header "Events.h" :namespace events
            (enum EventKind std::uint8_t (value Spawn) (value Damage)))
        (module unions :header "Unions.h" :namespace events
            (tagged-union Event :export-specifier PROJECT_API :discriminant @event_kind
                (alternative spawn std::uint32_t :tag Spawn)
                (alternative damage std::uint16_t :count 4 :tag Damage))))",
        {{"event_kind",
          context.storage.cpp_types.add(
              "events::EventKind",
              {context.storage.cpp_types.dependency("events::EventKind", "Events.h")})}})};

    auto const files{render_test_manifest(context, manifest)};

    ASSERT_EQ(files.size(), 2U);
    auto const& output{files.back().content};
    EXPECT_NE(output.find("#include \"Events.h\""), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API Event"), std::string::npos);
    EXPECT_NE(output.find("events::EventKind tag;"), std::string::npos);
    EXPECT_NE(output.find("union Payload"), std::string::npos);
    EXPECT_NE(output.find("// tag: Spawn"), std::string::npos);
    EXPECT_NE(output.find("std::uint32_t spawn;"), std::string::npos);
    EXPECT_NE(output.find("std::array<std::uint16_t, 4> damage;"), std::string::npos);
    EXPECT_NE(output.find("Payload payload;"), std::string::npos);
}

} // namespace
} // namespace lispb::codegen
