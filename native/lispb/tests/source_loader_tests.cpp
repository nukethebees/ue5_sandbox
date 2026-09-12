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
