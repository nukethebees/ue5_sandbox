#include <codegen/generator.h>
#include <codegen/json.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace codegen::single_allocation_tests {

auto schemas() -> std::vector<SoaSchema> {
    return {
        SoaSchema{.name = "Child",
                  .members = {{"small", SoaMemberKind::array, TypeRef{"uint8"}},
                              {"wide", SoaMemberKind::array, TypeRef{"Aligned256"}}}},
        SoaSchema{.name = "Rows",
                  .members = {{"ids", SoaMemberKind::array, TypeRef{"int32"}},
                              {"nested", SoaMemberKind::nested, TypeRef{"Child"}, {}, "Child"}},
                  .experimental_single_allocation = "SingleRows"},
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

TEST(SingleAllocationSoa, EmitsSiblingWithSharedViewsAndOneOwnerState) {
    auto const output{render(schemas())};
    auto const start{output.find("struct SingleRows")};
    ASSERT_NE(start, std::string::npos);
    auto const owner{output.substr(start)};
    EXPECT_NE(output.find("TArray<int32> ids;"), std::string::npos);
    EXPECT_EQ(owner.find("TArray<"), std::string::npos);
    EXPECT_NE(owner.find("using View = RowsView;"), std::string::npos);
    EXPECT_NE(owner.find("using ConstView = RowsConstView;"), std::string::npos);
    EXPECT_NE(owner.find("using size_type = int32;"), std::string::npos);
    EXPECT_NE(owner.find("using byte_size_type = SIZE_T;"), std::string::npos);
    EXPECT_NE(owner.find("std::byte* data_{};"), std::string::npos);
    EXPECT_NE(owner.find("size_type num_{};"), std::string::npos);
    EXPECT_NE(owner.find("size_type capacity_{};"), std::string::npos);
    EXPECT_NE(owner.find("SingleRows(SingleRows const&) = delete;"), std::string::npos);
    EXPECT_NE(owner.find("RowsView{{columns.ids, count}, ChildView{{columns.nested_small, "
                         "count}, {columns.nested_wide, count}}}"),
              std::string::npos);
    EXPECT_NE(owner.find("ChildConstView{{columns.nested_small, count}, "
                         "{columns.nested_wide, count}}"),
              std::string::npos);
    EXPECT_EQ(owner.find("validate_array_sizes"), std::string::npos);
    EXPECT_NE(owner.find(
                  "struct SingleRowsStorage : ml::single_allocation_experiment::StorageOperations"),
              std::string::npos);
    EXPECT_NE(owner.find("struct SingleRows : SingleRowsStorage"), std::string::npos);
    EXPECT_EQ(owner.find("void add_uninitialised"), std::string::npos);
    EXPECT_NE(owner.find("get_data(this Self& self)"), std::string::npos);
    EXPECT_NE(owner.find("get_data(this Self& self, size_type const offset)"), std::string::npos);
    EXPECT_NE(owner.find("std::conditional_t<std::is_const_v<Self>, std::byte const, std::byte>"),
              std::string::npos);
    EXPECT_NE(owner.find("make_data_unchecked(new_data, new_blocks)"), std::string::npos);
    EXPECT_NE(owner.find("make_data_unchecked(static_cast<std::byte const*>(data_), old_blocks)"),
              std::string::npos);
    EXPECT_EQ(owner.find("_data(size_type"), std::string::npos);
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

TEST(SingleAllocationSoa, SharesTypeChecksAlignmentAndCopySizesAcrossNestedLeaves) {
    auto input{schemas()};
    input.front().members.push_back({"xs", SoaMemberKind::array, TypeRef{"float"}});
    input.back().members.push_back({"ys", SoaMemberKind::array, TypeRef{"float"}});
    auto const output{render(input)};
    auto const owner{output.substr(output.find("struct SingleRows"))};
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
    input.back().experimental_single_allocation = "RowsView";
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
        .experimental_single_allocation = "SingleGrandparent",
    });
    auto const output{render(input)};
    EXPECT_NE(output.find("layout_align(first_nested_wide_block_end, first_ids_alignment)"),
              std::string::npos);
    EXPECT_NE(output.find("ChildView{{columns.first_nested_small, count}, "
                          "{columns.first_nested_wide, count}}"),
              std::string::npos);
    EXPECT_NE(output.find("ChildConstView{{columns.second_nested_small, count}, "
                          "{columns.second_nested_wide, count}}"),
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
