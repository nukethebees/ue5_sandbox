#include <codegen/json.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace codegen {
namespace {

class TemporaryManifest {
public:
    TemporaryManifest() {
        static int sequence{0};
        directory_ = std::filesystem::temp_directory_path() /
                     ("sandbox-codegen-test-" + std::to_string(++sequence));
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

    auto path(std::string const& name) const -> std::filesystem::path {
        return directory_ / name;
    }

private:
    std::filesystem::path directory_;
};

TEST(Json, LoadsTypedSoaManifest) {
    TemporaryManifest files;
    files.write("types.json",
                R"({"types":{"handle":{"spelling":"FHandle","header":"Handle.h","operations":{"remove_at_swap":"remove_at_swap"}}}})");
    files.write("modules.json",
                R"({"modules":[{"kind":"soa","name":"example","header":"Generated.h","structs":[{"name":"FData","members":[{"name":"handles","kind":"array","type":"@handle"}],"operations":["all"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    auto const manifest{load_manifest(files.path("manifest.json"))};

    ASSERT_EQ(manifest.types.size(), 1);
    EXPECT_EQ(manifest.types.at("handle").spelling, "FHandle");
    EXPECT_EQ(manifest.types.at("handle").operation(TypeOperation::remove_at_swap),
              "remove_at_swap");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.structs.size(), 1);
    EXPECT_EQ(module.structs.front().operations, all_storage_operations());
    EXPECT_EQ(resolve_type(module.structs.front().members.front().type, manifest.types).spelling,
              "FHandle");
}

TEST(Json, ReportsUnknownFieldsWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("modules.json",
                R"({"modules":[{"kind":"umbrella","name":"all","header":"All.h","headers":[],"typo":true}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("/modules/0"), std::string::npos);
                EXPECT_NE(std::string{error.what()}.find("typo"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, RejectsUnknownTypeReferencesAfterDecoding) {
    std::map<std::string, CppType> const types;
    EXPECT_THROW(resolve_type(TypeRef{"@missing", {}, std::nullopt}, types),
                 std::invalid_argument);
}

} // namespace
} // namespace codegen
