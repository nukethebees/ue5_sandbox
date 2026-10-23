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
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
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
    auto& store{files.store()};
    auto& record_members{store.record_members};
    auto& records{store.records};
    files.write_root(R"((module records :header "Records.h"
      (record Value :comparison three-way :comparison-noexcept true
        (member absent int)
        (member zero int :initializer "")
        (member explicit_value int :initializer "7")
        (function read int :const true :noexcept true :constexpr true :nodiscard true
          :body ("return explicit_value;"))
        (function validate bool :const true :noexcept true))))");
    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{records.index(schema)};
    auto const members{store.items(records.members[schema_row])};
    EXPECT_FALSE(record_members.initializer[record_members.index(members[0])].has_value());
    EXPECT_EQ(files.text(record_members.initializer[record_members.index(members[1])]), "");
    EXPECT_EQ(files.text(record_members.initializer[record_members.index(members[2])]), "7");
    EXPECT_EQ(records.comparison[schema_row], RecordComparison::three_way);
    ASSERT_EQ(records.functions[schema_row].size(), 2U);
    EXPECT_TRUE(
        store.functions
            .is_constexpr[store.functions.index(store.items(records.functions[schema_row])[0])]);
    auto const graph{lispb::schema::resolve_type_graph(manifest)};
    auto const& record{graph.definition(*find_test_type(graph, "records", "Value"))};
    auto const& definitions{graph.definitions()};
    auto const& semantic_members{definitions.record_members};
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
    auto& store{files.store()};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
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
        store.items(normal_modules.declarations[normal_modules.index(manifest.modules.front())])
            .front()};
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
    EXPECT_EQ(
        files.text(type_refs.name[type_refs.index(store.functions.return_type[store.functions.index(
            store.items(store.records.functions[store.records.index(editable)])[0])])]),
        "Renamed");
    auto const output{files.render(manifest).front().content};
    EXPECT_LT(output.find("struct B;"), output.find("struct A {"));
    EXPECT_LT(output.find("struct A;"), output.find("struct B {"));
    EXPECT_LT(output.find("struct A {"), output.find("struct B {"));
    EXPECT_NE(output.find("auto ref() -> B const&;"), std::string::npos);
    EXPECT_EQ(
        files.text(declaration_name(
            store,
            store.items(normal_modules.declarations[normal_modules.index(manifest.modules.front())])
                .front())),
        "A");
}

TEST(SourceLoader, RecordMethodSignaturesOrderAliasesEnumsAndHeaderBodyValues) {
    TemporaryManifest files;
    auto& store{files.store()};
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
            store,
            store
                .items(store.normal_modules
                           .declarations[store.normal_modules.index(manifest.modules.front())])
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
    auto& store{files.store()};
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
            store
                .items(store.normal_modules
                           .declarations[store.normal_modules.index(manifest.modules.front())])
                .front()};
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
      (module providers :header "Providers.h" :namespace providers
        (struct Rows (member xs array float))
        (record Registered))
      (module global :header "Global.h" (record Global)))");
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
          (record Consumer )"} +
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
      (record Consumer (function make (type-ref providers::Layout :suffix "*"))))
      (module providers :header "Providers.h" :namespace providers
        (struct Layout :layout-only true (member xs array float))))");
    try {
        static_cast<void>(files.graph());
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("has no C++ owner")) << error.what();
    }
    files.write_root(R"((module values :header "Values.h"
      (record A (function make B :body ("return {};")))
      (record B (function make A :body ("return {};")))))");
    try {
        static_cast<void>(files.graph());
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
        static_cast<void>(files.graph());
        FAIL();
    } catch (std::invalid_argument const& error) {
        EXPECT_TRUE(std::string{error.what()}.contains("complete-definition dependency cycle"));
        EXPECT_TRUE(std::string{error.what()}.contains("first::A -> second::B -> first::A"));
    }
}

