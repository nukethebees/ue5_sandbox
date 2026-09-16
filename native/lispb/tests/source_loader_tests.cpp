#include <codegen/source_loader.h>

#include <codegen/manifest_error.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace codegen {
namespace {

class TemporaryManifest {
  public:
    TemporaryManifest() {
        static int sequence{};
        directory_ = std::filesystem::temp_directory_path() /
                     ("lispb-manifest-test-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory_);
    }

    ~TemporaryManifest() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    void write(std::string const& name, std::string const& content) const {
        std::ofstream output{directory_ / name};
        output << content;
    }

    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }

    void write_root(std::string const& modules) const {
        write("types.lispb", "");
        write("modules.lispb", modules);
    }

    auto load() const -> Manifest {
        std::filesystem::path const modules[]{path("modules.lispb")};
        return load_sources(path("types.lispb"), modules);
    }
  private:
    std::filesystem::path directory_;
};

TEST(SourceLoader, ReadsCommentsAndTypedSoa) {
    TemporaryManifest files;
    files.write("types.lispb", R"(
; Shared type definition.
(type handle
  :spelling "FHandle"
  :header "Handle.h"
  :pass-by value
  (operation add-element add :pass-by value)
  (operation remove-at-swap remove_at_swap)
  (operation set-element set :pass-by value))
)");
    files.write("modules.lispb", R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    :operations (all)
    (member handles array @handle)))
)");
    auto const manifest{files.load()};

    ASSERT_EQ(manifest.types.size(), 1);
    EXPECT_EQ(manifest.types.at("handle").spelling, "FHandle");
    EXPECT_EQ(manifest.types.at("handle").operation(TypeOperation::add_element), "add");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.structs.front().operations, all_storage_operations());
    EXPECT_EQ(resolve_type(module.structs.front().members.front().type, manifest.types).spelling,
              "FHandle");
}

TEST(SourceLoader, ReadsStandardLibrarySoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module native_data
  :header "NativeData.h"
  :backend standard-library
  (struct Data
    :operations (all)
    (member values array int32)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.backend, SoaBackend::standard_library);
    EXPECT_FALSE(module.settings.source.has_value());
}

TEST(SourceLoader, RejectsUnknownSoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module native_data
  :header "NativeData.h"
  :backend portable
  (struct Data
    (member values array int32)))
)");

    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, ReadsSoaFieldMaskMetadata) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    :field-mask-name FFieldMask
    :field-enum-name EField
    (member masks array FFieldMask)
    (member values array int32
      :mask-field true
      :mask-dimensions ((row_index "3") (column_index "4")))))
)");

    auto const manifest{files.load()};
    auto const& schema{std::get<SoaModuleSchema>(manifest.modules.front()).structs.front()};
    ASSERT_TRUE(schema.field_mask_name.has_value());
    EXPECT_EQ(*schema.field_mask_name, "FFieldMask");
    ASSERT_TRUE(schema.field_enum_name.has_value());
    EXPECT_EQ(*schema.field_enum_name, "EField");
    auto const& member{schema.members[1]};
    EXPECT_TRUE(member.mask_field);
    ASSERT_EQ(member.mask_dimensions.size(), 2);
    EXPECT_EQ(member.mask_dimensions[0].index_name, "row_index");
    EXPECT_EQ(member.mask_dimensions[0].extent, "3");
    EXPECT_EQ(member.mask_dimensions[1].index_name, "column_index");
    EXPECT_EQ(member.mask_dimensions[1].extent, "4");
}

TEST(SourceLoader, ReadsPackedValueModule) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  :namespace project
  (packed-value FighterState
    :storage std::uint32_t
    :invalid-value 0x7fffffff
    (field entity_index std::uint32_t :bits 24 :range-helper true)
    (field state State :bits 8 :kind enum)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<PackedValueModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.values.size(), 1);
    auto const& value{module.values.front()};
    EXPECT_EQ(value.name, "FighterState");
    EXPECT_EQ(value.storage_type.name, "std::uint32_t");
    EXPECT_EQ(value.invalid_value, std::uint64_t{0x7fffffff});
    ASSERT_EQ(value.fields.size(), 2);
    EXPECT_EQ(value.fields[0].bits, 24);
    EXPECT_EQ(value.fields[0].kind, PackedFieldKind::unsigned_integer);
    EXPECT_TRUE(value.fields[0].range_helper);
    EXPECT_EQ(value.fields[1].bits, 8);
    EXPECT_EQ(value.fields[1].kind, PackedFieldKind::enumeration);
}

TEST(SourceLoader, RejectsNonIntegerPackedFieldWidthWithSourceLocation) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage uint8
    (field value uint8 :bits 1.5)))
)");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:6:30"), std::string::npos);
        EXPECT_NE(message.find("packed field bits must be an integer"), std::string::npos);
    }
}

TEST(SourceLoader, RejectsNegativePackedInvalidValue) {
    TemporaryManifest files;
    files.write_root(R"(
(packed-value-module packed
  :header "Packed.h"
  (packed-value Value
    :storage uint8
    :invalid-value -1
    (field value uint8 :bits 8)))
)");

    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, ReportsSourceLocationForUnknownProperties) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all\n  :header \"All.h\"\n  :headers ()\n  :typo true)\n");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:4:3"), std::string::npos);
        EXPECT_NE(message.find("unknown property ':typo'"), std::string::npos);
    }
}

