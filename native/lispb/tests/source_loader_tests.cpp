#include "test_utilities.h"

#include <codegen/ast/indices.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/floating_point_format.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_device.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_role.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/source_loader.h>
#include <lispb/compilation.h>
#include <lispb/indices.h>
#include <lispb/schema/child_occurrences.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_origin.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

TEST(CompilerStorage, RebuildsAfterFullResetWithoutRetainingSourceText) {
    CompilerStorage storage;
    constexpr std::string_view source{
        "(module game :header \"Game.h\" (record Value (member count std::uint32_t)))"};
    std::optional<std::size_t> populated_bytes;
    for (int iteration{}; iteration < 3; ++iteration) {
        storage.reset();
        EXPECT_EQ(storage.strings.byte_size(), 0U);
        EXPECT_EQ(storage.diagnostics.size(), 0U);

        auto const file{storage.sources.add("reset.lispb", source)};
        auto const loaded{parse_module_source(storage, file)};
        ASSERT_EQ(loaded.modules.size(), 1U);
        EXPECT_EQ(storage.sources.text(file), source);
        EXPECT_TRUE(storage.sources.include_targets(file).empty());
        auto const bytes{storage.strings.byte_size()};
        if (populated_bytes) {
            EXPECT_EQ(bytes, *populated_bytes);
        }
        populated_bytes = bytes;
    }
}

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
        context.storage.sources.refresh();
    }

    auto path(std::string const& name) const -> std::filesystem::path { return directory_ / name; }

    void write_root(std::string const& modules) const {
        write("types.lispb", "");
        write("modules.lispb", modules);
    }

    auto load() const -> Manifest {
        std::filesystem::path const modules[]{path("modules.lispb")};
        return load_sources(context.storage, path("types.lispb"), modules);
    }
    auto render(Manifest manifest) const -> std::vector<GeneratedFile> {
        return render_test_manifest(context, std::move(manifest));
    }
    auto lower(Manifest manifest) const -> std::vector<ModuleId> {
        return lower_test_manifest(context, std::move(manifest));
    }
    auto graph() const -> lispb::schema::TypeGraph {
        auto manifest{load()};
        return lispb::schema::resolve_type_graph(manifest);
    }
    auto store() const -> Declarations& { return context.storage.declarations; }

    auto text(lispb::StringId id) const -> std::string_view {
        return context.storage.strings.view(id);
    }
    auto text(std::string_view value) const -> std::string_view { return value; }
    template <typename Id>
    auto text(std::optional<Id> id) const -> std::optional<std::string_view> {
        return id.transform([&](auto value) { return text(value); });
    }
    mutable CompilerTestContext context;
  private:
    std::filesystem::path directory_;
};

auto schema_at(Manifest const& manifest,
               std::size_t const module_index,
               std::size_t const declaration_index = 0) -> DeclarationHandle {
    auto const& store{manifest.declarations};
    auto const module{manifest.modules.at(module_index)};
    return store.items(store.normal_modules.declarations[module])[declaration_index];
}

TEST(SourceLoader, RecordValueSemanticsSurviveParsingGraphAndLowering) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& record_members{store.child_occurrences};
    auto& records{store.records};
    files.write_root(R"((module records :header "Records.h"
      (record Value :comparison three-way :comparison-noexcept true
        (member absent int)
        (member zero int :initializer "")
        (member explicit_value int :initializer "7")
        (function read int :const true :noexcept true :constexpr true :nodiscard true
          :body ("return explicit_value;"))
        (function validate bool :const true :noexcept true)
    (projections (cpp)))))");
    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{records.index(schema)};
    auto const members{store.items(records.members[schema_row])};
    EXPECT_FALSE(record_members.initializer[members[0]].has_value());
    EXPECT_EQ(files.text(record_members.initializer[members[1]]), "");
    EXPECT_EQ(files.text(record_members.initializer[members[2]]), "7");
    EXPECT_EQ(records.comparison[schema_row], RecordComparison::three_way);
    ASSERT_EQ(records.functions[schema_row].size(), 2U);
    EXPECT_TRUE(store.functions.is_constexpr[store.items(records.functions[schema_row])[0]]);
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const& record{graph.definition(*find_test_type(graph, "records", "Value"))};
    auto const& definitions{graph.definitions()};
    auto const& semantic_members{definitions.children};
    auto const resolved_members{definitions.records.members_of(record)};
    EXPECT_FALSE(
        semantic_members.initializer[semantic_members.index(resolved_members[0])].has_value());
    EXPECT_EQ(files.text(semantic_members.initializer[semantic_members.index(resolved_members[1])]),
              "");
    EXPECT_EQ(files.text(semantic_members.initializer[semantic_members.index(resolved_members[2])]),
              "7");
    auto const output{files.render(manifest).front().content};
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

struct InvalidSourceCase {
    char const* name;
    char const* source;
};

class SourceLoaderInvalidRecord : public testing::TestWithParam<InvalidSourceCase> {};

TEST_P(SourceLoaderInvalidRecord, RejectsInvalidDeclaration) {
    TemporaryManifest files;
    files.write_root(std::string{"(module records :header \"Records.h\" "} + GetParam().source +
                     ")");
    EXPECT_THROW(static_cast<void>(files.lower(files.load())), std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    SourceLoader,
    SourceLoaderInvalidRecord,
    testing::Values(InvalidSourceCase{"StatementInitializer",
                                      R"((record Value (member x int :initializer "0; int injected")
    (projections (cpp))))"},
                    InvalidSourceCase{"NoexceptWithoutComparison",
                                      R"((record Value :comparison-noexcept true (member x int)
    (projections (cpp))))"},
                    InvalidSourceCase{"MemberMethodNameCollision",
                                      R"((record Value (member x int) (function x int)
    (projections (cpp))))"},
                    InvalidSourceCase{"StaticConstMethod",
                                      R"((record Value (function read int :static true :const true)
    (projections (cpp))))"},
                    InvalidSourceCase{"ConstexprWithoutBody",
                                      R"((record Value (function read int :constexpr true)
    (projections (cpp))))"},
                    InvalidSourceCase{"DuplicateMethod",
                                      R"((record Value (function read int) (function read int)
    (projections (cpp))))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, RecordMethodDeclarationsForwardDeclareLaterAndMutualTypes) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    files.write_root(R"((module values :header "Values.h"
      (record A
        (function make B)
        (function accept void (parameter value B))
        (function ref auto :trailing-return-type (type-ref B :suffix " const&"))
        (function pointer (type-ref B :suffix "*") :body ("return nullptr;"))
    (projections (cpp)))
      (record B (function make A)
    (projections (cpp)))))");
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const a{*find_test_type(graph, "values", "A")};
    auto const b{*find_test_type(graph, "values", "B")};
    EXPECT_NE(std::ranges::find(graph.dependencies_of(a), b), graph.dependencies_of(a).end());
    EXPECT_TRUE(std::ranges::any_of(graph.type_uses(), [&](auto const& use) {
        return graph.occurrences().type[graph.occurrences().index(graph.target(use))] == b &&
               graph.kind(use) == TypeReferenceKind::function_declaration &&
               graph.occurrences().physical_form[graph.occurrences().index(graph.target(use))] ==
                   PhysicalTypeForm::value;
    }));
    auto editable{store.items(normal_modules.declarations[manifest.modules.front()]).front()};
    std::vector<std::pair<std::string, TypeReferenceKind>> const_uses;
    visit_type_references(store,
                          std::as_const(editable),
                          [&](TypeReferenceRole const& role, TypeRefId, TypeReferenceKind kind) {
                              const_uses.emplace_back(
                                  type_reference_role_text(files.context.storage.strings, role),
                                  kind);
                          });
    std::vector<std::pair<std::string, TypeReferenceKind>> mutable_uses;
    editable = rewrite_type_references(
        store, editable, [&](TypeReferenceRole const& role, TypeRefId id, TypeReferenceKind kind) {
            mutable_uses.emplace_back(type_reference_role_text(files.context.storage.strings, role),
                                      kind);
            auto const reference{type_refs.clone(id)};
            auto const reference_row{type_refs.index(reference)};
            if (files.text(type_refs.name[reference_row]) != "B") {
                return id;
            }
            type_refs.name[reference_row] = files.context.storage.strings.intern("Renamed");
            return reference;
        });
    EXPECT_EQ(mutable_uses, const_uses);
    EXPECT_EQ(files.text(type_refs.name[store.functions.return_type[store.items(
                  store.records.functions[editable])[0]]]),
              "Renamed");
    auto const output{files.render(manifest).front().content};
    EXPECT_LT(output.find("struct B;"), output.find("struct A {"));
    EXPECT_LT(output.find("struct A;"), output.find("struct B {"));
    EXPECT_NE(output.find("struct A {"), std::string::npos);
    EXPECT_NE(output.find("struct B {"), std::string::npos);
    EXPECT_NE(output.find("auto ref() -> B const&;"), std::string::npos);
    EXPECT_EQ(
        files.text(declaration_name(
            store, store.items(normal_modules.declarations[manifest.modules.front()]).front())),
        "A");
}

TEST(SourceLoader, RecordMethodSignaturesOrderScalarWrappersEnumsAndHeaderBodyValues) {
    TemporaryManifest files;
    auto& store{files.store()};
    files.write_root(R"((module values :header "Values.h"
      (record Consumer
        (function tag Tag)
        (function count (type-ref Count :suffix " const&"))
        (function make auto :trailing-return-type Provider :body ("return {};"))
        (function accept void (parameter value Argument) :body ("(void)value;"))
        (function self Consumer :body ("return *this;"))
    (projections (cpp)))
      (record Provider
    (projections (cpp)))
      (record Argument
    (projections (cpp)))
      (enum Tag std::uint8_t (value Value)
    (projections (cpp)))
      (integer-scalar Count :signed false :minimum 0 :maximum 255 :bit-width 8
    (projections (cpp :storage std::uint8_t)))))");
    auto manifest{files.load()};
    auto const output{files.render(manifest).front().content};
    auto const consumer{output.find("struct Consumer {")};
    for (auto const definition :
         {"struct Provider {", "struct Argument {", "enum class Tag", "struct Count"}) {
        EXPECT_LT(output.find(definition), consumer) << definition;
    }
    EXPECT_EQ(output.find("struct Tag;"), std::string::npos);
    EXPECT_NE(output.find("struct Count;"), std::string::npos);
    EXPECT_EQ(
        files.text(declaration_name(
            store,
            store.items(store.normal_modules.declarations[manifest.modules.front()]).front())),
        "Consumer");
}

