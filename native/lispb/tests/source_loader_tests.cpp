#include <codegen/generator.h>
#include <codegen/manifest_error.h>
#include <codegen/source_loader.h>
#include <codegen/validation.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
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
        std::filesystem::create_directories((directory_ / name).parent_path());
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

template <typename Schema>
auto schema_at(Manifest const& manifest,
               std::size_t const module_index,
               std::size_t const declaration_index = 0) -> Schema const& {
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.at(module_index))};
    return std::get<Schema>(module.declarations.at(declaration_index));
}

TEST(SourceLoader, RecordValueSemanticsSurviveParsingGraphAndLowering) {
    TemporaryManifest files;
    files.write_root(R"((module records :header "Records.h" :backend standard-library
      (record Value :comparison three-way :comparison-noexcept true
        (member absent int)
        (member zero int :initializer "")
        (member explicit_value int :initializer "7")
        (function read int :const true :noexcept true :constexpr true :nodiscard true
          :body ("return explicit_value;"))
        (function validate bool :const true :noexcept true))))");
    auto const manifest{files.load()};
    auto const& schema{schema_at<RecordSchema>(manifest, 0)};
    EXPECT_FALSE(schema.members[0].initializer.has_value());
    EXPECT_EQ(schema.members[1].initializer, "");
    EXPECT_EQ(schema.members[2].initializer, "7");
    EXPECT_EQ(schema.comparison, RecordComparison::three_way);
    ASSERT_EQ(schema.functions.size(), 2U);
    EXPECT_TRUE(schema.functions[0].is_constexpr);
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const& record{std::get<lispb::schema::RecordType>(
        graph.type(*graph.find_declared("records", "Value")).definition)};
    EXPECT_FALSE(record.members[0].initializer.has_value());
    EXPECT_EQ(record.members[1].initializer, "");
    EXPECT_EQ(record.members[2].initializer, "7");
    auto const output{render_modules(lower_modules(manifest)).front().content};
    for (auto const expected : {"int absent;",
                                "int zero{};",
                                "int explicit_value{7};",
                                "auto operator<=>(Value const&) const noexcept = default;",
                                "[[nodiscard]] constexpr int read() const noexcept",
                                "return explicit_value;",
                                "bool validate() const noexcept;",
                                "#include <compare>"}) {
        EXPECT_NE(output.find(expected), std::string::npos) << expected;
    }
}

TEST(SourceLoader, RecordRejectsStatementInitializersAndInvalidMethods) {
    TemporaryManifest files;
    for (auto const declaration :
         {R"((record Value (member x int :initializer "0; int injected")))",
          R"((record Value :comparison-noexcept true (member x int)))",
          R"((record Value (member x int) (function x int)))",
          R"((record Value (function read int :static true :const true)))",
          R"((record Value (function read int :constexpr true)))",
          R"((record Value (function read int) (function read int)))"}) {
        files.write_root(std::string{"(module records :header \"Records.h\" "} + declaration + ")");
        EXPECT_THROW(static_cast<void>(lower_modules(files.load())), std::exception) << declaration;
    }
}

TEST(SourceLoader, SoaStoragePolicySelectsRepresentations) {
    TemporaryManifest files;
    files.write_root(R"((module rows :header "Rows.h" :backend standard-library
      (struct VectorRows :storage vector (member values array int32))
      (struct CompactRows :storage single-allocation (member values array int32)
        (single-allocation SingleRows))
      (struct BothRows :storage both (member values array int32)
        (single-allocation SingleBothRows))))");
    auto const manifest{files.load()};
    EXPECT_TRUE(schema_at<SoaSchema>(manifest, 0, 0).emits_vector_storage());
    EXPECT_FALSE(schema_at<SoaSchema>(manifest, 0, 1).emits_vector_storage());
    EXPECT_TRUE(schema_at<SoaSchema>(manifest, 0, 2).emits_vector_storage());
    auto const output{render_modules(lower_modules(manifest)).front().content};
    EXPECT_NE(output.find("struct VectorRowsView"), std::string::npos);
    EXPECT_EQ(output.find("struct CompactRowsView"), std::string::npos);
    EXPECT_NE(output.find("struct BothRowsView"), std::string::npos);
    for (auto const* invalid : {"vector", "single-allocation", "both", "unknown"}) {
        files.write_root(
            std::string{R"((module rows :header "Rows.h" (struct Rows :storage )"} + invalid +
            " (member values array int32)" +
            (std::string_view{invalid} == "vector" ? " (single-allocation SingleRows)" : "") +
            "))");
        EXPECT_THROW(lower_modules(files.load()), std::exception);
    }
}

