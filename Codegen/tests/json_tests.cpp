#include <codegen/json.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }
  private:
    std::filesystem::path directory_;
};

TEST(Json, LoadsTypedSoaManifest) {
    TemporaryManifest files;
    files.write(
        "types.json",
        R"({"types":{"handle":{"spelling":"FHandle","header":"Handle.h","operations":{"remove_at_swap":"remove_at_swap"}}}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"soa","name":"example","header":"Generated.h","structs":[{"name":"FData","members":[{"name":"handles","kind":"array","type":"@handle"}],"operations":["all"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

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
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"umbrella","name":"all","header":"All.h","headers":[],"typo":true}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

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
    EXPECT_THROW(resolve_type(TypeRef{"@missing", {}, std::nullopt}, types), std::invalid_argument);
}

TEST(Json, RejectsMalformedDocumentsWithTheirFileName) {
    TemporaryManifest files;
    files.write("manifest.json", R"({"schema_version":5,)");

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

TEST(Json, RejectsDuplicateFieldsInsteadOfSilentlyOverwritingThem) {
    TemporaryManifest files;
    files.write("manifest.json",
                R"({"schema_version":5,"schema_version":5,"types":"types.json","modules":[]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                auto const message{std::string{error.what()}};
                EXPECT_NE(message.find("manifest.json"), std::string::npos);
                EXPECT_NE(message.find("duplicate field 'schema_version'"), std::string::npos);
                throw;
            }
        },
        ManifestError);

    files.write("manifest.json", R"({"schema_version":5,"types":"types.json","modules":[]})");
    files.write("types.json", R"({"types":{"value":{"spelling":"int32","spelling":"float"}}})");
    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, ReportsMissingRequiredFields) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("manifest.json", R"({"schema_version":5,"types":"types.json"})");

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
    files.write("modules.json",
                R"({"modules":[{"kind":"mystery","name":"bad","header":"Bad.h"}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, RejectsUnknownStorageOperations) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"soa","name":"bad","header":"Bad.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"operations":["explode"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"soa","name":"bad","header":"Bad.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"operations":["all","reset"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
}

TEST(Json, ReportsMissingReferencedDocuments) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["missing.json"]})");

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
        R"({"modules":[{"kind":"facade","name":"facade","header":"Facade.h","source":"Facade.cpp","namespace":"project","facade":{"name":"FFacade","target_type":"@target","target_member_name":"target","methods":[{"name":"get","return_type":{"name":"@vector","suffix":" const&"},"parameters":[{"type":"int32","name":"index","default":"0"}],"const":true,"target_name":"get_value"}],"validation":["check(target);"],"validation_dependencies":["check"],"friends":["FOwner"],"definitions_in_source":true}},{"kind":"vector_soa","name":"vectors","header":"Vectors.h","source":"Vectors.cpp","storage_name":"FVectors","value_type":"float","components":["xs","ys"],"equivalent_type":"@vector","fixed":{"storage_name":"TFixedVectors","containers":["TFixedVectorArray"]}},{"kind":"homogeneous_soa","name":"homogeneous","header":"Values.h","source":"Values.cpp","layouts":[{"name":"Values","components":["xs","ys"],"value_types":[{"type":"float","suffix":"f","equivalent_type":"@vector","input_types":["@vector"]}],"export_specifier":"PROJECT_API"}]},{"kind":"umbrella","name":"all","header":"All.h","headers":["Vectors.h","Values.h"]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    auto const manifest{load_manifest(files.path("manifest.json"))};

    ASSERT_EQ(manifest.modules.size(), 4);
    auto const& facade{std::get<FacadeModuleSchema>(manifest.modules[0])};
    EXPECT_EQ(facade.settings.namespace_name, "project");
    EXPECT_TRUE(facade.facade.definitions_in_source);
    ASSERT_EQ(facade.facade.methods.front().parameters.size(), 1);
    EXPECT_EQ(facade.facade.methods.front().parameters.front().default_value, "0");
    EXPECT_EQ(facade.facade.methods.front().return_type.suffix, " const&");
    EXPECT_TRUE(facade.facade.methods.front().is_const);

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

