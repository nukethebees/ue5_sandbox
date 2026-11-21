#include <ioj/levels/authoring/identifiers.h>

#include <gtest/gtest.h>

namespace ioj::levels::authoring::tests {
TEST(NativeEditorIdentifiers, CanonicalizesAsciiSymbols) {
    struct Case {
        std::string_view input;
        std::string_view expected;
    };
    for (auto const test : {Case{"Enemy Group", "enemy-group"},
                            Case{"---", "entity"},
                            Case{"42 bots", "entity-42-bots"},
                            Case{"\xc3\x89lite", "lite"},
                            Case{"a\xe2\x98\x83z", "a-z"}}) {
        auto const id{canonical_symbol(test.input, "entity")};
        EXPECT_EQ(id, test.expected);
        EXPECT_TRUE(is_canonical_symbol(id));
    }
    EXPECT_FALSE(is_canonical_symbol("\xc3\xa9"));
    EXPECT_FALSE(is_canonical_symbol("Enemy"));
}

}