TEST(SourceLoader, AliasEmissionKeepsSemanticIdentityAndDependencies) {
    TemporaryManifest files;
    files.write_root(
        R"((module vitals :header "Vitals.h" :namespace fixture :backend standard-library
      (record Vessel (member health Health))
      (struct Fleet (member healths array Health))
      (packed-value PackedHealth :storage std::uint16_t (field health @health :bits auto))
      (integer-scalar Health :signed false :minimum 0 :maximum 65534 :bit-width 16
        :cpp-type @native_uint16 :cpp-emission alias
        (code Invalid :value 65535 :sentinel true)
        (relation references Vessel)))
      (module consumers :header "Consumers.h" :namespace fixture
        (record Other (member health @health))))");
    files.write("types.lispb", R"(
      (type native_uint16 :spelling "std::uint16_t" :header "cstdint" :pass-by value)
      (type health :spelling "fixture::Health" :header "Vitals.h" :pass-by value))");
    auto const manifest{files.load()};
    auto const& scalar{schema_at<IntegerScalarSchema>(manifest, 0, 3)};
    EXPECT_EQ(scalar.cpp_emission, IntegerScalarCppEmission::alias);
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const health{*graph.find_declared("vitals", "Health")};
    auto const& semantic{std::get<lispb::schema::IntegerScalarType>(graph.type(health).definition)};
    EXPECT_EQ(semantic.bit_width, 16);
    EXPECT_EQ(semantic.maximum_value, PackedIntegerValue{65534});
    ASSERT_TRUE(semantic.cpp_representation.has_value());
    EXPECT_EQ(semantic.cpp_representation->cpp_type.spelling, "std::uint16_t");
    ASSERT_TRUE(semantic.relationship.has_value());
    EXPECT_EQ(semantic.named_codes.size(), 1);
    EXPECT_EQ(graph.find_registered("health"), health);
    auto const vessel{*graph.find_declared("vitals", "Vessel")};
    auto const& record{std::get<lispb::schema::RecordType>(graph.type(vessel).definition)};
    EXPECT_EQ(record.members[0].semantic_type.type, health);
    auto const packed{*graph.find_declared("vitals", "PackedHealth")};
    auto const& field{std::get<lispb::schema::PackedField>(
        std::get<lispb::schema::PackedType>(graph.type(packed).definition).segments[0])};
    EXPECT_EQ(field.semantic_type.type, health);
    for (auto const consumer : {vessel,
                                packed,
                                *graph.find_declared("vitals", "Fleet"),
                                *graph.find_declared("consumers", "Other")}) {
        EXPECT_NE(std::ranges::find(graph.dependencies_of(consumer), health),
                  graph.dependencies_of(consumer).end());
        EXPECT_NE(std::ranges::find(graph.users_of(health), consumer),
                  graph.users_of(health).end());
    }
    auto const generated{render_modules(lower_modules(manifest))};
    auto const& header{generated.front().content};
    EXPECT_NE(header.find("using Health = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("#include <cstdint>"), std::string::npos);
    EXPECT_LT(header.find("using Health ="), header.find("struct Vessel"));
    EXPECT_LT(header.find("using Health ="), header.find("struct Fleet"));
    EXPECT_EQ(header.find("Health_Invalid"), std::string::npos);
}

TEST(SourceLoader, ScalarEmissionOnlyEstablishesRepresentationForAliases) {
    for (auto const mode : {IntegerScalarCppEmission::none,
                            IntegerScalarCppEmission::constants,
                            IntegerScalarCppEmission::constants_with_names,
                            IntegerScalarCppEmission::alias}) {
        auto const name{std::string{integer_scalar_cpp_emission_name(mode)}};
        SCOPED_TRACE(name);
        TemporaryManifest files;
        files.write_root(
            "(module reasons :header \"Reasons.h\"\n"
            "  (integer-scalar Context :signed false :minimum 0 :maximum 10\n"
            "    (relation references Reason))\n"
            "  (integer-scalar Reason :signed false :minimum 0 :maximum 10 :bit-width 8\n"
            "    :cpp-emission " +
            name + (mode == IntegerScalarCppEmission::none ? "" : " :cpp-type @byte") +
            "\n    (code Unknown :value 0)\n"
            "    (code Invalid :value 255 :sentinel true)\n"
            "    (relation references Context)))");
        files.write("types.lispb", R"((type byte :spelling "std::uint8_t" :header "cstdint"))");
        auto const manifest{files.load()};
        EXPECT_EQ(schema_at<IntegerScalarSchema>(manifest, 0, 1).cpp_emission, mode);
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const reason{*graph.find_declared("reasons", "Reason")};
        auto const context{*graph.find_declared("reasons", "Context")};
        auto const byte{*graph.find_registered("byte")};
        auto const& scalar{
            std::get<lispb::schema::IntegerScalarType>(graph.type(reason).definition)};
        auto const alias{mode == IntegerScalarCppEmission::alias};
        EXPECT_EQ(scalar.cpp_representation.has_value(), alias);
        if (alias) {
            EXPECT_EQ(scalar.cpp_representation->type, byte);
        }
        EXPECT_EQ(scalar.bit_width, 8);
        EXPECT_EQ(scalar.named_codes.size(), 2);
        ASSERT_TRUE(scalar.relationship.has_value());
        EXPECT_EQ(scalar.relationship->target.type, context);
        auto const& related{
            std::get<lispb::schema::IntegerScalarType>(graph.type(context).definition)};
        EXPECT_FALSE(related.cpp_representation.has_value());
        ASSERT_TRUE(related.relationship.has_value());
        EXPECT_EQ(related.relationship->target.type, reason);
        EXPECT_EQ(std::ranges::find(graph.dependencies_of(reason), byte) !=
                      graph.dependencies_of(reason).end(),
                  alias);
        EXPECT_EQ(std::ranges::find(graph.users_of(byte), reason) != graph.users_of(byte).end(),
                  alias);
        EXPECT_NE(std::ranges::find(graph.dependencies_of(reason), context),
                  graph.dependencies_of(reason).end());
        EXPECT_NE(std::ranges::find(graph.users_of(context), reason),
                  graph.users_of(context).end());
        EXPECT_NE(std::ranges::find(graph.dependencies_of(context), reason),
                  graph.dependencies_of(context).end());
        EXPECT_NE(std::ranges::find(graph.users_of(reason), context), graph.users_of(reason).end());

        auto const header{render_modules(lower_modules(manifest)).front().content};
        auto const constants{mode == IntegerScalarCppEmission::constants ||
                             mode == IntegerScalarCppEmission::constants_with_names};
        EXPECT_EQ(header.contains("using Reason = std::uint8_t;"), alias);
        EXPECT_EQ(header.contains("inline constexpr std::uint8_t Reason_Invalid"), constants);
        EXPECT_EQ(header.contains("Reason_name("),
                  mode == IntegerScalarCppEmission::constants_with_names);
        EXPECT_EQ(header.contains("#include <cstdint>"), mode != IntegerScalarCppEmission::none);
        EXPECT_FALSE(header.contains("Context"));
    }
}

TEST(SourceLoader, ScalarAliasesShareCppTypeIncludeResolution) {
    for (auto const* representation : {"@native_int32", "std::int32_t", "int32", "uint32"}) {
        SCOPED_TRACE(representation);
        TemporaryManifest files;
        files.write_root("(module values :header \"Values.h\"\n"
                         "  (integer-scalar Value :signed false :minimum 0 :maximum 100\n"
                         "    :cpp-emission alias :cpp-type " +
                         std::string{representation} + "))");
        files.write("types.lispb",
                    R"((type native_int32 :spelling "std::int32_t" :header "cstdint"))");
        auto const header{render_modules(lower_modules(files.load())).front().content};
        auto const standard{std::string_view{representation}.starts_with('@') ||
                            std::string_view{representation}.starts_with("std::")};
        auto const spelling{standard ? "std::int32_t" : representation};
        EXPECT_TRUE(header.contains("using Value = " + std::string{spelling} + ";"));
        auto const include{standard ? "#include <cstdint>" : "#include \"CoreTypes.h\""};
        auto const position{header.find(include)};
        ASSERT_NE(position, std::string::npos);
        EXPECT_EQ(header.find(include, position + 1), std::string::npos);
    }
}

TEST(SourceLoader, PackedDefaultsAreValidatedWithoutChangingImplicitSentinels) {
    TemporaryManifest files;
    files.write_root(R"((module orders :header "Orders.h"
      (packed-value Order :storage std::uint8_t
        (field task std::uint8_t :bits 1 :default 1)
        (field target std::uint8_t :bits 1 :default 0))
      (packed-value Partial :storage std::uint8_t
        (field task std::uint8_t :bits 1 :default 1)
        (field target std::uint8_t :bits 1))
      (packed-value Implicit :storage std::uint8_t
        (field task std::uint8_t :bits 1))
      (packed-value Invalid :storage std::uint8_t :invalid-value 255
        (field task std::uint8_t :bits 1))))");
    auto const manifest{files.load()};
    EXPECT_EQ(std::get<PackedFieldSchema>(schema_at<PackedValueSchema>(manifest, 0).segments[0])
                  .default_value,
              PackedIntegerValue{1});
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto default_raw = [&](char const* name) {
        return std::get<lispb::schema::PackedType>(
                   graph.type(*graph.find_declared("orders", name)).definition)
            .default_raw_value;
    };
    EXPECT_EQ(default_raw("Order"), 1);
    EXPECT_EQ(default_raw("Partial"), std::nullopt);
    EXPECT_EQ(default_raw("Implicit"), 0);
    EXPECT_EQ(default_raw("Invalid"), 255);
    auto const header{render_modules(lower_modules(manifest)).front().content};
    EXPECT_NE(header.find("Partial() noexcept = delete"), std::string::npos);
    EXPECT_NE(header.find("return Partial{RawTag{}, raw}"), std::string::npos);
}