TEST(Json, LoadsEnumModulesAndConversions) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"enum","name":"modes","header":"Modes.h","source":"Modes.cpp","helper_namespace":"ml","enums":[{"name":"EMode","underlying_type":"uint8","reflection":"blueprint","export_specifier":"PROJECT_API","values":[{"name":"First"},{"name":"Readable","value":"7","display_name":"Readable Value"},{"name":"COUNT","hidden":true}],"conversions":["lex_to_string","string_view","string","lex_to_display_string","display_string_view","display_string"]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    auto const manifest{load_manifest(files.path("manifest.json"))};

    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<EnumModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.helper_namespace, "ml");
    ASSERT_EQ(module.enums.size(), 1);
    auto const& schema{module.enums.front()};
    EXPECT_EQ(schema.name, "EMode");
    EXPECT_EQ(schema.reflection, EnumReflection::blueprint);
    EXPECT_EQ(schema.export_specifier, "PROJECT_API");
    ASSERT_EQ(schema.values.size(), 3);
    EXPECT_EQ(schema.values[1].initializer, "7");
    EXPECT_EQ(schema.values[1].display_name, "Readable Value");
    EXPECT_TRUE(schema.values[2].hidden);
    EXPECT_EQ(schema.conversions.size(), 6);
}

TEST(Json, LoadsStaticTableModules) {
    TemporaryManifest files;
    files.write(
        "types.json",
        R"({"types":{"value":{"spelling":"FValue","header":"Value.h"},"point":{"spelling":"FPoint","header":"Point.h"}}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"static_table","name":"tables","header":"Tables.h","namespace":"project","tables":[{"name":"FValues","export_specifier":"PROJECT_API","rows":[{"name":"first"},{"name":"second"}],"columns":[{"name":"ids","type":"int32"},{"name":"values","type":"@value"}],"groups":[{"name":"point","type":"@point","columns":["ids","values"]}]}]}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

    auto const manifest{load_manifest(files.path("manifest.json"))};

    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& module{std::get<StaticTableModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.settings.namespace_name, "project");
    ASSERT_EQ(module.tables.size(), 1);
    auto const& table{module.tables.front()};
    EXPECT_EQ(table.name, "FValues");
    EXPECT_EQ(table.export_specifier, "PROJECT_API");
    ASSERT_EQ(table.rows.size(), 2);
    EXPECT_EQ(table.rows[1].name, "second");
    ASSERT_EQ(table.columns.size(), 2);
    EXPECT_EQ(table.columns[1].name, "values");
    EXPECT_EQ(table.columns[1].type.name, "@value");
    ASSERT_EQ(table.groups.size(), 1);
    EXPECT_EQ(table.groups.front().name, "point");
    EXPECT_EQ(table.groups.front().type.name, "@point");
    EXPECT_EQ(table.groups.front().columns, std::vector<std::string>({"ids", "values"}));
}

TEST(Json, RejectsUnknownEnumReflectionAndConversionValues) {
    for (auto const& enum_body : {
             R"({"name":"EMode","underlying_type":"uint8","reflection":"reflected","values":[{"name":"Value"}]})",
             R"({"name":"EMode","underlying_type":"uint8","values":[{"name":"Value"}],"conversions":["text"]})",
         }) {
        TemporaryManifest files;
        files.write("types.json", R"({"types":{}})");
        files.write("modules.json",
                    std::string{R"({"modules":[{"kind":"enum","name":"modes","header":"Modes.h","source":"Modes.cpp","enums":[)"} +
                        enum_body + "]}]}");
        files.write("manifest.json",
                    R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");
        EXPECT_THROW(load_manifest(files.path("manifest.json")), ManifestError);
    }
}

TEST(Json, ReportsMissingNestedRequiredFieldsWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":{}})");
    files.write(
        "modules.json",
        R"({"modules":[{"kind":"facade","name":"facade","header":"Facade.h","facade":{"name":"FFacade","target_type":"FTarget","target_member_name":"target","methods":[{"name":"get"}]}}]})");
    files.write("manifest.json",
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

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
                R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");

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

TEST(Json, RejectsEveryCustomParsedCollectionWhenItIsNotAnArray) {
    struct Case {
        std::string modules;
        std::string expected_path;
    };
    std::vector<Case> const cases{
        {R"({"modules":[{"kind":"soa","name":"soa","header":"Soa.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"functions":{}}]}]})",
         "/functions must be an array"},
        {R"({"modules":[{"kind":"soa","name":"soa","header":"Soa.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"functions":[{"name":"update","return_type":"void","parameters":{}}]}]}]})",
         "/parameters must be an array"},
        {R"({"modules":[{"kind":"facade","name":"facade","header":"Facade.h","facade":{"name":"FFacade","target_type":"FTarget","target_member_name":"target","methods":[{"name":"get","return_type":"int32","parameters":{}}]}}]})",
         "/parameters must be an array"},
        {R"({"modules":[{"kind":"homogeneous_soa","name":"values","header":"Values.h","source":"Values.cpp","layouts":[{"name":"Values","components":["xs"],"value_types":[{"type":"float","suffix":"f","input_types":{}}]}]}]})",
         "/input_types must be an array"},
        {R"({"modules":[{"kind":"static_table","name":"tables","header":"Tables.h","tables":[{"name":"FValues","rows":[{"name":"first"}],"columns":[{"name":"values","type":"float"}],"groups":{}}]}]})",
         "/groups must be an array"},
    };

    for (std::size_t index{0}; index < cases.size(); ++index) {
        SCOPED_TRACE(index);
        TemporaryManifest files;
        files.write("types.json", R"({"types":{}})");
        files.write("modules.json", cases[index].modules);
        files.write("manifest.json",
                    R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");
        EXPECT_THROW(
            {
                try {
                    static_cast<void>(load_manifest(files.path("manifest.json")));
                } catch (ManifestError const& error) {
                    EXPECT_NE(std::string{error.what()}.find(cases[index].expected_path),
                              std::string::npos);
                    throw;
                }
            },
            ManifestError);
    }
}

