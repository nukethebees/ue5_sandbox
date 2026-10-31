#include <ioj/display_name.h>

#include <gtest/gtest.h>

#include <utility>

TEST(NativeCoreDisplayName, FormatsWordSeparatorsCaseAndNumberBoundaries) {
    for (auto const& [identifier, expected] : {std::pair{"a_b_c", "A B C"},
                                               std::pair{"level_one", "Level One"},
                                               std::pair{"test_level_42", "Test Level 42"},
                                               std::pair{"", ""},
                                               std::pair{"singleword", "Singleword"},
                                               std::pair{"__42", "42"},
                                               std::pair{"Level1", "Level 1"},
                                               std::pair{"Level123", "Level 123"}}) {
        SCOPED_TRACE(identifier);
        EXPECT_EQ(ioj::format_level_display_name(identifier), expected);
    }
}