TEST(SourceLoader, RejectsRemovedSoaRepresentationSyntax) {
    TemporaryManifest files;
    for (auto const declaration :
         {"(struct Rows :storage vector (member values array float))",
          "(struct Rows :storage both (member values array float))",
          "(struct Rows :storage single-allocation (member values array float))",
          "(struct Rows :vector-components (values) (member values array float))",
          "(struct Rows (member values array float) (single-allocation Owner))",
          "(struct Rows (member values array float) (fixed FixedRows))",
          "(struct Rows (member child nested Child :nested-schema Child))",
          "(vector-soa Rows :value-type float :components (xs ys) :equivalent-type Point)"}) {
        SCOPED_TRACE(declaration);
        files.write_root(std::string{R"((module rows :header "Rows.h" )"} + declaration + ")");
        EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
    }
}

TEST(SourceLoader, AliasEmissionKeepsSemanticIdentityAndDependencies) {
    TemporaryManifest files;
    files.write_root(
        R"((module vitals :header "Vitals.h" :namespace fixture
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
        auto const spelling{std::string_view{representation} == "uint32" ? "std::uint32_t"
                                                                         : "std::int32_t"};
        EXPECT_TRUE(header.contains("using Value = " + std::string{spelling} + ";"));
        auto const include{"#include <cstdint>"};
        auto const position{header.find(include)};
        ASSERT_NE(position, std::string::npos);
        EXPECT_EQ(header.find(include, position + 1), std::string::npos);
        EXPECT_EQ(header.find("CoreTypes.h"), std::string::npos);
    }
}

