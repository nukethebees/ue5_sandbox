#include "test_utilities.h"

#include <codegen/ast/indices.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
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
#include <codegen/schema/integer_scalar_cpp_emission.h>
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
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_member_schemas.h>
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
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/soa_storage.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_role.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/source_loader.h>
#include <lispb/compilation.h>
#include <lispb/indices.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
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
    auto text(lispb::IdentifierId id) const -> std::string_view {
        return context.storage.strings.view(id);
    }
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
    return store.items(
        store.normal_modules.declarations[store.normal_modules.index(module)])[declaration_index];
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
    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    EXPECT_FALSE(files.store()
                     .record_members
                     .initializer[files.store().record_members.index(files.store().items(
                         files.store().records.members[files.store().records.index(schema)])[0])]
                     .has_value());
    EXPECT_EQ(
        files.text(files.store().record_members.initializer[files.store().record_members.index(
            files.store().items(
                files.store().records.members[files.store().records.index(schema)])[1])]),
        "");
    EXPECT_EQ(
        files.text(files.store().record_members.initializer[files.store().record_members.index(
            files.store().items(
                files.store().records.members[files.store().records.index(schema)])[2])]),
        "7");
    EXPECT_EQ(files.store().records.comparison[files.store().records.index(schema)],
              RecordComparison::three_way);
    ASSERT_EQ(files.store().records.functions[files.store().records.index(schema)].size(), 2U);
    EXPECT_TRUE(
        files.store().functions.is_constexpr[files.store().functions.index(files.store().items(
            files.store().records.functions[files.store().records.index(schema)])[0])]);
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const& record{graph.definition(*find_test_type(graph, "records", "Value"))};
    EXPECT_FALSE(graph.definitions()
                     .record_members
                     .initializer[graph.definitions().record_members.index(
                         graph.definitions().records.members_of(record)[0])]
                     .has_value());
    EXPECT_EQ(
        files.text(
            graph.definitions().record_members.initializer[graph.definitions().record_members.index(
                graph.definitions().records.members_of(record)[1])]),
        "");
    EXPECT_EQ(
        files.text(
            graph.definitions().record_members.initializer[graph.definitions().record_members.index(
                graph.definitions().records.members_of(record)[2])]),
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
        EXPECT_THROW(static_cast<void>(files.lower(files.load())), std::exception) << declaration;
    }
}

TEST(SourceLoader, RecordMethodDeclarationsForwardDeclareLaterAndMutualTypes) {
    TemporaryManifest files;
    files.write_root(R"((module values :header "Values.h"
      (record A
        (function make B)
        (function accept void (parameter value B))
        (function ref auto :trailing-return-type (type-ref B :suffix " const&"))
        (function pointer (type-ref B :suffix "*") :body ("return nullptr;")))
      (record B (function make A))))");
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
    auto editable{
        files.store()
            .items(files.store().normal_modules.declarations[files.store().normal_modules.index(
                manifest.modules.front())])
            .front()};
    std::vector<std::pair<std::string, TypeReferenceKind>> const_uses;
    visit_type_references(files.store(),
                          std::as_const(editable),
                          [&](TypeReferenceRole const& role, TypeRefId, TypeReferenceKind kind) {
                              const_uses.emplace_back(
                                  type_reference_role_text(files.context.storage.strings, role),
                                  kind);
                          });
    std::vector<std::pair<std::string, TypeReferenceKind>> mutable_uses;
    editable = rewrite_type_references(
        files.store(),
        editable,
        [&](TypeReferenceRole const& role, TypeRefId id, TypeReferenceKind kind) {
            mutable_uses.emplace_back(type_reference_role_text(files.context.storage.strings, role),
                                      kind);
            auto const reference{files.store().type_refs.clone(id)};
            if (files.text(
                    files.store().type_refs.name[files.store().type_refs.index(reference)]) !=
                "B") {
                return id;
            }
            files.store().type_refs.name[files.store().type_refs.index(reference)] =
                files.context.storage.strings.intern("Renamed");
            return reference;
        });
    EXPECT_EQ(mutable_uses, const_uses);
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().functions.return_type[files.store().functions.index(files.store().items(
                files.store().records.functions[files.store().records.index(editable)])[0])])]),
        "Renamed");
    auto const output{files.render(manifest).front().content};
    EXPECT_LT(output.find("struct B;"), output.find("struct A {"));
    EXPECT_LT(output.find("struct A;"), output.find("struct B {"));
    EXPECT_LT(output.find("struct A {"), output.find("struct B {"));
    EXPECT_NE(output.find("auto ref() -> B const&;"), std::string::npos);
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store()
                .items(files.store().normal_modules.declarations[files.store().normal_modules.index(
                    manifest.modules.front())])
                .front())),
        "A");
}

TEST(SourceLoader, RecordMethodSignaturesOrderAliasesEnumsAndHeaderBodyValues) {
    TemporaryManifest files;
    files.write_root(R"((module values :header "Values.h"
      (record Consumer
        (function tag Tag)
        (function count (type-ref Count :suffix " const&"))
        (function make auto :trailing-return-type Provider :body ("return {};"))
        (function accept void (parameter value Argument) :body ("(void)value;"))
        (function self Consumer :body ("return *this;")))
      (record Provider)
      (record Argument)
      (enum Tag std::uint8_t (value Value))
      (integer-scalar Count :signed false :minimum 0 :maximum 255 :bit-width 8
        :cpp-emission alias :cpp-type std::uint8_t)))");
    auto manifest{files.load()};
    auto const output{files.render(manifest).front().content};
    auto const consumer{output.find("struct Consumer {")};
    for (auto const definition :
         {"struct Provider {", "struct Argument {", "enum class Tag", "using Count ="}) {
        EXPECT_LT(output.find(definition), consumer) << definition;
    }
    EXPECT_EQ(output.find("struct Tag;"), std::string::npos);
    EXPECT_EQ(output.find("struct Count;"), std::string::npos);
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store()
                .items(files.store().normal_modules.declarations[files.store().normal_modules.index(
                    manifest.modules.front())])
                .front())),
        "Consumer");
}

TEST(SourceLoader, RecordMethodDeclarationsForwardDeclareCrossModuleTypes) {
    TemporaryManifest files;
    files.write_root(R"((module consumers :header "Consumers.h" :namespace consumers
      (record Consumer
        (function make providers::Provider)
        (function accept void (parameter value (type-ref providers::Provider :suffix " const&")))
        (function pointer auto :trailing-return-type (type-ref providers::Provider :suffix "*"))))
      (module providers :header "generated/Provider.h" :header-include "api/Provider.h"
        :namespace providers (record Provider)))");
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
        (function move auto :trailing-return-type (type-ref second::B :suffix "&&"))))
      (module second :header "Second.h" :namespace second
        (record B
          (function make auto :trailing-return-type first::A)
          (function pointer (type-ref first::A :suffix "*"))
          (function ref auto :trailing-return-type (type-ref first::A :suffix " const&")))))");
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
    for (auto const& [parameter_type, default_value] :
         {std::pair{"B", "{}"},
          std::pair{"(type-ref B :suffix \" const&\")", "{}"},
          std::pair{"(type-ref B :suffix \"*\")", "nullptr"}}) {
        files.write_root(std::string{R"((module values :header "Values.h"
          (record A
            (function consume Result (parameter value )"} +
                         parameter_type + " :default \"" + default_value + R"("))
            (function inspect auto :trailing-return-type Result
              (parameter existing (type-ref C :suffix " const&"))
              (parameter value )" +
                         parameter_type + " :default \"" + default_value + R"(")))
          (record B)
          (record Result)
          (record C)))");
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
            files.store()
                .items(files.store().normal_modules.declarations[files.store().normal_modules.index(
                    manifest.modules.front())])
                .front()};
        std::vector<TypeReferenceKind> const_kinds;
        visit_type_references(files.store(),
                              std::as_const(declaration),
                              [&](TypeReferenceRole const&, TypeRefId, TypeReferenceKind kind) {
                                  const_kinds.push_back(kind);
                              });
        std::vector<TypeReferenceKind> mutable_kinds;
        declaration = rewrite_type_references(
            files.store(),
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
        EXPECT_LT(output.find("struct A {"), output.find("struct Result {"));
        EXPECT_LT(output.find("struct A {"), output.find("struct C {"));
    }
}

