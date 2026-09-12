#include <codegen/manifest.h>

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
        write("manifest.lispb",
              "(codegen-manifest :schema-version 11 :types \"types.lispb\" "
              ":modules (\"modules.lispb\"))");
    }
  private:
    std::filesystem::path directory_;
};

TEST(Manifest, ReadsCommentsAndTypedSoa) {
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
    files.write(
        "manifest.lispb",
        R"((codegen-manifest :schema-version 11 :types "types.lispb" :modules ("modules.lispb")))");

    auto const manifest{load_manifest(files.path("manifest.lispb"))};

    ASSERT_EQ(manifest.types.size(), 1);
    EXPECT_EQ(manifest.types.at("handle").spelling, "FHandle");
    EXPECT_EQ(manifest.types.at("handle").operation(TypeOperation::add_element), "add");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.structs.front().operations, all_storage_operations());
    EXPECT_EQ(resolve_type(module.structs.front().members.front().type, manifest.types).spelling,
              "FHandle");
}

TEST(Manifest, ReportsSourceLocationForUnknownProperties) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all\n  :header \"All.h\"\n  :headers ()\n  :typo true)\n");

    try {
        static_cast<void>(load_manifest(files.path("manifest.lispb")));
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:4:3"), std::string::npos);
        EXPECT_NE(message.find("unknown property ':typo'"), std::string::npos);
    }
}

TEST(Manifest, RejectsDuplicateProperties) {
    TemporaryManifest files;
    files.write(
        "manifest.lispb",
        R"((codegen-manifest :schema-version 11 :schema-version 11 :types "types.lispb" :modules ()))");
    EXPECT_THROW(load_manifest(files.path("manifest.lispb")), ManifestError);
}

TEST(Manifest, RejectsMalformedDocumentsWithTheirFileName) {
    TemporaryManifest files;
    files.write("manifest.lispb", "(codegen-manifest");
    try {
        static_cast<void>(load_manifest(files.path("manifest.lispb")));
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        EXPECT_NE(std::string{error.what()}.find("manifest.lispb"), std::string::npos);
    }
}

TEST(Manifest, RejectsUnsupportedSchemaVersions) {
    TemporaryManifest files;
    files.write("manifest.lispb",
                R"((codegen-manifest :schema-version 12 :types "types.lispb" :modules ()))");
    EXPECT_THROW(load_manifest(files.path("manifest.lispb")), ManifestError);
}

TEST(Manifest, ExplainsVersionOneFunctionQualifierMigration) {
    TemporaryManifest files;
    files.write("manifest.lispb",
                R"((codegen-manifest :schema-version 1 :types "types.lispb" :modules ()))");
    try {
        static_cast<void>(load_manifest(files.path("manifest.lispb")));
        FAIL() << "Expected manifest error";
    } catch (ManifestError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("schema version 1 is obsolete"), std::string::npos);
        EXPECT_NE(message.find("'const' and 'noexcept'"), std::string::npos);
    }
}

TEST(Manifest, RejectsMissingReferencedDocuments) {
    TemporaryManifest files;
    files.write("manifest.lispb",
                R"((codegen-manifest :schema-version 11 :types "missing.lispb" :modules ()))");
    EXPECT_THROW(load_manifest(files.path("manifest.lispb")), ManifestError);
}

TEST(Manifest, RejectsUnknownModuleDeclarations) {
    TemporaryManifest files;
    files.write_root("(mystery-module bad :header \"Bad.h\")");
    EXPECT_THROW(load_manifest(files.path("manifest.lispb")), ManifestError);
}

TEST(Manifest, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write_root(R"(
(soa-module bad
  :header "Bad.h"
  (struct FData
    :operations (all reset)
    (member values array int32)))
)");
    EXPECT_THROW(load_manifest(files.path("manifest.lispb")), ManifestError);
}

TEST(Manifest, LoadsStructuredTypeReferencesAndFacadeStorage) {
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

    auto const manifest{load_manifest(files.path("manifest.lispb"))};
    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules.front()).facade};
    EXPECT_TRUE(facade.reference_target);
    EXPECT_TRUE(facade.definitions_in_source);
    EXPECT_EQ(facade.methods.front().return_type.suffix, " const&");
    EXPECT_EQ(facade.methods.front().target_name, "get_value");
    EXPECT_TRUE(facade.methods.front().is_const);
}

TEST(Manifest, LoadsSettingsControls) {
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

    auto const manifest{load_manifest(files.path("manifest.lispb"))};
    auto const& module{std::get<SettingsModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.settings_list.size(), 1);
    EXPECT_EQ(module.settings_list.front().apply_mode, SettingApplyMode::deferred);
    EXPECT_EQ(module.settings_list.front().control.kind, SettingControlKind::float_range);
    EXPECT_EQ(module.settings_list.front().control.step, 0.5);
}

} // namespace
} // namespace codegen
