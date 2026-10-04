#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace codegen::single_allocation_tests {

TEST(SingleAllocationSoa, StdlibBackendReusesLayoutWithoutUnrealDependencies) {
    std::vector<SoaSchema> structs{
        {.name = "Child", .members = {{"xs", SoaMemberKind::array, TypeRef{"float"}}}},
        {.name = "Rows",
         .members = {{"ids", SoaMemberKind::array, TypeRef{"std::int32_t"}},
                     {"nested", SoaMemberKind::nested, TypeRef{"Child"}, {}, "Child"},
                     {"local_types", SoaMemberKind::array, TypeRef{"example::Token"}},
                     {"nested_types", SoaMemberKind::array, TypeRef{"example::sub::Token"}},
                     {"foreign_types", SoaMemberKind::array, TypeRef{"other::Token"}},
                     {"shadowed_types", SoaMemberKind::array, TypeRef{"example::Layout"}}},
         .single_allocation = "SingleRows"}};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
            .settings = {.name = "native", .header = "Native.h", .namespace_name = "example"},
            .declarations = {std::make_move_iterator(structs.begin()),
                             std::make_move_iterator(structs.end())},
            .soa_backend = SoaBackend::standard_library}}}))};
    auto const& output{files.front().content};
    EXPECT_EQ(output.find("struct RowsView"), std::string::npos);
    EXPECT_EQ(output.find("struct RowsConstView"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<std::int32_t> IdsColumn"), std::string::npos);
    EXPECT_NE(output.find("auto ids() const"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<float> NestedXsColumn{IdsColumn}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<Token> LocalTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<sub::Token> NestedTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<other::Token> ForeignTypesColumn"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<example::Layout> ShadowedTypesColumn"), std::string::npos);
    EXPECT_EQ(output.find("using Base::count_;"), std::string::npos);
    EXPECT_EQ(output.find("this->count_"), std::string::npos);
    EXPECT_EQ(output.find("CompactViewState"), std::string::npos);
    EXPECT_NE(output.find("State* state_{};"), std::string::npos);
    EXPECT_NE(output.find("transfer_n<Overlapping>(destination.nested_xs"), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::default_construct_n(columns.nested_xs, count);"),
              std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::source_data(nested_view.xs())"), std::string::npos);
    EXPECT_NE(output.find("Operations::release_storage(*this);"), std::string::npos);
    EXPECT_NE(output.find("transfer_columns<false>(destination, source, num_);"),
              std::string::npos);
}

TEST(SingleAllocationSoa, StdlibRowOperationsUseNestedEquivalentValues) {
    std::vector<SoaSchema> structs{
        {.name = "Points",
         .members = {{"xs", SoaMemberKind::array, TypeRef{"float"}},
                     {"ys", SoaMemberKind::array, TypeRef{"float"}}},
         .equivalent_type = TypeRef{"Point"},
         .layout_only = true},
        {.name = "Rows",
         .members = {{"ids", SoaMemberKind::array, TypeRef{"std::int32_t"}},
                     {"points", SoaMemberKind::nested, TypeRef{"Points"}, {}, "Points"}}}};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{.settings = {.name = "native", .header = "Native.h"},
                                       .declarations = {std::make_move_iterator(structs.begin()),
                                                        std::make_move_iterator(structs.end())},
                                       .soa_backend = SoaBackend::standard_library}}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("void set(size_type const index, std::int32_t const new_ids, Point const "
                          "new_points) const"),
              std::string::npos);
    EXPECT_NE(output.find("points.set(index, new_points);"), std::string::npos);
    EXPECT_NE(
        output.find("auto add(std::int32_t const new_ids, Point const new_points) -> size_type"),
        std::string::npos);
    EXPECT_NE(output.find("ids.emplace_back(new_ids);"), std::string::npos);
    EXPECT_NE(output.find("points.add(new_points);"), std::string::npos);
    EXPECT_EQ(output.find("add_defaulted(1);"), std::string::npos);
}

auto schemas() -> std::vector<SoaSchema> {
    return {
        SoaSchema{.name = "Child",
                  .members = {{"small", SoaMemberKind::array, TypeRef{"uint8"}},
                              {"wide", SoaMemberKind::array, TypeRef{"Aligned256"}}}},
        SoaSchema{.name = "Rows",
                  .members = {{"ids", SoaMemberKind::array, TypeRef{"int32"}},
                              {"nested", SoaMemberKind::nested, TypeRef{"Child"}, {}, "Child"}},
                  .single_allocation = "SingleRows"},
    };
}