TEST(SourceLoader, RecordParameterDefaultsRetainCrossModuleIncludes) {
    TemporaryManifest files;
    for (auto const& [parameter_type, default_value] :
         {std::pair{"second::B", "{}"},
          std::pair{"(type-ref second::B :suffix \" const&\")", "{}"},
          std::pair{"(type-ref second::B :suffix \"*\")", "nullptr"}}) {
        files.write_root(std::string{R"((module first :header "First.h" :namespace first
          (record A (function consume void (parameter value )"} +
                         parameter_type + " :default \"" + default_value + R"("))))
          (module second :header "generated/Second.h" :header-include "api/Second.h"
            :namespace second (record B)))");
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
                 (parameter value (type-ref B :suffix " const&") :default "{}")))
               (record B (function inspect void
                 (parameter value (type-ref A :suffix " const&") :default "{}")))))",
             R"((module first :header "First.h" :namespace first
               (record A (function consume void (parameter value second::B :default "{}"))))
               (module second :header "Second.h" :namespace second
                 (record B (function consume void (parameter value first::A :default "{}")))))"}) {
        files.write_root(schema);
        try {
            static_cast<void>(files.lower(files.load()));
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
        (function count (type-ref providers::Count :suffix " const&"))))
      (module providers :header "generated/Provider.h" :header-include "api/Provider.h"
        :namespace providers (record Provider))
      (module tags :header "Tags.h" :namespace providers
        (enum Tag std::uint8_t (value Value)))
      (module counts :header "Counts.h" :namespace providers
        (integer-scalar Count :signed false :minimum 0 :maximum 255 :bit-width 8
          :cpp-emission alias :cpp-type std::uint8_t)))");
    auto const output{files.render(files.load()).front().content};
    for (auto const header : {"api/Provider.h", "Tags.h", "Counts.h"}) {
        EXPECT_NE(output.find(std::string{"#include \""} + header + "\""), std::string::npos);
    }
    for (auto const type : {"Provider", "Tag", "Count"}) {
        EXPECT_EQ(output.find(std::string{"struct "} + type + ";"), std::string::npos);
    }
}