TEST(SourceLoader, PackedDefaultsAreValidatedWithoutChangingImplicitSentinels) {
    TemporaryManifest files;
    auto& store{files.store()};
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
        store.packed_fields.default_value[store.packed_fields.index(store.items(
            store.packed_values.segments[store.packed_values.index(schema_at(manifest, 0))])[0])],
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
    auto& store{files.store()};
    auto& packed_fields{store.packed_fields};
    auto& type_refs{store.type_refs};
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
    auto field{store.items(
        store.packed_values.segments[store.packed_values.index(schema_at(manifest, 0))])[0]};
    auto const field_row{packed_fields.index(field)};
    EXPECT_FALSE(lispb::schema::packed_integer_domain(graph, type, field).has_value());
    packed_fields.bits[field_row].reset();
    EXPECT_TRUE(lispb::schema::packed_integer_domain(graph, type, field).has_value());
    packed_fields.bits[field_row] = 32;
    packed_fields.type[field_row] = [&] {
        auto const new_type_refs{type_refs.append()};
        type_refs.name[type_refs.index(new_type_refs)] =
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
    auto& store{files.store()};
    auto& normal_modules{store.normal_modules};
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
    auto& store{files.store()};
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
        store.normal_modules.declarations[store.normal_modules.index(manifest.modules.front())]};
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
  (enum FDataView uint8
    (value One))
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
    auto const& member{store.items(store.soas.members[store.soas.index(schema)])[0]};
    auto const member_row{soa_members.index(member)};
    EXPECT_EQ(files.text(files.context.storage.cpp_types.spelling(
                  resolve_type(soa_members.type[member_row], manifest.types))),
              "FHandle");
    ASSERT_TRUE(soa_members.relationship[member_row].has_value());
    auto const relation_row{semantic_relations.index(*soa_members.relationship[member_row])};
    EXPECT_EQ(semantic_relations.kind[relation_row], SemanticRelationKind::offset_into);
    EXPECT_EQ(
        files.text(
            store.type_refs.name[store.type_refs.index(semantic_relations.target[relation_row])]),
        "FData");
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
          (member engine_value @engine_integer))))");
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
    auto& packed_fields{store.packed_fields};
    auto& packed_named_codes{store.packed_named_codes};
    auto& packed_reserved_bits{store.packed_reserved_bits};
    auto& packed_values{store.packed_values};
    auto& semantic_relations{store.semantic_relations};
    auto& type_refs{store.type_refs};
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 1);
    auto const& value{schema_at(manifest, 0)};
    auto const value_row{packed_values.index(value)};
    EXPECT_EQ(files.text(packed_values.name[value_row]), "FighterState");
    EXPECT_EQ(files.text(type_refs.name[type_refs.index(packed_values.storage_type[value_row])]),
              "std::uint32_t");
    EXPECT_EQ(packed_values.byte_order[value_row], PackedByteOrder::big_endian);
    EXPECT_EQ(packed_values.bit_order[value_row], PackedBitOrder::most_significant_first);
    EXPECT_EQ(packed_values.invalid_value[value_row], std::uint64_t{0x7fffffff});
    EXPECT_TRUE(packed_values.mutable_value[value_row]);
    ASSERT_EQ(packed_values.segments[value_row].size(), 3);
    auto const& index{store.items(packed_values.segments[value_row])[0]};
    auto const index_row{packed_fields.index(index)};
    EXPECT_FALSE(packed_fields.bits[index_row].has_value());
    EXPECT_EQ(packed_fields.kind[index_row], PackedFieldKind::unsigned_integer);
    EXPECT_TRUE(packed_fields.range_helper[index_row]);
    EXPECT_EQ(packed_fields.minimum_value[index_row], 0U);
    EXPECT_EQ(packed_fields.maximum_value[index_row], 1'000'000U);
    ASSERT_EQ(packed_fields.named_codes[index_row].size(), 2U);
    auto const codes{store.items(packed_fields.named_codes[index_row])};
    auto const player_row{packed_named_codes.index(codes[0])};
    auto const invalid_row{packed_named_codes.index(codes[1])};
    EXPECT_EQ(files.text(packed_named_codes.name[player_row]), "Player");
    EXPECT_EQ(packed_named_codes.value[player_row], 42U);
    EXPECT_FALSE(packed_named_codes.sentinel[player_row]);
    EXPECT_EQ(files.text(packed_named_codes.name[invalid_row]), "Invalid");
    EXPECT_EQ(packed_named_codes.value[invalid_row], 1'048'575U);
    EXPECT_TRUE(packed_named_codes.sentinel[invalid_row]);
    ASSERT_TRUE(packed_fields.relationship[index_row].has_value());
    EXPECT_EQ(
        semantic_relations.kind[semantic_relations.index(*packed_fields.relationship[index_row])],
        SemanticRelationKind::index_into);
    EXPECT_EQ(files.text(
                  type_refs.name[type_refs.index(semantic_relations.target[semantic_relations.index(
                      *packed_fields.relationship[index_row])])]),
              "FighterState");
    auto const& reserved{store.items(packed_values.segments[value_row])[1]};
    auto const reserved_row{packed_reserved_bits.index(reserved)};
    EXPECT_EQ(files.text(packed_reserved_bits.name[reserved_row]), "future");
    EXPECT_EQ(packed_reserved_bits.bits[reserved_row], 4);
    auto const& state{store.items(packed_values.segments[value_row])[2]};
    auto const state_row{packed_fields.index(state)};
    EXPECT_EQ(packed_fields.bits[state_row], 8);
    EXPECT_EQ(packed_fields.kind[state_row], PackedFieldKind::enumeration);
}

TEST(SourceLoader, ReadsSignedArbitraryWidthPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.packed_fields};
    auto& packed_named_codes{store.packed_named_codes};
    auto& packed_values{store.packed_values};
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
    auto const value_row{packed_values.index(value)};
    EXPECT_FALSE(packed_values.byte_order[value_row].has_value());
    EXPECT_FALSE(packed_values.bit_order[value_row].has_value());
    auto const& delta{store.items(packed_values.segments[value_row])[0]};
    auto const delta_row{packed_fields.index(delta)};
    EXPECT_EQ(
        files.text(store.type_refs.name[store.type_refs.index(packed_fields.type[delta_row])]),
        "std::int32_t");
    EXPECT_FALSE(packed_fields.bits[delta_row].has_value());
    EXPECT_EQ(packed_fields.kind[delta_row], PackedFieldKind::signed_integer);
    EXPECT_EQ(packed_fields.minimum_value[delta_row], PackedIntegerValue{-100});
    EXPECT_EQ(packed_fields.maximum_value[delta_row], PackedIntegerValue{100});
    ASSERT_EQ(packed_fields.named_codes[delta_row].size(), 2U);
    EXPECT_EQ(
        packed_named_codes
            .value[packed_named_codes.index(store.items(packed_fields.named_codes[delta_row])[0])],
        PackedIntegerValue{0});
    EXPECT_EQ(
        packed_named_codes
            .value[packed_named_codes.index(store.items(packed_fields.named_codes[delta_row])[1])],
        PackedIntegerValue{-128});
    EXPECT_TRUE(packed_named_codes.sentinel[packed_named_codes.index(
        store.items(packed_fields.named_codes[delta_row])[1])]);
}

