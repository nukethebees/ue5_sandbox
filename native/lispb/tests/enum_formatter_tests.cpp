#include <ioj/structural_diff.h>

#include <gtest/gtest.h>
#include <PlainEnums.h>

#include <format>
#include <iterator>
#include <string>

namespace lispb::tests {
using namespace codegen_compile_fixture;

static_assert(std::formattable<EPlainFixture, char>);
static_assert(!std::formattable<EReorderedFixture, char>);
static_assert(!std::formattable<EPlainFixture, wchar_t>);

TEST(GeneratedEnumFormatter, SelectsExistingStringRepresentations) {
    auto const value{EPlainFixture::ReadableName};
    EXPECT_EQ(std::format("{} / {:n} / {:s} / {:l}", value, value, value, value),
              "ReadableName / ReadableName / readable_name / Readable Name");
    EXPECT_EQ(std::format("{:l}", EPlainFixture::First), "First");
    EXPECT_EQ(std::format("{}", ENameOnlyFixture::First), "First");
}

TEST(GeneratedEnumFormatter, PreservesStringFormattingAndDynamicArgumentIndexing) {
    auto const value{EPlainFixture::First};
    EXPECT_EQ(std::format("{:>8}", value), "   First");
    EXPECT_EQ(std::format("{:s*^9}", value), "**first**");
    EXPECT_EQ(std::format("{:l.3}", EPlainFixture::ReadableName), "Rea");
    EXPECT_EQ(std::format("{:s>{}}:{}", value, 8, 42), "   first:42");
    EXPECT_EQ(std::format("{0:s>{1}.{2}}", value, 7, 3), "    fir");
    EXPECT_EQ(std::format("{:ns>8}", value), "sssFirst");

    std::string output;
    std::format_to(std::back_inserter(output), "{:l}", EPlainFixture::ReadableName);
    EXPECT_EQ(output, "Readable Name");
}

TEST(GeneratedEnumFormatter, RejectsUnavailableRepresentationsAndInvalidSpecifications) {
    auto value{ENameOnlyFixture::First};
    for (auto const specification : {"{:s}", "{:l}", "{:d}", "{:x}", "{:nq}"}) {
        SCOPED_TRACE(specification);
        EXPECT_THROW(static_cast<void>(std::vformat(specification, std::make_format_args(value))),
                     std::format_error);
    }
}

TEST(GeneratedEnumFormatter, SuppliesReadableStructuralDiffValues) {
    ioj::DiffReport result;
    result.diff("state", EPlainFixture::First, EPlainFixture::ReadableName);
    EXPECT_EQ(result.message(), "state\n  Expected: First\n  Actual:   ReadableName\n");
}
}
