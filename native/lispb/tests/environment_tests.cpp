#include <codegen/ast/environment_guard.h>
#include <codegen/ast/environments.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

namespace lispb::codegen {
namespace {

TEST(Environments, ResolvesDeclarationsThroughIntroducedScopesAndParents) {
    Strings strings;
    Environments environments;
    auto const a{strings.intern("a")};
    auto const foo{strings.intern("Foo")};
    auto const value{strings.intern("Value")};
    auto const namespace_scope{
        environments.named(environments.global(), EnvironmentKind::namespace_, a)};
    auto const record{
        CppDeclarationId{NodeId{(static_cast<std::uint32_t>(NodeKind::struct_) << 24) | 7}}};
    auto const record_scope{
        environments.named(namespace_scope, EnvironmentKind::record, foo, record)};
    auto const alias{CppDeclarationId{
        NodeId{(static_cast<std::uint32_t>(NodeKind::using_declaration) << 24) | 7}}};
    environments.declare(record_scope, value, alias);
    auto const block{environments.append(record_scope, EnvironmentKind::block)};

    EXPECT_TRUE(EnvironmentId{}.is_null());
    EXPECT_TRUE(environments.parent(environments.global()).is_null());
    EXPECT_EQ(
        environments.lookup(environments.global(), std::array{a, foo, value}).symbol.declaration,
        alias);
    EXPECT_EQ(environments.lookup(block, std::array{foo}).symbol.declaration, record);
    EXPECT_EQ(environments.lookup(block, std::array{value}).symbol.declaration, alias);
    EXPECT_TRUE(environments.lookup(block, std::array{value}).symbol.introduced_scope.is_null());
    EXPECT_NE(record, alias);
    EXPECT_EQ(environments.named(namespace_scope, EnvironmentKind::record, foo), record_scope);
}

TEST(Environments, StopsAtHidingDeclarationsAndUnknownOrAmbiguousScopes) {
    Strings strings;
    Environments environments;
    auto const a{strings.intern("a")};
    auto const token{strings.intern("Token")};
    auto const outer{environments.named(environments.global(), EnvironmentKind::namespace_, a)};
    auto const target{CppDeclarationId{FunctionId{1}}};
    environments.declare(outer, token, target);
    auto const block{environments.append(outer, EnvironmentKind::block)};
    environments.declare(block, a, CppDeclarationId{FunctionParameterId{1}});

    EXPECT_EQ(environments.lookup(block, std::array{a, token}).status, LookupStatus::missing);
    EXPECT_EQ(environments.lookup(block, std::array{token}).symbol.declaration, target);
    environments.mark_unknown(block);
    EXPECT_EQ(environments.lookup(block, std::array{token}).status, LookupStatus::unknown);

    auto const anonymous{environments.append(outer, EnvironmentKind::namespace_)};
    environments.declare(anonymous, token, CppDeclarationId{FunctionId{2}});
    environments.import_symbols(outer, anonymous);
    EXPECT_EQ(environments.lookup(outer, std::array{token}).status, LookupStatus::unknown);
}

TEST(Environments, RestoresNestedSelectionsAndResetsAllColumns) {
    Environments environments;
    auto current{environments.global()};
    auto const record{environments.append(current, EnvironmentKind::record)};
    {
        EnvironmentGuard const guard{current, record};
        EXPECT_EQ(current, record);
        {
            auto const block{environments.append(current, EnvironmentKind::block)};
            EnvironmentGuard const inner{current, block};
            EXPECT_EQ(current, block);
        }
        EXPECT_EQ(current, record);
    }
    EXPECT_EQ(current, environments.global());
    environments.clear();
    EXPECT_EQ(environments.size(), 1);
    EXPECT_EQ(environments.kind(current), EnvironmentKind::global);
    EXPECT_TRUE(environments.symbols(current).empty());
}

TEST(Environments, ChecksCapacityBeforeNarrowingAnEnvironmentIndex) {
    Environments environments;
    auto const limit{EnvironmentId::null_value};
    for (std::uint32_t index{1}; index < limit; ++index) {
        EXPECT_EQ(environments.append(environments.global(), EnvironmentKind::block).value, index);
    }

    EXPECT_EQ(environments.size(), limit);
    EXPECT_THROW(environments.append(environments.global(), EnvironmentKind::block),
                 std::length_error);
    environments.clear();
    EXPECT_EQ(environments.append(environments.global(), EnvironmentKind::block).value, 1);
}

}
}