TEST(SourceLoader, ReadsLinearQuantizedPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.packed_fields};
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
    auto const& health{
        store.items(store.packed_values.segments[store.packed_values.index(value)])[0]};
    auto const health_row{packed_fields.index(health)};
    EXPECT_EQ(
        files.text(store.type_refs.name[store.type_refs.index(packed_fields.type[health_row])]),
        "project::HealthQ8");
    EXPECT_FALSE(packed_fields.bits[health_row].has_value());
    EXPECT_EQ(packed_fields.kind[health_row], PackedFieldKind::linear_quantized);
}

TEST(SourceLoader, ReadsFixedPointPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.packed_fields};
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
    auto const& velocity{
        store.items(store.packed_values.segments[store.packed_values.index(value)])[0]};
    auto const velocity_row{packed_fields.index(velocity)};
    EXPECT_EQ(
        files.text(store.type_refs.name[store.type_refs.index(packed_fields.type[velocity_row])]),
        "project::VelocityQ12_4");
    EXPECT_FALSE(packed_fields.bits[velocity_row].has_value());
    EXPECT_EQ(packed_fields.kind[velocity_row], PackedFieldKind::fixed_point);
}

TEST(SourceLoader, ReadsMiniFloatPackedField) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& packed_fields{store.packed_fields};
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
    auto const& component{
        store.items(store.packed_values.segments[store.packed_values.index(value)])[0]};
    auto const component_row{packed_fields.index(component)};
    EXPECT_EQ(
        files.text(store.type_refs.name[store.type_refs.index(packed_fields.type[component_row])]),
        "project::PositionF12");
    EXPECT_FALSE(packed_fields.bits[component_row].has_value());
    EXPECT_EQ(packed_fields.kind[component_row], PackedFieldKind::mini_float);
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 3U);
    auto const& scalar{schema_at(manifest, 0)};
    auto const scalar_row{integer_scalars.index(scalar)};
    EXPECT_EQ(files.text(integer_scalars.name[scalar_row]), "DamageReason");
    EXPECT_FALSE(integer_scalars.signedness[scalar_row]);
    EXPECT_EQ(integer_scalars.minimum_value[scalar_row], PackedIntegerValue{0});
    EXPECT_EQ(integer_scalars.maximum_value[scalar_row], PackedIntegerValue{10});
    EXPECT_FALSE(integer_scalars.bit_width[scalar_row].has_value());
    EXPECT_EQ(integer_scalars.cpp_emission[scalar_row],
              IntegerScalarCppEmission::constants_with_names);
    ASSERT_TRUE(integer_scalars.cpp_type[scalar_row].has_value());
    EXPECT_EQ(files.text(type_refs.name[type_refs.index(*integer_scalars.cpp_type[scalar_row])]),
              "std::uint8_t");
    ASSERT_EQ(integer_scalars.named_codes[scalar_row].size(), 2U);
    EXPECT_EQ(packed_named_codes.value[packed_named_codes.index(
                  store.items(integer_scalars.named_codes[scalar_row])[1])],
              PackedIntegerValue{15});
    EXPECT_TRUE(packed_named_codes.sentinel[packed_named_codes.index(
        store.items(integer_scalars.named_codes[scalar_row])[1])]);
    ASSERT_TRUE(integer_scalars.relationship[scalar_row].has_value());
    EXPECT_EQ(semantic_relations
                  .kind[semantic_relations.index(*integer_scalars.relationship[scalar_row])],
              SemanticRelationKind::index_into);
    EXPECT_EQ(files.text(
                  type_refs.name[type_refs.index(semantic_relations.target[semantic_relations.index(
                      *integer_scalars.relationship[scalar_row])])]),
              "EntityTable");
    EXPECT_FALSE(
        semantic_relations.unit[semantic_relations.index(*integer_scalars.relationship[scalar_row])]
            .has_value());
    auto const& offset{schema_at(manifest, 0, 2)};
    auto const offset_row{integer_scalars.index(offset)};
    ASSERT_TRUE(integer_scalars.relationship[offset_row].has_value());
    EXPECT_EQ(semantic_relations
                  .kind[semantic_relations.index(*integer_scalars.relationship[offset_row])],
              SemanticRelationKind::offset_into);
    EXPECT_EQ(semantic_relations
                  .unit[semantic_relations.index(*integer_scalars.relationship[offset_row])],
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 6U);
    auto const& representation{schema_at(manifest, 1, 0)};
    auto const representation_row{linear_quantizeds.index(representation)};
    EXPECT_EQ(files.text(linear_quantizeds.name[representation_row]), "HealthQ8");
    EXPECT_EQ(
        files.text(type_refs.name[type_refs.index(linear_quantizeds.source[representation_row])]),
        "project::Health");
    EXPECT_EQ(linear_quantizeds.bit_width[representation_row], 8U);
    EXPECT_EQ(linear_quantizeds.reserved_codes[representation_row], 1U);
    EXPECT_EQ(linear_quantizeds.clipping[representation_row], QuantizationClipping::clamp);
    auto const& varint{schema_at(manifest, 1, 1)};
    auto const varint_row{integer_varints.index(varint)};
    EXPECT_EQ(files.text(integer_varints.name[varint_row]), "HealthVarint");
    EXPECT_EQ(files.text(type_refs.name[type_refs.index(integer_varints.source[varint_row])]),
              "project::Health");
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
    EXPECT_EQ(files.text(type_refs.name[type_refs.index(optional_sentinels.source[optional_row])]),
              "project::Health");
    EXPECT_EQ(files.text(optional_sentinels.sentinel[optional_row]), "Invalid");
    auto const& presence{schema_at(manifest, 1, 5)};
    auto const presence_row{optional_presence_bits.index(presence)};
    EXPECT_EQ(files.text(optional_presence_bits.name[presence_row]), "PresentHealth");
    EXPECT_EQ(
        files.text(type_refs.name[type_refs.index(optional_presence_bits.source[presence_row])]),
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
    (value Active :value "7")))
)");

    auto manifest{files.load()};
    auto const& schema{schema_at(manifest, 0)};
    auto const schema_row{enums.index(schema)};
    EXPECT_EQ(enums.bit_width[schema_row], 3);
    EXPECT_EQ(enums.signedness[schema_row], schema::EnumSignedness::unsigned_);
    EXPECT_TRUE(
        std::ranges::equal(store.items(enums.helpers[schema_row]),
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
    auto& store{files.store()};
    auto& enums{store.enums};
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
    (value Above :value "1")))
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
    (value Pending :value "0xfe" :sentinel true)))
)");

    auto manifest{files.load()};
    auto const& values{store.enums.values[store.enums.index(schema_at(manifest, 0))]};
    ASSERT_EQ(values.size(), 3U);
    EXPECT_FALSE(enumerators.sentinel[enumerators.index(store.items(values)[0])]);
    EXPECT_TRUE(enumerators.sentinel[enumerators.index(store.items(values)[1])]);
    EXPECT_TRUE(enumerators.sentinel[enumerators.index(store.items(values)[2])]);
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
    auto& store{files.store()};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Zero :value 0)
    (value High :value "0x7f")))
)schema");

    auto manifest{files.load()};
    auto const& values{enums.values[enums.index(schema_at(manifest, 0))]};
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(files.text(enumerators.initializer[enumerators.index(store.items(values)[0])]), "0");
    EXPECT_EQ(files.text(enumerators.initializer[enumerators.index(store.items(values)[1])]),
              "0x7f");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value -1)))
)schema");
    auto const negative_manifest{files.load()};
    auto const& negative_values{enums.values[enums.index(schema_at(negative_manifest, 0))]};
    EXPECT_EQ(
        files.text(enumerators.initializer[enumerators.index(store.items(negative_values)[0])]),
        "-1");

    files.write_root(R"schema(
(module enums
  :header "Enums.h"
  (enum State uint8
    (value Invalid :value "static_cast<uint8>(1)")))
)schema");
    auto const opaque_manifest{files.load()};
    auto const& opaque_values{enums.values[enums.index(schema_at(opaque_manifest, 0))]};
    EXPECT_EQ(files.text(enumerators.initializer[enumerators.index(store.items(opaque_values)[0])]),
              "static_cast<uint8>(1)");
}