auto render(std::vector<SoaSchema> structs) -> std::string {
    auto const files{render_modules(lower_modules(
        Manifest{.schema_version = manifest_schema_version,
                 .modules = {NormalModuleSchema{
                     .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
                     .declarations = {std::make_move_iterator(structs.begin()),
                                      std::make_move_iterator(structs.end())}}}}))};
    return files.front().content;
}

TEST(SingleAllocationSoa, TypedColumnOperationsUseLayoutCursorAndPreserveNestedPaths) {
    auto input{schemas()};
    input.front().members = {{"values", SoaMemberKind::array, TypeRef{"int32"}}};
    auto const output{render(input)};
    EXPECT_NE(output.find("ml::soa_storage::default_construct_n(columns.ids, count);"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::default_construct_n(columns.nested_values, count);"),
              std::string::npos);
    EXPECT_NE(output.find("transfer_columns<false>(columns + index, columns + source, move_count)"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::LayoutCursor cursor{blocks};"), std::string::npos);
    EXPECT_NE(output.find("cursor.column_pointer(data, Layout::IdsColumn)"), std::string::npos);
    EXPECT_NE(output.find("cursor.column_pointer(data, Layout::NestedValuesColumn)"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::source_data(nested_view.values())"), std::string::npos);
    EXPECT_EQ(output.find("_bytes{elements_to_"), std::string::npos);
    auto const start{output.find("void copy_live_columns(")};
    ASSERT_NE(start, std::string::npos);
    auto const body{output.substr(start)};
    EXPECT_NE(body.find("transfer_columns<false>(destination, source, num_);"), std::string::npos);
    EXPECT_EQ(body.find("destination.nested_values"), std::string::npos);
}

TEST(SingleAllocationSoa, SchemaTaggedSourcesNeedNoOrdinaryView) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("SoaSourceFor<Columns, SingleRows, size_type>"), std::string::npos);
    EXPECT_NE(output.find("using soa_schema = RowsSchema;"), std::string::npos);
    EXPECT_NE(output.find("nested_view.wide()"), std::string::npos);
    EXPECT_EQ(output.find("SchemaConstView"), std::string::npos);
    EXPECT_EQ(output.find("ordinary_source_aliases_storage"), std::string::npos);
    EXPECT_EQ(output.find("auto columns()"), std::string::npos);
    EXPECT_EQ(output.find("struct RowsView"), std::string::npos);
}

TEST(SingleAllocationSoa, OverlappingCopyUsesMoveWithoutChangingAppend) {
    for (auto const backend : {SoaBackend::unreal, SoaBackend::standard_library}) {
        SCOPED_TRACE(static_cast<int>(backend));
        auto input{schemas()};
        input.back().operations = {StorageOperation::copy_element, StorageOperation::append_from};
        auto const files{render_modules(lower_modules(
            Manifest{.schema_version = manifest_schema_version,
                     .modules = {NormalModuleSchema{
                         .settings = {.name = "copy", .header = "Copy.h", .source = "Copy.cpp"},
                         .declarations = {input.begin(), input.end()},
                         .soa_backend = backend}}}))};
        auto const& output{files.front().content};
        auto const append_begin{output.find("void append_columns(")};
        auto const copy_begin{output.find("void copy_columns_from(")};
        auto const live_copy_begin{output.find("void copy_live_columns(")};
        ASSERT_NE(append_begin, std::string::npos);
        ASSERT_NE(copy_begin, std::string::npos);
        ASSERT_NE(live_copy_begin, std::string::npos);
        ASSERT_LT(append_begin, copy_begin);
        ASSERT_LT(copy_begin, live_copy_begin);
        auto const append{output.substr(append_begin, copy_begin - append_begin)};
        auto const copy{output.substr(copy_begin, live_copy_begin - copy_begin)};
        EXPECT_NE(append.find("transfer_columns<false>"), std::string::npos);
        EXPECT_EQ(append.find("source.ids()"), std::string::npos);
        EXPECT_NE(copy.find("transfer_columns<true>"), std::string::npos);
        EXPECT_EQ(copy.find("source.ids()"), std::string::npos);
    }
    EXPECT_NE(render(schemas()).find("void copy_columns_from("), std::string::npos);
}

TEST(SingleAllocationSoa, AllocatorVariantsApplyToNestedColumns) {
    auto module{NormalModuleSchema{
        .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
        .declarations =
            [] {
                auto values{schemas()};
                values.back().storage = SoaStorage::both;
                return std::vector<DeclarationSchema>{values.begin(), values.end()};
            }(),
        .soa_array_allocators = {{"Test", TypeRef{"TestArrayAllocator"}},
                                 {"Other", TypeRef{"OtherArrayAllocator"}}}}};
    auto const files{render_modules(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("TArray<uint8, TestArrayAllocator> small;"), std::string::npos);
    EXPECT_NE(output.find("TArray<int32, TestArrayAllocator> ids;"), std::string::npos);
    EXPECT_NE(output.find("TestChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<Aligned256, OtherArrayAllocator> wide;"), std::string::npos);
    EXPECT_NE(output.find("OtherChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<uint8> small;"), std::string::npos);
    EXPECT_EQ(output.find("TestSingleRows"), std::string::npos);
    module.soa_array_allocators.push_back({"Test", TypeRef{"OtherArrayAllocator"}});
    EXPECT_THROW(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}),
        std::invalid_argument);
}

TEST(SingleAllocationSoa, ArrayAllocatorVariantsSkipSingleOnlySchemas) {
    auto values{schemas()};
    NormalModuleSchema module{
        .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
        .declarations = {values.begin(), values.end()},
        .soa_array_allocators = {{"Custom", TypeRef{"CustomAllocator"}}}};
    auto const files{render_modules(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("struct CustomChild"), std::string::npos);
    EXPECT_EQ(output.find("struct CustomRows"), std::string::npos);
    EXPECT_NE(output.find("struct SingleRows"), std::string::npos);
}

TEST(SingleAllocationSoa, UsesRuntimePmrResourceForEveryOwner) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("std::pmr::get_default_resource()"), std::string::npos);
    EXPECT_NE(output.find("std::pmr::memory_resource* resource"), std::string::npos);
    EXPECT_NE(output.find("friend Operations;"), std::string::npos);
    EXPECT_NE(output.find("get_memory_resource() const noexcept"), std::string::npos);
    EXPECT_EQ(output.find("MimallocStorageAllocator"), std::string::npos);
}

TEST(SingleAllocationSoa, EmitsCompactViewsAndSharedOwnerState) {
    auto input{schemas()};
    for (auto& schema : input) {
        schema.operations = {StorageOperation::append_from};
    }
    auto const output{render(input)};
    EXPECT_NE(output.find("struct SingleRows : protected ml::soa_storage::StorageState"),
              std::string::npos);
    EXPECT_NE(output.find("using View = RowsSingleView;"), std::string::npos);
    EXPECT_NE(output.find("using ConstView = RowsSingleConstView;"), std::string::npos);
    EXPECT_NE(output.find("struct RowsSingleView_nested"), std::string::npos);
    EXPECT_NE(output.find("validate_compact_view<RowsSingleView>()"), std::string::npos);
    EXPECT_NE(output.find("validate_compact_view<RowsSingleConstView>()"), std::string::npos);
    EXPECT_NE(output.find("column_view<TArrayView<Element<Aligned256>>>"), std::string::npos);
    EXPECT_NE(output.find("friend ml::soa_storage_detail::CompactViewOperations;"),
              std::string::npos);
    EXPECT_NE(output.find("using Layout = RowsSingleLayout;"), std::string::npos);
    EXPECT_NE(output.find("state_, offset_, count_, Layout::NestedWideColumn"), std::string::npos);
    EXPECT_EQ(output.find("ml::soa_storage_detail::view_column_data<"), std::string::npos);
    EXPECT_EQ(output.find("auto columns() const"), std::string::npos);
    EXPECT_NE(output.find("for_each_removal_run(num_, indices"), std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::source_data(nested_view.wide())"), std::string::npos);
    EXPECT_NE(output.find("SingleRows(SingleRows const&) = delete"), std::string::npos);
    input.front().members = {{"xs", SoaMemberKind::array, TypeRef{"double"}},
                             {"ys", SoaMemberKind::array, TypeRef{"double"}}};
    auto vectors{render(input)};
    EXPECT_NE(vectors.find(
                  "auto view_nested() const -> "
                  "std::conditional_t<Const, RowsSingleConstView_nested, RowsSingleView_nested>"),
              std::string::npos);
    input.front().vector_components = {"xs", "ys"};
    vectors = render(input);
    EXPECT_NE(vectors.find("std::conditional_t<Const, ml::soa::Vector2ConstView<double>, "
                           "ml::soa::Vector2View<double>>"),
              std::string::npos);
    input.front().members.push_back({"zs", SoaMemberKind::array, TypeRef{"double"}});
    input.front().vector_components.push_back("zs");
    vectors = render(input);
    EXPECT_NE(vectors.find("ml::soa::Vector3View<double>"), std::string::npos);
    input.front().vector_components = {"pitches", "yaws", "rolls"};
    for (std::size_t index{}; index < input.front().members.size(); ++index) {
        input.front().members[index].name = input.front().vector_components[index];
    }
    auto const rotations{render(input)};
    EXPECT_NE(rotations.find("ml::soa::RotatorSoAView<double>"), std::string::npos);
    EXPECT_NE(rotations.find("ml::soa::RotatorSoAConstView<double>"), std::string::npos);
    EXPECT_NE(rotations.find("Layout::NestedPitchesColumn"), std::string::npos);
    EXPECT_NE(rotations.find("Layout::NestedYawsColumn"), std::string::npos);
    EXPECT_NE(rotations.find("Layout::NestedRollsColumn"), std::string::npos);
    EXPECT_NE(rotations.find("return three_column_view<"), std::string::npos);
    EXPECT_EQ(rotations.find("NestedYawsColumn.offset(blocks) - first"), std::string::npos);
    EXPECT_EQ(rotations.find("struct RowsSingleView_nested"), std::string::npos);
    input.front().members.back().type = TypeRef{"float"};
    EXPECT_THROW(render(input), std::invalid_argument);
}

TEST(SingleAllocationSoa, OwnerBorrowingRequiresLvalues) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("auto operator=(SingleRows&& other) -> SingleRows&"), std::string::npos);
    EXPECT_EQ(
        output.find("inline RowsSingleConstView::RowsSingleConstView(RowsSingleView const& other)"),
        std::string::npos);
    EXPECT_NE(output.find("using Base::Base;"), std::string::npos);
    EXPECT_EQ(output.find("using Operations::"), std::string::npos);
    EXPECT_EQ(output.find("private ml::soa_storage::StorageOperations"), std::string::npos);
    EXPECT_EQ(output.find("get_view() && ->"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsEmptySchema) {
    auto input{schemas()};
    input.back().members.clear();
    EXPECT_THROW(render(input), std::invalid_argument);
}

TEST(SingleAllocationSoa, RejectsCompactViewNameCollisions) {
    auto input{schemas()};
    input.front().members.front().name = "validate";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().single_allocation = "RowsSingleLayout";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().single_allocation = "RowsSingleConstView";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.push_back({"view_nested", SoaMemberKind::array, TypeRef{"float"}});
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.front().name = "RowsSingleView_nested";
    input.back().members.back().type = TypeRef{"RowsSingleView_nested"};
    input.back().members.back().nested_schema = "RowsSingleView_nested";
    EXPECT_THROW(render(input), std::invalid_argument);
}
TEST(SingleAllocationSoa, EmitsOrderedAlignedBlocksAndExplicitBulkRelocation) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("LayoutPolicy::capacity_granularity"), std::string::npos);
    EXPECT_NE(output.find("LayoutPolicy::column_gap"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<int32> IdsColumn{LayoutStart}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<uint8> NestedSmallColumn{IdsColumn}"), std::string::npos);
    EXPECT_NE(output.find("layout_bytes(byte_size_type blocks)"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<Aligned256> NestedWideColumn{NestedSmallColumn}"),
              std::string::npos);
    EXPECT_NE(output.find("ColumnLayoutStart LayoutStart{}"), std::string::npos);
    EXPECT_NE(output.find("capacity_block_bound(NestedWideColumn)"), std::string::npos);
    EXPECT_NE(output.find("destination.nested_wide"), std::string::npos);
    EXPECT_NE(output.find("source_data(nested_view.wide())"), std::string::npos);
    EXPECT_NE(output.find("LayoutCursor cursor{blocks}"), std::string::npos);
    EXPECT_EQ(output.find("nested_wide_bytes"), std::string::npos);
}

TEST(SingleAllocationSoa, KeepsAbiChecksInTheGeneratedLayout) {
    auto const output{render(schemas())};
    auto const start{output.find("struct RowsSingleLayout")};
    auto const end{output.find("struct RowsSingleConstView :", start)};
    ASSERT_NE(start, std::string::npos);
    ASSERT_NE(end, std::string::npos);
    auto const layout{output.substr(start, end - start)};
    EXPECT_NE(layout.find("static_assert(max_capacity >= capacity_granularity)"),
              std::string::npos);
    EXPECT_EQ(layout.find("validate_layout"), std::string::npos);
}

TEST(SingleAllocationSoa, SharesTypeChecksAlignmentAndCopySizesAcrossNestedLeaves) {
    auto input{schemas()};
    input.front().members.push_back({"xs", SoaMemberKind::array, TypeRef{"float"}});
    input.back().members.push_back({"ys", SoaMemberKind::array, TypeRef{"float"}});
    auto const output{render(input)};
    auto const owner{output.substr(output.find("struct RowsSingleLayout"))};
    auto occurrences = [&](std::string const& expression) {
        std::size_t count{};
        for (auto position{owner.find(expression)}; position != std::string::npos;
             position = owner.find(expression, position + expression.size())) {
            ++count;
        }
        return count;
    };
    EXPECT_EQ(occurrences("supported_leaf<float>"), 0);
    EXPECT_EQ(occurrences("sizeof(float)"), 0);
    EXPECT_EQ(occurrences("destination.ys, source.ys, count)"), 1);
    EXPECT_EQ(occurrences("source_data(source.ys())"), 1);
    EXPECT_EQ(occurrences("_maximum_alignment"), 0);
    EXPECT_NE(owner.find("ColLayout<float> YsColumn{NestedXsColumn}"), std::string::npos);
    EXPECT_NE(owner.find("capacity_block_bound(YsColumn)"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsInvalidFlatteningAndOwnerNames) {
    auto input{schemas()};
    input.back().members.back().nested_schema.reset();
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.back().nested_schema = "Missing";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.front().members = {{"cycle", SoaMemberKind::nested, TypeRef{"Rows"}, {}, "Rows"}};
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.back().type = TypeRef{"Opaque"};
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().single_allocation = "ChildView";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.front().name = "SingleRowsStorage";
    input.back().members.back().type = TypeRef{"SingleRowsStorage"};
    input.back().members.back().nested_schema = "SingleRowsStorage";
    EXPECT_NO_THROW(render(input));
    input = schemas();
    input.back().members.push_back({"nested_small", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.push_back({"layout_start", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_NO_THROW(render(input));
    input = schemas();
    input.back().members.push_back({"col_layout", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_NO_THROW(render(input));
    input = schemas();
    input.back().members.front().nested_schema = "Child";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.front().name = "trailing_";
    EXPECT_THROW(render(input), std::invalid_argument);
}

TEST(SingleAllocationSoa, FlattensMultipleLevelsAndRepeatedNestedSchemas) {
    auto input{schemas()};
    input.push_back(SoaSchema{
        .name = "Grandparent",
        .members = {{"first", SoaMemberKind::nested, TypeRef{"Rows"}, {}, "Rows"},
                    {"second", SoaMemberKind::nested, TypeRef{"Rows"}, {}, "Rows"}},
        .single_allocation = "SingleGrandparent",
    });
    auto const output{render(input)};
    EXPECT_NE(output.find("ColLayout<int32> SecondIdsColumn{FirstNestedWideColumn}"),
              std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_first"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_second_nested"), std::string::npos);
    EXPECT_NE(output.find("second_nested_view.wide()"), std::string::npos);
}

TEST(SingleAllocationSoa, CommonApiIsEmittedForEveryRequestedRepresentation) {
    for (auto const backend : {SoaBackend::unreal, SoaBackend::standard_library}) {
        for (auto const policy :
             {SoaStorage::vector, SoaStorage::single_allocation, SoaStorage::both}) {
            SCOPED_TRACE(static_cast<int>(backend));
            SCOPED_TRACE(soa_storage_name(policy));
            SoaSchema schema{
                .name = "LogicalRows",
                .view_name = "NamedView",
                .const_view_name = "NamedConstView",
                .members = {{"values", SoaMemberKind::array, TypeRef{"float"}}},
                .operations = {StorageOperation::set_num},
                .export_specifier = "ROWS_API",
                .functions = {{.name = "first",
                               .return_type = TypeRef{"float"},
                               .body_lines = {"return $column(values)[0];"},
                               .is_const = true},
                              {.name = "external_first",
                               .return_type = TypeRef{"float"},
                               .body_lines = {"return $column(values)[0];"},
                               .is_const = true,
                               .definition_in_source = true}},
                .mutable_view_functions = {{.name = "clear_first",
                                            .return_type = TypeRef{"void"},
                                            .body_lines = {"$column(values)[0] = 0;"}}},
                .using_declarations = {"Value = float"},
                .storage = policy,
                .const_view_functions = {{.name = "read_first",
                                          .return_type = TypeRef{"float"},
                                          .body_lines = {"return $column(values)[0];"},
                                          .is_const = true}}};
            if (policy != SoaStorage::vector) {
                schema.single_allocation = "CompactRows";
            }
            auto const files{render_modules(lower_modules(
                Manifest{.schema_version = manifest_schema_version,
                         .modules = {NormalModuleSchema{
                             .settings = {.name = "api", .header = "Api.h", .source = "Api.cpp"},
                             .declarations = {schema},
                             .soa_backend = backend}}}))};
            auto const& header{files.front().content};
            auto const& source{files.back().content};
            EXPECT_NE(header.find("struct ROWS_API NamedView"), std::string::npos);
            EXPECT_NE(header.find("struct ROWS_API NamedConstView"), std::string::npos);
            EXPECT_NE(header.find("using Value = float;"), std::string::npos);
            EXPECT_NE(header.find("read_first() const"), std::string::npos);
            EXPECT_NE(header.find("clear_first()"), std::string::npos);
            EXPECT_EQ(header.find("$column("), std::string::npos);
            if (policy != SoaStorage::vector) {
                EXPECT_NE(header.find("struct ROWS_API CompactRows"), std::string::npos);
                EXPECT_NE(source.find("CompactRows::external_first() const"), std::string::npos);
                EXPECT_EQ(header.find("using Operations::"), std::string::npos);
                EXPECT_NE(header.find("void copy_columns_from("), std::string::npos);
            }
            if (policy == SoaStorage::single_allocation) {
                EXPECT_EQ(header.find("struct LogicalRows {"), std::string::npos);
                EXPECT_EQ(header.find("LogicalRowsSingleViewImpl"), std::string::npos);
            }
            if (policy == SoaStorage::both) {
                EXPECT_NE(header.find("struct ROWS_API LogicalRowsSingleView"), std::string::npos);
                EXPECT_NE(source.find("LogicalRows::external_first() const"), std::string::npos);
            }
        }
    }
}

TEST(SingleAllocationSoa, RejectsIncompatibleRepresentationSettingsWithLocalDiagnostics) {
    auto check = [](SoaSchema schema, std::string_view message) {
        try {
            render({std::move(schema)});
            FAIL() << "Expected rejection containing " << message;
        } catch (std::invalid_argument const& error) {
            EXPECT_NE(std::string_view{error.what()}.find(message), std::string_view::npos);
        }
    };
    SoaSchema const compact{.name = "Rows",
                            .members = {{"values", SoaMemberKind::array, TypeRef{"float"}}},
                            .single_allocation = "Owner"};
    auto schema{compact};
    schema.array_allocator = TypeRef{"Allocator"};
    check(schema, "array-allocator requires vector storage");
    schema = compact;
    schema.copy_element_memberwise = true;
    check(schema, "single-allocation leaves use trivial copying");
    schema = compact;
    schema.storage = SoaStorage::vector;
    check(schema, "storage vector cannot declare");
    schema = compact;
    schema.layout_only = true;
    check(schema, "layout-only cannot declare");
    for (auto const policy : {SoaStorage::single_allocation, SoaStorage::both}) {
        schema = compact;
        schema.single_allocation.reset();
        schema.storage = policy;
        check(schema, "requires a single-allocation owner");
    }
    schema = compact;
    schema.functions = {{.name = "broken",
                         .return_type = TypeRef{"float"},
                         .body_lines = {"return $column(missing)[0];"}}};
    check(schema, "has no column 'missing'");
}

}
