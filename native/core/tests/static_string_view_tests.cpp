#include <sandbox/core/static_string_view.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace {
using ml::StaticStringView;

static_assert(std::is_constructible_v<StaticStringView, decltype("hello")>);
static_assert(!std::is_constructible_v<StaticStringView, char (&)[6]>);
static_assert(!std::is_constructible_v<StaticStringView, char*>);
static_assert(!std::is_constructible_v<StaticStringView, char const*>);
static_assert(!std::is_constructible_v<StaticStringView, std::string>);
static_assert(!std::is_constructible_v<StaticStringView, std::string_view>);
static_assert(!std::is_convertible_v<StaticStringView, std::string_view>);
static_assert(std::is_trivially_copyable_v<StaticStringView>);

constexpr StaticStringView compile_time_value{"hello"};
static_assert(compile_time_value.size() == 5);
static_assert(compile_time_value.view() == "hello");

constexpr char static_text[]{"static"};
constexpr StaticStringView static_value{static_text};
static_assert(static_value.view() == "static");

// A type trait cannot distinguish this accepted array-reference type from an automatic const
// array. StaticStringViewRejectsAutomaticStorage covers the required compile-time rejection.
static_assert(std::is_constructible_v<StaticStringView, char const (&)[6]>);
}

TEST(NativeCoreStaticStringView, ExposesLiteral) {
    StaticStringView const value{"hello"};

    EXPECT_EQ(value.size(), 5);
    EXPECT_EQ(value.view(), "hello");
    EXPECT_EQ(value.data(), value.view().data());
}

TEST(NativeCoreStaticStringView, SupportsEmptyLiteral) {
    StaticStringView const value{""};

    EXPECT_EQ(value.size(), 0);
    EXPECT_TRUE(value.view().empty());
}

TEST(NativeCoreStaticStringView, PreservesEmbeddedNulls) {
    StaticStringView const value{"a\0b"};

    EXPECT_EQ(value.size(), 3);
    EXPECT_EQ(value.view(), std::string_view("a\0b", 3));
    EXPECT_EQ(value.data()[1], '\0');
    EXPECT_EQ(value.data()[2], 'b');
}
