#include <codegen/generator.h>
#include <codegen/source_loader.h>
#include <lispb/project.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace codegen::single_allocation_tests {

TEST(SingleAllocationSoa, StdlibBackendReusesLayoutWithoutUnrealDependencies) {
    std::vector<SoaSchema> structs{
        {.name = "Child", .members = {{"xs", SoaMemberKind::array, TypeRef{"float"}}}},
        {.name = "Rows",
         .members = {{"ids", SoaMemberKind::array, TypeRef{"std::int32_t"}},
                     {"nested", SoaMemberKind::nested, TypeRef{"Child"}, {}, "Child"}},
         .single_allocation = "SingleRows",
         .single_allocation_variants = {{"CustomSingleRows", TypeRef{"CustomAllocator"}}}}};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{.settings = {.name = "native", .header = "Native.h"},
                                       .declarations = {std::make_move_iterator(structs.begin()),
                                                        std::make_move_iterator(structs.end())},
                                       .soa_backend = SoaBackend::standard_library}}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("ml::native_soa::Vector<std::int32_t> ids"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<std::int32_t> IdsColumn"), std::string::npos);
    EXPECT_NE(output.find("std::span<std::int32_t const> ids"), std::string::npos);
    EXPECT_NE(output.find("std::span<float> xs"), std::string::npos);
    EXPECT_NE(output.find("void set(size_type const index, std::int32_t const new_ids, float const "
                          "new_nested_xs) const"),
              std::string::npos);
    EXPECT_NE(output.find("nested.xs[static_cast<std::size_t>(index)] = new_nested_xs;"),
              std::string::npos);
    EXPECT_NE(
        output.find("auto add(std::int32_t const new_ids, float const new_nested_xs) -> size_type"),
        std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::vector_storage_ops::append_rows(*this, 1, [&]"),
              std::string::npos);
    EXPECT_NE(output.find("ids.emplace_back(new_ids);"), std::string::npos);
    EXPECT_NE(output.find("nested.xs.emplace_back(new_nested_xs);"), std::string::npos);
    EXPECT_EQ(output.find("add_defaulted(1);"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<float> NestedXsColumn{IdsColumn}"), std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::copy_n(destination.nested_xs"), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::default_construct_n(columns.nested_xs, count);"),
              std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::source_data(source.nested.xs), count);"),
              std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::free(data_, allocation_alignment);"), std::string::npos);
    EXPECT_NE(output.find("CustomAllocator::free(data_);"), std::string::npos);
    EXPECT_EQ(output.find("CustomAllocator::free(data_, allocation_alignment)"), std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::copy_n(destination.nested_xs, source.nested_xs, num_);"),
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
    EXPECT_NE(output.find("columns.nested_values + index, columns.nested_values + source, "
                          "move_count"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::LayoutCursor cursor{blocks};"), std::string::npos);
    EXPECT_NE(output.find("pointer_at(Layout::IdsColumn, cursor.advance(Layout::IdsColumn))"),
              std::string::npos);
    EXPECT_NE(output.find("pointer_at(Layout::NestedValuesColumn, "
                          "cursor.advance(Layout::NestedValuesColumn))"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::source_data(source.nested.values), count"),
              std::string::npos);
    EXPECT_EQ(output.find("_bytes{elements_to_"), std::string::npos);
    auto const start{output.find("void reallocate(size_type const new_capacity)")};
    ASSERT_NE(start, std::string::npos);
    auto const body{output.substr(start)};
    auto const guard{body.find("if (num_ > 0)")};
    auto const typed_copy{body.find("ml::soa_storage::copy_n(")};
    auto const copy{
        body.find("ml::soa_storage::copy_n(destination.nested_values, source.nested_values, "
                  "num_);")};
    auto const release{body.find("MimallocStorageAllocator::free(data_);")};
    auto const publish{body.find("data_ = new_data;")};
    ASSERT_NE(guard, std::string::npos);
    ASSERT_NE(typed_copy, std::string::npos);
    ASSERT_NE(copy, std::string::npos);
    ASSERT_NE(release, std::string::npos);
    ASSERT_NE(publish, std::string::npos);
    EXPECT_LT(guard, typed_copy);
    EXPECT_LE(typed_copy, copy);
    EXPECT_LT(copy, release);
    EXPECT_LT(release, publish);
}

TEST(SingleAllocationSoa, EmitsDirectOrdinaryConstViewAppend) {
    auto input{schemas()};
    input.back().const_view_name = "ReadOnlyRows";
    auto const output{render(input)};
    EXPECT_NE(output.find("using ml::soa_storage::StorageOperations::append_from;"),
              std::string::npos);
    auto const overload{output.find("auto append_from(ReadOnlyRows const& source) -> size_type")};
    ASSERT_NE(overload, std::string::npos);
    auto const overload_end{output.find("\n    }", overload)};
    ASSERT_NE(overload_end, std::string::npos);
    auto const body{output.substr(overload, overload_end - overload)};
    EXPECT_NE(body.find("source.validate_array_sizes();"), std::string::npos);
    EXPECT_NE(body.find("append_columns(source, first, count);"), std::string::npos);
    EXPECT_EQ(body.find("source.columns()"), std::string::npos);
    EXPECT_EQ(body.find("get_view("), std::string::npos);
    EXPECT_NE(output.find("ordinary_source_aliases_storage(ReadOnlyRows const& source)"),
              std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::any_column(source, aliases)"), std::string::npos);
}

TEST(SingleAllocationSoa, AllocatorVariantsApplyToNestedColumns) {
    auto module{NormalModuleSchema{
        .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
        .declarations =
            [] {
                auto values{schemas()};
                return std::vector<DeclarationSchema>{values.begin(), values.end()};
            }(),
        .soa_array_allocators = {{"Malloc", TypeRef{"MallocAllocator"}},
                                 {"Realloc", TypeRef{"ReallocAllocator"}}}}};
    auto const files{render_modules(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("TArray<uint8, MallocAllocator> small;"), std::string::npos);
    EXPECT_NE(output.find("TArray<int32, MallocAllocator> ids;"), std::string::npos);
    EXPECT_NE(output.find("MallocChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<Aligned256, ReallocAllocator> wide;"), std::string::npos);
    EXPECT_NE(output.find("ReallocChild nested;"), std::string::npos);
    EXPECT_NE(output.find("TArray<uint8> small;"), std::string::npos);
    EXPECT_EQ(output.find("MallocSingleRows"), std::string::npos);
    module.soa_array_allocators.push_back({"Malloc", TypeRef{"ReallocAllocator"}});
    EXPECT_THROW(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}),
        std::invalid_argument);
}

TEST(SingleAllocationSoa, SingleAllocatorVariantPreservesViewsAndRoutesOwnership) {
    auto input{schemas()};
    input.back().single_allocation_variants = {{"CustomSingle", TypeRef{"CustomAllocator"}}};
    auto const output{render(input)};
    EXPECT_NE(output.find("struct CustomSingle"), std::string::npos);
    EXPECT_EQ(output.find("struct CustomSingleStorage"), std::string::npos);
    EXPECT_NE(output.find("CustomAllocator::allocate("), std::string::npos);
    EXPECT_NE(output.find("CustomAllocator::free(data_)"), std::string::npos);
    EXPECT_NE(output.find("MimallocStorageAllocator::free(data_)"), std::string::npos);
    input.back().single_allocation_variants.front().name = input.back().name;
    EXPECT_THROW(render(input), std::invalid_argument);
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
    EXPECT_EQ(output.find("struct RowsSingleView_nested"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleView) == 16"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleConstView) == 16"), std::string::npos);
    EXPECT_NE(output.find("column_data_unchecked<Aligned256>"), std::string::npos);
    EXPECT_NE(output.find("auto columns() const"), std::string::npos);
    EXPECT_NE(output.find("for_each_removal_run(num_, indices"), std::string::npos);
    EXPECT_NE(output.find("ml::soa_storage::source_data(source.nested.wide)"), std::string::npos);
    EXPECT_NE(output.find("SingleRows(SingleRows const&) = delete"), std::string::npos);
    EXPECT_NE(output.find("nested.append_from(other.nested)"), std::string::npos);
    input.front().members = {{"xs", SoaMemberKind::array, TypeRef{"double"}},
                             {"ys", SoaMemberKind::array, TypeRef{"double"}}};
    auto vectors{render(input)};
    EXPECT_NE(vectors.find("auto view_nested() const -> "
                           "std::conditional_t<Const, ChildConstView, ChildView>"),
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
    input.front().members.back().type = TypeRef{"float"};
    EXPECT_THROW(render(input), std::invalid_argument);
}

TEST(SingleAllocationSoa, OwnerBorrowingRequiresLvalues) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("auto operator=(SingleRows&& other) noexcept -> SingleRows&"),
              std::string::npos);
    EXPECT_NE(
        output.find("inline RowsSingleConstView::RowsSingleConstView(RowsSingleView const& other)"),
        std::string::npos);
    EXPECT_NE(output.find("if (!state_ || !state_->data_)"), std::string::npos);
    EXPECT_NE(output.find("auto get_view(this Self&& self) -> ViewFor<Self>"), std::string::npos);
    EXPECT_NE(output.find("requires std::is_lvalue_reference_v<Self>"), std::string::npos);
    EXPECT_EQ(output.find("get_view() && ->"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsEmptySchema) {
    auto input{schemas()};
    input.back().members.clear();
    EXPECT_THROW(render(input), std::invalid_argument);
}

TEST(SingleAllocationSoa, RejectsCompactViewNameCollisions) {
    auto input{schemas()};
    input.front().members.front().name = "columns";
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
    EXPECT_NE(output.find("source_data(source.nested.wide)"), std::string::npos);
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
    EXPECT_NE(
        layout.find("static_assert(allocation_alignment <= std::numeric_limits<uint32>::max()"),
        std::string::npos);
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
    EXPECT_EQ(occurrences("columns.ys + source, move_count)"), 1);
    EXPECT_EQ(occurrences("source_data(source.ys)"), 1);
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
    input.back().single_allocation = "RowsView";
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
    EXPECT_NE(output.find("auto view_first() const -> "
                          "std::conditional_t<Const, RowsConstView, RowsView>"),
              std::string::npos);
    EXPECT_NE(output.find("auto view_second() const -> "
                          "std::conditional_t<Const, RowsConstView, RowsView>"),
              std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleLayout::SecondNestedWideColumn.offset(blocks)"),
              std::string::npos);
}

TEST(SingleAllocationSoa, BenchmarkSchemaTracksFighterLeafOrderAndWidths) {
    auto const project_root{
        std::filesystem::path{IOJ_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{lispb::load_project(project_root / "lispb/project.lispb")};
    auto const& target{std::get<lispb::CppSchemaTarget>(project.targets.at("sandbox-code"))};
    std::vector<std::filesystem::path> sources;
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{load_sources(project.root / target.types, sources)};
    auto find_struct = [&](std::string_view const module_name,
                           std::string_view const struct_name) -> SoaSchema const& {
        for (auto const& module : manifest.modules) {
            auto const* soa{std::get_if<NormalModuleSchema>(&module)};
            if (soa == nullptr || soa->settings.name != module_name) {
                continue;
            }
            for (auto const& declaration : soa->declarations) {
                auto const* schema{std::get_if<SoaSchema>(&declaration)};
                if (schema != nullptr && schema->name == struct_name) {
                    return *schema;
                }
            }
        }
        throw std::runtime_error{"Missing " + std::string{struct_name} + " schema in module " +
                                 std::string{module_name}};
    };

    auto const& fighter{find_struct("fighters_soa", "FighterEntityData").members};
    std::vector<SoaMemberSchema> experiment;
    for (auto const& member : find_struct("single_allocation_experiment", "EntityData").members) {
        if (member.type.name != "Countdown8" && member.type.name != "Countdown16" &&
            member.type.name != "PeriodicCountdown16") {
            experiment.push_back(member);
            continue;
        }
        auto const& countdown{find_struct("single_allocation_experiment", *member.nested_schema)};
        for (auto leaf : countdown.members) {
            leaf.name = leaf.name == "counters" ? member.name : member.name + "_" + leaf.name;
            experiment.push_back(std::move(leaf));
        }
    }
    ASSERT_EQ(fighter.size(), experiment.size());
    std::map<std::string, std::string> const equivalents{
        {"@native_health", "int32"},
        {"@native_health_index", "int32"},
        {"@native_unique_id", "@soa_experiment_EntityId"},
        {"@native_fighter_task", "@soa_experiment_Task"},
        {"@native_team", "@soa_experiment_Team"},
        {"@native_vectors_3f", "Vectors"}};
    for (std::size_t index{}; index < fighter.size(); ++index) {
        auto const expected_name{fighter[index].name == "health_indices" ? "healths"
                                                                         : fighter[index].name};
        EXPECT_EQ(expected_name, experiment[index].name);
        EXPECT_EQ(fighter[index].kind, experiment[index].kind);
        auto const& type{fighter[index].type.name};
        auto const found{equivalents.find(type)};
        EXPECT_EQ(experiment[index].type.name, found == equivalents.end() ? type : found->second);
    }
}
}