TEST(Json, RejectsNullForPresentOptionalSchemaValues) {
    struct Case {
        std::string modules;
        std::string expected_path;
    };
    std::vector<Case> const cases{
        {R"({"modules":[{"kind":"umbrella","name":"all","header":"All.h","source":null,"headers":["Value.h"]}]})",
         "/source"},
        {R"({"modules":[{"kind":"soa","name":"soa","header":"Soa.h","structs":[{"name":"FData","members":[{"name":"values","kind":"array","type":"int32"}],"equivalent_type":null}]}]})",
         "/equivalent_type"},
        {R"({"modules":[{"kind":"vector_soa","name":"vectors","header":"Vectors.h","source":"Vectors.cpp","storage_name":"FVectors","value_type":"float","components":["xs"],"equivalent_type":"FValue","fixed":null}]})",
         "/fixed"},
        {R"({"modules":[{"kind":"homogeneous_soa","name":"values","header":"Values.h","source":"Values.cpp","layouts":[{"name":"Values","components":["xs"],"value_types":[{"type":"float","suffix":"f","input_types":null}]}]}]})",
         "/input_types"},
    };

    for (std::size_t index{0}; index < cases.size(); ++index) {
        SCOPED_TRACE(index);
        TemporaryManifest files;
        files.write("types.json", R"({"types":{}})");
        files.write("modules.json", cases[index].modules);
        files.write("manifest.json",
                    R"({"schema_version":5,"types":"types.json","modules":["modules.json"]})");
        EXPECT_THROW(
            {
                try {
                    static_cast<void>(load_manifest(files.path("manifest.json")));
                } catch (ManifestError const& error) {
                    EXPECT_NE(std::string{error.what()}.find(cases[index].expected_path),
                              std::string::npos);
                    throw;
                }
            },
            ManifestError);
    }
}

TEST(Json, RejectsNonObjectTypeRegistriesWithTheirPath) {
    TemporaryManifest files;
    files.write("types.json", R"({"types":[]})");
    files.write("manifest.json", R"({"schema_version":5,"types":"types.json","modules":[]})");

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
    files.write("manifest.json", R"({"schema_version":6,"types":"types.json","modules":[]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                EXPECT_NE(std::string{error.what()}.find("/schema_version"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

TEST(Json, ExplainsHowToMigrateSchemaVersionOneFunctionQualifiers) {
    TemporaryManifest files;
    files.write("manifest.json", R"({"schema_version":1,"types":"types.json","modules":[]})");

    EXPECT_THROW(
        {
            try {
                static_cast<void>(load_manifest(files.path("manifest.json")));
            } catch (ManifestError const& error) {
                auto const message{std::string{error.what()}};
                EXPECT_NE(message.find("schema version 1 is obsolete"), std::string::npos);
                EXPECT_NE(message.find("'const' and 'noexcept'"), std::string::npos);
                throw;
            }
        },
        ManifestError);
}

} // namespace
} // namespace codegen
