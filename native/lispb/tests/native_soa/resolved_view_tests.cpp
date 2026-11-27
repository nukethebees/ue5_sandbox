#include <ioj/vectors3f.h>

#include <gtest/gtest.h>

#include <array>
#include <span>

TEST(ResolvedView, SlicesIndependentColumns) {
    std::array<float, 2> xs{1.f, 2.f};
    std::array<float, 2> ys{3.f, 4.f};
    std::array<float, 2> zs{5.f, 6.f};
    ioj::vectors3f::View view{std::span{xs}, std::span{ys}, std::span{zs}};
    ioj::vectors3f::ConstView const read{view};
    auto const row{read.slice(1, 1)};
    EXPECT_EQ(row.xs().data(), &xs[1]);
    EXPECT_EQ(row.ys().data(), &ys[1]);
    EXPECT_EQ(row.zs().data(), &zs[1]);
    view.set(1, {7.f, 8.f, 9.f});
    EXPECT_EQ(row[0], (ioj::Vector3f{7.f, 8.f, 9.f}));
}