TEST(SourceLoader, IncludedExternalScalarsRetainExternalIdentityAndEnrichPlainReferences) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"
      (record Values (member x double) (member y int8) (member z std::int8_t) (member w @signed_byte))))");
    files.write("types.lispb",
                R"((include "common/types.lispb") (include "common/./scalars.lispb"))");
    files.write("common/types.lispb", R"((include "scalars.lispb"))");
    files.write("common/scalars.lispb", R"(
      (type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))");
    auto const registry{load_type_registry(files.path("types.lispb"))};
    ASSERT_EQ(registry.sources.size(), 3);
    EXPECT_EQ(registry.declarations.at("signed_byte").source_file_index, 2);
    auto const manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const registered{*graph.find_registered("signed_byte")};
    EXPECT_EQ(graph.type(registered).identity.origin,
              lispb::schema::TypeOrigin::registered_external);
    auto const& record{std::get<lispb::schema::RecordType>(
        graph.type(*graph.find_declared("example", "Values")).definition)};
    auto const& floating{std::get<lispb::schema::ExternalType>(
        graph.type(record.members[0].semantic_type.type).definition)};
    EXPECT_EQ(std::get<FloatingPointFormat>(floating.semantics),
              FloatingPointFormat::ieee754_binary64);
    for (std::size_t index{1}; index < record.members.size(); ++index) {
        auto const& node{graph.type(record.members[index].semantic_type.type)};
        EXPECT_TRUE(std::holds_alternative<lispb::schema::ExternalType>(node.definition));
        auto const* scalar{lispb::schema::integer_domain(node)};
        ASSERT_NE(scalar, nullptr);
        EXPECT_TRUE(scalar->signedness);
        EXPECT_EQ(scalar->bit_width, 8);
        EXPECT_EQ(scalar->minimum_value, PackedIntegerValue{-128});
        EXPECT_EQ(scalar->maximum_value, PackedIntegerValue{127});
    }
    std::filesystem::path const modules[]{files.path("modules.lispb")};
    auto const compiled{compile_sources(files.path("types.lispb"), modules)};
    EXPECT_NE(
        std::ranges::find(compiled.dependencies,
                          std::filesystem::weakly_canonical(files.path("common/scalars.lispb"))),
        compiled.dependencies.end());
    EXPECT_EQ(compiled.dependencies.size(), 4);
}

TEST(SourceLoader, ExternalIntegerDomainsDriveRepresentationsAndPackedWidths) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"
      (linear-quantized Quantized :source @index :bits 4)
      (integer-varint Encoded :source @index :encoding unsigned)
      (optional-sentinel MaybeIndex :source @index :sentinel invalid)
      (optional-presence-bit PresentIndex :source @index)
      (packed-value Packed :storage uint32
        (field index @index :bits auto)
        (field subset uint32 :bits 24))))");
    files.write("types.lispb", R"(
      (type index :spelling "NativeIndex" (integer :signed false :bit-width 8 :maximum 254
        (code invalid :value 255 :sentinel true)))
      (type uint32 :spelling "uint32" (integer :signed false :bit-width 32)))");
    auto const manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const source{*graph.find_registered("index")};
    EXPECT_EQ(graph.users_of(source).size(), 5);
    auto const& packed{std::get<lispb::schema::PackedType>(
        graph.type(*graph.find_declared("example", "Packed")).definition)};
    EXPECT_EQ(std::get<lispb::schema::PackedField>(packed.segments[0]).bit_width, 8);
    EXPECT_EQ(std::get<lispb::schema::PackedField>(packed.segments[1]).bit_width, 24);
    auto const output{render_modules(lower_modules(manifest))};
    ASSERT_EQ(output.size(), 1);
    EXPECT_EQ(output[0].content.find("struct NativeIndex"), std::string::npos);
    EXPECT_NE(output[0].content.find("std::uint8_t"), std::string::npos);
}

TEST(SourceLoader, ExternalMetadataPreservesExplicitWidthCppFieldEmission) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"
      (packed-value Packed :storage uint64
        (field subset uint32 :bits 24)
        (field alias @legacy :bits 24 :minimum 0 :maximum 100)
        (reserved future :bits 16))
      (record Values (member x int8) (member y double))))");
    files.write("types.lispb", R"((type legacy :spelling "uint32"))");
    auto const original{render_modules(lower_modules(files.load()))};
    files.write("types.lispb", R"(
      (type legacy :spelling "uint32")
      (type integer :spelling "uint32" (integer :signed false :bit-width 32))
      (type byte :spelling "int8" (integer :signed true :bit-width 8))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))");
    auto const manifest{files.load()};
    auto const enriched{render_modules(lower_modules(manifest))};
    ASSERT_EQ(original.size(), 1);
    ASSERT_EQ(enriched.size(), 1);
    EXPECT_EQ(original[0].content, enriched[0].content);

    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const& node{graph.type(*graph.find_registered("integer"))};
    auto field{std::get<PackedFieldSchema>(schema_at<PackedValueSchema>(manifest, 0).segments[0])};
    EXPECT_EQ(lispb::schema::packed_integer_domain(node, field), nullptr);
    field.bits.reset();
    EXPECT_NE(lispb::schema::packed_integer_domain(node, field), nullptr);
    field.bits = 32;
    field.type.name = "@integer";
    EXPECT_EQ(lispb::schema::packed_integer_domain(node, field), nullptr);
}

TEST(SourceLoader, RejectsInvalidExternalDomainsAndIncludes) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"))");
    for (
        auto const* source :
        {R"((type x :spelling "X" (integer :signed true :bit-width 0)))",
         R"((type x :spelling "X" (integer :signed true :bit-width 8 :maximum 128)))",
         R"((type x :spelling "X" (integer :signed false :bit-width 8 :minimum -1)))",
         R"((type x :spelling "X" (integer :signed false :bit-width 8 (code invalid :value 255 :sentinel true))))",
         R"((type x :spelling "X" (floating-point :format invented)))",
         R"((type x :spelling "X" (integer :signed true :bit-width 8) (floating-point :format ieee754-binary32)))",
         R"((type x :spelling "int8" (integer :signed true :bit-width 8)) (type y :spelling "std::int8_t" (integer :signed false :bit-width 8)))",
         R"((type x :spelling "X") (type x :spelling "Y"))",
         R"((include "missing.lispb"))",
         R"((include "types.lispb"))"}) {
        SCOPED_TRACE(source);
        files.write("types.lispb", source);
        EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::exception);
    }
}

