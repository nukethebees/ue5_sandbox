#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <string>

namespace codegen {
namespace {

TEST(UnionLowering, GeneratesRawAlternativesAndFixedArrays) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .types = {{"value", CppType{"Value", "Project/Value.h"}}},
        .modules = {NormalModuleSchema{
            .settings =
                ModuleSettings{.name = "unions", .header = "Unions.h", .namespace_name = "project"},
            .declarations = {UnionSchema{
                .name = "Payload",
                .alternatives = {{.name = "identifier", .type = TypeRef{"std::uint32_t"}},
                                 {.name = "values", .type = TypeRef{"@value"}, .count = 3}},
                .export_specifier = "PROJECT_API"}}}}};

    auto const files{render_modules(lower_modules(manifest))};

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
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "unions", .header = "Unions.h"},
            .declarations = {
                UnionSchema{.name = "Outer",
                            .alternatives = {{.name = "inner", .type = TypeRef{"Inner"}}}},
                UnionSchema{.name = "Inner",
                            .alternatives = {{.name = "value", .type = TypeRef{"float"}}}}}}}};

    auto const files{render_modules(lower_modules(manifest))};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("union Inner"), output.find("union Outer"));
}

TEST(UnionLowering, GeneratesTaggedAggregateWithExplicitPayloadUnion) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .types = {{"event_kind", CppType{"events::EventKind", "Events.h"}}},
        .modules = {NormalModuleSchema{
                        .settings = ModuleSettings{.name = "events",
                                                   .header = "Events.h",
                                                   .namespace_name = "events"},
                        .declarations = {EnumSchema{
                            .name = "EventKind",
                            .underlying_type = TypeRef{"std::uint8_t"},
                            .values = {EnumeratorSchema{"Spawn"}, EnumeratorSchema{"Damage"}}}}},
                    NormalModuleSchema{.settings = ModuleSettings{.name = "unions",
                                                                  .header = "Unions.h",
                                                                  .namespace_name = "events"},
                                       .declarations = {TaggedUnionSchema{
                                           .name = "Event",
                                           .discriminant = TypeRef{"@event_kind"},
                                           .alternatives = {{.name = "spawn",
                                                             .type = TypeRef{"std::uint32_t"},
                                                             .tag = "Spawn"},
                                                            {.name = "damage",
                                                             .type = TypeRef{"std::uint16_t"},
                                                             .count = 4,
                                                             .tag = "Damage"}},
                                           .export_specifier = "PROJECT_API"}}}}};

    auto const files{render_modules(lower_modules(manifest))};

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
} // namespace codegen
