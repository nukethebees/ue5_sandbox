#include <lispb/strings.h>
#include <lispb/temporary_string_scope.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace lispb {

TEST(Strings, DeduplicatesTextAndSourceSlicesWithoutCopyingSourceBytes) {
    Strings strings;
    auto const source{strings.intern_retained("prefix shared suffix")};
    auto const bytes{strings.byte_size()};
    auto const word{strings.slice(source, 7, 6)};
    EXPECT_EQ(strings.view(word), "shared");
    EXPECT_EQ(strings.view(word).data(), strings.view(source).data() + 7);
    EXPECT_EQ(strings.intern("shared"), word);
    EXPECT_EQ(strings.slice(source, 7, 6), word);
    EXPECT_EQ(strings.find("shared"), word);
    EXPECT_EQ(strings.byte_size(), bytes);
    EXPECT_EQ(strings.slice_count(), 3U);
    EXPECT_EQ(strings.intern(""), StringId{});
    EXPECT_EQ(strings.find("missing"), StringId{});
}

TEST(Strings, PromotionPreservesCanonicalIdsAndSurvivesTemporaryReset) {
    Strings strings;
    StringId source;
    StringId promoted;
    {
        TemporaryStringScope scope{strings};
        source = strings.intern("prefix promoted suffix");
        promoted = strings.slice(source, 7, 8);
        EXPECT_EQ(strings.intern_retained("promoted"), promoted);
        EXPECT_EQ(strings.intern("promoted"), promoted);
    }
    strings.reset_temporary();
    EXPECT_EQ(strings.view(promoted), "promoted");
    EXPECT_EQ(strings.find("promoted"), promoted);
    EXPECT_EQ(strings.find("prefix promoted suffix"), StringId{});
    EXPECT_EQ(strings.byte_size(), 8U);
    EXPECT_EQ(strings.slice_count(), 2U);

    for (int i{}; i < 100; ++i) {
        {
            TemporaryStringScope scope{strings};
            EXPECT_EQ(strings.view(strings.intern(std::to_string(i))), std::to_string(i));
        }
        strings.reset_temporary();
        EXPECT_EQ(strings.slice_count(), 2U);
        EXPECT_EQ(strings.find("promoted"), promoted);
    }
}

TEST(Strings, RetainedSourceSlicesAndIdsSurviveCharacterBufferGrowth) {
    Strings strings;
    auto const source{strings.intern_retained("prefix shared suffix")};
    StringId word;
    {
        TemporaryStringScope scope{strings};
        word = strings.slice(source, 7, 6);
        static_cast<void>(strings.intern(std::string(100000, 't')));
    }
    static_cast<void>(strings.intern(std::string(100000, 'r')));
    strings.reset_temporary();
    EXPECT_EQ(strings.view(source), "prefix shared suffix");
    EXPECT_EQ(strings.view(word), "shared");
    EXPECT_EQ(strings.intern("shared"), word);
    EXPECT_EQ(strings.view(word).data(), strings.view(source).data() + 7);
}

TEST(Strings, InterningAnExistingTemporaryIdRetainsItOutsideTheScope) {
    Strings strings;
    StringId temporary;
    {
        TemporaryStringScope scope{strings};
        temporary = strings.intern("value");
    }
    EXPECT_EQ(strings.intern(temporary), temporary);
    strings.reset_temporary();
    EXPECT_EQ(strings.view(temporary), "value");
    strings.clear();
    EXPECT_EQ(strings.byte_size(), 0U);
    EXPECT_EQ(strings.slice_count(), 1U);
    EXPECT_EQ(strings.find("value"), StringId{});
}

TEST(Strings, EmbeddedNullsArePartOfTheInternedValue) {
    Strings strings;
    auto const text{std::string_view{"a\0b", 3}};
    auto const id{strings.intern(text)};
    EXPECT_EQ(strings.intern(text), id);
    EXPECT_NE(strings.intern("a"), id);
    EXPECT_EQ(strings.view(id), text);
}

}