TEST(SourceLoader, ExternalIntegerDefaultsCoverFull64BitDomains) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"))");
    files.write("types.lispb", R"(
      (type signed64 :spelling "int64" (integer :signed true :bit-width 64))
      (type unsigned64 :spelling "uint64" (integer :signed false :bit-width 64)))");
    auto const graph{lispb::schema::resolve_type_graph(files.load())};
    auto const* signed_domain{
        lispb::schema::integer_domain(graph.type(*graph.find_registered("signed64")))};
    auto const* unsigned_domain{
        lispb::schema::integer_domain(graph.type(*graph.find_registered("unsigned64")))};
    ASSERT_NE(signed_domain, nullptr);
    ASSERT_NE(unsigned_domain, nullptr);
    EXPECT_EQ(signed_domain->minimum_value,
              PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63));
    EXPECT_EQ(unsigned_domain->maximum_value,
              PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()});
}

TEST(SourceLoader, ExternalRepresentationSourcesObeyIntegerDomainRules) {
    TemporaryManifest files;
    files.write("types.lispb", R"(
      (type real :spelling "double" (floating-point :format ieee754-binary64))
      (type opaque :spelling "Opaque")
      (type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
      (type byte :spelling "uint8" (integer :signed false :bit-width 8)))");
    for (auto const* declaration :
         {"(linear-quantized Q :source @real :bits 4)",
          "(integer-varint V :source @opaque :encoding unsigned)",
          "(integer-varint V :source @signed_byte :encoding unsigned)",
          "(integer-varint V :source @byte :encoding zigzag)",
          "(optional-sentinel S :source @byte :sentinel missing)",
          "(optional-presence-bit P :source @real)",
          "(optional-presence-bit P :source (type-ref @byte :suffix \"*\"))",
          "(record double (member x int8))"}) {
        SCOPED_TRACE(declaration);
        files.write("modules.lispb",
                    "(module example :header \"Example.h\" " + std::string{declaration} + ")");
        EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::exception);
    }
}

TEST(SourceLoader, NormalModuleKeepsMixedDeclarationOrderAndResolvesTypes) {
    TemporaryManifest files;
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  :namespace example
  (enum State uint8
    (value Alive)
    (value Dead))
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 100
    :bit-width auto)
  (linear-quantized HealthQ7
    :source Health
    :bits 7
    :reserved-codes 1
    :clipping clamp)
  (record Snapshot
    (member state State)
    (member health Health)))
)");

    auto const manifest{files.load()};
    validate_manifest(manifest);
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.declarations.size(), 4);
    EXPECT_EQ(declaration_name(module.declarations[0]), "State");
    EXPECT_EQ(declaration_name(module.declarations[1]), "Health");
    EXPECT_EQ(declaration_name(module.declarations[2]), "HealthQ7");
    EXPECT_EQ(declaration_name(module.declarations[3]), "Snapshot");

    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const snapshot{graph.find_declared("mixed", "Snapshot")};
    ASSERT_TRUE(snapshot.has_value());
    auto const& record{std::get<lispb::schema::RecordType>(graph.type(*snapshot).definition)};
    ASSERT_EQ(record.members.size(), 2);
    EXPECT_EQ(graph.type(record.members[0].semantic_type.type).identity.name, "State");
    EXPECT_EQ(graph.type(record.members[1].semantic_type.type).identity.name, "Health");

    auto const generated{render_modules(lower_modules(manifest))};
    ASSERT_EQ(generated.size(), 1);
    auto const enum_position{generated.front().content.find("enum class State")};
    auto const record_position{generated.front().content.find("struct Snapshot")};
    EXPECT_NE(enum_position, std::string::npos);
    EXPECT_NE(record_position, std::string::npos);
    EXPECT_LT(enum_position, record_position);
}

TEST(SourceLoader, RejectsRetiredDeclarationModuleHeads) {
    TemporaryManifest files;
    for (auto const* head : {"enum-module",
                             "soa-module",
                             "static-table-module",
                             "homogeneous-soa-module",
                             "packed-value-module",
                             "scalar-module",
                             "representation-module",
                             "record-module",
                             "union-module",
                             "vector-soa-module",
                             "facade-module"}) {
        SCOPED_TRACE(head);
        files.write_root("(" + std::string{head} + " retired :header \"Retired.h\")");
        try {
            static_cast<void>(files.load());
            FAIL() << "Retired module head was accepted";
        } catch (ManifestError const& error) {
            EXPECT_NE(std::string{error.what()}.find("unknown module declaration"),
                      std::string::npos);
        }
    }
}

TEST(SourceLoader, OrdersPhysicalDependenciesWithoutChangingDeclarationIndices) {
    TemporaryManifest files;
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  (record Outer (member payload Payload))
  (integer-scalar Index :signed false :minimum 0 :maximum 255)
  (union Payload (alternative inner Inner))
  (record Inner (member value float (relation references Outer))))
)");
    auto const manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    ASSERT_TRUE(graph.find_declared("mixed", "Inner").has_value());
    auto const files_out{render_modules(lower_modules(manifest))};
    auto const& header{files_out.front().content};
    EXPECT_LT(header.find("struct Inner"), header.find("union Payload"));
    EXPECT_LT(header.find("union Payload"), header.find("struct Outer"));
    auto const& declarations{std::get<NormalModuleSchema>(manifest.modules.front()).declarations};
    EXPECT_EQ(declaration_name(declarations[0]), "Outer");
    EXPECT_EQ(declaration_name(declarations[1]), "Index");
    EXPECT_EQ(declaration_name(declarations[2]), "Payload");
    EXPECT_EQ(declaration_name(declarations[3]), "Inner");
}

TEST(SourceLoader, NormalModuleRejectsCrossKindGeneratedCppNameCollision) {
    TemporaryManifest files;
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  (enum FDataView uint8
    (value One))
  (struct FData
    (member items array int32)))
)");

    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);
}

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
(module example
  :header "Generated.h"
  (struct FData
    :operations (all)
    (member handles array @handle
      (relation offset_into FData :unit elements))))
)");
    auto const manifest{files.load()};

    ASSERT_EQ(manifest.types.size(), 1);
    EXPECT_EQ(manifest.types.at("handle").cpp_type.spelling, "FHandle");
    EXPECT_EQ(manifest.types.at("handle").cpp_type.operation(TypeOperation::add_element), "add");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& schema{schema_at<SoaSchema>(manifest, 0)};
    EXPECT_EQ(schema.operations, all_storage_operations());
    auto const& member{schema.members.front()};
    EXPECT_EQ(resolve_type(member.type, manifest.types).spelling, "FHandle");
    ASSERT_TRUE(member.relationship.has_value());
    EXPECT_EQ(member.relationship->kind, SemanticRelationKind::offset_into);
    EXPECT_EQ(member.relationship->target.name, "FData");
    EXPECT_EQ(member.relationship->unit, SemanticRelationUnit::elements);
}