TEST(SourceLoader, RecordMethodDeclarationsForwardDeclareCrossModuleTypes) {
    TemporaryManifest files;
    files.write_root(R"((module consumers :header "Consumers.h" :namespace consumers
      (record Consumer
        (function make providers::Provider)
        (function accept void (parameter value (type-ref providers::Provider :suffix " const&")))
        (function pointer auto :trailing-return-type (type-ref providers::Provider :suffix "*"))
    (projections (cpp))))
      (module providers :header "generated/Provider.h" :header-include "api/Provider.h"
        :namespace providers (record Provider
    (projections (cpp)))))");
    auto const output{files.render(files.load()).front().content};
    EXPECT_EQ(output.find("#include \"api/Provider.h\""), std::string::npos);
    EXPECT_LT(output.find("namespace providers {"), output.find("struct Provider;"));
    EXPECT_LT(output.find("struct Provider;"), output.find("namespace consumers {"));
    EXPECT_EQ(output.find("struct Provider;", output.find("struct Provider;") + 1),
              std::string::npos);
    EXPECT_NE(output.find("providers::Provider make();"), std::string::npos);
    EXPECT_NE(output.find("providers::Provider const& value"), std::string::npos);
    EXPECT_NE(output.find("-> providers::Provider*;"), std::string::npos);
}

TEST(SourceLoader, RecordMethodDeclarationsForwardDeclareMutualCrossModuleTypes) {
    TemporaryManifest files;
    files.write_root(R"((module first :header "First.h" :namespace first
      (record A
        (function make second::B)
        (function accept void (parameter value second::B))
        (function pointer (type-ref second::B :suffix "*"))
        (function ref (type-ref second::B :suffix " const&"))
        (function move auto :trailing-return-type (type-ref second::B :suffix "&&"))
    (projections (cpp))))
      (module second :header "Second.h" :namespace second
        (record B
          (function make auto :trailing-return-type first::A)
          (function pointer (type-ref first::A :suffix "*"))
          (function ref auto :trailing-return-type (type-ref first::A :suffix " const&"))
    (projections (cpp)))))");
    auto const output{files.render(files.load())};
    ASSERT_EQ(output.size(), 2U);
    EXPECT_EQ(output[0].content.find("#include \"Second.h\""), std::string::npos);
    EXPECT_EQ(output[1].content.find("#include \"First.h\""), std::string::npos);
    EXPECT_LT(output[0].content.find("namespace second {"), output[0].content.find("struct B;"));
    EXPECT_LT(output[0].content.find("struct B;"), output[0].content.find("namespace first {"));
    EXPECT_LT(output[1].content.find("namespace first {"), output[1].content.find("struct A;"));
    EXPECT_LT(output[1].content.find("struct A;"), output[1].content.find("namespace second {"));
}

TEST(SourceLoader, RecordParameterDefaultsRequireCompleteTypesWithoutChangingOtherSignatures) {
    TemporaryManifest files;
    auto& store{files.store()};
    for (auto const& [parameter_type, default_value] :
         {std::pair{"B", "{}"},
          std::pair{"(type-ref B :suffix \" const&\")", "{}"},
          std::pair{"(type-ref B :suffix \"*\")", "nullptr"}}) {
        files.write_root(std::string{R"((module values :header "Values.h"
          (record A (projections (cpp))
            (function consume Result (parameter value )"} +
                         parameter_type + " :default \"" + default_value + R"("))
            (function inspect auto :trailing-return-type Result
              (parameter existing (type-ref C :suffix " const&"))
              (parameter value )" +
                         parameter_type + " :default \"" + default_value + R"(")))
          (record B
    (projections (cpp)))
          (record Result
    (projections (cpp)))
          (record C
    (projections (cpp)))))");
        auto manifest{files.load()};
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const b{*find_test_type(graph, "values", "B")};
        auto const result{*find_test_type(graph, "values", "Result")};
        auto const c{*find_test_type(graph, "values", "C")};
        for (auto const& use : graph.type_uses()) {
            if (graph.occurrences().type[graph.occurrences().index(graph.target(use))] == b) {
                EXPECT_EQ(graph.kind(use), TypeReferenceKind::complete_definition);
            } else if (graph.occurrences().type[graph.occurrences().index(graph.target(use))] ==
                           result ||
                       graph.occurrences().type[graph.occurrences().index(graph.target(use))] ==
                           c) {
                EXPECT_EQ(graph.kind(use), TypeReferenceKind::function_declaration);
            }
        }
        auto declaration{
            store.items(store.normal_modules.declarations[manifest.modules.front()]).front()};
        std::vector<TypeReferenceKind> const_kinds;
        visit_type_references(store,
                              std::as_const(declaration),
                              [&](TypeReferenceRole const&, TypeRefId, TypeReferenceKind kind) {
                                  const_kinds.push_back(kind);
                              });
        std::vector<TypeReferenceKind> mutable_kinds;
        declaration = rewrite_type_references(
            store,
            declaration,
            [&](TypeReferenceRole const&, TypeRefId id, TypeReferenceKind kind) {
                mutable_kinds.push_back(kind);
                return id;
            });
        EXPECT_EQ(mutable_kinds, const_kinds);
        EXPECT_EQ(std::ranges::count(mutable_kinds, TypeReferenceKind::complete_definition), 2);
        auto const output{files.render(manifest).front().content};
        EXPECT_LT(output.find("struct B {"), output.find("struct A {"));
        EXPECT_EQ(output.find("struct B;"), std::string::npos);
        EXPECT_LT(output.find("struct Result;"), output.find("struct A {"));
        EXPECT_LT(output.find("struct C;"), output.find("struct A {"));
        EXPECT_NE(output.find("struct Result {"), std::string::npos);
        EXPECT_NE(output.find("struct C {"), std::string::npos);
    }
}

TEST(SourceLoader, RecordParameterDefaultsRetainCrossModuleIncludes) {
    TemporaryManifest files;
    for (auto const& [parameter_type, default_value] :
         {std::pair{"second::B", "{}"},
          std::pair{"(type-ref second::B :suffix \" const&\")", "{}"},
          std::pair{"(type-ref second::B :suffix \"*\")", "nullptr"}}) {
        files.write_root(std::string{R"((module first :header "First.h" :namespace first
          (record A (projections (cpp)) (function consume void (parameter value )"} +
                         parameter_type + " :default \"" + default_value + R"("))))
          (module second :header "generated/Second.h" :header-include "api/Second.h"
            :namespace second (record B
    (projections (cpp)))))");
        auto const output{files.render(files.load()).front().content};
        EXPECT_NE(output.find("#include \"api/Second.h\""), std::string::npos);
        EXPECT_EQ(output.find("struct B;"), std::string::npos);
    }
}

TEST(SourceLoader, RecordParameterDefaultsRejectCompleteDefinitionCycles) {
    TemporaryManifest files;
    for (auto const& schema : {
             R"((module values :header "Values.h"
               (record A (function inspect void
                 (parameter value (type-ref B :suffix " const&") :default "{}"))
    (projections (cpp)))
               (record B (function inspect void
                 (parameter value (type-ref A :suffix " const&") :default "{}"))
    (projections (cpp)))))",
             R"((module first :header "First.h" :namespace first
               (record A (function consume void (parameter value second::B :default "{}"))
    (projections (cpp))))
               (module second :header "Second.h" :namespace second
                 (record B (function consume void (parameter value first::A :default "{}"))
    (projections (cpp)))))"}) {
        files.write_root(schema);
        try {
            static_cast<void>(files.graph());
            FAIL();
        } catch (std::invalid_argument const& error) {
            EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"))
                << error.what();
        }
    }
}

TEST(SourceLoader, RecordMethodCrossModuleCompleteTypesRetainIncludes) {
    TemporaryManifest files;
    files.write_root(R"((module consumers :header "Consumers.h" :namespace consumers
      (record Consumer
        (function make providers::Provider :body ("return {};"))
        (function accept void (parameter value providers::Provider) :body ("(void)value;"))
        (function tag providers::Tag)
        (function count (type-ref providers::Count :suffix " const&"))
    (projections (cpp))))
      (module providers :header "generated/Provider.h" :header-include "api/Provider.h"
        :namespace providers (record Provider
    (projections (cpp))))
      (module tags :header "Tags.h" :namespace providers
        (enum Tag std::uint8_t (value Value)
    (projections (cpp))))
      (module counts :header "Counts.h" :namespace providers
        (integer-scalar Count :signed false :minimum 0 :maximum 255 :bit-width 8
    (projections (cpp :storage std::uint8_t)))))");
    auto const output{files.render(files.load()).front().content};
    for (auto const header : {"api/Provider.h", "Tags.h"}) {
        EXPECT_NE(output.find(std::string{"#include \""} + header + "\""), std::string::npos);
    }
    for (auto const type : {"Provider", "Tag"}) {
        EXPECT_EQ(output.find(std::string{"struct "} + type + ";"), std::string::npos);
    }
    EXPECT_EQ(output.find("#include \"Counts.h\""), std::string::npos);
    EXPECT_NE(output.find("struct Count;"), std::string::npos);
}

TEST(SourceLoader, RecordMethodCrossModuleForwardsRespectPhysicalOwnersAndExternalHeaders) {
    TemporaryManifest files;
    files.write_root(R"((module consumers :header "Consumers.h" :namespace consumers
      (record Consumer
        (function rows providers::Rows)
        (function value @external)
        (function registered @registered)
        (function global Global)
    (projections (cpp))))
      (module providers :header "Providers.h" :namespace providers
        (struct Rows (member xs array float))
        (record Registered
    (projections (cpp))))
      (module global :header "Global.h" (record Global
    (projections (cpp)))))");
    files.write("types.lispb", R"(
      (type external :spelling "external::Value" :header "External.h")
      (type registered :spelling "providers::Registered" :header "Providers.h"))");
    auto const output{files.render(files.load()).front().content};
    EXPECT_NE(output.find("struct Rows;"), std::string::npos);
    EXPECT_NE(output.find("providers::Rows rows();"), std::string::npos);
    EXPECT_NE(output.find("struct Global;"), std::string::npos);
    EXPECT_EQ(output.find("#include \"Global.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"External.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Providers.h\""), std::string::npos);
    EXPECT_EQ(output.find("struct Value;"), std::string::npos);
    EXPECT_EQ(output.find("struct Registered;"), std::string::npos);
}

