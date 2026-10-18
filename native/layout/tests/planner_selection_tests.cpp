#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/planner_selection.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace ioj::layout {
namespace planner_selection_tests {

TEST(PlannerSelection, TypeSwitchClearsAllLocalWorkloads) {
    EditorSession editor;
    auto const document{editor.from_source(R"(
        (module test :header "Test.h"
            (record Record (member value std::uint32_t))
            (packed-value Packed :storage std::uint8_t (field bits std::uint8_t :bits 8))))")};
    ASSERT_TRUE(document.valid());
    PlannerSelection selection;
    auto const record{document.find_type("test", "Record")};
    ASSERT_TRUE(selection.select_type(document.types(), record));
    selection.field = "value";
    selection.packed_access_fields["bits"] = AccessOperation::read;
    selection.record_access_members["value"] = AccessOperation::write;
    selection.soa_access_columns["values"] = AccessOperation::read_write;
    selection.packed_access_set_explicit = true;
    selection.record_access_set_explicit = true;
    selection.soa_access_set_explicit = true;
    EXPECT_FALSE(selection.select_type(document.types(), record));
    EXPECT_EQ(selection.field, "value");
    ASSERT_TRUE(selection.select_type(document.types(), document.find_type("test", "Packed")));
    EXPECT_TRUE(selection.field.empty());
    EXPECT_TRUE(selection.packed_access_fields.empty());
    EXPECT_TRUE(selection.record_access_members.empty());
    EXPECT_TRUE(selection.soa_access_columns.empty());
    EXPECT_FALSE(selection.packed_access_set_explicit);
    EXPECT_FALSE(selection.record_access_set_explicit);
    EXPECT_FALSE(selection.soa_access_set_explicit);
    EXPECT_TRUE(selection.select_type(document.types(), std::nullopt));
    EXPECT_FALSE(selection.type);
    EXPECT_FALSE(selection.identity());
}

struct ReconcileCase {
    char const* name;
    char const* replacement;
    bool selected;
    bool retained_member;
    bool explicit_empty{};
};

class SelectionReconcileTest : public testing::TestWithParam<ReconcileCase> {};

TEST_P(SelectionReconcileTest, RetainsOnlyValidSelectionAndWorkload) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const original{editor.from_source(R"(
        (module test :header "Test.h" (record Record (member value std::uint32_t))))")};
    auto const replacement{editor.from_source(std::string{R"((module test :header "Test.h" )"} +
                                              test.replacement + ')')};
    ASSERT_TRUE(original.valid());
    ASSERT_TRUE(replacement.valid());
    PlannerSelection selection;
    ASSERT_TRUE(selection.select_type(original.types(), original.find_type("test", "Record")));
    selection.field = "value";
    selection.record_access_set_explicit = true;
    if (!test.explicit_empty) {
        selection.record_access_members["value"] = AccessOperation::read;
    }

    selection.reconcile(replacement.types(), std::nullopt);

    EXPECT_EQ(selection.type.has_value(), test.selected);
    EXPECT_EQ(selection.identity().has_value(), test.selected);
    if (test.selected) {
        EXPECT_EQ(selection.type, replacement.find_type("test", "Record"));
    }
    EXPECT_EQ(selection.field, test.retained_member ? "value" : "");
    EXPECT_EQ(selection.record_access_members.size(),
              test.retained_member && !test.explicit_empty ? 1U : 0U);
    EXPECT_EQ(selection.record_access_set_explicit, test.retained_member);
}

INSTANTIATE_TEST_SUITE_P(
    DocumentChanges,
    SelectionReconcileTest,
    testing::Values(
        ReconcileCase{"InsertedEarlier",
                      "(record Inserted) (record Record (member value std::uint32_t))",
                      true,
                      true},
        ReconcileCase{"UnrelatedEditKeepsEmptyWorkload",
                      "(record Inserted) (record Record (member value std::uint32_t))",
                      true,
                      true,
                      true},
        ReconcileCase{
            "MemberRenamed", "(record Record (member renamed std::uint32_t))", true, false},
        ReconcileCase{"MemberRemoved", "(record Record)", true, false},
        ReconcileCase{"OwnerRemoved", "(record Other (member value std::uint32_t))", false, false},
        ReconcileCase{
            "OwnerRenamed", "(record Renamed (member value std::uint32_t))", false, false},
        ReconcileCase{
            "OwnerKindChanged",
            "(packed-value Record :storage std::uint32_t (field value std::uint32_t :bits 32))",
            true,
            false}),
    [](auto const& info) { return info.param.name; });

}
}