TEST(SourceLoader, ReadsStandardLibrarySoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(module native_data
  :header "NativeData.h"
  :backend standard-library
  (struct Data
    :operations (all)
    (member values array int32)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.soa_backend, SoaBackend::standard_library);
    EXPECT_FALSE(module.settings.source.has_value());
}

TEST(SourceLoader, RejectsUnknownSoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(module native_data
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
(module example
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
    auto const& schema{schema_at<SoaSchema>(manifest, 0)};
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
(module packed
  :header "Packed.h"
  :namespace project
  (packed-value FighterState
    :storage std::uint32_t
    :byte-order big
    :bit-order msb-first
    :invalid-value 0x7fffffff
    :mutable true
    (field entity_index std::uint32_t :bits auto :range-helper true :minimum 0 :maximum 1000000
      (code Player :value 42)
      (code Invalid :value 1048575 :sentinel true)
      (relation index_into FighterState))
    (reserved future :bits 4)
    (field state State :bits 8 :kind enum)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.declarations.size(), 1);
    auto const& value{schema_at<PackedValueSchema>(manifest, 0)};
    EXPECT_EQ(value.name, "FighterState");
    EXPECT_EQ(value.storage_type.name, "std::uint32_t");
    EXPECT_EQ(value.byte_order, PackedByteOrder::big_endian);
    EXPECT_EQ(value.bit_order, PackedBitOrder::most_significant_first);
    EXPECT_EQ(value.invalid_value, std::uint64_t{0x7fffffff});
    EXPECT_TRUE(value.mutable_value);
    ASSERT_EQ(value.segments.size(), 3);
    auto const& index{std::get<PackedFieldSchema>(value.segments[0])};
    EXPECT_FALSE(index.bits.has_value());
    EXPECT_EQ(index.kind, PackedFieldKind::unsigned_integer);
    EXPECT_TRUE(index.range_helper);
    EXPECT_EQ(index.minimum_value, 0U);
    EXPECT_EQ(index.maximum_value, 1'000'000U);
    ASSERT_EQ(index.named_codes.size(), 2U);
    EXPECT_EQ(index.named_codes[0].name, "Player");
    EXPECT_EQ(index.named_codes[0].value, 42U);
    EXPECT_FALSE(index.named_codes[0].sentinel);
    EXPECT_EQ(index.named_codes[1].name, "Invalid");
    EXPECT_EQ(index.named_codes[1].value, 1'048'575U);
    EXPECT_TRUE(index.named_codes[1].sentinel);
    ASSERT_TRUE(index.relationship.has_value());
    EXPECT_EQ(index.relationship->kind, SemanticRelationKind::index_into);
    EXPECT_EQ(index.relationship->target.name, "FighterState");
    auto const& reserved{std::get<PackedReservedBitsSchema>(value.segments[1])};
    EXPECT_EQ(reserved.name, "future");
    EXPECT_EQ(reserved.bits, 4);
    auto const& state{std::get<PackedFieldSchema>(value.segments[2])};
    EXPECT_EQ(state.bits, 8);
    EXPECT_EQ(state.kind, PackedFieldKind::enumeration);
}

TEST(SourceLoader, ReadsSignedArbitraryWidthPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(module packed
  :header "Packed.h"
  (packed-value SignedDelta
    :storage std::uint32_t
    (field delta std::int32_t :bits auto :kind signed :minimum -100 :maximum 100
      (code Origin :value 0)
      (code Unknown :value -128 :sentinel true))
    (reserved future :bits 24)))
)");

    auto const manifest{files.load()};
    auto const& value{schema_at<PackedValueSchema>(manifest, 0)};
    EXPECT_FALSE(value.byte_order.has_value());
    EXPECT_FALSE(value.bit_order.has_value());
    auto const& delta{std::get<PackedFieldSchema>(value.segments.front())};
    EXPECT_EQ(delta.type.name, "std::int32_t");
    EXPECT_FALSE(delta.bits.has_value());
    EXPECT_EQ(delta.kind, PackedFieldKind::signed_integer);
    EXPECT_EQ(delta.minimum_value, PackedIntegerValue{-100});
    EXPECT_EQ(delta.maximum_value, PackedIntegerValue{100});
    ASSERT_EQ(delta.named_codes.size(), 2U);
    EXPECT_EQ(delta.named_codes[0].value, PackedIntegerValue{0});
    EXPECT_EQ(delta.named_codes[1].value, PackedIntegerValue{-128});
    EXPECT_TRUE(delta.named_codes[1].sentinel);
}

TEST(SourceLoader, ReadsLinearQuantizedPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(module scalars
  :header "Scalars.h"
  :namespace project
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 1000
    :bit-width auto))
(module representations
  :header "Representations.h"
  :namespace project
  (linear-quantized HealthQ8
    :source project::Health
    :bits 8
    :reserved-codes 2
    :clipping clamp))
(module packed
  :header "Packed.h"
  :namespace project
  (packed-value Vitals
    :storage std::uint16_t
    (field health project::HealthQ8 :bits auto :kind linear-quantized)
    (field state std::uint8_t :bits 8)))
)");

    auto const manifest{files.load()};
    auto const& value{schema_at<PackedValueSchema>(manifest, manifest.modules.size() - 1)};
    auto const& health{std::get<PackedFieldSchema>(value.segments.front())};
    EXPECT_EQ(health.type.name, "project::HealthQ8");
    EXPECT_FALSE(health.bits.has_value());
    EXPECT_EQ(health.kind, PackedFieldKind::linear_quantized);
}

TEST(SourceLoader, ReadsFixedPointPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(module representations
  :header "Representations.h"
  :namespace project
  (fixed-point VelocityQ12_4
    :signed true
    :total-bits 16
    :fractional-bits 4
    :rounding toward-zero))
(module packed
  :header "Packed.h"
  :namespace project
  (packed-value Motion
    :storage std::uint32_t
    (field velocity project::VelocityQ12_4 :bits auto :kind fixed-point)
    (field state std::uint16_t :bits 16)))
)");

    auto const manifest{files.load()};
    auto const& value{schema_at<PackedValueSchema>(manifest, manifest.modules.size() - 1)};
    auto const& velocity{std::get<PackedFieldSchema>(value.segments.front())};
    EXPECT_EQ(velocity.type.name, "project::VelocityQ12_4");
    EXPECT_FALSE(velocity.bits.has_value());
    EXPECT_EQ(velocity.kind, PackedFieldKind::fixed_point);
}