TEST(SourceLoader, RecordMethodSignaturesRejectLayoutOnlyTypesAndCompleteDefinitionCycles) {
    TemporaryManifest files;
    for (auto const signature :
         {"(function make Layout)",
          "(function make (type-ref Layout :suffix \"*\"))",
          "(function make auto :trailing-return-type (type-ref Layout :suffix \"&\"))",
          "(function accept void (parameter value Layout))"}) {
        files.write_root(std::string{R"((module values :header "Values.h"
          (record Consumer (projections (cpp)) )"} +
                         signature + R"()
          (struct Layout :layout-only true (member xs array float))))");
        try {
            static_cast<void>(files.graph());
            FAIL() << signature;
        } catch (std::invalid_argument const& error) {
            EXPECT_TRUE(std::string{error.what()}.contains("has no C++ owner")) << error.what();
        }
    }
    files.write_root(R"((module consumers :header "Consumers.h"
      (record Consumer (function make (type-ref providers::Layout :suffix "*"))
    (projections (cpp))))
      (module providers :header "Providers.h" :namespace providers
        (struct Layout :layout-only true (member xs array float))))");
    try {
        static_cast<void>(files.graph());
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("has no C++ owner")) << error.what();
    }
    files.write_root(R"((module values :header "Values.h"
      (record A (function make B :body ("return {};"))
    (projections (cpp)))
      (record B (function make A :body ("return {};"))
    (projections (cpp)))))");
    try {
        static_cast<void>(files.graph());
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"));
        EXPECT_TRUE(std::string{error.what()}.contains("A -> B -> A"));
    }
    files.write_root(R"((module first :header "First.h" :namespace first
      (record A (function make second::B :body ("return {};"))
    (projections (cpp))))
      (module second :header "Second.h" :namespace second
        (record B (member a first::A)
    (projections (cpp)))))");
    try {
        static_cast<void>(files.graph());
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"));
        EXPECT_TRUE(std::string{error.what()}.contains("first::A -> second::B -> first::A"));
    }
}

class SourceLoaderInvalidDeclaration : public testing::TestWithParam<InvalidSourceCase> {};

TEST_P(SourceLoaderInvalidDeclaration, RejectsInvalidSyntax) {
    TemporaryManifest files;
    files.write_root(std::string{R"((module invalid :header "Invalid.h" )"} + GetParam().source +
                     ")");
    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
}

INSTANTIATE_TEST_SUITE_P(
    RemovedSoaSyntax,
    SourceLoaderInvalidDeclaration,
    testing::Values(
        InvalidSourceCase{"VectorStorage",
                          "(struct Rows :storage vector (member values array float))"},
        InvalidSourceCase{"BothStorage", "(struct Rows :storage both (member values array float))"},
        InvalidSourceCase{"SingleAllocationStorage",
                          "(struct Rows :storage single-allocation (member values array float))"},
        InvalidSourceCase{"VectorComponents",
                          "(struct Rows :vector-components (values) (member values array float))"},
        InvalidSourceCase{"SingleAllocationOwner",
                          "(struct Rows (member values array float) (single-allocation Owner))"},
        InvalidSourceCase{"FixedOwner",
                          "(struct Rows (member values array float) (fixed FixedRows))"},
        InvalidSourceCase{"NestedSchema",
                          "(struct Rows (member child nested Child :nested-schema Child))"},
        InvalidSourceCase{
            "VectorSoa",
            "(vector-soa Rows :value-type float :components (xs ys) :equivalent-type Point)"},
        InvalidSourceCase{
            "SingleAllocationAllocator",
            "(struct Rows :single-allocation-allocator Allocator (member ids array int))"},
        InvalidSourceCase{"SingleAllocationVariant",
                          "(struct Rows (member ids array int) (single-allocation Owner (variant "
                          "Pool Allocator)))"},
        InvalidSourceCase{"ViewName", "(struct Rows :view-name Values (member ids array int))"},
        InvalidSourceCase{"ConstViewName",
                          "(struct Rows :const-view-name ConstValues (member ids array int))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, ScalarProjectionKeepsSemanticIdentityAndDependencies) {
    TemporaryManifest files;
    files.write_root(
        R"((module vitals :header "Vitals.h" :namespace fixture
      (record Vessel (member health Health)
    (projections (cpp)))
      (struct Fleet (member healths array Health))
      (record PackedHealth :layout packed :bits 16 (member health @health :bits auto)
    (projections (cpp :storage std::uint16_t)))
      (integer-scalar Health :signed false :minimum 0 :maximum 65534 :bit-width 16
        (code Invalid :value 65535 :sentinel true)
        (relation references Vessel)
    (projections (cpp :storage @native_uint16))))
      (module consumers :header "Consumers.h" :namespace fixture
        (record Other (member health @health)
    (projections (cpp)))))");
    files.write("types.lispb", R"(
      (type native_uint16 :spelling "std::uint16_t" :header "cstdint" :pass-by value)
      (type health :spelling "fixture::Health" :header "Vitals.h" :pass-by value))");
    auto manifest{files.load()};
    auto const& scalar{schema_at(manifest, 0, 3)};
    EXPECT_TRUE(files.store()
                    .integer_scalars.cpp_projection[files.store().integer_scalars.index(scalar)]
                    .has_value());
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const health{*find_test_type(graph, "vitals", "Health")};
    auto const& semantic{graph.definition(health)};
    EXPECT_EQ(graph.definitions()
                  .integer_scalars.bit_width[graph.definitions().integer_scalars.index(semantic)],
              16);
    EXPECT_EQ(
        graph.definitions()
            .integer_scalars.maximum_value[graph.definitions().integer_scalars.index(semantic)],
        PackedIntegerValue{65534});
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions()
            .integer_scalars
            .cpp_representation[graph.definitions().integer_scalars.index(semantic)]));
    EXPECT_EQ(files.text(
                  graph.cpp_types().spelling(graph.occurrences().cpp_type[graph.occurrences().index(
                      graph.definitions().integer_scalars.cpp_representation
                          [graph.definitions().integer_scalars.index(semantic)])])),
              "std::uint16_t");
    ASSERT_TRUE(lispb::schema::is_valid(
        graph.definitions()
            .integer_scalars.relationship[graph.definitions().integer_scalars.index(semantic)]));
    EXPECT_EQ(graph.definitions()
                  .integer_scalars.named_codes[graph.definitions().integer_scalars.index(semantic)]
                  .size(),
              1);
    EXPECT_EQ(graph.find_registered(graph.strings().find("health")), health);
    auto const vessel{*find_test_type(graph, "vitals", "Vessel")};
    auto const& record{graph.definition(vessel)};
    EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions().children.semantic_type[graph.definitions().children.index(
                      graph.definitions().records.members_of(record)[0])])],
              health);
    auto const packed{*find_test_type(graph, "vitals", "PackedHealth")};
    auto const& field{graph.definitions().records.members_of(graph.definition(packed))[0]};
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions().children.semantic_type[graph.definitions().children.index(field)])],
        health);
    for (auto const consumer : {vessel,
                                packed,
                                *find_test_type(graph, "vitals", "Fleet"),
                                *find_test_type(graph, "consumers", "Other")}) {
        EXPECT_NE(std::ranges::find(graph.dependencies_of(consumer), health),
                  graph.dependencies_of(consumer).end());
        EXPECT_NE(std::ranges::find(graph.users_of(health), consumer),
                  graph.users_of(health).end());
    }
    auto const generated{files.render(manifest)};
    auto const& header{generated.front().content};
    EXPECT_NE(header.find("struct Health"), std::string::npos);
    EXPECT_NE(header.find("#include <cstdint>"), std::string::npos);
    EXPECT_LT(header.find("struct Health"), header.find("struct Vessel"));
    EXPECT_LT(header.find("struct Health"), header.find("struct Fleet"));
    EXPECT_EQ(header.find("Health_Invalid"), std::string::npos);
}

TEST(SourceLoader, ScalarProjectionControlsOwningRepresentation) {
    for (auto const projected : {false, true}) {
        TemporaryManifest files;
        auto const projection{projected ? "(projections (cpp))" : ""};
        files.write_root(
            "(module reasons :header \"Reasons.h\""
            " (integer-scalar Context :minimum 0 :maximum 10 (relation references Reason))"
            " (integer-scalar Reason :minimum 0 :maximum 10 :bit-width 8"
            " (code Unknown :value 0) (code Invalid :value 255 :sentinel true)"
            " (relation references Context) " +
            std::string{projection} + "))");
        auto const manifest{files.load()};
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const reason{*find_test_type(graph, "reasons", "Reason")};
        auto const context{*find_test_type(graph, "reasons", "Context")};
        auto const& scalars{graph.definitions().integer_scalars};
        auto const row{scalars.index(graph.definition(reason))};
        EXPECT_FALSE(lispb::schema::is_valid(scalars.cpp_representation[row]));
        EXPECT_EQ(graph.bit_layout(reason).storage_bits, 8U);
        EXPECT_EQ(
            files.store().integer_scalars.cpp_projection[schema_at(manifest, 0, 1)].has_value(),
            projected);
        EXPECT_EQ(scalars.bit_width[row], 8U);
        EXPECT_EQ(scalars.named_codes[row].size(), 2U);
        EXPECT_FALSE(lispb::schema::is_valid(
            scalars.cpp_representation[scalars.index(graph.definition(context))]));
        EXPECT_NE(std::ranges::find(graph.dependencies_of(reason), context),
                  graph.dependencies_of(reason).end());
        EXPECT_NE(std::ranges::find(graph.dependencies_of(context), reason),
                  graph.dependencies_of(context).end());
        auto const generated{files.render(manifest)};
        if (projected) {
            ASSERT_FALSE(generated.empty());
            EXPECT_TRUE(generated.front().content.contains("struct Reason"));
            EXPECT_FALSE(generated.front().content.contains("using Reason ="));
        }
    }
}

TEST(SourceLoader, ScalarProjectionInfersStorageAndSupportsExplicitWidth) {
    for (auto const* representation : {"auto", "std::uint32_t", "std::uint64_t"}) {
        TemporaryManifest files;
        files.write_root("(module values :header \"Values.h\""
                         " (integer-scalar Value :bit-width 24"
                         " (projections (cpp :storage " +
                         std::string{representation} + "))))");
        auto const header{files.render(files.load()).front().content};
        auto const expected{std::string_view{representation} == "std::uint64_t" ? "std::uint64_t"
                                                                                : "std::uint32_t"};
        EXPECT_TRUE(header.contains("using storage_type = " + std::string{expected}));
        EXPECT_TRUE(header.contains("raw_value()"));
        EXPECT_TRUE(header.contains("from_raw("));
        EXPECT_FALSE(header.contains("using Value ="));
    }
}

