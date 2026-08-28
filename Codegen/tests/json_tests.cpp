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

TEST(Json, RejectsMalformedDocumentsWithTheirFileName) {
    TemporaryManifest files;
    files.write("manifest.json", R"({"schema_version":1,)");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("manifest.json"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, ReportsMissingRequiredFields) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("manifest.json", R"({"schema_version":1,"types":"types.json"})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("modules"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, RejectsUnknownModuleKinds) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("modules.json", R"({"modules":[{"kind":"mystery","name":"bad","header":"Bad.h"}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, RejectsUnknownStorageOperations) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"soa","name":"bad","header":"Bad.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"operations":["explode"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"soa","name":"bad","header":"Bad.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"operations":["all","reset"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, ReportsMissingReferencedDocuments) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["missing.json"]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("missing.json"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, LoadsEveryModuleKindAndStructuredTypeReference) {
    TemporaryManifest files;
    files.write(
        "types.json",
        R"({"types":{"vector":{"spelling":"FVector2f","header":"Vector.h"},"target":{"spelling":"FTarget","header":"Target.h"}}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"facade","name":"facade","header":"Facade.h","source":"Facade.cpp","namespace":"project","facade":{"name":"FFacade","target_type":"@target","target_member_name":"target","methods":[{"name":"get","return_type":{"name":"@vector","suffix":" const&"},"parameters":[{"type":"int32","name":"index","default":"0"}],"suffix":" const","target_name":"get_value"}],"validation":["check(target);"],"validation_dependencies":["check"],"friends":["FOwner"],"definitions_in_source":true}},{"kind":"vector_soa","name":"vectors","header":"Vectors.h","source":"Vectors.cpp","storage_name":"FVectors","value_type":"float","components":["xs","ys"],"equivalent_type":"@vector","fixed":{"storage_name":"TFixedVectors","containers":["TFixedVectorArray"]}},{"kind":"homogeneous_soa","name":"homogeneous","header":"Values.h","source":"Values.cpp","layouts":[{"name":"Values","components":["xs","ys"],"value_types":[{"type":"float","suffix":"f","equivalent_type":"@vector","input_types":["@vector"]}],"export_specifier":"PROJECT_API"}]},{"kind":"umbrella","name":"all","header":"All.h","headers":["Vectors.h","Values.h"]}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    auto const manifest{load_manifest(files.path("manifest.json"))};

    ASSERT_EQ(manifest.modules.size(), 4);
    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules[0])};
    EXPECT_EQ(facade.settings.namespace_name, "project");
    EXPECT_TRUE(facade.facade.definitions_in_source);
    ASSERT_EQ(facade.facade.methods.front().parameters.size(), 1);
    EXPECT_EQ(facade.facade.methods.front().parameters.front().default_value, "0");
    EXPECT_EQ(facade.facade.methods.front().return_type.suffix, " const&");

    auto const& vector{std::get<VectorModuleSchema>(manifest.modules[1])};
    ASSERT_TRUE(vector.fixed.has_value());
    EXPECT_EQ(vector.fixed->storage_name, "TFixedVectors");
    EXPECT_EQ(vector.fixed->containers, std::vector<std::string>{"TFixedVectorArray"});

    auto const& homogeneous{std::get<HomogeneousModuleSchema>(manifest.modules[2])};
    auto const& value{homogeneous.layouts.front().value_types.front()};
    ASSERT_TRUE(value.equivalent_type.has_value());
    EXPECT_EQ(value.equivalent_type->name, "@vector");
    ASSERT_EQ(value.input_types.size(), 1);
    EXPECT_EQ(value.input_types.front().name, "@vector");
    EXPECT_EQ(homogeneous.layouts.front().export_specifier, "PROJECT_API");

    auto const& umbrella{std::get<UmbrellaModuleSchema>(manifest.modules[3])};
    EXPECT_EQ(umbrella.headers, std::vector<std::string>({"Vectors.h", "Values.h"}));
}

TEST(Json, ReportsMissingNestedRequiredFieldsWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"facade","name":"facade","header":"Facade.h","facade":{"name":"FFacade","target_type":"FTarget","target_member_name":"target","methods":[{"name":"get"}]}}]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                auto const message{std::string{error.what()}};
                EXPECT_NE(message.find("/facade/methods/0"), std::string::npos);
                EXPECT_NE(message.find("return_type"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, RejectsWrongCollectionTypesWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("modules.json", R"({"modules":{"kind":"umbrella"}})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("/modules must be an array"),
                          std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, RejectsNonObjectTypeRegistriesWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":[]})");
    files.write("manifest.json",
                R"({"schema_version":1,"types":"types.json","modules":[]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("/types must be an object"),
                          std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, RejectsUnsupportedSchemaVersionsWithTheirPath) {
    TemporaryManifest files;
    files.write("manifest.json",
                R"({"schema_version":2,"types":"types.json","modules":[]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("/schema_version"),
                          std::string::npos);
                throw;
            }
        },
        ManifestError);
}

} // namespace
} // namespace codegen
