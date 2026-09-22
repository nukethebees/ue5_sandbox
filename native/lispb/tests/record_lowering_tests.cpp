#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <string>

namespace codegen {
namespace {

TEST(RecordLowering, GeneratesSemanticMembersAndFixedArrays) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .types = {{"value", CppType{"Value", "Project/Value.h"}}},
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "records",
                                       .header = "Records.h",
                                       .namespace_name = "project"},
            .declarations =
                {RecordSchema{.name = "Position",
                              .members = {{.name = "x", .type = TypeRef{"float"}},
                                          {.name = "y", .type = TypeRef{"float"}}}},
                 RecordSchema{
                     .name = "Sample",
                     .members = {{.name = "position", .type = TypeRef{"Position"}},
                                 {.name = "values", .type = TypeRef{"@value"}, .count = 3}},
                     .export_specifier = "PROJECT_API"}}}},
    };

    auto const files{render_modules(lower_modules(manifest))};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("#include <array>"), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("namespace project"), std::string::npos);
    EXPECT_NE(output.find("struct Position"), std::string::npos);
    EXPECT_NE(output.find("float x;"), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API Sample"), std::string::npos);
    EXPECT_NE(output.find("Position position;"), std::string::npos);
    EXPECT_NE(output.find("std::array<Value, 3> values;"), std::string::npos);
}

TEST(RecordLowering, AllowsEmptyAndRejectsDuplicateAndZeroCountMembers) {
    auto module{NormalModuleSchema{
        .settings = ModuleSettings{.name = "records", .header = "Records.h"},
        .declarations = {RecordSchema{.name = "Record",
                                      .members = {{.name = "value", .type = TypeRef{"float"}}}}}}};
    auto manifest{Manifest{.schema_version = manifest_schema_version, .modules = {module}}};
    EXPECT_NO_THROW(static_cast<void>(lower_modules(manifest)));

    std::get<codegen::RecordSchema>(
        std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
        .members.clear();
    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 1U);
    EXPECT_NE(files.front().content.find("struct Record"), std::string::npos);

    auto& members{std::get<codegen::RecordSchema>(
                      std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
                      .members};
    members = {{.name = "value", .type = TypeRef{"float"}},
               {.name = "value", .type = TypeRef{"float"}}};
    EXPECT_THROW(static_cast<void>(lower_modules(manifest)), std::invalid_argument);

    members = {{.name = "values", .type = TypeRef{"float"}, .count = 0}};
    EXPECT_THROW(static_cast<void>(lower_modules(manifest)), std::invalid_argument);
}

TEST(RecordLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "records", .header = "Records.h"},
            .declarations = {RecordSchema{
                                 .name = "Sample",
                                 .members = {{.name = "position", .type = TypeRef{"Position"}}}},
                             RecordSchema{.name = "Position",
                                          .members = {{.name = "x", .type = TypeRef{"float"}}}}}}},
    };

    auto const files{render_modules(lower_modules(manifest))};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("struct Position"), output.find("struct Sample"));
}

} // namespace
} // namespace codegen