TEST(SourceLoader, ReadsMiniFloatPackedField) {
    TemporaryManifest files;
    files.write_root(R"(
(module representations
  :header "Representations.h"
  :namespace project
  (mini-float PositionF12
    :sign-bits 1
    :exponent-bits 5
    :significand-bits 6
    :bias 15))
(module packed
  :header "Packed.h"
  :namespace project
  (packed-value Position
    :storage std::uint16_t
    (field component project::PositionF12 :bits auto :kind mini-float)
    (field state std::uint8_t :bits 4)))
)");

    auto const manifest{files.load()};
    auto const& value{schema_at<PackedValueSchema>(manifest, manifest.modules.size() - 1)};
    auto const& component{std::get<PackedFieldSchema>(value.segments.front())};
    EXPECT_EQ(component.type.name, "project::PositionF12");
    EXPECT_FALSE(component.bits.has_value());
    EXPECT_EQ(component.kind, PackedFieldKind::mini_float);
}

TEST(SourceLoader, RejectsUnknownPackedPhysicalOrdering) {
    TemporaryManifest files;
    files.write_root(R"(
(module packed
  :header "Packed.h"
  (packed-value Value
    :storage std::uint32_t
    :byte-order middle
    (field value std::uint32_t :bits 32)))
)");
    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);

    files.write_root(R"(
(module packed
  :header "Packed.h"
  (packed-value Value
    :storage std::uint32_t
    :bit-order byte-first
    (field value std::uint32_t :bits 32)))
)");
    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, ReadsStandaloneIntegerScalarDomain) {
    TemporaryManifest files;
    files.write_root(R"(
(module semantic_values
  :header "SemanticValues.h"
  :namespace project
  (integer-scalar DamageReason
    :signed false
    :minimum 0
    :maximum 10
    :bit-width auto
    :cpp-emission constants-with-names
    :cpp-type std::uint8_t
    (code Unknown :value 0)
    (code Invalid :value 15 :sentinel true)
    (relation index_into EntityTable))
  (integer-scalar EntityTable
    :signed false
    :minimum 0
    :maximum 1023
    :bit-width 10)
  (integer-scalar PayloadOffset
    :signed false
    :minimum 0
    :maximum 65535
    :bit-width 16
    (relation offset_into EntityTable :unit bytes)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.declarations.size(), 3U);
    auto const& scalar{schema_at<IntegerScalarSchema>(manifest, 0)};
    EXPECT_EQ(scalar.name, "DamageReason");
    EXPECT_FALSE(scalar.signedness);
    EXPECT_EQ(scalar.minimum_value, PackedIntegerValue{0});
    EXPECT_EQ(scalar.maximum_value, PackedIntegerValue{10});
    EXPECT_FALSE(scalar.bit_width.has_value());
    EXPECT_EQ(scalar.cpp_emission, IntegerScalarCppEmission::constants_with_names);
    ASSERT_TRUE(scalar.cpp_type.has_value());
    EXPECT_EQ(scalar.cpp_type->name, "std::uint8_t");
    ASSERT_EQ(scalar.named_codes.size(), 2U);
    EXPECT_EQ(scalar.named_codes[1].value, PackedIntegerValue{15});
    EXPECT_TRUE(scalar.named_codes[1].sentinel);
    ASSERT_TRUE(scalar.relationship.has_value());
    EXPECT_EQ(scalar.relationship->kind, SemanticRelationKind::index_into);
    EXPECT_EQ(scalar.relationship->target.name, "EntityTable");
    EXPECT_FALSE(scalar.relationship->unit.has_value());
    auto const& offset{schema_at<IntegerScalarSchema>(manifest, 0, 2)};
    ASSERT_TRUE(offset.relationship.has_value());
    EXPECT_EQ(offset.relationship->kind, SemanticRelationKind::offset_into);
    EXPECT_EQ(offset.relationship->unit, SemanticRelationUnit::bytes);
}

TEST(SourceLoader, ReadsPhysicalRepresentations) {
    TemporaryManifest files;
    files.write_root(R"(
(module semantic_values
  :header "SemanticValues.h"
  :namespace project
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 1000
    :bit-width auto
    (code Invalid :value 1023 :sentinel true)))
(module representations
  :header "Representations.h"
  :namespace project
  (linear-quantized HealthQ8
    :source project::Health
    :bits 8
    :reserved-codes 1
    :clipping clamp)
  (integer-varint HealthVarint
    :source project::Health
    :encoding unsigned)
  (fixed-point VelocityQ12_4
    :signed true
    :total-bits 16
    :fractional-bits 4
    :rounding toward-zero
    :minimum -2.5
    :maximum 3.75)
  (mini-float CompactFloat
    :sign-bits 1
    :exponent-bits 5
    :significand-bits 10
    :bias 15)
  (optional-sentinel OptionalHealth
    :source project::Health
    :sentinel Invalid)
  (optional-presence-bit PresentHealth
    :source project::Health))
)");

    auto const manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2U);
    auto const& module{std::get<NormalModuleSchema>(manifest.modules[1])};
    ASSERT_EQ(module.declarations.size(), 6U);
    auto const& representation{schema_at<LinearQuantizedSchema>(manifest, 1, 0)};
    EXPECT_EQ(representation.name, "HealthQ8");
    EXPECT_EQ(representation.source.name, "project::Health");
    EXPECT_EQ(representation.bit_width, 8U);
    EXPECT_EQ(representation.reserved_codes, 1U);
    EXPECT_EQ(representation.clipping, QuantizationClipping::clamp);
    auto const& varint{schema_at<IntegerVarintSchema>(manifest, 1, 1)};
    EXPECT_EQ(varint.name, "HealthVarint");
    EXPECT_EQ(varint.source.name, "project::Health");
    EXPECT_EQ(varint.encoding, IntegerVarintEncoding::unsigned_varint);
    auto const& fixed_point{schema_at<FixedPointSchema>(manifest, 1, 2)};
    EXPECT_EQ(fixed_point.name, "VelocityQ12_4");
    EXPECT_TRUE(fixed_point.signedness);
    EXPECT_EQ(fixed_point.total_bits, 16U);
    EXPECT_EQ(fixed_point.fractional_bits, 4U);
    EXPECT_EQ(fixed_point.rounding, FixedPointRounding::toward_zero);
    EXPECT_EQ(fixed_point.minimum_value, "-2.5");
    EXPECT_EQ(fixed_point.maximum_value, "3.75");
    auto const& mini_float{schema_at<MiniFloatSchema>(manifest, 1, 3)};
    EXPECT_EQ(mini_float.name, "CompactFloat");
    EXPECT_EQ(mini_float.sign_bits, 1U);
    EXPECT_EQ(mini_float.exponent_bits, 5U);
    EXPECT_EQ(mini_float.significand_bits, 10U);
    EXPECT_EQ(mini_float.exponent_bias, 15);
    auto const& optional{schema_at<OptionalSentinelSchema>(manifest, 1, 4)};
    EXPECT_EQ(optional.name, "OptionalHealth");
    EXPECT_EQ(optional.source.name, "project::Health");
    EXPECT_EQ(optional.sentinel, "Invalid");
    auto const& presence{schema_at<OptionalPresenceBitSchema>(manifest, 1, 5)};
    EXPECT_EQ(presence.name, "PresentHealth");
    EXPECT_EQ(presence.source.name, "project::Health");
}

