#include <ioj/levels/authoring/level_definition_reader.h>

#include <gtest/gtest.h>

namespace ioj::levels::authoring::tests {
TEST(NativeLevelParser, ParsesOwnedAstWithoutLinkingTheInterpreter) {
    s7::Ast ast;
    ast.text_bytes = "levelidfixturetitleTitle";
    ast.nodes = {
        {.kind = s7::NodeKind::List, .range = {0, 5}},
        {.kind = s7::NodeKind::Keyword, .range = {0, 5}},
        {.kind = s7::NodeKind::Keyword, .range = {5, 2}},
        {.kind = s7::NodeKind::Symbol, .range = {7, 7}},
        {.kind = s7::NodeKind::Keyword, .range = {14, 5}},
        {.kind = s7::NodeKind::String, .range = {19, 5}},
    };
    ast.child_indices = {1, 2, 3, 4, 5};
    auto const result{parse_level(ast)};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->metadata.id, LevelId{"fixture"});
    EXPECT_EQ(result->metadata.title, "Title");
    auto const validation{validate_level(*result)};
    EXPECT_FALSE(validation);
}
}