TEST(SourceLoader, PackedDefaultsAreValidatedWithoutChangingImplicitSentinels) {
    TemporaryManifest files;
    auto& store{files.store()};
    files.write_root(R"((module orders :header "Orders.h"
      (record Order :layout packed :bits 8
        (member task @Order_taskEncoding :bits 1 :default 1)
        (member target @Order_targetEncoding :bits 1 :default 0)
    (projections (cpp :storage std::uint8_t)))
      (record Partial :layout packed :bits 8
        (member task @Partial_taskEncoding :bits 1 :default 1)
        (member target @Partial_targetEncoding :bits 1)
    (projections (cpp :storage std::uint8_t)))
      (record Implicit :layout packed :bits 8
        (member task @Implicit_taskEncoding :bits 1)
    (projections (cpp :storage std::uint8_t)))
      (record Invalid :layout packed :bits 8 :invalid-value 255
        (member task @Invalid_taskEncoding :bits 1)
    (projections (cpp :storage std::uint8_t)))
      (integer-scalar Order_taskEncoding :signed false :bit-width 1)
      (integer-scalar Order_targetEncoding :signed false :bit-width 1)
      (integer-scalar Partial_taskEncoding :signed false :bit-width 1)
      (integer-scalar Partial_targetEncoding :signed false :bit-width 1)
      (integer-scalar Implicit_taskEncoding :signed false :bit-width 1)
      (integer-scalar Invalid_taskEncoding :signed false :bit-width 1)))");
    auto manifest{files.load()};
    EXPECT_EQ(store.child_occurrences
                  .default_value[store.items(store.records.members[schema_at(manifest, 0)])[0]],
              PackedIntegerValue{1});
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto default_raw = [&](char const* name) {
        return graph.bit_layout(*find_test_type(graph, "orders", name)).default_raw_value;
    };
    EXPECT_EQ(default_raw("Order"), 1);
    EXPECT_EQ(default_raw("Partial"), std::nullopt);
    EXPECT_EQ(default_raw("Implicit"), 0);
    EXPECT_EQ(default_raw("Invalid"), 255);
    auto const header{files.render(manifest).front().content};
    EXPECT_NE(header.find("Partial() noexcept = delete"), std::string::npos);
    EXPECT_NE(header.find("return std::bit_cast<Partial>(raw)"), std::string::npos);
    EXPECT_EQ(header.find("RawTag"), std::string::npos);
}

TEST(SourceLoader, IncludedExternalScalarsRetainExternalIdentityAndEnrichPlainReferences) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"
      (record Values (member x double) (member y int8) (member z std::int8_t) (member w @signed_byte)
    (projections (cpp)))))");
    files.write("types.lispb",
                R"((include "common/types.lispb") (include "common/./scalars.lispb"))");
    files.write("common/types.lispb", R"((include "scalars.lispb"))");
    files.write("common/scalars.lispb", R"(
      (type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))");
    auto const registry{load_type_registry(files.context.storage, files.path("types.lispb"))};
    ASSERT_EQ(registry.sources.size(), 3);
    EXPECT_EQ(files.context.storage.syntax
                  .span(registry.declarations.at(files.context.storage.strings.find("signed_byte")))
                  .file,
              registry.sources[2]);
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const registered{*graph.find_registered(graph.strings().find("signed_byte"))};
    EXPECT_EQ(graph.origin(registered), lispb::schema::TypeOrigin::registered_external);
    auto const& record{graph.definition(*find_test_type(graph, "example", "Values"))};
    auto const& floating{graph.definition(graph.occurrences().type[graph.occurrences().index(
        graph.definitions().children.semantic_type[graph.definitions().children.index(
            graph.definitions().records.members_of(record)[0])])])};
    EXPECT_EQ(graph.definitions()
                  .externals.floating_semantics[graph.definitions().externals.index(floating)],
              FloatingPointFormat::ieee754_binary64);
    for (std::size_t index{1}; index < graph.definitions().records.members_of(record).size();
         ++index) {
        auto const& node{graph.occurrences().type[graph.occurrences().index(
            graph.definitions().children.semantic_type[graph.definitions().children.index(
                graph.definitions().records.members_of(record)[index])])]};
        EXPECT_TRUE(
            ((graph.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external));
        auto const scalar{lispb::schema::integer_domain(
            graph,
            graph.occurrences().type[graph.occurrences().index(
                graph.definitions().children.semantic_type[graph.definitions().children.index(
                    graph.definitions().records.members_of(record)[index])])])};
        ASSERT_TRUE(scalar.has_value());
        EXPECT_TRUE(
            graph.definitions()
                .integer_scalars.signedness[graph.definitions().integer_scalars.index(*scalar)]);
        EXPECT_EQ(
            graph.definitions()
                .integer_scalars.bit_width[graph.definitions().integer_scalars.index(*scalar)],
            8);
        EXPECT_EQ(
            graph.definitions()
                .integer_scalars.minimum_value[graph.definitions().integer_scalars.index(*scalar)],
            PackedIntegerValue{-128});
        EXPECT_EQ(
            graph.definitions()
                .integer_scalars.maximum_value[graph.definitions().integer_scalars.index(*scalar)],
            PackedIntegerValue{127});
    }
    std::filesystem::path const modules[]{files.path("modules.lispb")};
    lispb::Compilation compiled;
    lispb::schema::TypeGraph compile_graph{files.store(), files.context.storage.cpp_types};
    compile_schema(files.context.storage,
                   files.context.ir,
                   manifest,
                   compile_graph,
                   compiled,
                   files.path("types.lispb"),
                   modules,
                   true);
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
      (record Packed :layout packed :bits 32
        (member index @index :bits auto)
        (member subset @Packed_subsetEncoding :bits 24)
    (projections (cpp :storage uint32)))
      (integer-scalar Packed_subsetEncoding :signed false :bit-width 24)))");
    files.write("types.lispb", R"(
      (type index :spelling "NativeIndex" (integer :signed false :bit-width 8 :maximum 254
        (code invalid :value 255 :sentinel true)))
      (type uint32 :spelling "uint32" (integer :signed false :bit-width 32)))");
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const source{*graph.find_registered(graph.strings().find("index"))};
    EXPECT_EQ(graph.users_of(source).size(), 5);
    auto const packed{*find_test_type(graph, "example", "Packed")};
    auto const& layout{graph.bit_layout(packed)};
    ASSERT_EQ(layout.placements.size(), 2U);
    EXPECT_EQ(layout.placements[0].bits, 8U);
    EXPECT_EQ(layout.placements[1].bits, 24U);
    auto const output{files.render(manifest)};
    ASSERT_EQ(output.size(), 1);
    EXPECT_EQ(output[0].content.find("struct NativeIndex"), std::string::npos);
    EXPECT_NE(output[0].content.find("std::uint8_t"), std::string::npos);
}

TEST(SourceLoader, PackedOccurrenceCannotTruncateReusableEncoding) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Packed.h"
      (integer-scalar Index :bit-width 32)
      (record Packed :layout packed :bits 64
        (member index @Index :bits 24)
        (projections (cpp)))))");
    EXPECT_THROW(files.graph(), std::exception);
}

class SourceLoaderInvalidExternalType : public testing::TestWithParam<InvalidSourceCase> {};

TEST_P(SourceLoaderInvalidExternalType, RejectsInvalidDomainOrInclude) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"))");
    files.write("types.lispb", GetParam().source);
    EXPECT_THROW(files.graph(), std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    SourceLoader,
    SourceLoaderInvalidExternalType,
    testing::Values(
        InvalidSourceCase{"ZeroWidth",
                          R"((type x :spelling "X" (integer :signed true :bit-width 0)))"},
        InvalidSourceCase{
            "SignedMaximum",
            R"((type x :spelling "X" (integer :signed true :bit-width 8 :maximum 128)))"},
        InvalidSourceCase{
            "UnsignedMinimum",
            R"((type x :spelling "X" (integer :signed false :bit-width 8 :minimum -1)))"},
        InvalidSourceCase{
            "SentinelInDomain",
            R"((type x :spelling "X" (integer :signed false :bit-width 8 (code invalid :value 255 :sentinel true))))"},
        InvalidSourceCase{"UnknownFloatFormat",
                          R"((type x :spelling "X" (floating-point :format invented)))"},
        InvalidSourceCase{
            "ConflictingDomains",
            R"((type x :spelling "X" (integer :signed true :bit-width 8) (floating-point :format ieee754-binary32)))"},
        InvalidSourceCase{
            "ConflictingAliases",
            R"((type x :spelling "int8" (integer :signed true :bit-width 8)) (type y :spelling "std::int8_t" (integer :signed false :bit-width 8)))"},
        InvalidSourceCase{"DuplicateName", R"((type x :spelling "X") (type x :spelling "Y"))"},
        InvalidSourceCase{"MissingInclude", R"((include "missing.lispb"))"},
        InvalidSourceCase{"RecursiveInclude", R"((include "types.lispb"))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, ExternalIntegerDefaultsCoverFull64BitDomains) {
    TemporaryManifest files;
    files.write_root(R"((module example :header "Example.h"))");
    files.write("types.lispb", R"(
      (type signed64 :spelling "int64" (integer :signed true :bit-width 64))
      (type unsigned64 :spelling "uint64" (integer :signed false :bit-width 64)))");
    auto const graph{files.graph()};
    auto const signed_domain{lispb::schema::integer_domain(
        graph, *graph.find_registered(graph.strings().find("signed64")))};
    auto const unsigned_domain{lispb::schema::integer_domain(
        graph, *graph.find_registered(graph.strings().find("unsigned64")))};
    ASSERT_TRUE(signed_domain.has_value());
    ASSERT_TRUE(unsigned_domain.has_value());
    EXPECT_EQ(
        graph.definitions().integer_scalars.minimum_value[graph.definitions().integer_scalars.index(
            *signed_domain)],
        PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63));
    EXPECT_EQ(
        graph.definitions().integer_scalars.maximum_value[graph.definitions().integer_scalars.index(
            *unsigned_domain)],
        PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()});
}

class SourceLoaderInvalidRepresentation : public testing::TestWithParam<InvalidSourceCase> {};