TEST(SourceLoader, RecordMethodCrossModuleForwardsRespectPhysicalOwnersAndExternalHeaders) {
    TemporaryManifest files;
    files.write_root(R"((module consumers :header "Consumers.h" :namespace consumers
      (record Consumer
        (function rows providers::Rows)
        (function value @external)
        (function registered @registered)
        (function global Global)))
      (module providers :header "Providers.h" :namespace providers :backend standard-library
        (struct Rows :storage single-allocation (member xs array float)
          (single-allocation OwnedRows))
        (record Registered))
      (module global :header "Global.h" (record Global)))");
    files.write("types.lispb", R"(
      (type external :spelling "external::Value" :header "External.h")
      (type registered :spelling "providers::Registered" :header "Providers.h"))");
    auto const output{files.render(files.load()).front().content};
    EXPECT_NE(output.find("struct OwnedRows;"), std::string::npos);
    EXPECT_NE(output.find("providers::OwnedRows rows();"), std::string::npos);
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
        files.write_root(std::string{R"((module values :header "Values.h" :backend standard-library
          (record Consumer )"} +
                         signature + R"()
          (struct Layout :layout-only true (member xs array float))))");
        try {
            static_cast<void>(files.lower(files.load()));
            FAIL() << signature;
        } catch (std::invalid_argument const& error) {
            EXPECT_TRUE(std::string{error.what()}.contains("has no C++ owner")) << error.what();
        }
    }
    files.write_root(R"((module consumers :header "Consumers.h"
      (record Consumer (function make (type-ref providers::Layout :suffix "*"))))
      (module providers :header "Providers.h" :namespace providers :backend standard-library
        (struct Layout :layout-only true (member xs array float))))");
    try {
        static_cast<void>(files.lower(files.load()));
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("has no C++ owner")) << error.what();
    }
    files.write_root(R"((module values :header "Values.h"
      (record A (function make B :body ("return {};")))
      (record B (function make A :body ("return {};")))))");
    try {
        static_cast<void>(files.lower(files.load()));
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"));
        EXPECT_TRUE(std::string{error.what()}.contains("A -> B -> A"));
    }
    files.write_root(R"((module first :header "First.h" :namespace first
      (record A (function make second::B :body ("return {};"))))
      (module second :header "Second.h" :namespace second
        (record B (member a first::A))))");
    try {
        static_cast<void>(files.lower(files.load()));
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"));
        EXPECT_TRUE(std::string{error.what()}.contains("first::A -> second::B -> first::A"));
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
    auto manifest{files.load()};
    EXPECT_TRUE(files.store().soas.emits_vector_storage(schema_at(manifest, 0, 0)));
    EXPECT_FALSE(files.store().soas.emits_vector_storage(schema_at(manifest, 0, 1)));
    EXPECT_TRUE(files.store().soas.emits_vector_storage(schema_at(manifest, 0, 2)));
    auto const output{files.render(manifest).front().content};
    EXPECT_NE(output.find("struct VectorRowsView"), std::string::npos);
    EXPECT_EQ(output.find("struct CompactRowsView"), std::string::npos);
    EXPECT_NE(output.find("struct BothRowsView"), std::string::npos);
    for (auto const* invalid : {"vector", "single-allocation", "both", "unknown"}) {
        files.write_root(
            std::string{R"((module rows :header "Rows.h" (struct Rows :storage )"} + invalid +
            " (member values array int32)" +
            (std::string_view{invalid} == "vector" ? " (single-allocation SingleRows)" : "") +
            "))");
        EXPECT_THROW(files.lower(files.load()), std::exception);
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
    auto manifest{files.load()};
    auto const& scalar{schema_at(manifest, 0, 3)};
    EXPECT_EQ(
        files.store().integer_scalars.cpp_emission[files.store().integer_scalars.index(scalar)],
        IntegerScalarCppEmission::alias);
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
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(record)[0])])],
              health);
    auto const packed{*find_test_type(graph, "vitals", "PackedHealth")};
    auto const& field{graph.definitions().packed.segments_of(graph.definition(packed))[0]};
    EXPECT_EQ(
        graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .packed_fields.semantic_type[graph.definitions().packed_fields.index(field)])],
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
        auto manifest{files.load()};
        EXPECT_EQ(files.store().integer_scalars.cpp_emission[files.store().integer_scalars.index(
                      schema_at(manifest, 0, 1))],
                  mode);
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        auto const reason{*find_test_type(graph, "reasons", "Reason")};
        auto const context{*find_test_type(graph, "reasons", "Context")};
        auto const byte{*graph.find_registered(graph.strings().find("byte"))};
        auto const& scalar{graph.definition(reason)};
        auto const alias{mode == IntegerScalarCppEmission::alias};
        EXPECT_EQ(lispb::schema::is_valid(
                      graph.definitions()
                          .integer_scalars
                          .cpp_representation[graph.definitions().integer_scalars.index(scalar)]),
                  alias);
        if (alias) {
            EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                          graph.definitions().integer_scalars.cpp_representation
                              [graph.definitions().integer_scalars.index(scalar)])],
                      byte);
        }
        EXPECT_EQ(graph.definitions()
                      .integer_scalars.bit_width[graph.definitions().integer_scalars.index(scalar)],
                  8);
        EXPECT_EQ(
            graph.definitions()
                .integer_scalars.named_codes[graph.definitions().integer_scalars.index(scalar)]
                .size(),
            2);
        ASSERT_TRUE(lispb::schema::is_valid(
            graph.definitions()
                .integer_scalars.relationship[graph.definitions().integer_scalars.index(scalar)]));
        EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                      graph.relationships().target[graph.relationships().index(
                          graph.definitions().integer_scalars.relationship
                              [graph.definitions().integer_scalars.index(scalar)])])],
                  context);
        auto const& related{graph.definition(context)};
        EXPECT_FALSE(lispb::schema::is_valid(
            graph.definitions()
                .integer_scalars
                .cpp_representation[graph.definitions().integer_scalars.index(related)]));
        ASSERT_TRUE(lispb::schema::is_valid(
            graph.definitions()
                .integer_scalars.relationship[graph.definitions().integer_scalars.index(related)]));
        EXPECT_EQ(graph.occurrences().type[graph.occurrences().index(
                      graph.relationships().target[graph.relationships().index(
                          graph.definitions().integer_scalars.relationship
                              [graph.definitions().integer_scalars.index(related)])])],
                  reason);
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

        auto const header{files.render(manifest).front().content};
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
        auto const header{files.render(files.load()).front().content};
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
    auto manifest{files.load()};
    EXPECT_EQ(
        files.store()
            .packed_fields.default_value[files.store().packed_fields.index(files.store().items(
                files.store().packed_values.segments[files.store().packed_values.index(
                    schema_at(manifest, 0))])[0])],
        PackedIntegerValue{1});
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto default_raw = [&](char const* name) {
        return graph.definitions().packed.default_raw_value[graph.definitions().packed.index(
            graph.definition(*find_test_type(graph, "orders", name)))];
    };
    EXPECT_EQ(default_raw("Order"), 1);
    EXPECT_EQ(default_raw("Partial"), std::nullopt);
    EXPECT_EQ(default_raw("Implicit"), 0);
    EXPECT_EQ(default_raw("Invalid"), 255);
    auto const header{files.render(manifest).front().content};
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
        graph.definitions().record_members.semantic_type[graph.definitions().record_members.index(
            graph.definitions().records.members_of(record)[0])])])};
    EXPECT_EQ(graph.definitions()
                  .externals.floating_semantics[graph.definitions().externals.index(floating)],
              FloatingPointFormat::ieee754_binary64);
    for (std::size_t index{1}; index < graph.definitions().records.members_of(record).size();
         ++index) {
        auto const& node{graph.occurrences().type[graph.occurrences().index(
            graph.definitions()
                .record_members.semantic_type[graph.definitions().record_members.index(
                    graph.definitions().records.members_of(record)[index])])]};
        EXPECT_TRUE(
            ((graph.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external));
        auto const scalar{lispb::schema::integer_domain(
            graph,
            graph.occurrences().type[graph.occurrences().index(
                graph.definitions()
                    .record_members.semantic_type[graph.definitions().record_members.index(
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
      (packed-value Packed :storage uint32
        (field index @index :bits auto)
        (field subset uint32 :bits 24))))");
    files.write("types.lispb", R"(
      (type index :spelling "NativeIndex" (integer :signed false :bit-width 8 :maximum 254
        (code invalid :value 255 :sentinel true)))
      (type uint32 :spelling "uint32" (integer :signed false :bit-width 32)))");
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const source{*graph.find_registered(graph.strings().find("index"))};
    EXPECT_EQ(graph.users_of(source).size(), 5);
    auto const& packed{graph.definition(*find_test_type(graph, "example", "Packed"))};
    EXPECT_EQ(graph.definitions().packed_fields.bit_width[graph.definitions().packed_fields.index(
                  graph.definitions().packed.segments_of(packed)[0])],
              8);
    EXPECT_EQ(graph.definitions().packed_fields.bit_width[graph.definitions().packed_fields.index(
                  graph.definitions().packed.segments_of(packed)[1])],
              24);
    auto const output{files.render(manifest)};
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
    auto const original{files.render(files.load())};
    files.write("types.lispb", R"(
      (type legacy :spelling "uint32")
      (type integer :spelling "uint32" (integer :signed false :bit-width 32))
      (type byte :spelling "int8" (integer :signed true :bit-width 8))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))");
    auto manifest{files.load()};
    auto const enriched{files.render(manifest)};
    ASSERT_EQ(original.size(), 1);
    ASSERT_EQ(enriched.size(), 1);
    EXPECT_EQ(original[0].content, enriched[0].content);

    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const type{*graph.find_registered(graph.strings().find("integer"))};
    auto field{files.store().items(
        files.store()
            .packed_values.segments[files.store().packed_values.index(schema_at(manifest, 0))])[0]};
    EXPECT_FALSE(lispb::schema::packed_integer_domain(graph, type, field).has_value());
    files.store().packed_fields.bits[files.store().packed_fields.index(field)].reset();
    EXPECT_TRUE(lispb::schema::packed_integer_domain(graph, type, field).has_value());
    files.store().packed_fields.bits[files.store().packed_fields.index(field)] = 32;
    files.store().packed_fields.type[files.store().packed_fields.index(field)] = [&] {
        auto const new_type_refs{files.store().type_refs.append()};
        files.store().type_refs.name[files.store().type_refs.index(new_type_refs)] =
            files.context.storage.strings.intern("@integer");
        return new_type_refs;
    }();
    EXPECT_FALSE(lispb::schema::packed_integer_domain(graph, type, field).has_value());
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
        EXPECT_THROW(files.graph(), std::exception);
    }
}

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
        EXPECT_THROW(files.graph(), std::exception);
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

    auto manifest{files.load()};
    lispb::schema::resolve_type_graph(manifest);
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              4);
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store().items(
                files.store()
                    .normal_modules.declarations[files.store().normal_modules.index(module)])[0])),
        "State");
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store().items(
                files.store()
                    .normal_modules.declarations[files.store().normal_modules.index(module)])[1])),
        "Health");
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store().items(
                files.store()
                    .normal_modules.declarations[files.store().normal_modules.index(module)])[2])),
        "HealthQ7");
    EXPECT_EQ(
        files.text(declaration_name(
            files.store(),
            files.store().items(
                files.store()
                    .normal_modules.declarations[files.store().normal_modules.index(module)])[3])),
        "Snapshot");

    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const snapshot{find_test_type(graph, "mixed", "Snapshot")};
    ASSERT_TRUE(snapshot.has_value());
    auto const& record{graph.definition(*snapshot)};
    ASSERT_EQ(graph.definitions().records.members_of(record).size(), 2);
    EXPECT_EQ(files.text(graph.name(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
                          graph.definitions().records.members_of(record)[0])])])),
              "State");
    EXPECT_EQ(files.text(graph.name(graph.occurrences().type[graph.occurrences().index(
                  graph.definitions()
                      .record_members.semantic_type[graph.definitions().record_members.index(
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
    files.write_root(R"(
(module mixed
  :header "Mixed.h"
  (record Outer (member payload Payload))
  (integer-scalar Index :signed false :minimum 0 :maximum 255)
  (union Payload (alternative inner Inner))
  (record Inner (member value float (relation references Outer))))
)");
    auto manifest{files.load()};
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    ASSERT_TRUE(find_test_type(graph, "mixed", "Inner").has_value());
    auto const files_out{files.render(manifest)};
    auto const& header{files_out.front().content};
    EXPECT_LT(header.find("struct Inner"), header.find("union Payload"));
    EXPECT_LT(header.find("union Payload"), header.find("struct Outer"));
    auto const& declarations{
        files.store().normal_modules.declarations[files.store().normal_modules.index(
            manifest.modules.front())]};
    EXPECT_EQ(files.text(declaration_name(files.store(), files.store().items(declarations)[0])),
              "Outer");
    EXPECT_EQ(files.text(declaration_name(files.store(), files.store().items(declarations)[1])),
              "Index");
    EXPECT_EQ(files.text(declaration_name(files.store(), files.store().items(declarations)[2])),
              "Payload");
    EXPECT_EQ(files.text(declaration_name(files.store(), files.store().items(declarations)[3])),
              "Inner");
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

    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);
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
    EXPECT_TRUE(std::ranges::equal(
        files.store().items(files.store().soas.operations[files.store().soas.index(schema)]),
        all_storage_operations()));
    auto const& member{
        files.store().items(files.store().soas.members[files.store().soas.index(schema)])[0]};
    EXPECT_EQ(files.text(files.context.storage.cpp_types.spelling(resolve_type(
                  files.store().soa_members.type[files.store().soa_members.index(member)],
                  manifest.types))),
              "FHandle");
    ASSERT_TRUE(files.store()
                    .soa_members.relationship[files.store().soa_members.index(member)]
                    .has_value());
    EXPECT_EQ(
        files.store().semantic_relations.kind[files.store().semantic_relations.index(
            *files.store().soa_members.relationship[files.store().soa_members.index(member)])],
        SemanticRelationKind::offset_into);
    EXPECT_EQ(files.text(files.store().type_refs.name[files.store().type_refs.index(
                  files.store().semantic_relations.target[files.store().semantic_relations.index(
                      *files.store()
                           .soa_members.relationship[files.store().soa_members.index(member)])])]),
              "FData");
    EXPECT_EQ(
        files.store().semantic_relations.unit[files.store().semantic_relations.index(
            *files.store().soa_members.relationship[files.store().soa_members.index(member)])],
        SemanticRelationUnit::elements);
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    EXPECT_EQ(files.store().normal_modules.soa_backend[files.store().normal_modules.index(module)],
              SoaBackend::standard_library);
    EXPECT_FALSE(
        files.store()
            .module_settings_store
            .source[files.store().module_settings_store.index(
                files.store().normal_modules.settings[files.store().normal_modules.index(module)])]
            .has_value());
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

    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
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

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    ASSERT_TRUE(files.store().soas.field_mask_name[files.store().soas.index(schema)].has_value());
    EXPECT_EQ(files.text(*files.store().soas.field_mask_name[files.store().soas.index(schema)]),
              "FFieldMask");
    ASSERT_TRUE(files.store().soas.field_enum_name[files.store().soas.index(schema)].has_value());
    EXPECT_EQ(files.text(*files.store().soas.field_enum_name[files.store().soas.index(schema)]),
              "EField");
    auto const& member{
        files.store().items(files.store().soas.members[files.store().soas.index(schema)])[1]};
    EXPECT_TRUE(files.store().soa_members.mask_field[files.store().soa_members.index(member)]);
    ASSERT_EQ(
        files.store().soa_members.mask_dimensions[files.store().soa_members.index(member)].size(),
        2);
    EXPECT_EQ(
        files.text(
            files.store().soa_mask_dimensions.index_name[files.store().soa_mask_dimensions.index(
                files.store().items(files.store().soa_members.mask_dimensions
                                        [files.store().soa_members.index(member)])[0])]),
        "row_index");
    EXPECT_EQ(
        files.text(files.store().soa_mask_dimensions.extent[files.store().soa_mask_dimensions.index(
            files.store().items(
                files.store()
                    .soa_members.mask_dimensions[files.store().soa_members.index(member)])[0])]),
        "3");
    EXPECT_EQ(
        files.text(
            files.store().soa_mask_dimensions.index_name[files.store().soa_mask_dimensions.index(
                files.store().items(files.store().soa_members.mask_dimensions
                                        [files.store().soa_members.index(member)])[1])]),
        "column_index");
    EXPECT_EQ(
        files.text(files.store().soa_mask_dimensions.extent[files.store().soa_mask_dimensions.index(
            files.store().items(
                files.store()
                    .soa_members.mask_dimensions[files.store().soa_members.index(member)])[1])]),
        "4");
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              1);
    auto const& value{schema_at(manifest, 0)};
    EXPECT_EQ(
        files.text(files.store().packed_values.name[files.store().packed_values.index(value)]),
        "FighterState");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().packed_values.storage_type[files.store().packed_values.index(value)])]),
        "std::uint32_t");
    EXPECT_EQ(files.store().packed_values.byte_order[files.store().packed_values.index(value)],
              PackedByteOrder::big_endian);
    EXPECT_EQ(files.store().packed_values.bit_order[files.store().packed_values.index(value)],
              PackedBitOrder::most_significant_first);
    EXPECT_EQ(files.store().packed_values.invalid_value[files.store().packed_values.index(value)],
              std::uint64_t{0x7fffffff});
    EXPECT_TRUE(
        files.store().packed_values.mutable_value[files.store().packed_values.index(value)]);
    ASSERT_EQ(files.store().packed_values.segments[files.store().packed_values.index(value)].size(),
              3);
    auto const& index{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[0]};
    EXPECT_FALSE(
        files.store().packed_fields.bits[files.store().packed_fields.index(index)].has_value());
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(index)],
              PackedFieldKind::unsigned_integer);
    EXPECT_TRUE(files.store().packed_fields.range_helper[files.store().packed_fields.index(index)]);
    EXPECT_EQ(files.store().packed_fields.minimum_value[files.store().packed_fields.index(index)],
              0U);
    EXPECT_EQ(files.store().packed_fields.maximum_value[files.store().packed_fields.index(index)],
              1'000'000U);
    ASSERT_EQ(
        files.store().packed_fields.named_codes[files.store().packed_fields.index(index)].size(),
        2U);
    EXPECT_EQ(
        files.text(files.store().packed_named_codes.name[files.store().packed_named_codes.index(
            files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[0])]),
        "Player");
    EXPECT_EQ(
        files.store()
            .packed_named_codes.value[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[0])],
        42U);
    EXPECT_FALSE(
        files.store()
            .packed_named_codes.sentinel[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[0])]);
    EXPECT_EQ(
        files.text(files.store().packed_named_codes.name[files.store().packed_named_codes.index(
            files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[1])]),
        "Invalid");
    EXPECT_EQ(
        files.store()
            .packed_named_codes.value[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[1])],
        1'048'575U);
    EXPECT_TRUE(
        files.store()
            .packed_named_codes.sentinel[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(index)])[1])]);
    ASSERT_TRUE(files.store()
                    .packed_fields.relationship[files.store().packed_fields.index(index)]
                    .has_value());
    EXPECT_EQ(
        files.store().semantic_relations.kind[files.store().semantic_relations.index(
            *files.store().packed_fields.relationship[files.store().packed_fields.index(index)])],
        SemanticRelationKind::index_into);
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().semantic_relations.target[files.store().semantic_relations.index(
                *files.store()
                     .packed_fields.relationship[files.store().packed_fields.index(index)])])]),
        "FighterState");
    auto const& reserved{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[1]};
    EXPECT_EQ(
        files.text(files.store().packed_reserved_bits.name[files.store().packed_reserved_bits.index(
            reserved)]),
        "future");
    EXPECT_EQ(
        files.store().packed_reserved_bits.bits[files.store().packed_reserved_bits.index(reserved)],
        4);
    auto const& state{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[2]};
    EXPECT_EQ(files.store().packed_fields.bits[files.store().packed_fields.index(state)], 8);
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(state)],
              PackedFieldKind::enumeration);
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

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, 0)};
    EXPECT_FALSE(files.store()
                     .packed_values.byte_order[files.store().packed_values.index(value)]
                     .has_value());
    EXPECT_FALSE(files.store()
                     .packed_values.bit_order[files.store().packed_values.index(value)]
                     .has_value());
    auto const& delta{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[0]};
    EXPECT_EQ(files.text(files.store().type_refs.name[files.store().type_refs.index(
                  files.store().packed_fields.type[files.store().packed_fields.index(delta)])]),
              "std::int32_t");
    EXPECT_FALSE(
        files.store().packed_fields.bits[files.store().packed_fields.index(delta)].has_value());
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(delta)],
              PackedFieldKind::signed_integer);
    EXPECT_EQ(files.store().packed_fields.minimum_value[files.store().packed_fields.index(delta)],
              PackedIntegerValue{-100});
    EXPECT_EQ(files.store().packed_fields.maximum_value[files.store().packed_fields.index(delta)],
              PackedIntegerValue{100});
    ASSERT_EQ(
        files.store().packed_fields.named_codes[files.store().packed_fields.index(delta)].size(),
        2U);
    EXPECT_EQ(
        files.store()
            .packed_named_codes.value[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(delta)])[0])],
        PackedIntegerValue{0});
    EXPECT_EQ(
        files.store()
            .packed_named_codes.value[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(delta)])[1])],
        PackedIntegerValue{-128});
    EXPECT_TRUE(
        files.store()
            .packed_named_codes.sentinel[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .packed_fields.named_codes[files.store().packed_fields.index(delta)])[1])]);
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

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& health{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[0]};
    EXPECT_EQ(files.text(files.store().type_refs.name[files.store().type_refs.index(
                  files.store().packed_fields.type[files.store().packed_fields.index(health)])]),
              "project::HealthQ8");
    EXPECT_FALSE(
        files.store().packed_fields.bits[files.store().packed_fields.index(health)].has_value());
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(health)],
              PackedFieldKind::linear_quantized);
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

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& velocity{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[0]};
    EXPECT_EQ(files.text(files.store().type_refs.name[files.store().type_refs.index(
                  files.store().packed_fields.type[files.store().packed_fields.index(velocity)])]),
              "project::VelocityQ12_4");
    EXPECT_FALSE(
        files.store().packed_fields.bits[files.store().packed_fields.index(velocity)].has_value());
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(velocity)],
              PackedFieldKind::fixed_point);
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

    auto manifest{files.load()};
    auto const& value{schema_at(manifest, manifest.modules.size() - 1)};
    auto const& component{files.store().items(
        files.store().packed_values.segments[files.store().packed_values.index(value)])[0]};
    EXPECT_EQ(files.text(files.store().type_refs.name[files.store().type_refs.index(
                  files.store().packed_fields.type[files.store().packed_fields.index(component)])]),
              "project::PositionF12");
    EXPECT_FALSE(
        files.store().packed_fields.bits[files.store().packed_fields.index(component)].has_value());
    EXPECT_EQ(files.store().packed_fields.kind[files.store().packed_fields.index(component)],
              PackedFieldKind::mini_float);
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
    EXPECT_THROW(static_cast<void>(files.load()), lispb::codegen::sexpr::SourceError);

    files.write_root(R"(
(module packed
  :header "Packed.h"
  (packed-value Value
    :storage std::uint32_t
    :bit-order byte-first
    (field value std::uint32_t :bits 32)))
)");
    EXPECT_THROW(static_cast<void>(files.load()), lispb::codegen::sexpr::SourceError);
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              3U);
    auto const& scalar{schema_at(manifest, 0)};
    EXPECT_EQ(
        files.text(files.store().integer_scalars.name[files.store().integer_scalars.index(scalar)]),
        "DamageReason");
    EXPECT_FALSE(
        files.store().integer_scalars.signedness[files.store().integer_scalars.index(scalar)]);
    EXPECT_EQ(
        files.store().integer_scalars.minimum_value[files.store().integer_scalars.index(scalar)],
        PackedIntegerValue{0});
    EXPECT_EQ(
        files.store().integer_scalars.maximum_value[files.store().integer_scalars.index(scalar)],
        PackedIntegerValue{10});
    EXPECT_FALSE(files.store()
                     .integer_scalars.bit_width[files.store().integer_scalars.index(scalar)]
                     .has_value());
    EXPECT_EQ(
        files.store().integer_scalars.cpp_emission[files.store().integer_scalars.index(scalar)],
        IntegerScalarCppEmission::constants_with_names);
    ASSERT_TRUE(files.store()
                    .integer_scalars.cpp_type[files.store().integer_scalars.index(scalar)]
                    .has_value());
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            *files.store().integer_scalars.cpp_type[files.store().integer_scalars.index(scalar)])]),
        "std::uint8_t");
    ASSERT_EQ(files.store()
                  .integer_scalars.named_codes[files.store().integer_scalars.index(scalar)]
                  .size(),
              2U);
    EXPECT_EQ(
        files.store()
            .packed_named_codes.value[files.store().packed_named_codes.index(files.store().items(
                files.store()
                    .integer_scalars.named_codes[files.store().integer_scalars.index(scalar)])[1])],
        PackedIntegerValue{15});
    EXPECT_TRUE(files.store().packed_named_codes.sentinel[files.store().packed_named_codes.index(
        files.store().items(
            files.store()
                .integer_scalars.named_codes[files.store().integer_scalars.index(scalar)])[1])]);
    ASSERT_TRUE(files.store()
                    .integer_scalars.relationship[files.store().integer_scalars.index(scalar)]
                    .has_value());
    EXPECT_EQ(files.store().semantic_relations.kind[files.store().semantic_relations.index(
                  *files.store()
                       .integer_scalars.relationship[files.store().integer_scalars.index(scalar)])],
              SemanticRelationKind::index_into);
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().semantic_relations.target[files.store().semantic_relations.index(
                *files.store().integer_scalars.relationship[files.store().integer_scalars.index(
                    scalar)])])]),
        "EntityTable");
    EXPECT_FALSE(
        files.store()
            .semantic_relations
            .unit[files.store().semantic_relations.index(
                *files.store()
                     .integer_scalars.relationship[files.store().integer_scalars.index(scalar)])]
            .has_value());
    auto const& offset{schema_at(manifest, 0, 2)};
    ASSERT_TRUE(files.store()
                    .integer_scalars.relationship[files.store().integer_scalars.index(offset)]
                    .has_value());
    EXPECT_EQ(files.store().semantic_relations.kind[files.store().semantic_relations.index(
                  *files.store()
                       .integer_scalars.relationship[files.store().integer_scalars.index(offset)])],
              SemanticRelationKind::offset_into);
    EXPECT_EQ(files.store().semantic_relations.unit[files.store().semantic_relations.index(
                  *files.store()
                       .integer_scalars.relationship[files.store().integer_scalars.index(offset)])],
              SemanticRelationUnit::bytes);
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

    auto manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2U);
    auto const& module{manifest.modules[1]};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              6U);
    auto const& representation{schema_at(manifest, 1, 0)};
    EXPECT_EQ(files.text(files.store().linear_quantizeds.name[files.store().linear_quantizeds.index(
                  representation)]),
              "HealthQ8");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store()
                .linear_quantizeds.source[files.store().linear_quantizeds.index(representation)])]),
        "project::Health");
    EXPECT_EQ(
        files.store()
            .linear_quantizeds.bit_width[files.store().linear_quantizeds.index(representation)],
        8U);
    EXPECT_EQ(files.store().linear_quantizeds.reserved_codes[files.store().linear_quantizeds.index(
                  representation)],
              1U);
    EXPECT_EQ(
        files.store()
            .linear_quantizeds.clipping[files.store().linear_quantizeds.index(representation)],
        QuantizationClipping::clamp);
    auto const& varint{schema_at(manifest, 1, 1)};
    EXPECT_EQ(
        files.text(files.store().integer_varints.name[files.store().integer_varints.index(varint)]),
        "HealthVarint");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().integer_varints.source[files.store().integer_varints.index(varint)])]),
        "project::Health");
    EXPECT_EQ(files.store().integer_varints.encoding[files.store().integer_varints.index(varint)],
              IntegerVarintEncoding::unsigned_varint);
    auto const& fixed_point{schema_at(manifest, 1, 2)};
    EXPECT_EQ(
        files.text(files.store().fixed_points.name[files.store().fixed_points.index(fixed_point)]),
        "VelocityQ12_4");
    EXPECT_TRUE(
        files.store().fixed_points.signedness[files.store().fixed_points.index(fixed_point)]);
    EXPECT_EQ(files.store().fixed_points.total_bits[files.store().fixed_points.index(fixed_point)],
              16U);
    EXPECT_EQ(
        files.store().fixed_points.fractional_bits[files.store().fixed_points.index(fixed_point)],
        4U);
    EXPECT_EQ(files.store().fixed_points.rounding[files.store().fixed_points.index(fixed_point)],
              FixedPointRounding::toward_zero);
    EXPECT_EQ(
        files.text(files.store()
                       .fixed_points.minimum_value[files.store().fixed_points.index(fixed_point)]),
        "-2.5");
    EXPECT_EQ(
        files.text(files.store()
                       .fixed_points.maximum_value[files.store().fixed_points.index(fixed_point)]),
        "3.75");
    auto const& mini_float{schema_at(manifest, 1, 3)};
    EXPECT_EQ(
        files.text(files.store().mini_floats.name[files.store().mini_floats.index(mini_float)]),
        "CompactFloat");
    EXPECT_EQ(files.store().mini_floats.sign_bits[files.store().mini_floats.index(mini_float)], 1U);
    EXPECT_EQ(files.store().mini_floats.exponent_bits[files.store().mini_floats.index(mini_float)],
              5U);
    EXPECT_EQ(
        files.store().mini_floats.significand_bits[files.store().mini_floats.index(mini_float)],
        10U);
    EXPECT_EQ(files.store().mini_floats.exponent_bias[files.store().mini_floats.index(mini_float)],
              15);
    auto const& optional{schema_at(manifest, 1, 4)};
    EXPECT_EQ(
        files.text(files.store()
                       .optional_sentinels.name[files.store().optional_sentinels.index(optional)]),
        "OptionalHealth");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store()
                .optional_sentinels.source[files.store().optional_sentinels.index(optional)])]),
        "project::Health");
    EXPECT_EQ(
        files.text(files.store().optional_sentinels.sentinel[files.store().optional_sentinels.index(
            optional)]),
        "Invalid");
    auto const& presence{schema_at(manifest, 1, 5)};
    EXPECT_EQ(
        files.text(
            files.store()
                .optional_presence_bits.name[files.store().optional_presence_bits.index(presence)]),
        "PresentHealth");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().optional_presence_bits.source[files.store().optional_presence_bits.index(
                presence)])]),
        "project::Health");
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
                 lispb::codegen::sexpr::SourceError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 1 :significand-bits 10 :bias 15"),
                 lispb::codegen::sexpr::SourceError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 63 :bias 15"),
                 lispb::codegen::sexpr::SourceError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 15 :significand-bits 49 :bias 15"),
                 lispb::codegen::sexpr::SourceError);
    EXPECT_THROW(load_mini_float(":sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 32768"),
                 lispb::codegen::sexpr::SourceError);
}

