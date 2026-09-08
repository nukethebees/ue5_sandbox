#include <codegen/generator.h>
#include <codegen/json.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace codegen::single_allocation_tests {

TEST(SingleAllocationSoa, StdlibBackendReusesLayoutWithoutUnrealDependencies) {
    std::vector<SoaSchema> structs{
        {.name = "Child", .members = {{"xs", SoaMemberKind::array, TypeRef{"float"}}}},
        {.name = "Rows",
         .members = {{"ids", SoaMemberKind::array, TypeRef{"std::int32_t"}},
                     {"nested", SoaMemberKind::nested, TypeRef{"Child"}, {}, "Child"}},
         .single_allocation = "SingleRows"}};
    auto const files{render_modules(lower_modules(
        Manifest{.schema_version = manifest_schema_version,
                 .modules = {SoaModuleSchema{.settings = {.name = "native", .header = "Native.h"},
                                             .structs = std::move(structs),
                                             .experimental_stdlib = true}}}))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("ml::native_soa::Vector<std::int32_t> ids"), std::string::npos);
    EXPECT_NE(output.find("std::span<std::int32_t const> ids"), std::string::npos);
    EXPECT_NE(output.find("std::span<float> xs"), std::string::npos);
    EXPECT_NE(output.find("layout_align(ids_block_end, nested_xs_alignment)"), std::string::npos);
    EXPECT_NE(output.find("std::memcpy(destination.nested_xs"), std::string::npos);
    EXPECT_EQ(output.find("TArray"), std::string::npos);
    EXPECT_EQ(output.find("FMemory"), std::string::npos);
    EXPECT_EQ(output.find("CoreMinimal"), std::string::npos);
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
    EXPECT_NE(output.find("using View = RowsSingleView<false>"), std::string::npos);
    EXPECT_NE(output.find("using ConstView = RowsSingleView<true>"), std::string::npos);
    EXPECT_NE(output.find("struct RowsSingleView_nested"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleView<false>) == 16"), std::string::npos);
    EXPECT_NE(output.find("sizeof(RowsSingleView_nested<false>) == 16"), std::string::npos);
    EXPECT_NE(output.find("column_data_unchecked<Aligned256>"), std::string::npos);
    EXPECT_NE(output.find("auto columns() const"), std::string::npos);
    EXPECT_NE(output.find("for_each_removal_run(num_, indices"), std::string::npos);
    EXPECT_NE(output.find("source.nested.wide.GetData()"), std::string::npos);
    EXPECT_NE(output.find("SingleRows(SingleRows const&) = delete"), std::string::npos);
    EXPECT_NE(output.find("nested.append_from(other.nested)"), std::string::npos);
}

TEST(SingleAllocationSoa, RejectsCompactViewNameCollisions) {
    auto input{schemas()};
    input.front().members.front().name = "columns";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().single_allocation = "RowsSingleLayout";
    EXPECT_THROW(render(input), std::invalid_argument);
    input = schemas();
    input.back().single_allocation = "RowsSingleView_nested";
    EXPECT_THROW(render(input), std::invalid_argument);
}
TEST(SingleAllocationSoa, EmitsOrderedAlignedBlocksAndExplicitBulkRelocation) {
    auto const output{render(schemas())};
    EXPECT_NE(output.find("capacity_granularity{64}"), std::string::npos);
    EXPECT_NE(output.find("layout_align(ids_block_end, nested_small_alignment)"),
              std::string::npos);
    EXPECT_NE(output.find("layout_align(nested_small_block_end, nested_wide_alignment)"),
              std::string::npos);
    EXPECT_NE(output.find("alignof(Aligned256) > 64 ? alignof(Aligned256) : 64"),
              std::string::npos);
    EXPECT_NE(output.find("sizeof(Aligned256) <= (max_allocation_size - "
                          "nested_wide_block_offset) / capacity_granularity"),
              std::string::npos);
    EXPECT_NE(output.find("FMemory::Memcpy(destination.nested_wide, "
                          "source.nested_wide, nested_wide_bytes)"),
              std::string::npos);
    EXPECT_NE(output.find("auto const elements_to_move{static_cast<byte_size_type>(move_count)};"),
              std::string::npos);
    EXPECT_NE(output.find(" + source, nested_wide_bytes"), std::string::npos);
    EXPECT_NE(output.find("ids_block_end"), std::string::npos);
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
    EXPECT_NE(owner.find("layout_align(nested_xs_block_end, nested_xs_alignment)"),
              std::string::npos);
    EXPECT_NE(owner.find("std::max({ids_alignment, nested_small_alignment, nested_wide_alignment, "
                         "nested_xs_alignment})"),
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
    EXPECT_NE(output.find("layout_align(first_nested_wide_block_end, first_ids_alignment)"),
              std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_first_nested"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleView_second_nested"), std::string::npos);
    EXPECT_NE(output.find("GrandparentSingleLayout::second_nested_wide_block_offset"),
              std::string::npos);
}

TEST(SingleAllocationSoa, BenchmarkSchemaTracksFighterLeafOrderAndWidths) {
    auto read = [](std::string const& filename) {
        std::ifstream stream{std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR} / "manifests" /
                             filename};
        return nlohmann::json::parse(stream);
    };
    auto const fighter_document = read("batch_game.json");
    auto const experiment_document = read("single_allocation_experiment.json");
    auto const& fighter{fighter_document["modules"][0]["structs"][0]["members"]};
    auto const& experiment{experiment_document["modules"][0]["structs"][4]["members"]};
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
        EXPECT_EQ(fighter[index]["name"], experiment[index]["name"]);
        EXPECT_EQ(fighter[index]["kind"], experiment[index]["kind"]);
        auto const type{fighter[index]["type"].get<std::string>()};
        auto const found{equivalents.find(type)};
        EXPECT_EQ(experiment[index]["type"], found == equivalents.end() ? type : found->second);
    }
}
}