TEST(SourceLoader, ReadsRecordModuleAndFixedArrays) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& record_members{store.record_members};
    auto& records{store.records};
    auto& semantic_relations{store.semantic_relations};
    auto& type_refs{store.type_refs};
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 2U);
    auto const& position{schema_at(manifest, 0, 0)};
    auto const position_row{records.index(position)};
    auto const& trail{schema_at(manifest, 0, 1)};
    auto const trail_row{records.index(trail)};
    EXPECT_EQ(files.text(records.name[position_row]), "Position");
    ASSERT_EQ(records.members[position_row].size(), 2U);
    EXPECT_EQ(files.text(type_refs.name[type_refs.index(record_members.type[record_members.index(
                  store.items(records.members[position_row])[0])])]),
              "float");
    EXPECT_FALSE(
        record_members.count[record_members.index(store.items(records.members[position_row])[0])]
            .has_value());
    EXPECT_EQ(files.text(records.export_specifier[trail_row]), "PROJECT_API");
    EXPECT_EQ(
        record_members.count[record_members.index(store.items(records.members[trail_row])[0])], 4);
    ASSERT_TRUE(record_members
                    .relationship[record_members.index(store.items(records.members[trail_row])[0])]
                    .has_value());
    EXPECT_EQ(semantic_relations
                  .kind[semantic_relations.index(*record_members.relationship[record_members.index(
                      store.items(records.members[trail_row])[0])])],
              SemanticRelationKind::contains);
    EXPECT_EQ(files.text(
                  type_refs.name[type_refs.index(semantic_relations.target[semantic_relations.index(
                      *record_members.relationship[record_members.index(
                          store.items(records.members[trail_row])[0])])])]),
              "Position");
}

