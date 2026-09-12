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
    auto const files{render_modules(lower_modules(
        Manifest{.schema_version = manifest_schema_version,
                 .modules = {SoaModuleSchema{.settings = {.name = "native", .header = "Native.h"},
                                             .structs = std::move(structs),
                                             .backend = SoaBackend::standard_library}}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("ml::native_soa::Vector<std::int32_t> ids"), std::string::npos);
    EXPECT_NE(output.find("std::span<std::int32_t const> ids"), std::string::npos);
    EXPECT_NE(output.find("std::span<float> xs"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<float> NestedXs{Ids}"), std::string::npos);
    EXPECT_NE(output.find("std::memcpy(destination.nested_xs"), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
    EXPECT_NE(output.find("#include <memory>"), std::string::npos);
    EXPECT_NE(output.find("#include <cstring>"), std::string::npos);
    EXPECT_NE(
        output.find("std::uninitialized_value_construct_n<float*>(columns.nested_xs, count);"),
        std::string::npos);
    EXPECT_NE(output.find(
                  "std::memcpy(destination.nested_xs, source.nested.xs.data(), nested_xs_bytes);"),
              std::string::npos);
    EXPECT_NE(output.find("ml::native_soa::free(data_, allocation_alignment);"), std::string::npos);
    EXPECT_NE(output.find("CustomAllocator::free(data_);"), std::string::npos);
    EXPECT_EQ(output.find("CustomAllocator::free(data_, allocation_alignment)"), std::string::npos);
    EXPECT_NE(output.find("std::memcpy(destination.nested_xs, source.nested_xs, nested_xs_bytes);"),
              std::string::npos);
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
                 .modules = {SoaModuleSchema{
                     .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
                     .structs = std::move(structs)}}}))};
    return files.front().content;
}

TEST(SingleAllocationSoa, TypedColumnOperationsReuseByteCountsAndPreserveNestedPaths) {
    auto input{schemas()};
    input.front().members = {{"values", SoaMemberKind::array, TypeRef{"int32"}}};
    auto const output{render(input)};
    EXPECT_NE(output.find("#include \"HAL/UnrealMemory.h\""), std::string::npos);
    EXPECT_NE(output.find("#include \"Templates/MemoryOps.h\""), std::string::npos);
    EXPECT_NE(output.find("DefaultConstructItems<int32>(columns.ids, count);"), std::string::npos);
    EXPECT_NE(output.find("DefaultConstructItems<int32>(columns.nested_values, count);"),
              std::string::npos);
    EXPECT_NE(output.find("auto const ids_bytes{elements_to_move * sizeof(int32)};"),
              std::string::npos);
    EXPECT_NE(
        output.find("columns.nested_values + index, columns.nested_values + source, ids_bytes"),
        std::string::npos);
    EXPECT_NE(output.find("auto const ids_bytes{elements_to_copy * sizeof(int32)};"),
              std::string::npos);
    EXPECT_NE(output.find("destination.nested_values, source.nested.values.GetData(), ids_bytes"),
              std::string::npos);
    EXPECT_EQ(output.find("auto const nested_values_bytes"), std::string::npos);
    auto const start{output.find("void reallocate(size_type const new_capacity)")};
    ASSERT_NE(start, std::string::npos);
    auto const body{output.substr(start)};
    auto const guard{body.find("if (num_ > 0)")};
    auto const bytes{body.find("auto const ids_bytes{live_count * sizeof(int32)};")};
    auto const copy{
        body.find("FMemory::Memcpy(destination.nested_values, source.nested_values, ids_bytes);")};
    auto const release{body.find("MimallocStorageAllocator::free(data_);")};
    auto const publish{body.find("data_ = new_data;")};
    ASSERT_NE(guard, std::string::npos);
    ASSERT_NE(bytes, std::string::npos);
    ASSERT_NE(copy, std::string::npos);
    ASSERT_NE(release, std::string::npos);
    ASSERT_NE(publish, std::string::npos);
    EXPECT_LT(guard, bytes);
    EXPECT_LT(bytes, copy);
    EXPECT_LT(copy, release);
    EXPECT_LT(release, publish);
}

TEST(SingleAllocationSoa, AllocatorVariantsApplyToNestedColumns) {
    auto module{SoaModuleSchema{
        .settings = {.name = "test", .header = "Test.h", .source = "Test.cpp"},
        .structs = schemas(),
        .experimental_array_allocators = {{"Malloc", TypeRef{"MallocAllocator"}},
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
    module.experimental_array_allocators.push_back({"Malloc", TypeRef{"ReallocAllocator"}});
    EXPECT_THROW(
        lower_modules(Manifest{.schema_version = manifest_schema_version, .modules = {module}}),
        std::invalid_argument);
}

TEST(SingleAllocationSoa, SingleAllocatorVariantPreservesViewsAndRoutesOwnership) {
    auto input{schemas()};
    input.back().single_allocation_variants = {{"CustomSingle", TypeRef{"CustomAllocator"}}};
    auto const output{render(input)};
    EXPECT_NE(output.find("struct CustomSingleStorage"), std::string::npos);
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
    EXPECT_NE(
        output.find(
            "struct SingleRowsStorage : RowsSingleLayout, protected ml::soa_storage::StorageState"),
        std::string::npos);
    EXPECT_NE(output.find("using View = RowsSingleView;"), std::string::npos);
    EXPECT_NE(output.find("using ConstView = RowsSingleConstView;"), std::string::npos);
    EXPECT_EQ(output.find("struct RowsSingleView_nested"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleView) == 16"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleConstView) == 16"), std::string::npos);
    EXPECT_NE(output.find("column_data_unchecked<Aligned256>"), std::string::npos);
    EXPECT_NE(output.find("auto columns() const"), std::string::npos);
    EXPECT_NE(output.find("for_each_removal_run(num_, indices"), std::string::npos);
    EXPECT_NE(output.find("source.nested.wide.GetData()"), std::string::npos);
    EXPECT_NE(output.find("SingleRows(SingleRows const&) = delete"), std::string::npos);
    EXPECT_NE(output.find("nested.append_from(other.nested)"), std::string::npos);
    input.front().members = {{"xs", SoaMemberKind::array, TypeRef{"double"}},
                             {"ys", SoaMemberKind::array, TypeRef{"double"}}};
    auto vectors{render(input)};
    EXPECT_NE(vectors.find("auto view_nested() const -> ml::soa::Vector2View<double>"),
              std::string::npos);
    EXPECT_NE(vectors.find("auto view_nested() const -> ml::soa::Vector2ConstView<double>"),
              std::string::npos);
    input.front().members.push_back({"zs", SoaMemberKind::array, TypeRef{"double"}});
    vectors = render(input);
    EXPECT_NE(vectors.find("ml::soa::Vector3View<double>"), std::string::npos);
    input.front().members.back().type = TypeRef{"float"};
    vectors = render(input);
    EXPECT_NE(vectors.find("auto view_nested() const -> ChildView"), std::string::npos);
}

TEST(SingleAllocationSoa, OwnerBorrowingRequiresLvalues) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("auto operator=(SingleRows&&) noexcept -> SingleRows& = default;"),
              std::string::npos);
    EXPECT_NE(
        output.find("inline RowsSingleConstView::RowsSingleConstView(RowsSingleView const& other)"),
        std::string::npos);
    EXPECT_NE(output.find("if (!state_ || !state_->data_)"), std::string::npos);
    EXPECT_NE(output.find("auto get_view() & -> View"), std::string::npos);
    EXPECT_NE(output.find("auto get_view() const & -> ConstView"), std::string::npos);
    EXPECT_NE(output.find("auto get_view() && -> View = delete"), std::string::npos);
    EXPECT_NE(output.find("auto get_view() const && -> ConstView = delete"), std::string::npos);
    EXPECT_NE(output.find("auto get_const_view() const && -> ConstView = delete"),
              std::string::npos);
    EXPECT_NE(output.find("auto slice(size_type, size_type) && -> View = delete"),
              std::string::npos);
    EXPECT_NE(output.find("auto left(size_type) const && -> ConstView = delete"),
              std::string::npos);
    EXPECT_NE(output.find("auto right(size_type) && -> View = delete"), std::string::npos);
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
    EXPECT_NE(output.find("capacity_granularity{64}"), std::string::npos);
    EXPECT_NE(output.find("column_gap{192}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<int32> Ids{LayoutStart}"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<uint8> NestedSmall{Ids}"), std::string::npos);
    EXPECT_NE(output.find("layout_bytes(byte_size_type blocks)"), std::string::npos);
    EXPECT_NE(output.find("ColLayout<Aligned256> NestedWide{NestedSmall}"), std::string::npos);
    EXPECT_NE(output.find("ColumnLayoutStart LayoutStart{capacity_granularity, column_gap, 64}"),
              std::string::npos);
    EXPECT_NE(output.find("sizeof(Aligned256) <= (max_allocation_size - "
                          "NestedWide.block_offset) / capacity_granularity"),
              std::string::npos);
    EXPECT_NE(output.find("FMemory::Memcpy(destination.nested_wide, "
                          "source.nested_wide, nested_wide_bytes)"),
              std::string::npos);
    EXPECT_NE(output.find("auto const elements_to_move{static_cast<byte_size_type>(move_count)};"),
              std::string::npos);
    EXPECT_NE(output.find(" + source, nested_wide_bytes"), std::string::npos);
    EXPECT_NE(output.find("NestedWide.data_end(blocks)"), std::string::npos);
    EXPECT_NE(output.find("Single-allocation leaf nested.wide requires"), std::string::npos);
}

TEST(SingleAllocationSoa, GroupsDiagnosticsInFoldableImmediateFunction) {
    auto const output{render(schemas())};
    auto const owner{output.substr(output.find("struct RowsSingleLayout"),
                                   output.find("struct SingleRowsStorage") -
                                       output.find("struct RowsSingleLayout"))};
    auto const validation{
        owner.find("inline static constexpr auto validate_layout = []() consteval -> bool {")};
    auto const invocation{owner.find("static_assert(validate_layout());")};
    ASSERT_NE(validation, std::string::npos);
    ASSERT_NE(invocation, std::string::npos);
    EXPECT_LT(validation, invocation);
    for (auto position{owner.find("static_assert(")}; position != std::string::npos;
         position = owner.find("static_assert(", position + 1)) {
        EXPECT_GT(position, validation);
        EXPECT_LE(position, invocation);
    }
    EXPECT_NE(owner.find("return true;", validation), std::string::npos);
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
    EXPECT_EQ(occurrences("supported_leaf<float>"), 1);
    EXPECT_EQ(occurrences("elements_to_move * sizeof(float)"), 1);
    EXPECT_EQ(occurrences("live_count * sizeof(float)"), 1);
    EXPECT_EQ(occurrences("columns.ys + source, nested_xs_bytes"), 1);
    EXPECT_EQ(occurrences("source.ys, nested_xs_bytes"), 1);
    EXPECT_EQ(occurrences("_maximum_alignment"), 0);
    EXPECT_EQ(occurrences("static_cast<byte_size_type>(capacity_ / capacity_granularity)"), 1);
    EXPECT_NE(owner.find("ColLayout<float> Ys{NestedXs}"), std::string::npos);
    EXPECT_NE(owner.find("maximum_alignment(Ids, NestedSmall, NestedWide, NestedXs, Ys)"),
              std::string::npos);
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
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.push_back({"nested_small", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.push_back({"layout_start", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().members.push_back({"col_layout", SoaMemberKind::array, TypeRef{"int32"}});
    EXPECT_THROW(render(input), std::invalid_argument);
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
    EXPECT_NE(output.find("ColLayout<int32> SecondIds{FirstNestedWide}"), std::string::npos);
    EXPECT_NE(output.find("auto view_first() const -> RowsView"), std::string::npos);
    EXPECT_NE(output.find("auto view_second() const -> RowsConstView"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleLayout::SecondNestedWide.offset(blocks)"),
              std::string::npos);
}

TEST(SingleAllocationSoa, BenchmarkSchemaTracksFighterLeafOrderAndWidths) {
    auto const project_root{
        std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{lispb::load_project(project_root / "lispb/project.lispb")};
    auto const& target{std::get<lispb::CppSchemaTarget>(project.targets.at("sandbox-code"))};
    std::vector<std::filesystem::path> sources;
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{load_sources(project.root / target.types, sources)};
    auto find_struct = [&](std::string_view const module_name) -> SoaSchema const& {
        for (auto const& module : manifest.modules) {
            auto const* soa{std::get_if<SoaModuleSchema>(&module)};
            if (soa == nullptr || soa->settings.name != module_name) {
                continue;
            }
            auto const found{std::ranges::find_if(
                soa->structs, [](SoaSchema const& schema) { return schema.name == "EntityData"; })};
            if (found != soa->structs.end()) {
                return *found;
            }
        }
        throw std::runtime_error{"Missing EntityData schema in module " + std::string{module_name}};
    };

    auto const& fighter{find_struct("test_capital_ship_fighters_soa").members};
    auto const& experiment{find_struct("single_allocation_experiment").members};
    ASSERT_EQ(fighter.size(), experiment.size());
    std::map<std::string, std::string> const equivalents{
        {"@registry_handle", "@soa_experiment_Handle"},
        {"@fighter_task", "@soa_experiment_Task"},
        {"@team", "@soa_experiment_Team"},
        {"@vectors_3f", "Vectors"},
        {"@tick_countdown_8", "Countdown8"},
        {"@tick_countdown_16", "Countdown16"},
        {"@periodic_tick_countdown_16", "PeriodicCountdown16"}};
    for (std::size_t index{}; index < fighter.size(); ++index) {
        EXPECT_EQ(fighter[index].name, experiment[index].name);
        EXPECT_EQ(fighter[index].kind, experiment[index].kind);
        auto const& type{fighter[index].type.name};
        auto const found{equivalents.find(type)};
        EXPECT_EQ(experiment[index].type.name, found == equivalents.end() ? type : found->second);
    }
}
}