TEST_P(SourceLoaderInvalidRepresentation, RequiresValidIntegerDomain) {
    TemporaryManifest files;
    files.write("types.lispb", R"(
      (type real :spelling "double" (floating-point :format ieee754-binary64))
      (type opaque :spelling "Opaque")
      (type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
      (type byte :spelling "uint8" (integer :signed false :bit-width 8)))");
    files.write("modules.lispb",
                std::string{"(module example :header \"Example.h\" "} + GetParam().source + ")");
    EXPECT_THROW(files.graph(), std::exception);
}

INSTANTIATE_TEST_SUITE_P(
    SourceLoader,
    SourceLoaderInvalidRepresentation,
    testing::Values(
        InvalidSourceCase{"QuantizedReal", "(linear-quantized Q :source @real :bits 4)"},
        InvalidSourceCase{"OpaqueVarint", "(integer-varint V :source @opaque :encoding unsigned)"},
        InvalidSourceCase{"UnsignedEncodingForSignedVarint",
                          "(integer-varint V :source @signed_byte :encoding unsigned)"},
        InvalidSourceCase{"ZigzagEncodingForUnsignedVarint",
                          "(integer-varint V :source @byte :encoding zigzag)"},
        InvalidSourceCase{"MissingSentinel",
                          "(optional-sentinel S :source @byte :sentinel missing)"},
        InvalidSourceCase{"RealPresenceBit", "(optional-presence-bit P :source @real)"},
        InvalidSourceCase{"PointerPresenceBit",
                          "(optional-presence-bit P :source (type-ref @byte :suffix \"*\"))"},
        InvalidSourceCase{"BuiltinNameCollision", "(record double (member x int8))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, NormalModuleKeepsMixedDeclarationOrderAndResolvesTypes) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& normal_modules{store.normal_modules};
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  :namespace example
  (enum State uint8
    (value Alive)
    (value Dead)
    (projections (cpp)))
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 100
    :bit-width auto
    (projections (cpp)))
  (linear-quantized HealthQ7
    :source Health
    :bits 7
    :reserved-codes 1
    :clipping clamp)
  (record Snapshot
    (member state State)
    (member health Health)
    (projections (cpp))))
)");

    auto manifest{files.load()};
    lispb::schema::resolve_type_graph(manifest);
    auto const& module{manifest.modules.front()};
    auto const module_row{normal_modules.index(module)};
    auto const declarations{store.items(normal_modules.declarations[module_row])};
    ASSERT_EQ(declarations.size(), 4);
    EXPECT_EQ(files.text(declaration_name(store, declarations[0])), "State");
    EXPECT_EQ(files.text(declaration_name(store, declarations[1])), "Health");
    EXPECT_EQ(files.text(declaration_name(store, declarations[2])), "HealthQ7");
    EXPECT_EQ(files.text(declaration_name(store, declarations[3])), "Snapshot");

    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const snapshot{find_test_type(graph, "mixed", "Snapshot")};
    ASSERT_TRUE(snapshot.has_value());
    auto const& record{graph.definition(*snapshot)};
    ASSERT_EQ(graph.definitions().records.members_of(record).size(), 2);
    EXPECT_EQ(files.text(graph.name(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions().children.semantic_type[graph.definitions().children.index(
                      graph.definitions().records.members_of(record)[0])])])),
              "State");
    EXPECT_EQ(files.text(graph.name(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions().children.semantic_type[graph.definitions().children.index(
                      graph.definitions().records.members_of(record)[1])])])),
              "Health");

    auto const generated{files.render(manifest)};
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
        } catch (lispb::codegen::sexpr::SourceError const& error) {
            EXPECT_NE(std::string{error.what()}.find("unknown module declaration"),
                      std::string::npos);
        }
    }
}

TEST(SourceLoader, OrdersPhysicalDependenciesWithoutChangingDeclarationIndices) {
    TemporaryManifest files;
    auto& store{files.store()};
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  (record Outer (member payload Payload)
    (projections (cpp)))
  (integer-scalar Index :signed false :minimum 0 :maximum 255)
  (union Payload (member inner Inner)
    (projections (cpp)))
  (record Inner (member value float (relation references Outer))
    (projections (cpp))))
)");
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    ASSERT_TRUE(find_test_type(graph, "mixed", "Inner").has_value());
    auto const files_out{files.render(manifest)};
    auto const& header{files_out.front().content};
    EXPECT_LT(header.find("struct Inner"), header.find("union Payload"));
    EXPECT_LT(header.find("union Payload"), header.find("struct Outer"));
    auto const& declarations{store.normal_modules.declarations[manifest.modules.front()]};
    EXPECT_EQ(files.text(declaration_name(store, store.items(declarations)[0])), "Outer");
    EXPECT_EQ(files.text(declaration_name(store, store.items(declarations)[1])), "Index");
    EXPECT_EQ(files.text(declaration_name(store, store.items(declarations)[2])), "Payload");
    EXPECT_EQ(files.text(declaration_name(store, store.items(declarations)[3])), "Inner");
}

TEST(SourceLoader, NormalModuleRejectsCrossKindGeneratedCppNameCollision) {
    TemporaryManifest files;
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  (enum f_data uint8
    (value One)
    (projections (cpp)))
  (struct FData
    (member items array int32)))
)");

    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);
}

TEST(SourceLoader, ReadsCommentsAndTypedSoa) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& semantic_relations{store.semantic_relations};
    auto& soa_members{store.soa_members};
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
    (member handles array @handle
      (relation offset_into FData :unit elements))))
)");
    auto manifest{files.load()};

    ASSERT_EQ(manifest.types.entries.size(), 1);
    EXPECT_EQ(files.text(files.context.storage.cpp_types.spelling(
                  manifest.types.registered
                      .cpp_type[manifest.types.registered.index(*manifest.types.find("handle"))])),
              "FHandle");
    EXPECT_EQ(files.context.storage.cpp_types.operation(
                  manifest.types.registered
                      .cpp_type[manifest.types.registered.index(*manifest.types.find("handle"))],
                  TypeOperation::add_element),
              "add");
    ASSERT_EQ(manifest.modules.size(), 1);
    auto const& schema{schema_at(manifest, 0)};
    auto const& member{store.items(store.soas.members[schema])[0]};
    auto const member_row{soa_members.index(member)};
    EXPECT_EQ(files.text(files.context.storage.cpp_types.spelling(
                  resolve_type(soa_members.type[member_row], manifest.types))),
              "FHandle");
    ASSERT_TRUE(soa_members.relationship[member_row].has_value());
    auto const relation_row{semantic_relations.index(*soa_members.relationship[member_row])};
    EXPECT_EQ(semantic_relations.kind[relation_row], SemanticRelationKind::offset_into);
    EXPECT_EQ(files.text(store.type_refs.name[semantic_relations.target[relation_row]]), "FData");
    EXPECT_EQ(semantic_relations.unit[relation_row], SemanticRelationUnit::elements);
}

TEST(SourceLoader, OrdinarySoaNeedsNoSourceOutput) {
    TemporaryManifest files;
    files.write_root(R"((module native_data :header "NativeData.h"
        (struct Data (member values array std::int32_t))))");
    auto const output{files.render(files.load()).front().content};
    EXPECT_NE(output.find("struct Data"), std::string::npos);
    EXPECT_NE(output.find("std::pmr::memory_resource&"), std::string::npos);
}

TEST(SourceLoader, NativeIntegerSpellingsPreserveExplicitExternalTypes) {
    TemporaryManifest files;
    files.write_root(R"((module values :header "Values.h"
        (record Values
          (member signed_value int32)
          (member unsigned_value uint16)
          (member engine_value @engine_integer)
    (projections (cpp)))))");
    files.write("types.lispb", R"(
        (type engine_integer :spelling int32 :header "EngineInteger.h"))");
    auto const output{files.render(files.load()).front().content};
    EXPECT_NE(output.find("#include <cstdint>"), std::string::npos);
    EXPECT_NE(output.find("std::int32_t signed_value"), std::string::npos);
    EXPECT_NE(output.find("std::uint16_t unsigned_value"), std::string::npos);
    EXPECT_NE(output.find("int32 engine_value"), std::string::npos);
    EXPECT_NE(output.find("#include \"EngineInteger.h\""), std::string::npos);
}

TEST(SourceLoader, RejectsRemovedSoaBackend) {
    TemporaryManifest files;
    files.write_root(R"(
(module native_data
  :header "NativeData.h"
  :backend portable
  (struct Data
    (member values array int32)))
)");

    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
}

TEST(SourceLoader, ReadsSoaFieldMaskMetadata) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& soa_mask_dimensions{store.soa_mask_dimensions};
    auto& soa_members{store.soa_members};
    auto& soas{store.soas};
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

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{soas.index(schema)};
    ASSERT_TRUE(soas.field_mask_name[schema_row].has_value());
    EXPECT_EQ(files.text(*soas.field_mask_name[schema_row]), "FFieldMask");
    ASSERT_TRUE(soas.field_enum_name[schema_row].has_value());
    EXPECT_EQ(files.text(*soas.field_enum_name[schema_row]), "EField");
    auto const& member{store.items(soas.members[schema_row])[1]};
    auto const member_row{soa_members.index(member)};
    EXPECT_TRUE(soa_members.mask_field[member_row]);
    auto const dimensions{store.items(soa_members.mask_dimensions[member_row])};
    ASSERT_EQ(dimensions.size(), 2);
    auto const row_dimension{soa_mask_dimensions.index(dimensions[0])};
    auto const column_dimension{soa_mask_dimensions.index(dimensions[1])};
    EXPECT_EQ(files.text(soa_mask_dimensions.index_name[row_dimension]), "row_index");
    EXPECT_EQ(files.text(soa_mask_dimensions.extent[row_dimension]), "3");
    EXPECT_EQ(files.text(soa_mask_dimensions.index_name[column_dimension]), "column_index");
    EXPECT_EQ(files.text(soa_mask_dimensions.extent[column_dimension]), "4");
}

