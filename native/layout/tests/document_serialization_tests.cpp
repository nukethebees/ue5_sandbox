#include "document_test_support.hpp"
#include <ioj/layout/document.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout {
namespace document_serialization_tests {

auto rendered(Document const& document) -> std::string {
    auto const sources{document.sources()};
    EXPECT_EQ(sources.size(), 1U);
    return sources.empty() ? std::string{} : std::string{sources.front().text};
}

struct SerializationCase {
    char const* name;
    std::string_view source;
    std::function<void(std::string const&)> verify;
};

class DocumentSerializationTest : public testing::TestWithParam<SerializationCase> {};

TEST_P(DocumentSerializationTest, CanonicalRenderingRoundTripsWithoutFurtherChanges) {
    auto const& test{GetParam()};
    auto const document{Document::from_source(test.source)};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const output{rendered(document)};
    test.verify(output);
    auto const reloaded{Document::from_source(output)};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_EQ(rendered(reloaded), output);
}

INSTANTIATE_TEST_SUITE_P(
    Sources,
    DocumentSerializationTest,
    testing::Values(
        SerializationCase{"QuotedCppTypes",
                          R"((module game :header "Game.h"
                (record Data (member value "unsigned int")
                    (member pointer (type-ref "unsigned int" :suffix " const*")))))",
                          [](auto const& output) {
                              EXPECT_TRUE(output.contains("\"unsigned int\""));
                              EXPECT_TRUE(output.contains("\" const*\""));
                          }},
        SerializationCase{
            "WholeFile",
            R"((module game :header "Game.h" (record First(member x float))(record Second (member y First))))",
            [](auto const& output) {
                EXPECT_TRUE(output.contains("(record First"));
                EXPECT_TRUE(output.contains("(record Second"));
                EXPECT_TRUE(output.contains('\n'));
            }},
        SerializationCase{"BoundaryComments",
                          R"(; file header
(module game :header "Game.h"
    ; declaration documentation
    (record Data
        (member value float) ; inline member comment
        ; record footer
    )
    ; module footer
)
; file footer
)",
                          [](auto const& output) {
                              for (auto const comment : {"file header",
                                                         "declaration documentation",
                                                         "inline member comment",
                                                         "record footer",
                                                         "module footer",
                                                         "file footer"}) {
                                  EXPECT_TRUE(output.contains(comment)) << comment;
                              }
                              EXPECT_LT(output.find("; file header"), output.find("(module"));
                              EXPECT_GT(output.find("; file footer"), output.find("(record"));
                          }},
        SerializationCase{"RawPayload",
                          R"lisp((module game :header "Game.h"
            :prelude #cpp{int  value = 1; // preserve  spaces
char const* text = "; not a LispB comment";
}cpp#
            (record Data (member value float))))lisp",
                          [](auto const& output) {
                              EXPECT_TRUE(output.contains(
                                  "int  value = 1; // preserve  spaces\n"
                                  "char const* text = \"; not a LispB comment\";\n"));
                          }},
        SerializationCase{"WindowsLineEndings",
                          "(module game :header \"Game.h\"\r\n"
                          "  ; retained comment\r\n"
                          "  (record Data (member value float)))\r\n",
                          [](auto const& output) {
                              for (std::size_t index{}; index < output.size(); ++index) {
                                  if (output[index] == '\n') {
                                      ASSERT_GT(index, 0U);
                                      EXPECT_EQ(output[index - 1], '\r');
                                  }
                              }
                          }},
        SerializationCase{"SettingsAndUmbrella",
                          R"(
(settings-module settings :header "Settings.h" :source "Settings.cpp"
    :api-name SettingsAccess :state-name SettingsState
    (category video "Video")
    (setting vsync "VSync" :category video :value-type bool :backend engine :apply deferred
        (control toggle)))
(umbrella-module all :header "All.h" :headers ("Settings.h"))
)",
                          [](auto const& output) {
                              EXPECT_TRUE(output.contains("(settings-module settings"));
                              EXPECT_TRUE(output.contains("(setting vsync"));
                              EXPECT_TRUE(output.contains("(umbrella-module all"));
                          }}),
    [](auto const& info) { return info.param.name; });

struct CommentEditCase {
    char const* name;
    std::function<void(Transaction&)> change;
    std::function<void(std::string const&)> verify;
};