TEST(SourceLoader, ReadsExplicitEnumBitWidth) {
    TemporaryManifest files;
    files.write_root(R"(
(module states
  :header "States.h"
  (enum State std::uint8_t
    :native-api true
    :helpers (to-string traits)
    :bit-width 3
    :signed false
    (value Idle :value "0")
    (value Active :value "7")))
)");

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    EXPECT_EQ(files.store().enums.bit_width[files.store().enums.index(schema)], 3);
    EXPECT_EQ(files.store().enums.signedness[files.store().enums.index(schema)],
              schema::EnumSignedness::unsigned_);
    EXPECT_TRUE(std::ranges::equal(
        files.store().items(files.store().enums.helpers[files.store().enums.index(schema)]),
        std::array{NativeEnumHelper::to_string, NativeEnumHelper::traits}));
    for (auto const options : {":native-api true :helpers (unknown)",
                               ":native-api true :helpers traits",
                               ":helpers (traits)"}) {
        files.write_root(
            std::string{"(module states :header \"States.h\" (enum State std::uint8_t "} + options +
            " (value Idle :value \"0\")))");
        EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
    }
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

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    EXPECT_FALSE(
        files.store().enums.underlying_type[files.store().enums.index(schema)].has_value());
    EXPECT_EQ(files.store().enums.bit_width[files.store().enums.index(schema)], 3);
    EXPECT_EQ(files.store().enums.signedness[files.store().enums.index(schema)],
              schema::EnumSignedness::unsigned_);
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

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    EXPECT_EQ(files.store().enums.signedness[files.store().enums.index(schema)],
              schema::EnumSignedness::signed_);
    EXPECT_FALSE(files.store().enums.bit_width[files.store().enums.index(schema)].has_value());
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

    auto manifest{files.load()};
    auto const& values{
        files.store().enums.values[files.store().enums.index(schema_at(manifest, 0))]};
    ASSERT_EQ(values.size(), 3U);
    EXPECT_FALSE(
        files.store()
            .enumerators.sentinel[files.store().enumerators.index(files.store().items(values)[0])]);
    EXPECT_TRUE(
        files.store()
            .enumerators.sentinel[files.store().enumerators.index(files.store().items(values)[1])]);
    EXPECT_TRUE(
        files.store()
            .enumerators.sentinel[files.store().enumerators.index(files.store().items(values)[2])]);
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

    EXPECT_THROW(static_cast<void>(files.load()), lispb::codegen::sexpr::SourceError);
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

    auto manifest{files.load()};
    auto const& values{
        files.store().enums.values[files.store().enums.index(schema_at(manifest, 0))]};
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(files.text(files.store().enumerators.initializer[files.store().enumerators.index(
                  files.store().items(values)[0])]),
              "0");
    EXPECT_EQ(files.text(files.store().enumerators.initializer[files.store().enumerators.index(
                  files.store().items(values)[1])]),
              "0x7f");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value -1)))
)schema");
    auto const negative_manifest{files.load()};
    auto const& negative_values{
        files.store().enums.values[files.store().enums.index(schema_at(negative_manifest, 0))]};
    EXPECT_EQ(files.text(files.store().enumerators.initializer[files.store().enumerators.index(
                  files.store().items(negative_values)[0])]),
              "-1");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value "static_cast<uint8>(1)")))
)schema");
    auto const opaque_manifest{files.load()};
    auto const& opaque_values{
        files.store().enums.values[files.store().enums.index(schema_at(opaque_manifest, 0))]};
    EXPECT_EQ(files.text(files.store().enumerators.initializer[files.store().enumerators.index(
                  files.store().items(opaque_values)[0])]),
              "static_cast<uint8>(1)");
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              2U);
    auto const& position{schema_at(manifest, 0, 0)};
    auto const& trail{schema_at(manifest, 0, 1)};
    EXPECT_EQ(files.text(files.store().records.name[files.store().records.index(position)]),
              "Position");
    ASSERT_EQ(files.store().records.members[files.store().records.index(position)].size(), 2U);
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store()
                .record_members.type[files.store().record_members.index(files.store().items(
                    files.store().records.members[files.store().records.index(position)])[0])])]),
        "float");
    EXPECT_FALSE(files.store()
                     .record_members
                     .count[files.store().record_members.index(files.store().items(
                         files.store().records.members[files.store().records.index(position)])[0])]
                     .has_value());
    EXPECT_EQ(
        files.text(files.store().records.export_specifier[files.store().records.index(trail)]),
        "PROJECT_API");
    EXPECT_EQ(
        files.store().record_members.count[files.store().record_members.index(files.store().items(
            files.store().records.members[files.store().records.index(trail)])[0])],
        4);
    ASSERT_TRUE(files.store()
                    .record_members
                    .relationship[files.store().record_members.index(files.store().items(
                        files.store().records.members[files.store().records.index(trail)])[0])]
                    .has_value());
    EXPECT_EQ(files.store().semantic_relations.kind[files.store().semantic_relations.index(
                  *files.store().record_members.relationship[files.store().record_members.index(
                      files.store().items(
                          files.store().records.members[files.store().records.index(trail)])[0])])],
              SemanticRelationKind::contains);
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().semantic_relations.target[files.store().semantic_relations.index(
                *files.store().record_members.relationship[files.store().record_members.index(
                    files.store().items(files.store().records.members[files.store().records.index(
                        trail)])[0])])])]),
        "Position");
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              1U);
    auto const& payload{schema_at(manifest, 0)};
    EXPECT_EQ(files.text(files.store().unions.name[files.store().unions.index(payload)]),
              "Payload");
    EXPECT_EQ(
        files.text(files.store().unions.export_specifier[files.store().unions.index(payload)]),
        "PROJECT_API");
    ASSERT_EQ(files.store().unions.alternatives[files.store().unions.index(payload)].size(), 2U);
    EXPECT_EQ(
        files.text(files.store().union_alternatives.name[files.store().union_alternatives.index(
            files.store().items(
                files.store().unions.alternatives[files.store().unions.index(payload)])[0])]),
        "identifier");
    EXPECT_FALSE(
        files.store()
            .union_alternatives
            .count[files.store().union_alternatives.index(files.store().items(
                files.store().unions.alternatives[files.store().unions.index(payload)])[0])]
            .has_value());
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store()
                .union_alternatives.type[files.store().union_alternatives.index(files.store().items(
                    files.store().unions.alternatives[files.store().unions.index(payload)])[1])])]),
        "std::uint8_t");
    EXPECT_EQ(
        files.store()
            .union_alternatives.count[files.store().union_alternatives.index(files.store().items(
                files.store().unions.alternatives[files.store().unions.index(payload)])[1])],
        12);
}