TEST(SourceLoader, ReadsPackedValueModule) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.child_occurrences};
    auto& packed_named_codes{store.packed_named_codes};
    auto& packed_reserved_bits{store.child_occurrences};
    auto& packed_values{store.records};
    auto& semantic_relations{store.semantic_relations};
    auto& type_refs{store.type_refs};
    files.write_root(R"(
(module packed
  :header "Packed.h"
  :namespace project
  (record FighterState
    :layout packed :bits 32
    :byte-order big
    :bit-order msb-first
    :invalid-value 0x7fffffff
    (member entity_index @FighterState_entity_indexEncoding
      (relation index_into FighterState))
    (reserved future :bits 4)
    (member state State :bits 8)
    (projections (cpp :storage std::uint32_t)))
      (integer-scalar FighterState_entity_indexEncoding :signed false :minimum 0 :maximum 1000000 (code Player :value 42) (code Invalid :value 1048575 :sentinel true)))
)");

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(store.normal_modules.declarations[module].size(), 2);
    auto const& value{schema_at(manifest, 0)};
    auto const value_row{packed_values.index(value)};
    EXPECT_EQ(files.text(packed_values.name[value_row]), "FighterState");
    EXPECT_EQ(files.text(type_refs.name[*packed_values.cpp_projection[value_row]->storage]),
              "std::uint32_t");
    EXPECT_EQ(packed_values.byte_order[value_row], PackedByteOrder::big_endian);
    EXPECT_EQ(packed_values.bit_order[value_row], PackedBitOrder::most_significant_first);
    EXPECT_EQ(packed_values.invalid_value[value_row], std::uint64_t{0x7fffffff});
    EXPECT_EQ(packed_values.layout[value_row], LayoutKind::packed);
    ASSERT_EQ(packed_values.members[value_row].size(), 3);
    auto const& index{store.items(packed_values.members[value_row])[0]};
    auto const index_row{packed_fields.index(index)};
    EXPECT_FALSE(packed_fields.bits[index_row].has_value());

    auto const scalar{schema_at(manifest, 0, 1)};
    EXPECT_EQ(store.integer_scalars.minimum_value[scalar], 0U);
    EXPECT_EQ(store.integer_scalars.maximum_value[scalar], 1'000'000U);
    ASSERT_EQ(store.integer_scalars.named_codes[scalar].size(), 2U);
    auto const codes{store.items(store.integer_scalars.named_codes[scalar])};
    auto const player_row{packed_named_codes.index(codes[0])};
    auto const invalid_row{packed_named_codes.index(codes[1])};
    EXPECT_EQ(files.text(packed_named_codes.name[player_row]), "Player");
    EXPECT_EQ(packed_named_codes.value[player_row], 42U);
    EXPECT_FALSE(packed_named_codes.sentinel[player_row]);
    EXPECT_EQ(files.text(packed_named_codes.name[invalid_row]), "Invalid");
    EXPECT_EQ(packed_named_codes.value[invalid_row], 1'048'575U);
    EXPECT_TRUE(packed_named_codes.sentinel[invalid_row]);
    ASSERT_TRUE(packed_fields.relationship[index_row].has_value());
    EXPECT_EQ(semantic_relations.kind[*packed_fields.relationship[index_row]],
              SemanticRelationKind::index_into);
    EXPECT_EQ(
        files.text(
            type_refs.name[semantic_relations.target[*packed_fields.relationship[index_row]]]),
        "FighterState");
    auto const& reserved{store.items(packed_values.members[value_row])[1]};
    auto const reserved_row{packed_reserved_bits.index(reserved)};
    EXPECT_EQ(files.text(packed_reserved_bits.name[reserved_row]), "future");
    EXPECT_EQ(packed_reserved_bits.bits[reserved_row], 4);
    auto const& state{store.items(packed_values.members[value_row])[2]};
    auto const state_row{packed_fields.index(state)};
    EXPECT_EQ(packed_fields.bits[state_row], 8);
}

TEST(SourceLoader, ReadsSignedArbitraryWidthPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.child_occurrences};
    auto& packed_named_codes{store.packed_named_codes};
    auto& packed_values{store.records};
    files.write_root(R"(
(module packed
  :header "Packed.h"
  (record SignedDelta
    :layout packed :bits 32
    (member delta @SignedDelta_deltaEncoding)
    (reserved future :bits 24)
    (projections (cpp :storage std::uint32_t)))
      (integer-scalar SignedDelta_deltaEncoding :signed true :minimum -100 :maximum 100 (code Origin :value 0) (code Unknown :value -128 :sentinel true)))
)");

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, 0)};
    auto const value_row{packed_values.index(value)};
    EXPECT_FALSE(packed_values.byte_order[value_row].has_value());
    EXPECT_FALSE(packed_values.bit_order[value_row].has_value());
    auto const& delta{store.items(packed_values.members[value_row])[0]};
    auto const delta_row{packed_fields.index(delta)};
    EXPECT_EQ(files.text(store.type_refs.name[packed_fields.type[delta_row]]),
              "@SignedDelta_deltaEncoding");
    EXPECT_FALSE(packed_fields.bits[delta_row].has_value());

    auto const scalar{schema_at(manifest, 0, 1)};
    EXPECT_EQ(store.integer_scalars.minimum_value[scalar], PackedIntegerValue{-100});
    EXPECT_EQ(store.integer_scalars.maximum_value[scalar], PackedIntegerValue{100});
    ASSERT_EQ(store.integer_scalars.named_codes[scalar].size(), 2U);
    EXPECT_EQ(packed_named_codes.value[store.items(store.integer_scalars.named_codes[scalar])[0]],
              PackedIntegerValue{0});
    EXPECT_EQ(packed_named_codes.value[store.items(store.integer_scalars.named_codes[scalar])[1]],
              PackedIntegerValue{-128});
    EXPECT_TRUE(
        packed_named_codes.sentinel[store.items(store.integer_scalars.named_codes[scalar])[1]]);
}

TEST(SourceLoader, ReadsLinearQuantizedPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.child_occurrences};
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
  (record Vitals
    :layout packed :bits 16
    (member health project::HealthQ8 :bits auto)
    (member state std::uint8_t :bits 8)
    (projections (cpp :storage std::uint16_t))))
)");

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& health{store.items(store.records.members[value])[0]};
    auto const health_row{packed_fields.index(health)};
    EXPECT_EQ(files.text(store.type_refs.name[packed_fields.type[health_row]]),
              "project::HealthQ8");
    EXPECT_FALSE(packed_fields.bits[health_row].has_value());
}

TEST(SourceLoader, ReadsFixedPointPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.child_occurrences};
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
  (record Motion
    :layout packed :bits 32
    (member velocity project::VelocityQ12_4 :bits auto)
    (member state std::uint16_t :bits 16)
    (projections (cpp :storage std::uint32_t))))
)");

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& velocity{store.items(store.records.members[value])[0]};
    auto const velocity_row{packed_fields.index(velocity)};
    EXPECT_EQ(files.text(store.type_refs.name[packed_fields.type[velocity_row]]),
              "project::VelocityQ12_4");
    EXPECT_FALSE(packed_fields.bits[velocity_row].has_value());
}

TEST(SourceLoader, ReadsMiniFloatPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.child_occurrences};
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
  (record Position
    :layout packed :bits 16
    (member component project::PositionF12 :bits auto)
    (member state @Position_stateEncoding :bits 4)
    (projections (cpp :storage std::uint16_t)))
      (integer-scalar Position_stateEncoding :signed false :bit-width 4))
)");

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& component{store.items(store.records.members[value])[0]};
    auto const component_row{packed_fields.index(component)};
    EXPECT_EQ(files.text(store.type_refs.name[packed_fields.type[component_row]]),
              "project::PositionF12");
    EXPECT_FALSE(packed_fields.bits[component_row].has_value());
}