TEST(SourceLoader, RejectsInvalidMiniFloatWidthsAndBias) {
    auto load_mini_float = [](std::string const& properties) {
        TemporaryManifest files;
        files.write_root("(module representations\n"
                         "  :header \"Representations.h\"\n"
                         "  (mini-float Invalid " +
                         properties + "))\n");
        return files.load();
    };

    EXPECT_THROW(load_mini_float(":sign-bits 2 :exponent-bits 5 :significand-bits 10 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 1 :significand-bits 10 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 63 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 15 :significand-bits 49 :bias 15"),
                 ManifestError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 32768"),
                 ManifestError);
}

TEST(SourceLoader, ReadsExplicitEnumBitWidth) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")))
)");

    auto const manifest{files.load()};
    auto const& schema{schema_at<EnumSchema>(manifest, 0)};
    EXPECT_EQ(schema.bit_width, 3);
    EXPECT_EQ(schema.signedness, false);
}

TEST(SourceLoader, ReadsEnumWithoutCppBackingType) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")))
)");

    auto const manifest{files.load()};
    auto const& schema{schema_at<EnumSchema>(manifest, 0)};
    EXPECT_FALSE(schema.underlying_type.has_value());
    EXPECT_EQ(schema.bit_width, 3);
    EXPECT_EQ(schema.signedness, false);
}

TEST(SourceLoader, ReadsExplicitSignedEnumDomain) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum Delta std::int8_t
    :signed true
    (value Below :value "-1")
    (value Above :value "1")))
)");

    auto const manifest{files.load()};
    auto const& schema{schema_at<EnumSchema>(manifest, 0)};
    EXPECT_EQ(schema.signedness, true);
    EXPECT_FALSE(schema.bit_width.has_value());
}

TEST(SourceLoader, ReadsNamedEnumSentinels) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    (value Ready :value "0")
    (value Invalid :value "0xff" :sentinel true)
    (value Pending :value "0xfe" :sentinel true)))
)");

    auto const manifest{files.load()};
    auto const& values{schema_at<EnumSchema>(manifest, 0).values};
    ASSERT_EQ(values.size(), 3U);
    EXPECT_FALSE(values[0].sentinel);
    EXPECT_TRUE(values[1].sentinel);
    EXPECT_TRUE(values[2].sentinel);
}

TEST(SourceLoader, RejectsEnumBitWidthOutsideNativeAnalysisRange) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    :bit-width 65
    (value Idle)))
)");

    EXPECT_THROW(static_cast<void>(files.load()), ManifestError);
}

TEST(SourceLoader, PreservesNumericAndOpaqueEnumInitializers) {
    TemporaryManifest files;
    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Zero :value 0)
    (value High :value "0x7f")))
)schema");

    auto const manifest{files.load()};
    auto const& values{schema_at<EnumSchema>(manifest, 0).values};
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].initializer, "0");
    EXPECT_EQ(values[1].initializer, "0x7f");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value -1)))
)schema");
    auto const negative_manifest{files.load()};
    auto const& negative_values{schema_at<EnumSchema>(negative_manifest, 0).values};
    EXPECT_EQ(negative_values.front().initializer, "-1");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value "static_cast<uint8>(1)")))
)schema");
    auto const opaque_manifest{files.load()};
    auto const& opaque_values{schema_at<EnumSchema>(opaque_manifest, 0).values};
    EXPECT_EQ(opaque_values.front().initializer, "static_cast<uint8>(1)");
}

TEST(SourceLoader, ReadsRecordModuleAndFixedArrays) {
    TemporaryManifest files;
    files.write_root(R"(
(module data
  :header "Data.h"
  :namespace project
  (record Position
    (member x float)
    (member y float))
  (record Trail
    :export-specifier PROJECT_API
    (member points Position :count 4
      (relation contains Position))))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.declarations.size(), 2U);
    auto const& position{schema_at<RecordSchema>(manifest, 0, 0)};
    auto const& trail{schema_at<RecordSchema>(manifest, 0, 1)};
    EXPECT_EQ(position.name, "Position");
    ASSERT_EQ(position.members.size(), 2U);
    EXPECT_EQ(position.members[0].type.name, "float");
    EXPECT_FALSE(position.members[0].count.has_value());
    EXPECT_EQ(trail.export_specifier, "PROJECT_API");
    EXPECT_EQ(trail.members[0].count, 4);
    ASSERT_TRUE(trail.members[0].relationship.has_value());
    EXPECT_EQ(trail.members[0].relationship->kind, SemanticRelationKind::contains);
    EXPECT_EQ(trail.members[0].relationship->target.name, "Position");
}

TEST(SourceLoader, ReadsRawUnionModuleAndFixedArrayAlternatives) {
    TemporaryManifest files;
    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  :namespace project
  (union Payload
    :export-specifier PROJECT_API
    (alternative identifier std::uint32_t)
    (alternative bytes std::uint8_t :count 12)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.declarations.size(), 1U);
    auto const& payload{schema_at<UnionSchema>(manifest, 0)};
    EXPECT_EQ(payload.name, "Payload");
    EXPECT_EQ(payload.export_specifier, "PROJECT_API");
    ASSERT_EQ(payload.alternatives.size(), 2U);
    EXPECT_EQ(payload.alternatives[0].name, "identifier");
    EXPECT_FALSE(payload.alternatives[0].count.has_value());
    EXPECT_EQ(payload.alternatives[1].type.name, "std::uint8_t");
    EXPECT_EQ(payload.alternatives[1].count, 12);
}

TEST(SourceLoader, RejectsInvalidRawUnionAlternatives) {
    TemporaryManifest files;
    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t :count 0)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);

    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t)
    (alternative value std::uint16_t)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);
}