TEST(SourceLoader, RejectsInvalidRawUnionAlternatives) {
    TemporaryManifest files;
    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t :count 0)))
)");
    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);

    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (union Payload
    (alternative value std::uint32_t)
    (alternative value std::uint16_t)))
)");
    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);
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

    auto manifest{files.load()};
    auto const& module{manifest.modules[1]};
    ASSERT_EQ(files.store()
                  .normal_modules.declarations[files.store().normal_modules.index(module)]
                  .size(),
              1U);
    auto const& tagged{schema_at(manifest, 1)};
    EXPECT_EQ(
        files.text(files.store().tagged_unions.name[files.store().tagged_unions.index(tagged)]),
        "Event");
    EXPECT_EQ(
        files.text(files.store().type_refs.name[files.store().type_refs.index(
            files.store().tagged_unions.discriminant[files.store().tagged_unions.index(tagged)])]),
        "events::EventKind");
    EXPECT_EQ(
        files.text(files.store()
                       .tagged_unions.export_specifier[files.store().tagged_unions.index(tagged)]),
        "PROJECT_API");
    ASSERT_EQ(
        files.store().tagged_unions.alternatives[files.store().tagged_unions.index(tagged)].size(),
        2U);
    EXPECT_EQ(
        files.text(
            files.store()
                .tagged_union_alternatives
                .tag[files.store().tagged_union_alternatives.index(files.store().items(
                    files.store().tagged_unions.alternatives[files.store().tagged_unions.index(
                        tagged)])[0])]),
        "Spawn");
    EXPECT_FALSE(
        files.store()
            .tagged_union_alternatives
            .count[files.store().tagged_union_alternatives.index(files.store().items(
                files.store()
                    .tagged_unions.alternatives[files.store().tagged_unions.index(tagged)])[0])]
            .has_value());
    EXPECT_EQ(
        files.text(
            files.store()
                .tagged_union_alternatives
                .tag[files.store().tagged_union_alternatives.index(files.store().items(
                    files.store().tagged_unions.alternatives[files.store().tagged_unions.index(
                        tagged)])[1])]),
        "Damage");
    EXPECT_EQ(
        files.store().tagged_union_alternatives.count[files.store().tagged_union_alternatives.index(
            files.store().items(
                files.store()
                    .tagged_unions.alternatives[files.store().tagged_unions.index(tagged)])[1])],
        4);
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
    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);

    files.write_root(R"(
(module payloads
  :header "Payloads.h"
  (tagged-union Event
    :discriminant std::uint8_t
    (alternative first std::uint32_t :tag Value)
    (alternative second std::uint16_t :tag Value)))
)");
    EXPECT_THROW(lispb::schema::resolve_type_graph(files.load()), std::invalid_argument);
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
    } catch (lispb::codegen::sexpr::SourceError const& error) {
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

    EXPECT_THROW(static_cast<void>(files.load()), lispb::codegen::sexpr::SourceError);
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

TEST(SourceLoader, RejectsAllCombinedWithSpecificOperations) {
    TemporaryManifest files;
    files.write_root(R"(
(module bad
  :header "Bad.h"
  (struct FData
    :operations (all reset)
    (member values array int32)))
)");
    EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
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

    auto manifest{files.load()};
    auto const& facade{schema_at(manifest, 0)};
    EXPECT_TRUE(files.store().facades.reference_target[files.store().facades.index(facade)]);
    EXPECT_TRUE(files.store().facades.definitions_in_source[files.store().facades.index(facade)]);
    EXPECT_EQ(
        files.text(files.store().type_refs.suffix[files.store().type_refs.index(
            files.store()
                .facade_methods.return_type[files.store().facade_methods.index(files.store().items(
                    files.store().facades.methods[files.store().facades.index(facade)])[0])])]),
        " const&");
    EXPECT_EQ(
        files.text(files.store().facade_methods.target_name[files.store().facade_methods.index(
            files.store().items(
                files.store().facades.methods[files.store().facades.index(facade)])[0])]),
        "get_value");
    EXPECT_TRUE(files.store()
                    .facade_methods.is_const[files.store().facade_methods.index(files.store().items(
                        files.store().facades.methods[files.store().facades.index(facade)])[0])]);
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

    auto manifest{files.load()};
    ASSERT_EQ(manifest.modules.size(), 2);
#ifdef _WIN32
    auto const newline{std::string{"\r\n"}};
#else
    auto const newline{std::string{"\n"}};
#endif

    auto const& soa{manifest.modules[0]};
    ASSERT_EQ(
        files.store()
            .module_settings_store
            .prelude_lines[files.store().module_settings_store.index(
                files.store().normal_modules.settings[files.store().normal_modules.index(soa)])]
            .size(),
        1);
    EXPECT_EQ(
        files.text(files.store().items(
            files.store()
                .module_settings_store.prelude_lines[files.store().module_settings_store.index(
                    files.store()
                        .normal_modules.settings[files.store().normal_modules.index(soa)])])[0]),
        "class FForward;" + newline + "#define GENERATED_PATH \"C:\\\\generated\"");
    auto const& data{schema_at(manifest, 0)};
    ASSERT_EQ(files.store().soas.functions[files.store().soas.index(data)].size(), 2);
    ASSERT_EQ(files.store()
                  .functions
                  .body_lines[files.store().functions.index(files.store().items(
                      files.store().soas.functions[files.store().soas.index(data)])[0])]
                  .size(),
              1);
    EXPECT_EQ(
        files.text(files.store().items(
            files.store().functions.body_lines[files.store().functions.index(files.store().items(
                files.store().soas.functions[files.store().soas.index(data)])[0])])[0]),
        "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
            "values[0] += dt;");
    ASSERT_EQ(files.store()
                  .functions
                  .body_lines[files.store().functions.index(files.store().items(
                      files.store().soas.functions[files.store().soas.index(data)])[1])]
                  .size(),
              1);
    EXPECT_EQ(
        files.text(files.store().items(
            files.store().functions.body_lines[files.store().functions.index(files.store().items(
                files.store().soas.functions[files.store().soas.index(data)])[1])])[0]),
        "values[0] = 0;");

    auto const& facade{schema_at(manifest, 1)};
    ASSERT_EQ(files.store().facades.validation_lines[files.store().facades.index(facade)].size(),
              1);
    EXPECT_EQ(files.text(files.store().items(
                  files.store().facades.validation_lines[files.store().facades.index(facade)])[0]),
              "checkf(target != nullptr, TEXT(\"missing target\"));");
}

TEST(SourceLoader, AcceptsEmptyCppBodyAndRejectsWrongRawTag) {
    TemporaryManifest files;
    files.write_root(R"(
(module example
  :header "Generated.h"
  (struct FData
    (function empty void :body #cpp{}cpp#)))
)");
    auto manifest{files.load()};
    auto const& function{files.store().items(
        files.store().soas.functions[files.store().soas.index(schema_at(manifest, 0))])[0]};
    ASSERT_EQ(files.store().functions.body_lines[files.store().functions.index(function)].size(),
              1);
    EXPECT_EQ(files.text(files.store().items(
                  files.store().functions.body_lines[files.store().functions.index(function)])[0]),
              "");

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
    ASSERT_EQ(files.store()
                  .settings_modules.settings_list[files.store().settings_modules.index(module)]
                  .size(),
              1);
    EXPECT_EQ(
        files.store().settings.apply_mode[files.store().settings.index(files.store().items(
            files.store()
                .settings_modules.settings_list[files.store().settings_modules.index(module)])[0])],
        SettingApplyMode::deferred);
    EXPECT_EQ(
        files.store().settings.device[files.store().settings.index(files.store().items(
            files.store()
                .settings_modules.settings_list[files.store().settings_modules.index(module)])[0])],
        SettingDevice::keyboard_mouse);
    EXPECT_EQ(
        files.store().setting_controls.kind[files.store().setting_controls.index(
            files.store().settings.control[files.store().settings.index(files.store().items(
                files.store().settings_modules.settings_list[files.store().settings_modules.index(
                    module)])[0])])],
        SettingControlKind::float_range);
    EXPECT_EQ(
        files.store().setting_controls.step[files.store().setting_controls.index(
            files.store().settings.control[files.store().settings.index(files.store().items(
                files.store().settings_modules.settings_list[files.store().settings_modules.index(
                    module)])[0])])],
        0.5);
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

    auto manifest{files.load()};
    auto const& module{manifest.modules.front()};
    EXPECT_EQ(
        files.store().settings.device[files.store().settings.index(files.store().items(
            files.store()
                .settings_modules.settings_list[files.store().settings_modules.index(module)])[0])],
        SettingDevice::shared);
}

TEST(SourceLoader, SingleOnlyLogicalApiIsPreserved) {
    TemporaryManifest files;
    files.write_root(R"(
(module api :header "Api.h" :source "Api.cpp" :backend standard-library
  (struct Rows :view-name Values :const-view-name ConstValues
    :export-specifier API :using-declarations ("Value = float")
    :operations (set-num append-from) :equivalent-type ValueRow
    (member values array float)
    (function first float :const true :body #cpp{return $column(values)[0];}cpp#)
    (function noop void :body ())
    (view-function read float :body #cpp{return $column(values)[0];}cpp#)
    (mutable-view-function write void :body #cpp{$column(values)[0] = 1;}cpp#)
    (single-allocation Owner)))
)");
    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    EXPECT_EQ(files.store().soas.selected_storage(schema), SoaStorage::single_allocation);
    ASSERT_EQ(files.store().soas.const_view_functions[files.store().soas.index(schema)].size(), 1U);
    EXPECT_TRUE(files.store().functions.is_const[files.store().functions.index(files.store().items(
        files.store().soas.const_view_functions[files.store().soas.index(schema)])[0])]);
    ASSERT_EQ(files.store().soas.mutable_view_functions[files.store().soas.index(schema)].size(),
              1U);
    auto const output{files.render(manifest).front().content};
    EXPECT_NE(output.find("struct API Owner"), std::string::npos);
    EXPECT_NE(output.find("std::pmr::memory_resource* resource"), std::string::npos);
    EXPECT_NE(output.find("return this->values()[0];"), std::string::npos);
    EXPECT_EQ(output.find("struct Rows {"), std::string::npos);
    EXPECT_NE(output.find("void noop() {"), std::string::npos);
}

TEST(SourceLoader, RejectsRemovedSingleAllocationAllocatorSyntax) {
    TemporaryManifest files;
    for (auto const declaration : {"(struct Rows :single-allocation-allocator Allocator (member "
                                   "ids array int) (single-allocation Owner))",
                                   "(struct Rows (member ids array int) (single-allocation Owner "
                                   "(variant Pool Allocator)))"}) {
        files.write_root(std::string{"(module api :header \"Api.h\" "} + declaration + ")");
        EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
    }
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
    } catch (lispb::codegen::sexpr::SourceError const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("view-function must be const"));
    }
}

} // namespace
} // namespace lispb::codegen