INSTANTIATE_TEST_SUITE_P(
    PackedRecords,
    SourceLoaderInvalidDeclaration,
    testing::Values(
        InvalidSourceCase{"UnknownByteOrder",
                          "(record Value :layout packed :bits 32 :byte-order middle (member "
                          "value std::uint32_t :bits 32))"},
        InvalidSourceCase{"UnknownBitOrder",
                          "(record Value :layout packed :bits 32 :bit-order byte-first (member "
                          "value std::uint32_t :bits 32))"},
        InvalidSourceCase{"NegativeInvalidValue",
                          "(record Value :layout packed :bits 8 :invalid-value -1 (member value "
                          "uint8 :bits 8))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, ReadsStandaloneIntegerScalarDomain) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& integer_scalars{store.integer_scalars};
    auto& packed_named_codes{store.packed_named_codes};
    auto& semantic_relations{store.semantic_relations};
    auto& type_refs{store.type_refs};
    files.write_root(R"(
(module semantic_values
  :header "SemanticValues.h"
  :namespace project
  (integer-scalar DamageReason
    :signed false
    :minimum 0
    :maximum 10
    :bit-width auto
    (code Unknown :value 0)
    (code Invalid :value 15 :sentinel true)
    (relation index_into EntityTable)
    (projections (cpp :storage std::uint8_t)))
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(store.normal_modules.declarations[module].size(), 3U);
    auto const& scalar{schema_at(manifest, 0)};
    auto const scalar_row{integer_scalars.index(scalar)};
    EXPECT_EQ(files.text(integer_scalars.name[scalar_row]), "DamageReason");
    EXPECT_FALSE(integer_scalars.signedness[scalar_row]);
    EXPECT_EQ(integer_scalars.minimum_value[scalar_row], PackedIntegerValue{0});
    EXPECT_EQ(integer_scalars.maximum_value[scalar_row], PackedIntegerValue{10});
    EXPECT_FALSE(integer_scalars.bit_width[scalar_row].has_value());
    ASSERT_TRUE(integer_scalars.cpp_projection[scalar_row].has_value());
    ASSERT_TRUE(integer_scalars.cpp_projection[scalar_row]->storage.has_value());
    EXPECT_EQ(files.text(type_refs.name[*integer_scalars.cpp_projection[scalar_row]->storage]),
              "std::uint8_t");
    ASSERT_EQ(integer_scalars.named_codes[scalar_row].size(), 2U);
    EXPECT_EQ(packed_named_codes.value[store.items(integer_scalars.named_codes[scalar_row])[1]],
              PackedIntegerValue{15});
    EXPECT_TRUE(
        packed_named_codes.sentinel[store.items(integer_scalars.named_codes[scalar_row])[1]]);
    ASSERT_TRUE(integer_scalars.relationship[scalar_row].has_value());
    EXPECT_EQ(semantic_relations.kind[*integer_scalars.relationship[scalar_row]],
              SemanticRelationKind::index_into);
    EXPECT_EQ(
        files.text(
            type_refs.name[semantic_relations.target[*integer_scalars.relationship[scalar_row]]]),
        "EntityTable");
    EXPECT_FALSE(semantic_relations.unit[*integer_scalars.relationship[scalar_row]].has_value());
    auto const& offset{schema_at(manifest, 0, 2)};
    auto const offset_row{integer_scalars.index(offset)};
    ASSERT_TRUE(integer_scalars.relationship[offset_row].has_value());
    EXPECT_EQ(semantic_relations.kind[*integer_scalars.relationship[offset_row]],
              SemanticRelationKind::offset_into);
    EXPECT_EQ(semantic_relations.unit[*integer_scalars.relationship[offset_row]],
              SemanticRelationUnit::bytes);
}

TEST(SourceLoader, ReadsPhysicalRepresentations) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& fixed_points{store.fixed_points};
    auto& integer_varints{store.integer_varints};
    auto& linear_quantizeds{store.linear_quantizeds};
    auto& mini_floats{store.mini_floats};
    auto& optional_presence_bits{store.optional_presence_bits};
    auto& optional_sentinels{store.optional_sentinels};
    auto& type_refs{store.type_refs};
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

    auto manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2U);
    auto const& module{manifest.modules[1]};
    ASSERT_EQ(store.normal_modules.declarations[module].size(), 6U);
    auto const& representation{schema_at(manifest, 1, 0)};
    auto const representation_row{linear_quantizeds.index(representation)};
    EXPECT_EQ(files.text(linear_quantizeds.name[representation_row]), "HealthQ8");
    EXPECT_EQ(files.text(type_refs.name[linear_quantizeds.source[representation_row]]),
              "project::Health");
    EXPECT_EQ(linear_quantizeds.bit_width[representation_row], 8U);
    EXPECT_EQ(linear_quantizeds.reserved_codes[representation_row], 1U);
    EXPECT_EQ(linear_quantizeds.clipping[representation_row], QuantizationClipping::clamp);
    auto const& varint{schema_at(manifest, 1, 1)};
    auto const varint_row{integer_varints.index(varint)};
    EXPECT_EQ(files.text(integer_varints.name[varint_row]), "HealthVarint");
    EXPECT_EQ(files.text(type_refs.name[integer_varints.source[varint_row]]), "project::Health");
    EXPECT_EQ(integer_varints.encoding[varint_row], IntegerVarintEncoding::unsigned_varint);
    auto const& fixed_point{schema_at(manifest, 1, 2)};
    auto const fixed_point_row{fixed_points.index(fixed_point)};
    EXPECT_EQ(files.text(fixed_points.name[fixed_point_row]), "VelocityQ12_4");
    EXPECT_TRUE(fixed_points.signedness[fixed_point_row]);
    EXPECT_EQ(fixed_points.total_bits[fixed_point_row], 16U);
    EXPECT_EQ(fixed_points.fractional_bits[fixed_point_row], 4U);
    EXPECT_EQ(fixed_points.rounding[fixed_point_row], FixedPointRounding::toward_zero);
    EXPECT_EQ(files.text(fixed_points.minimum_value[fixed_point_row]), "-2.5");
    EXPECT_EQ(files.text(fixed_points.maximum_value[fixed_point_row]), "3.75");
    auto const& mini_float{schema_at(manifest, 1, 3)};
    auto const mini_float_row{mini_floats.index(mini_float)};
    EXPECT_EQ(files.text(mini_floats.name[mini_float_row]), "CompactFloat");
    EXPECT_EQ(mini_floats.sign_bits[mini_float_row], 1U);
    EXPECT_EQ(mini_floats.exponent_bits[mini_float_row], 5U);
    EXPECT_EQ(mini_floats.significand_bits[mini_float_row], 10U);
    EXPECT_EQ(mini_floats.exponent_bias[mini_float_row], 15);
    auto const& optional{schema_at(manifest, 1, 4)};
    auto const optional_row{optional_sentinels.index(optional)};
    EXPECT_EQ(files.text(optional_sentinels.name[optional_row]), "OptionalHealth");
    EXPECT_EQ(files.text(type_refs.name[optional_sentinels.source[optional_row]]),
              "project::Health");
    EXPECT_EQ(files.text(optional_sentinels.sentinel[optional_row]), "Invalid");
    auto const& presence{schema_at(manifest, 1, 5)};
    auto const presence_row{optional_presence_bits.index(presence)};
    EXPECT_EQ(files.text(optional_presence_bits.name[presence_row]), "PresentHealth");
    EXPECT_EQ(files.text(type_refs.name[optional_presence_bits.source[presence_row]]),
              "project::Health");
}

INSTANTIATE_TEST_SUITE_P(
    MiniFloat,
    SourceLoaderInvalidDeclaration,
    testing::Values(
        InvalidSourceCase{
            "SignWidth",
            "(mini-float Invalid :sign-bits 2 :exponent-bits 5 :significand-bits 10 :bias 15)"},
        InvalidSourceCase{
            "ExponentWidth",
            "(mini-float Invalid :sign-bits 1 :exponent-bits 1 :significand-bits 10 :bias 15)"},
        InvalidSourceCase{
            "SignificandWidth",
            "(mini-float Invalid :sign-bits 1 :exponent-bits 5 :significand-bits 63 :bias 15)"},
        InvalidSourceCase{
            "TotalWidth",
            "(mini-float Invalid :sign-bits 1 :exponent-bits 15 :significand-bits 49 :bias 15)"},
        InvalidSourceCase{
            "Bias",
            "(mini-float Invalid :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 32768)"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, ReadsExplicitEnumBitWidth) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& enums{store.enums};
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    :native-api true
    :helpers (to-string traits)
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{enums.index(schema)};
    EXPECT_EQ(enums.bit_width[schema_row], 3);
    EXPECT_EQ(enums.signedness[schema_row], schema::EnumSignedness::unsigned_);
    EXPECT_TRUE(
        std::ranges::equal(store.items(enums.helpers[schema_row]),
                           std::array{NativeEnumHelper::to_string, NativeEnumHelper::traits}));
}

TEST(SourceLoader, ReadsEnumWithoutCppBackingType) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& enums{store.enums};
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{enums.index(schema)};
    EXPECT_FALSE(enums.underlying_type[schema_row].has_value());
    EXPECT_EQ(enums.bit_width[schema_row], 3);
    EXPECT_EQ(enums.signedness[schema_row], schema::EnumSignedness::unsigned_);
}

TEST(SourceLoader, ReadsExplicitSignedEnumDomain) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& enums{store.enums};
    files.write_root(R"(
(module states
  :header "States.h"
  (enum Delta std::int8_t
    :signed true
    (value Below :value "-1")
    (value Above :value "1")
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{enums.index(schema)};
    EXPECT_EQ(enums.signedness[schema_row], schema::EnumSignedness::signed_);
    EXPECT_FALSE(enums.bit_width[schema_row].has_value());
}

TEST(SourceLoader, ReadsNamedEnumSentinels) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& enumerators{store.enumerators};
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    (value Ready :value "0")
    (value Invalid :value "0xff" :sentinel true)
    (value Pending :value "0xfe" :sentinel true)
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& values{store.enums.values[schema_at(manifest, 0)]};
    ASSERT_EQ(values.size(), 3U);
    EXPECT_FALSE(enumerators.sentinel[store.items(values)[0]]);
    EXPECT_TRUE(enumerators.sentinel[store.items(values)[1]]);
    EXPECT_TRUE(enumerators.sentinel[store.items(values)[2]]);
}

INSTANTIATE_TEST_SUITE_P(
    EnumOptions,
    SourceLoaderInvalidDeclaration,
    testing::Values(
        InvalidSourceCase{"BitWidthOutsideNativeRange",
                          "(enum State std::uint8_t :bit-width 65 (value Idle))"},
        InvalidSourceCase{
            "UnknownHelper",
            R"((enum State std::uint8_t :native-api true :helpers (unknown) (value Idle :value "0")
    (projections (cpp))))"},
        InvalidSourceCase{
            "NonListHelpers",
            R"((enum State std::uint8_t :native-api true :helpers traits (value Idle :value "0")
    (projections (cpp))))"},
        InvalidSourceCase{"HelperWithoutNativeApi",
                          R"((enum State std::uint8_t :helpers (traits) (value Idle :value "0")
    (projections (cpp))))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, PreservesNumericAndOpaqueEnumInitializers) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Zero :value 0)
    (value High :value "0x7f")
    (projections (cpp))))
)schema");

    auto manifest{files.load()};
    auto const& values{enums.values[schema_at(manifest, 0)]};
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(files.text(enumerators.initializer[store.items(values)[0]]), "0");
    EXPECT_EQ(files.text(enumerators.initializer[store.items(values)[1]]), "0x7f");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value -1)
    (projections (cpp))))
)schema");
    auto const negative_manifest{files.load()};
    auto const& negative_values{enums.values[schema_at(negative_manifest, 0)]};
    EXPECT_EQ(files.text(enumerators.initializer[store.items(negative_values)[0]]), "-1");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value "static_cast<uint8>(1)")
    (projections (cpp))))
)schema");
    auto const opaque_manifest{files.load()};
    auto const& opaque_values{enums.values[schema_at(opaque_manifest, 0)]};
    EXPECT_EQ(files.text(enumerators.initializer[store.items(opaque_values)[0]]),
              "static_cast<uint8>(1)");
}

TEST(SourceLoader, ReadsRecordModuleAndFixedArrays) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& record_members{store.child_occurrences};
    auto& records{store.records};
    auto& semantic_relations{store.semantic_relations};
    auto& type_refs{store.type_refs};
    files.write_root(R"(
(module data
  :header "Data.h"
  :namespace project
  (record Position
    (member x float)
    (member y float)
    (projections (cpp)))
  (record Trail
    :export-specifier PROJECT_API
    (member points Position :count 4
      (relation contains Position))
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(store.normal_modules.declarations[module].size(), 2U);
    auto const& position{schema_at(manifest, 0, 0)};
    auto const position_row{records.index(position)};
    auto const& trail{schema_at(manifest, 0, 1)};
    auto const trail_row{records.index(trail)};
    EXPECT_EQ(files.text(records.name[position_row]), "Position");
    ASSERT_EQ(records.members[position_row].size(), 2U);
    EXPECT_EQ(
        files.text(
            type_refs.name[record_members.type[store.items(records.members[position_row])[0]]]),
        "float");
    EXPECT_FALSE(record_members.count[store.items(records.members[position_row])[0]].has_value());
    EXPECT_EQ(files.text(records.export_specifier[trail_row]), "PROJECT_API");
    EXPECT_EQ(record_members.count[store.items(records.members[trail_row])[0]], 4);
    ASSERT_TRUE(
        record_members.relationship[store.items(records.members[trail_row])[0]].has_value());
    EXPECT_EQ(semantic_relations
                  .kind[*record_members.relationship[store.items(records.members[trail_row])[0]]],
              SemanticRelationKind::contains);
    EXPECT_EQ(files.text(
                  type_refs.name[semantic_relations.target[*record_members.relationship[store.items(
                      records.members[trail_row])[0]]]]),
              "Position");
}

TEST(SourceLoader, ReadsRawUnionModuleAndFixedArrayAlternatives) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& union_alternatives{store.child_occurrences};
    auto& unions{store.unions};
    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  :namespace project
  (union Payload
    :export-specifier PROJECT_API
    (member identifier std::uint32_t)
    (member bytes std::uint8_t :count 12)
    (projections (cpp))))
)");

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(store.normal_modules.declarations[module].size(), 1U);
    auto const& payload{schema_at(manifest, 0)};
    auto const payload_row{unions.index(payload)};
    EXPECT_EQ(files.text(unions.name[payload_row]), "Payload");
    EXPECT_EQ(files.text(unions.export_specifier[payload_row]), "PROJECT_API");
    ASSERT_EQ(unions.alternatives[payload_row].size(), 2U);
    EXPECT_EQ(files.text(union_alternatives.name[store.items(unions.alternatives[payload_row])[0]]),
              "identifier");
    EXPECT_FALSE(
        union_alternatives.count[store.items(unions.alternatives[payload_row])[0]].has_value());
    EXPECT_EQ(
        files.text(
            store.type_refs
                .name[union_alternatives.type[store.items(unions.alternatives[payload_row])[1]]]),
        "std::uint8_t");
    EXPECT_EQ(union_alternatives.count[store.items(unions.alternatives[payload_row])[1]], 12);
}

