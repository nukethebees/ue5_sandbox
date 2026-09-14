#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace codegen {
namespace {

auto valid_module() -> PackedValueModuleSchema {
    return PackedValueModuleSchema{
        .settings = ModuleSettings{.name = "packed", .header = "Packed.h"},
        .values = {PackedValueSchema{
            .name = "FighterState",
            .storage_type = TypeRef{"std::uint32_t"},
            .fields =
                {
                    PackedFieldSchema{"entity_index", TypeRef{"std::uint32_t"}, 24},
                    PackedFieldSchema{
                        "state", TypeRef{"FighterStateKind"}, 8, PackedFieldKind::enumeration},
                },
        }},
    };
}

auto lower(PackedValueModuleSchema module) -> std::string {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

TEST(PackedValue, LowersTypedFieldsAndThreeWayComparison) {
    auto const header{lower(valid_module())};

    EXPECT_NE(header.find("using storage_type = std::uint32_t;"), std::string::npos);
    EXPECT_NE(header.find("entity_index_offset{0}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_value_mask{storage_type{0xffffff}}"), std::string::npos);
    EXPECT_NE(header.find("state_offset{24}"), std::string::npos);
    EXPECT_NE(header.find("state_mask{storage_type{0xff000000}}"), std::string::npos);
    EXPECT_NE(header.find("operator<=>(FighterState const&) const noexcept = default"),
              std::string::npos);
    EXPECT_NE(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_NE(header.find("std::underlying_type_t<FighterStateKind>"), std::string::npos);
    EXPECT_NE(header.find("std::is_standard_layout_v<FighterState>"), std::string::npos);
}

TEST(PackedValue, RejectsInvalidLayoutsAndTypes) {
    auto module{valid_module()};
    module.values.front().fields.front().bits = 0;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().fields.front().bits = 25;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().storage_type = TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().fields.front().type = TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().fields.front() = PackedFieldSchema{"flag", TypeRef{"bool"}, 2};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.settings.source = "Packed.cpp";
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, RejectsGeneratedApiCollisions) {
    auto module{valid_module()};
    module.values.front().fields[1].name = "set_entity_index";
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, ValidatesKnownEnumUnderlyingType) {
    auto module{valid_module()};
    module.values.front().fields.back().type = TypeRef{"@state"};
    EnumModuleSchema enums{
        .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
        .enums = {EnumSchema{
            .name = "FighterStateKind",
            .underlying_type = TypeRef{"int8"},
            .values = {EnumeratorSchema{"Value"}},
        }},
    };
    Manifest manifest{
        .schema_version = manifest_schema_version,
        .types = {{"state", CppType{"FighterStateKind"}}},
        .modules = {std::move(enums), std::move(module)},
    };

    try {
        static_cast<void>(lower_modules(manifest));
        FAIL() << "Expected validation error";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find(
                      "enum must have an unsigned fixed-width underlying type"),
                  std::string::npos);
    }
}

} // namespace
} // namespace codegen