class DocumentCommentEditTest : public testing::TestWithParam<CommentEditCase> {};

TEST_P(DocumentCommentEditTest, CommentsFollowTheirOwnersAndUndoRestoresThem) {
    auto const& test{GetParam()};
    auto document{Document::from_source(R"(
; file documentation
(module game :header "Game.h"
    ; data documentation
    (record Data
        ; x documentation
        (member x std::uint32_t) ; x inline
        ; y documentation
        (member y std::uint32_t)))
)")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const before{rendered(document)};
    auto transaction{document.transaction()};
    test.change(transaction);
    ASSERT_TRUE(document.commit(transaction)) << diagnostic_messages(document);
    auto const after{rendered(document)};
    test.verify(after);
    EXPECT_TRUE(after.contains("; file documentation"));
    ASSERT_TRUE(document.undo());
    EXPECT_EQ(rendered(document), before);
    ASSERT_TRUE(document.redo());
    EXPECT_EQ(rendered(document), after);
}

INSTANTIATE_TEST_SUITE_P(
    Edits,
    DocumentCommentEditTest,
    testing::Values(
        CommentEditCase{"RenameDeclaration",
                        [](auto& tx) { tx.rename_declaration("game", "Data", "Renamed"); },
                        [](auto const& output) {
                            EXPECT_TRUE(output.contains("(record Renamed"));
                            EXPECT_TRUE(output.contains("; data documentation"));
                            EXPECT_TRUE(output.contains("; x documentation"));
                        }},
        CommentEditCase{"RenameAndReorderChild",
                        [](auto& tx) {
                            tx.rename_member("game", "Data", "x", "renamed");
                            tx.reorder_member("game", "Data", "renamed", 1);
                        },
                        [](auto const& output) {
                            EXPECT_TRUE(output.contains("; x documentation"));
                            EXPECT_TRUE(output.contains("; x inline"));
                            EXPECT_LT(output.find("(member y"), output.find("; x documentation"));
                            EXPECT_LT(output.find("; x documentation"),
                                      output.find("(member renamed"));
                        }},
        CommentEditCase{"DeleteChild",
                        [](auto& tx) { tx.delete_member("game", "Data", "x"); },
                        [](auto const& output) {
                            EXPECT_FALSE(output.contains("; x documentation"));
                            EXPECT_FALSE(output.contains("; x inline"));
                            EXPECT_TRUE(output.contains("; y documentation"));
                            EXPECT_TRUE(output.contains("; data documentation"));
                        }},
        CommentEditCase{"RecreateChildWithSameName",
                        [](auto& tx) {
                            tx.delete_member("game", "Data", "x");
                            tx.add_member("game", "Data", "x", "float");
                        },
                        [](auto const& output) {
                            EXPECT_TRUE(output.contains("(member x"));
                            EXPECT_FALSE(output.contains("; x documentation"));
                            EXPECT_FALSE(output.contains("; x inline"));
                        }},
        CommentEditCase{"RecreateDeclarationWithSameName",
                        [](auto& tx) {
                            tx.delete_declaration("game", "Data");
                            tx.create_declaration("game",
                                                  "(record Data (member replacement float))");
                        },
                        [](auto const& output) {
                            EXPECT_TRUE(output.contains("(record Data"));
                            EXPECT_FALSE(output.contains("; data documentation"));
                            EXPECT_FALSE(output.contains("; x documentation"));
                            EXPECT_FALSE(output.contains("; y documentation"));
                        }}),
    [](auto const& info) { return info.param.name; });

TEST(DocumentSerialization, RemovedPropertyRetainsCommentOnItsParent) {
    auto document{Document::from_source(R"(
        (module game :header "Game.h"
            (record Data
                (member values float :count 4 ; count explanation
                )))
    )")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto transaction{document.transaction()};
    transaction.set_member_count("game", "Data", "values", std::nullopt);
    ASSERT_TRUE(document.commit(transaction)) << diagnostic_messages(document);
    auto const output{rendered(document)};
    EXPECT_FALSE(output.contains(":count"));
    EXPECT_TRUE(output.contains("; count explanation"));
    auto const reloaded{Document::from_source(output)};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_EQ(rendered(reloaded), output);
}

}
}