class SourceLoaderInvalidUnion : public testing::TestWithParam<InvalidSourceCase> {};

TEST_P(SourceLoaderInvalidUnion, RejectsInvalidAlternatives) {
    TemporaryManifest files;
    files.write_root(std::string{R"((module payloads :header "Payloads.h" )"} + GetParam().source +
                     ")");
    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    SourceLoader,
    SourceLoaderInvalidUnion,
    testing::Values(
        InvalidSourceCase{"ZeroCount", "(union Payload (member value std::uint32_t :count 0))"},
        InvalidSourceCase{
            "DuplicateName",
            "(union Payload (member value std::uint32_t) (member value std::uint16_t))"},
        InvalidSourceCase{"DuplicateCase",
                          "(union Event (case Value std::uint32_t) (case Value std::uint16_t))"},
        InvalidSourceCase{"UnknownTag",
                          "(enum Kind :bit-width 1 (value Value)) "
                          "(union Payload (case Other std::uint8_t)) "
                          "(record Event :layout packed (member kind @Kind) "
                          "(member payload @Payload :tag kind) (projections (cpp)))"}),
    [](auto const& info) { return info.param.name; });

TEST(SourceLoader, ReadsUnionDiscriminatorOnMemberOccurrence) {
    TemporaryManifest files;
    files.write_root(R"((module events :header "Events.h"
      (enum Kind :bit-width 2 (value Empty) (value Data))
      (record Empty)
      (integer-scalar Payload :bit-width 24)
      (union Content (case Empty @Empty) (case Data @Payload))
      (record Event :layout packed :bits 32
        (member kind @Kind)
        (member payload @Content :bits 30 :tag kind)
        (projections (cpp)))))");
    auto const manifest{files.load()};
    auto const& store{files.store()};
    auto const union_id{schema_at(manifest, 0, 3)};
    auto const alternatives{store.items(store.unions.alternatives[store.unions.index(union_id)])};
    ASSERT_EQ(alternatives.size(), 2U);
    EXPECT_EQ(
        files.text(
            store.child_occurrences.case_label[store.child_occurrences.index(alternatives[0])]),
        "Empty");
    EXPECT_EQ(
        files.text(
            store.child_occurrences.case_label[store.child_occurrences.index(alternatives[1])]),
        "Data");
    auto const event{schema_at(manifest, 0, 4)};
    auto const members{store.items(store.records.members[store.records.index(event)])};
    ASSERT_EQ(members.size(), 2U);
    EXPECT_EQ(files.text(store.child_occurrences.tag[store.child_occurrences.index(members[1])]),
              "kind");
    EXPECT_EQ(store.child_occurrences.bits[store.child_occurrences.index(members[1])], 30U);
}

TEST(SourceLoader, RejectsNonIntegerPackedFieldWidthWithSourceLocation) {
    TemporaryManifest files;
    files.write_root(R"(
(module packed
  :header "Packed.h"
  (record Value
    :layout packed :bits 8
    (member value uint8 :bits 1.5)
    (projections (cpp :storage uint8))))
)");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:6:31"), std::string::npos) << message;
        EXPECT_NE(message.find("member bits must be an integer"), std::string::npos) << message;
    }
}

TEST(SourceLoader, ReportsSourceLocationForUnknownProperties) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all\n  :header \"All.h\"\n  :headers ()\n  :typo true)\n");

    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_NE(message.find("modules.lispb:4:3"), std::string::npos);
        EXPECT_NE(message.find("unknown property ':typo'"), std::string::npos);
    }
}

TEST(SourceLoader, RejectsUnknownModuleDeclarations) {
    TemporaryManifest files;
    files.write_root("(mystery-module bad :header \"Bad.h\")");
    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
}

TEST(SourceLoader, LoadsStructuredTypeReferencesAndFacadeStorage) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& facade_methods{store.facade_methods};
    auto& facades{store.facades};
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

    auto manifest{files.load()};
    auto const& facade{schema_at(manifest, 0)};
    auto const facade_row{facades.index(facade)};
    EXPECT_TRUE(facades.reference_target[facade_row]);
    EXPECT_TRUE(facades.definitions_in_source[facade_row]);
    auto const method_row{facade_methods.index(store.items(facades.methods[facade_row])[0])};
    EXPECT_EQ(files.text(store.type_refs.suffix[facade_methods.return_type[method_row]]),
              " const&");
    EXPECT_EQ(files.text(facade_methods.target_name[method_row]), "get_value");
    EXPECT_TRUE(facade_methods.is_const[method_row]);
}

TEST(SourceLoader, LoadsOpaqueCppBlocksAndKeepsQuotedBodiesCompatible) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& facades{store.facades};
    auto& functions{store.functions};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& soas{store.soas};
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

    auto manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2);
#ifdef _WIN32
    auto const newline{std::string{"\r\n"}};
#else
    auto const newline{std::string{"\n"}};
#endif

    auto const& soa{manifest.modules[0]};
    auto const soa_row{normal_modules.index(soa)};
    auto const module_row{module_settings_store.index(normal_modules.settings[soa_row])};
    auto const prelude{store.items(module_settings_store.prelude_lines[module_row])};
    ASSERT_EQ(prelude.size(), 1);
    EXPECT_EQ(files.text(prelude[0]),
              "class FForward;" + newline + "#define GENERATED_PATH \"C:\\\\generated\"");
    auto const& data{schema_at(manifest, 0)};
    auto const data_row{soas.index(data)};
    ASSERT_EQ(soas.functions[data_row].size(), 2);
    auto const members{store.items(soas.functions[data_row])};
    auto const update_body{store.items(functions.body_lines[members[0]])};
    ASSERT_EQ(update_body.size(), 1);
    EXPECT_EQ(files.text(update_body[0]),
              "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
                  "values[0] += dt;");
    auto const reset_body{store.items(functions.body_lines[members[1]])};
    ASSERT_EQ(reset_body.size(), 1);
    EXPECT_EQ(files.text(reset_body[0]), "values[0] = 0;");

    auto const& facade{schema_at(manifest, 1)};
    auto const facade_row{facades.index(facade)};
    ASSERT_EQ(facades.validation_lines[facade_row].size(), 1);
    EXPECT_EQ(files.text(store.items(facades.validation_lines[facade_row])[0]),
              "checkf(target != nullptr, TEXT(\"missing target\"));");
}

TEST(SourceLoader, AcceptsEmptyCppBodyAndRejectsWrongRawTag) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& functions{store.functions};
    files.write_root(R"(
(module example
  :header "Generated.h"
  (struct FData
    (function empty void :body #cpp{}cpp#)))
)");
    auto manifest{files.load()};
    auto const& function{store.items(store.soas.functions[schema_at(manifest, 0)])[0]};
    auto const function_row{functions.index(function)};
    ASSERT_EQ(functions.body_lines[function_row].size(), 1);
    EXPECT_EQ(files.text(store.items(functions.body_lines[function_row])[0]), "");

    files.write_root(R"(
(module example
  :header "Generated.h"
  (struct FData
    (function wrong void :body #hlsl{return 0;}hlsl#)))
)");
    try {
        static_cast<void>(files.load());
        FAIL() << "Expected manifest error";
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        auto const message{std::string{error.what()}};
        EXPECT_TRUE(message.contains("body requires a #cpp raw literal; got #hlsl"));
        EXPECT_TRUE(message.contains("modules.lispb:5:32"));
    }
}

TEST(SourceLoader, RejectsRawLiteralForOrdinaryTextField) {
    TemporaryManifest files;
    files.write_root("(umbrella-module all :header #cpp{Generated.h}cpp# :headers ())");

    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
}

TEST(SourceLoader, LoadsSettingsControls) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& setting_controls{store.setting_controls};
    auto& settings{store.settings};
    auto& settings_modules{store.settings_modules};
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    auto const module_row{settings_modules.index(module)};
    ASSERT_EQ(settings_modules.settings_list[module_row].size(), 1);
    EXPECT_EQ(settings.apply_mode[store.items(settings_modules.settings_list[module_row])[0]],
              SettingApplyMode::deferred);
    EXPECT_EQ(settings.device[store.items(settings_modules.settings_list[module_row])[0]],
              SettingDevice::keyboard_mouse);
    EXPECT_EQ(
        setting_controls
            .kind[settings.control[store.items(settings_modules.settings_list[module_row])[0]]],
        SettingControlKind::float_range);
    EXPECT_EQ(
        setting_controls
            .step[settings.control[store.items(settings_modules.settings_list[module_row])[0]]],
        0.5);
}

TEST(SourceLoader, SettingsDeviceDefaultsToShared) {
    TemporaryManifest files;
    auto& store{files.store()};
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    EXPECT_EQ(store.settings.device[store.items(store.settings_modules.settings_list[module])[0]],
              SettingDevice::shared);
}

TEST(SourceLoader, OrdinarySoaPreservesLogicalApi) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& soas{store.soas};
    files.write_root(R"(
(module api :header "Api.h" :source "Api.cpp"
  (struct Rows
    :export-specifier API :using-declarations ("Value = float")
    :equivalent-type ValueRow
    (member values array float)
    (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
    (function noop void :body ())
    (view-function read float :body #cpp{return $column(values)[0];}cpp#)
    (mutable-view-function write void :body #cpp{$column(values)[0] = 1;}cpp#)))
)");
    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{soas.index(schema)};
    ASSERT_EQ(soas.const_view_functions[schema_row].size(), 1U);
    EXPECT_TRUE(store.functions.is_const[store.items(soas.const_view_functions[schema_row])[0]]);
    ASSERT_EQ(soas.mutable_view_functions[schema_row].size(), 1U);
    auto const output{files.render(manifest).front().content};
    EXPECT_NE(output.find("struct API Rows"), std::string::npos);
    EXPECT_NE(output.find("std::pmr::memory_resource& resource"), std::string::npos);
    EXPECT_NE(output.find("return this->values()[0];"), std::string::npos);
    EXPECT_EQ(output.find("struct Rows {"), std::string::npos);
    EXPECT_NE(output.find("void noop() {"), std::string::npos);
}

TEST(SourceLoader, ReadOnlyViewFunctionRejectsMutableReceiver) {
    TemporaryManifest files;
    files.write_root(R"(
(module api :header "Api.h"
  (struct Rows
    (member values array float)
    (view-function read float :const false :body #cpp{return 0;}cpp#)))
)");
    try {
        static_cast<void>(files.load());
        FAIL() << "Expected a const receiver diagnostic";
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("view-function must be const"));
    }
}

} // namespace
} // namespace lispb::codegen