TEST(SourceLoader, ReadsTaggedUnionDiscriminantAndSymbolicMappings) {
    TemporaryManifest files;
    files.write_root(R"(
(module events
  :header "Events.h"
  (enum EventKind std::uint8_t
    (value Spawn)
    (value Damage)
    (value Invalid :sentinel true)))
(module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant events::EventKind
    :export-specifier PROJECT_API
    (alternative spawn std::uint32_t :tag Spawn)
    (alternative damage std::uint16_t :count 4 :tag Damage)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<NormalModuleSchema>(manifest.modules[1])};
    ASSERT_EQ(module.declarations.size(), 1U);
    auto const& tagged{schema_at<TaggedUnionSchema>(manifest, 1)};
    EXPECT_EQ(tagged.name, "Event");
    EXPECT_EQ(tagged.discriminant.name, "events::EventKind");
    EXPECT_EQ(tagged.export_specifier, "PROJECT_API");
    ASSERT_EQ(tagged.alternatives.size(), 2U);
    EXPECT_EQ(tagged.alternatives[0].tag, "Spawn");
    EXPECT_FALSE(tagged.alternatives[0].count.has_value());
    EXPECT_EQ(tagged.alternatives[1].tag, "Damage");
    EXPECT_EQ(tagged.alternatives[1].count, 4);
}

TEST(SourceLoader, RejectsDuplicateTaggedUnionNamesAndTags) {
    TemporaryManifest files;
    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant std::uint8_t
    (alternative value std::uint32_t :tag Value)
    (alternative value std::uint16_t :tag Other)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);

    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant std::uint8_t
    (alternative first std::uint32_t :tag Value)
    (alternative second std::uint16_t :tag Value)))
)");
    EXPECT_THROW(validate_manifest(files.load()), std::invalid_argument);
}

TEST(SourceLoader, RejectsNonIntegerPackedFieldWidthWithSourceLocation) {
    TemporaryManifest files;
    files.write_root(R"(
(module packed
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
(module packed
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
(module bad
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
(module facade
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
    auto const& facade{schema_at<FacadeSchema>(manifest, 0)};
    EXPECT_TRUE(facade.reference_target);
    EXPECT_TRUE(facade.definitions_in_source);
    EXPECT_EQ(facade.methods.front().return_type.suffix, " const&");
    EXPECT_EQ(facade.methods.front().target_name, "get_value");
    EXPECT_TRUE(facade.methods.front().is_const);
}

TEST(SourceLoader, LoadsOpaqueCppBlocksAndKeepsQuotedBodiesCompatible) {
    TemporaryManifest files;
    files.write_root(
        "(module example\n"
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
        "(module facade\n"
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

    auto const& soa{std::get<NormalModuleSchema>(manifest.modules[0])};
    ASSERT_EQ(soa.settings.prelude_lines.size(), 1);
    EXPECT_EQ(soa.settings.prelude_lines[0],
              "class FForward;" + newline + "#define GENERATED_PATH \"C:\\\\generated\"");
    auto const& data{schema_at<SoaSchema>(manifest, 0)};
    ASSERT_EQ(data.functions.size(), 2);
    ASSERT_EQ(data.functions[0].body_lines.size(), 1);
    EXPECT_EQ(data.functions[0].body_lines[0],
              "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
                  "values[0] += dt;");
    EXPECT_EQ(data.functions[1].body_lines, (std::vector<std::string>{"values[0] = 0;"}));

    auto const& facade{schema_at<FacadeSchema>(manifest, 1)};
    EXPECT_EQ(facade.validation_lines,
              (std::vector<std::string>{"checkf(target != nullptr, TEXT(\"missing target\"));"}));
}

TEST(SourceLoader, AcceptsEmptyCppBodyAndRejectsWrongRawTag) {
    TemporaryManifest files;
    files.write_root(R"(
(module example
  :header "Generated.h"
  (struct FData
    (function empty void :body #cpp{}cpp#)))
)");
    auto const manifest{files.load()};
    auto const& function{schema_at<SoaSchema>(manifest, 0).functions[0]};
    EXPECT_EQ(function.body_lines, std::vector<std::string>{""});

    files.write_root(R"(
(module example
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
    :device keyboard-mouse
    (control float-range :min 50 :max 100 :step 0.5)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SettingsModuleSchema>(manifest.modules.front())};
    ASSERT_EQ(module.settings_list.size(), 1);
    EXPECT_EQ(module.settings_list.front().apply_mode, SettingApplyMode::deferred);
    EXPECT_EQ(module.settings_list.front().device, SettingDevice::keyboard_mouse);
    EXPECT_EQ(module.settings_list.front().control.kind, SettingControlKind::float_range);
    EXPECT_EQ(module.settings_list.front().control.step, 0.5);
}

TEST(SourceLoader, SettingsDeviceDefaultsToShared) {
    TemporaryManifest files;
    files.write_root(R"(
(settings-module settings
  :header "Settings.h"
  :api-name TSettingsAccess
  :state-name FSettingsState
  (category video "Video")
  (setting vsync "VSync"
    :category video
    :value-type bool
    :backend engine
    :apply deferred
    (control toggle)))
)");

    auto const manifest{files.load()};
    auto const& module{std::get<SettingsModuleSchema>(manifest.modules.front())};
    EXPECT_EQ(module.settings_list.front().device, SettingDevice::shared);
}

TEST(SourceLoader, SingleOnlyLogicalApiAndAllocatorFieldsArePreserved) {
    TemporaryManifest files;
    files.write_root(R"(
(module api :header "Api.h" :source "Api.cpp" :backend standard-library
  (struct Rows :view-name Values :const-view-name ConstValues
    :export-specifier API :using-declarations ("Value = float")
    :operations (set-num append-from) :equivalent-type ValueRow
    :single-allocation-allocator Allocator
    (member values array float)
    (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
    (function noop void :body ())
    (view-function read float :body #cpp{return $column(values)[0];}cpp#)
    (mutable-view-function write void :body #cpp{$column(values)[0] = 1;}cpp#)
    (single-allocation Owner)))
)");
    auto const manifest{files.load()};
    auto const& schema{schema_at<SoaSchema>(manifest, 0)};
    EXPECT_EQ(schema.selected_storage(), SoaStorage::single_allocation);
    EXPECT_EQ(schema.single_allocation_allocator->name, "Allocator");
    ASSERT_EQ(schema.const_view_functions.size(), 1U);
    EXPECT_TRUE(schema.const_view_functions.front().is_const);
    ASSERT_EQ(schema.mutable_view_functions.size(), 1U);
    auto const output{render_modules(lower_modules(manifest)).front().content};
    EXPECT_NE(output.find("struct API Owner"), std::string::npos);
    EXPECT_NE(output.find("Allocator::allocate"), std::string::npos);
    EXPECT_NE(output.find("return this->values()[0];"), std::string::npos);
    EXPECT_EQ(output.find("struct Rows {"), std::string::npos);
    EXPECT_NE(output.find("void noop() {"), std::string::npos);
}

TEST(SourceLoader, ReadOnlyViewFunctionRejectsMutableReceiver) {
    TemporaryManifest files;
    files.write_root(R"(
(module api :header "Api.h" :backend standard-library
  (struct Rows
    (member values array float)
    (view-function read float :const false :body #cpp{return 0;}cpp#)
    (single-allocation Owner)))
)");
    try {
        static_cast<void>(files.load());
        FAIL() << "Expected a const receiver diagnostic";
    } catch (ManifestError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("view-function must be const"));
    }
}

} // namespace
} // namespace codegen