TEST(SourceLoader, ReadsRawUnionModuleAndFixedArrayAlternatives) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& union_alternatives{store.union_alternatives};
    auto& unions{store.unions};
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 1U);
    auto const& payload{schema_at(manifest, 0)};
    auto const payload_row{unions.index(payload)};
    EXPECT_EQ(files.text(unions.name[payload_row]), "Payload");
    EXPECT_EQ(files.text(unions.export_specifier[payload_row]), "PROJECT_API");
    ASSERT_EQ(unions.alternatives[payload_row].size(), 2U);
    EXPECT_EQ(files.text(union_alternatives.name[union_alternatives.index(
                  store.items(unions.alternatives[payload_row])[0])]),
              "identifier");
    EXPECT_FALSE(
        union_alternatives
            .count[union_alternatives.index(store.items(unions.alternatives[payload_row])[0])]
            .has_value());
    EXPECT_EQ(
        files.text(store.type_refs
                       .name[store.type_refs.index(union_alternatives.type[union_alternatives.index(
                           store.items(unions.alternatives[payload_row])[1])])]),
        "std::uint8_t");
    EXPECT_EQ(
        union_alternatives
            .count[union_alternatives.index(store.items(unions.alternatives[payload_row])[1])],
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
    auto& store{files.store()};
    auto& tagged_union_alternatives{store.tagged_union_alternatives};
    auto& tagged_unions{store.tagged_unions};
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
    ASSERT_EQ(store.normal_modules.declarations[store.normal_modules.index(module)].size(), 1U);
    auto const& tagged{schema_at(manifest, 1)};
    auto const tagged_row{tagged_unions.index(tagged)};
    EXPECT_EQ(files.text(tagged_unions.name[tagged_row]), "Event");
    EXPECT_EQ(
        files.text(
            store.type_refs.name[store.type_refs.index(tagged_unions.discriminant[tagged_row])]),
        "events::EventKind");
    EXPECT_EQ(files.text(tagged_unions.export_specifier[tagged_row]), "PROJECT_API");
    ASSERT_EQ(tagged_unions.alternatives[tagged_row].size(), 2U);
    EXPECT_EQ(files.text(tagged_union_alternatives.tag[tagged_union_alternatives.index(
                  store.items(tagged_unions.alternatives[tagged_row])[0])]),
              "Spawn");
    EXPECT_FALSE(tagged_union_alternatives
                     .count[tagged_union_alternatives.index(
                         store.items(tagged_unions.alternatives[tagged_row])[0])]
                     .has_value());
    EXPECT_EQ(files.text(tagged_union_alternatives.tag[tagged_union_alternatives.index(
                  store.items(tagged_unions.alternatives[tagged_row])[1])]),
              "Damage");
    EXPECT_EQ(tagged_union_alternatives.count[tagged_union_alternatives.index(
                  store.items(tagged_unions.alternatives[tagged_row])[1])],
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
    EXPECT_EQ(
        files.text(
            store.type_refs.suffix[store.type_refs.index(facade_methods.return_type[method_row])]),
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
    auto const update_body{store.items(functions.body_lines[functions.index(members[0])])};
    ASSERT_EQ(update_body.size(), 1);
    EXPECT_EQ(files.text(update_body[0]),
              "if (dt <= 0.0f) {" + newline + "    return;" + newline + "}" + newline + newline +
                  "values[0] += dt;");
    auto const reset_body{store.items(functions.body_lines[functions.index(members[1])])};
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
    auto const& function{
        store.items(store.soas.functions[store.soas.index(schema_at(manifest, 0))])[0]};
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
    EXPECT_EQ(
        settings
            .apply_mode[settings.index(store.items(settings_modules.settings_list[module_row])[0])],
        SettingApplyMode::deferred);
    EXPECT_EQ(
        settings.device[settings.index(store.items(settings_modules.settings_list[module_row])[0])],
        SettingDevice::keyboard_mouse);
    EXPECT_EQ(setting_controls.kind[setting_controls.index(settings.control[settings.index(
                  store.items(settings_modules.settings_list[module_row])[0])])],
              SettingControlKind::float_range);
    EXPECT_EQ(setting_controls.step[setting_controls.index(settings.control[settings.index(
                  store.items(settings_modules.settings_list[module_row])[0])])],
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
    EXPECT_EQ(store.settings.device[store.settings.index(store.items(
                  store.settings_modules.settings_list[store.settings_modules.index(module)])[0])],
              SettingDevice::shared);
}

TEST(SourceLoader, OrdinarySoaPreservesLogicalApi) {
    TemporaryManifest files;
    auto& store{files.store()};
    auto& soas{store.soas};
    files.write_root(R"(
(module api :header "Api.h" :source "Api.cpp"
  (struct Rows :view-name Values :const-view-name ConstValues
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
    EXPECT_TRUE(store.functions.is_const[store.functions.index(
        store.items(soas.const_view_functions[schema_row])[0])]);
    ASSERT_EQ(soas.mutable_view_functions[schema_row].size(), 1U);
    auto const output{files.render(manifest).front().content};
    EXPECT_NE(output.find("struct API Rows"), std::string::npos);
    EXPECT_NE(output.find("std::pmr::memory_resource& resource"), std::string::npos);
    EXPECT_NE(output.find("return this->values()[0];"), std::string::npos);
    EXPECT_EQ(output.find("struct Rows {"), std::string::npos);
    EXPECT_NE(output.find("void noop() {"), std::string::npos);
}

TEST(SourceLoader, RejectsRemovedSingleAllocationAllocatorSyntax) {
    TemporaryManifest files;
    for (auto const declaration : {"(struct Rows :single-allocation-allocator Allocator (member "
                                   "ids array int))",
                                   "(struct Rows (member ids array int) (single-allocation Owner "
                                   "(variant Pool Allocator)))"}) {
        files.write_root(std::string{"(module api :header \"Api.h\" "} + declaration + ")");
        EXPECT_THROW(files.load(), lispb::codegen::sexpr::SourceError);
    }
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