TEST(SourceLoader, RejectsUnknownModuleDeclarations) {
    TemporaryManifest files;
    files.write_root("(mystery-module bad :header \"Bad.h\")");
    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module bad
  :header "Bad.h"
  (struct FData
    :operations (all reset)
    (member values array int32)))
)");
    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, LoadsStructuredTypeReferencesAndFacadeStorage) {
    TemporaryManifest files;
    files.write_root(R"(
(facade-module facade
  :header "Facade.h"
  :source "Facade.cpp"
  :namespace project
  (facade FFacade @target target
    :target-storage reference
    :definitions-in-source true
    (method get (type-ref @vector :suffix " const&")
      :const true
      :target-name get_value
      (parameter index int32 :default "0"))))
)");

    auto const manifest{files.load()};
    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules.front()).facade};
    EXPECT_TRUE(facade.reference_target);
    EXPECT_TRUE(facade.definitions_in_source);
    EXPECT_EQ(facade.methods.front().return_type.suffix, " const&");
    EXPECT_EQ(facade.methods.front().target_name, "get_value");
    EXPECT_TRUE(facade.methods.front().is_const);
}

TEST(SourceLoader, LoadsOpaqueCppBlocksAndKeepsQuotedBodiesCompatible) {
    TemporaryManifest files;
    files.write_root(
        "(soa-module example\n"
        "  :header \"Generated.h\"\n"
        "  :prelude #cpp{class FForward;\n"
        "#define GENERATED_PATH \"C:\\\\generated\"}cpp#\n"
        "  (struct FData\n"
        "    (member values array int32)\n"
        "    (function update void\n"
        "      :body #cpp{if (dt <= 0.0f) {\n"
        "    return;\n"
        "}\n"
        "\n"
        "values[0] += dt;}cpp#\n"
        "      (parameter dt float))\n"
        "    (function reset void\n"
        "      :body (\"values[0] = 0;\"))))\n"
        "(facade-module facade\n"
        "  :header \"Facade.h\"\n"
        "  (facade FFacade Target target\n"
        "    :validation #cpp{checkf(target != nullptr, TEXT(\"missing target\"));}cpp#))");

    auto const manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2);
#ifdef _WIN32
    auto const newline{std::string{"\r\n"}};
#else
    auto const newline{std::string{"\n"}};
#endif

    auto const& soa{std::get<SoaModuleSchema>(manifest.modules[0])};
    ASSERT_EQ(soa.settings.prelude_lines.size(), 1);
    EXPECT_EQ(soa.settings.prelude_lines[0],
              "class FForward;" + newline + "#define GENERATED_PATH \"C:\\\\generated\"");
    ASSERT_EQ(soa.structs[0].functions.size(), 2);
    ASSERT_EQ(soa.structs[0].functions[0].body_lines.size(), 1);
    EXPECT_EQ(soa.structs[0].functions[0].body_lines[0],
              "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
                  "values[0] += dt;");
    EXPECT_EQ(soa.structs[0].functions[1].body_lines, (std::vector<std::string>{"values[0] = 0;"}));

    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules[1]).facade};
    EXPECT_EQ(facade.validation_lines,
              (std::vector<std::string>{"checkf(target != nullptr, TEXT(\"missing target\"));"}));
}

TEST(SourceLoader, AcceptsEmptyCppBodyAndRejectsWrongRawTag) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    (function empty void :body #cpp{}cpp#)))
)");
    auto const manifest{files.load()};
    auto const& function{std::get<SoaModuleSchema>(manifest.modules[0]).structs[0].functions[0]};
    EXPECT_TRUE(function.body_lines.empty());

    files.write_root(R"(
(soa-module example
  :header "Generated.h"
  (struct FData
    (function wrong void :body #hlsl{return 0;}hlsl#)))
)");
    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_TRUE(message.contains("body requires a #cpp raw literal; got #hlsl"));
        EXPECT_TRUE(message.contains("modules.lispb:5:32"));
    }
}

TEST(SourceLoader, RejectsRawLiteralForOrdinaryTextField) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all :header #cpp{Generated.h}cpp# :headers ())");

    EXPECT_THROW(files.load(), ManifestError);
}

TEST(SourceLoader, LoadsSettingsControls) {
    TemporaryManifest files;
    files.write_root(R"(
(settings-module settings
  :header "Settings.h"
  :api-name TSettingsAccess
  :state-name FSettingsState
  (category video "Video")
  (setting resolution-scale "Resolution Scale"
    :category video
    :value-type float
    :backend engine
    :apply deferred
    (control float-range :min 50 :max 100 :step 0.5)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SettingsModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.settings_list.size(), 1);
    EXPECT_EQ(module.settings_list.front().apply_mode, SettingApplyMode::deferred);
    EXPECT_EQ(module.settings_list.front().control.kind, SettingControlKind::float_range);
    EXPECT_EQ(module.settings_list.front().control.step, 0.5);
}

} // namespace
} // namespace codegen
