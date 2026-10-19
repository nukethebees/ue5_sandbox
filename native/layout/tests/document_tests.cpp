#include "document_test_support.hpp"
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>

#include <lispb/diagnostics.h>

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace ioj::layout::document_tests {
constexpr std::string_view source{R"(
    (module game :header "Game.h" :backend standard-library
        (packed-value ShipState :storage std::uint16_t
            (field health std::uint16_t :bits 8)
            (field armor std::uint16_t :bits 8))
        (struct FighterData
            (member health array std::uint16_t)
            (member speed array float))))"};

auto health_bits(Document const& document) -> std::optional<int> {
    return document.query().module("game").packed_value("ShipState").field("health").bits();
}

struct EditCase {
    std::string_view name;
    std::function<void(Transaction&)> changes;
    std::function<void(Document const&)> verify;
    bool accepted{true};
};
class DocumentEdit : public testing::TestWithParam<EditCase> {};

TEST_P(DocumentEdit, PublishesOneValidTransaction) {
    EditorSession editor;
    auto document{editor.from_source(source)};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto const& test{GetParam()};
    auto transaction{document.transaction()};
    test.changes(transaction);
    ASSERT_EQ(document.commit(transaction), test.accepted);
    EXPECT_EQ(editor.diagnostics().size() == 0, test.accepted);
    test.verify(document);
    if (test.accepted) {
        EXPECT_TRUE(document.can_undo());
    } else {
        EXPECT_FALSE(document.can_undo());
        EXPECT_EQ(health_bits(document), 8);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Edits,
    DocumentEdit,
    testing::Values(
        EditCase{"ChangePackedFieldWidth",
                 [](Transaction& tx) { tx.set_field_bits("game", "ShipState", "health", 4); },
                 [](Document const& doc) { EXPECT_EQ(health_bits(doc), 4); }},
        EditCase{"TemporaryOverflow",
                 [](Transaction& tx) {
                     tx.set_field_bits("game", "ShipState", "health", 12);
                     tx.set_field_bits("game", "ShipState", "armor", 4);
                 },
                 [](Document const& doc) { EXPECT_EQ(health_bits(doc), 12); }},
        EditCase{"StorageAndWidthTogether",
                 [](Transaction& tx) {
                     tx.set_packed_storage("game", "ShipState", "std::uint8_t");
                     tx.set_field_bits("game", "ShipState", "health", 4);
                     tx.set_field_bits("game", "ShipState", "armor", 4);
                 },
                 [](Document const& doc) {
                     EXPECT_EQ(doc.query().module("game").packed_value("ShipState").storage_type(),
                               "std::uint8_t");
                 }},
        EditCase{"ChangeSoaMember",
                 [](Transaction& tx) {
                     tx.set_member_type("game", "FighterData", "health", "std::uint32_t");
                 },
                 [](Document const& doc) {
                     auto const members{doc.query().module("game").soa("FighterData").members()};
                     ASSERT_EQ(members.size(), 2);
                     EXPECT_EQ(members[0].name(), "health");
                     EXPECT_EQ(members[0].type(), "std::uint32_t");
                     EXPECT_EQ(members[1].name(), "speed");
                 }},
        EditCase{"CreateThenEdit",
                 [](Transaction& tx) {
                     tx.create_declaration("game",
                                           "(packed-value Other :storage std::uint8_t (field value "
                                           "std::uint8_t :bits 8))");
                     tx.set_field_bits("game", "Other", "value", 4);
                 },
                 [](Document const& doc) {
                     EXPECT_EQ(
                         doc.query().module("game").packed_value("Other").field("value").bits(), 4);
                 }},
        EditCase{"ReplaceDeclaration",
                 [](Transaction& tx) {
                     tx.replace_declaration("game",
                                            "ShipState",
                                            "(packed-value ShipState :storage std::uint16_t (field "
                                            "health std::uint16_t :bits 12))");
                 },
                 [](Document const& doc) { EXPECT_EQ(health_bits(doc), 12); }},
        EditCase{"DeleteDeclaration",
                 [](Transaction& tx) { tx.delete_declaration("game", "ShipState"); },
                 [](Document const& doc) {
                     EXPECT_FALSE(doc.query().module("game").packed_value("ShipState").exists());
                 }},
        EditCase{"RenameAndEditInOrder",
                 [](Transaction& tx) {
                     tx.rename_declaration("game", "ShipState", "State");
                     tx.set_field_bits("game", "State", "health", 4);
                 },
                 [](Document const& doc) {
                     EXPECT_FALSE(doc.query().module("game").packed_value("ShipState").exists());
                     EXPECT_EQ(
                         doc.query().module("game").packed_value("State").field("health").bits(),
                         4);
                 }},
        EditCase{"DuplicateThenEdit",
                 [](Transaction& tx) {
                     tx.duplicate_declaration("game", "ShipState", "Copy");
                     tx.set_field_bits("game", "Copy", "health", 4);
                 },
                 [](Document const& doc) {
                     EXPECT_EQ(health_bits(doc), 8);
                     EXPECT_EQ(
                         doc.query().module("game").packed_value("Copy").field("health").bits(), 4);
                 }},
        EditCase{"MoveToNewModule",
                 [](Transaction& tx) {
                     tx.create_module(R"((module destination :header "Destination.h"))");
                     tx.move_declaration("game", "ShipState", "destination");
                 },
                 [](Document const& doc) {
                     EXPECT_FALSE(doc.query().module("game").packed_value("ShipState").exists());
                     EXPECT_EQ(doc.query()
                                   .module("destination")
                                   .packed_value("ShipState")
                                   .field("health")
                                   .bits(),
                               8);
                 }},
        EditCase{
            "RenameModuleThenEdit",
            [](Transaction& tx) {
                tx.rename_module("game", "play");
                tx.set_field_bits("play", "ShipState", "health", 4);
            },
            [](Document const& doc) {
                EXPECT_FALSE(doc.query().module("game").exists());
                EXPECT_EQ(
                    doc.query().module("play").packed_value("ShipState").field("health").bits(), 4);
            }},
        EditCase{"RemoveModule",
                 [](Transaction& tx) { tx.delete_module("game"); },
                 [](Document const& doc) { EXPECT_FALSE(doc.query().module("game").exists()); }},
        EditCase{"AddRenameAndReorderSoaMember",
                 [](Transaction& tx) {
                     tx.add_member("game", "FighterData", "position", "double");
                     tx.rename_member("game", "FighterData", "position", "distance");
                     tx.reorder_member("game", "FighterData", "distance", 0);
                 },
                 [](Document const& doc) {
                     auto const members{doc.query().module("game").soa("FighterData").members()};
                     ASSERT_EQ(members.size(), 3);
                     EXPECT_EQ(members.front().name(), "distance");
                     EXPECT_EQ(members.front().type(), "double");
                     EXPECT_EQ(members.back().name(), "speed");
                 }},
        EditCase{"RemoveSoaMember",
                 [](Transaction& tx) { tx.delete_member("game", "FighterData", "health"); },
                 [](Document const& doc) {
                     auto const members{doc.query().module("game").soa("FighterData").members()};
                     ASSERT_EQ(members.size(), 1);
                     EXPECT_EQ(members.front().name(), "speed");
                 }},
        EditCase{
            "ReplacePackedField",
            [](Transaction& tx) {
                tx.delete_field("game", "ShipState", "armor");
                tx.add_field("game", "ShipState", "shields", "std::uint8_t", 4);
                tx.rename_field("game", "ShipState", "shields", "shield");
                tx.set_field_type("game", "ShipState", "shield", "std::uint16_t");
                tx.reorder_field("game", "ShipState", "shield", 0);
            },
            [](Document const& doc) {
                EXPECT_FALSE(
                    doc.query().module("game").packed_value("ShipState").field("armor").exists());
                EXPECT_EQ(
                    doc.query().module("game").packed_value("ShipState").field("shield").bits(), 4);
            }},
        EditCase{"RejectOverflow",
                 [](Transaction& tx) { tx.set_field_bits("game", "ShipState", "health", 12); },
                 [](Document const& doc) { EXPECT_EQ(health_bits(doc), 8); },
                 false},
        EditCase{"RejectUnknownField",
                 [](Transaction& tx) { tx.set_field_bits("game", "ShipState", "missing", 4); },
                 [](Document const& doc) { EXPECT_EQ(health_bits(doc), 8); },
                 false},
        EditCase{
            "RejectUnknownMember",
            [](Transaction& tx) { tx.set_member_type("game", "FighterData", "missing", "float"); },
            [](Document const& doc) {
                EXPECT_EQ(doc.query().module("game").soa("FighterData").member("health").type(),
                          "std::uint16_t");
            },
            false}),
    [](testing::TestParamInfo<EditCase> const& info) { return std::string{info.param.name}; });

TEST(DocumentHistory, FailedCommitPreservesRedoAndDiagnosticsAccumulate) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto first{doc.transaction()};
    first.set_field_bits("game", "ShipState", "health", 4);
    ASSERT_TRUE(doc.commit(first)) << diagnostic_messages(editor);
    ASSERT_TRUE(doc.undo());
    auto invalid{doc.transaction()};
    invalid.set_field_bits("game", "ShipState", "health", 20);
    EXPECT_FALSE(doc.commit(invalid));
    EXPECT_TRUE(doc.can_redo());
    auto const errors{editor.diagnostics().size()};
    ASSERT_GT(errors, 0);
    auto const message{editor.format_diagnostic(lispb::DiagnosticId{0})};
    auto const operation{editor.diagnostics().operation(lispb::DiagnosticId{0})};
    ASSERT_TRUE(doc.redo());
    EXPECT_EQ(editor.diagnostics().size(), errors);
    EXPECT_EQ(editor.last_diagnostic_begin(), errors);
    EXPECT_EQ(editor.format_diagnostic(lispb::DiagnosticId{0}), message);
    EXPECT_EQ(editor.diagnostics().operation(lispb::DiagnosticId{0}), operation);
    EXPECT_EQ(health_bits(doc), 4);
    ASSERT_TRUE(doc.undo());
    auto branch{doc.transaction()};
    branch.set_field_bits("game", "ShipState", "health", 6);
    ASSERT_TRUE(doc.commit(branch)) << diagnostic_messages(editor);
    EXPECT_FALSE(doc.can_redo());
    EXPECT_EQ(health_bits(doc), 6);
}

TEST(DocumentHistory, QueuedTransactionsResolveNamesInExecutionOrder) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto rename{doc.transaction()};
    rename.rename_declaration("game", "ShipState", "State");
    auto edit{doc.transaction()};
    edit.set_field_bits("game", "State", "health", 4);
    ASSERT_TRUE(doc.commit(rename)) << diagnostic_messages(editor);
    ASSERT_TRUE(doc.commit(edit)) << diagnostic_messages(editor);
    EXPECT_EQ(doc.query().module("game").packed_value("State").field("health").bits(), 4);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.query().module("game").packed_value("State").field("health").bits(), 8);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(health_bits(doc), 8);
}

TEST(DocumentQueries, MissingNamesPropagateWithoutErrors) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    EXPECT_FALSE(doc.query().module("missing").packed_value("ShipState").field("health").bits());
    EXPECT_FALSE(doc.query().module("game").soa("ShipState").exists());
    EXPECT_FALSE(doc.query().module("game").soa("FighterData").member("missing").type());
    EXPECT_EQ(editor.diagnostics().size(), 0);
}

TEST(DocumentQueries, EnumeratesModulesAndDeclarationsInAuthoredOrder) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto const modules{doc.query().modules()};
    ASSERT_EQ(modules.size(), 1);
    EXPECT_EQ(modules.front().name(), "game");
    auto const declarations{modules.front().declarations()};
    ASSERT_EQ(declarations.size(), 2);
    EXPECT_EQ(declarations[0].name(), "ShipState");
    EXPECT_EQ(declarations[1].name(), "FighterData");
    EXPECT_TRUE(declarations[0].source()->contains("packed-value ShipState"));
    EXPECT_FALSE(modules.front().declaration("missing").source());
}

TEST(DocumentLoad, InvalidSourceRecordsDiagnostics) {
    EditorSession editor;
    auto doc{editor.from_source("(module")};
    EXPECT_FALSE(doc.valid());
    EXPECT_GT(editor.diagnostics().size(), 0);
}

TEST(EditorSession, DocumentMovesPreserveQueuedTransactionsAndHistory) {
    EditorSession editor;
    auto document{editor.from_source(source)};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto queued{document.transaction()};
    queued.set_field_bits("game", "ShipState", "health", 4);
    auto moved{std::move(document)};
    ASSERT_TRUE(moved.commit(queued)) << diagnostic_messages(editor);
    EXPECT_EQ(health_bits(moved), 4);

    auto next{moved.transaction()};
    next.set_field_bits("game", "ShipState", "health", 6);
    EditorSession other_editor;
    auto replacement{other_editor.from_source("")};
    replacement = std::move(moved);
    EXPECT_EQ(&replacement.session(), &editor);
    ASSERT_TRUE(replacement.commit(next)) << diagnostic_messages(editor);
    EXPECT_EQ(health_bits(replacement), 6);
    ASSERT_TRUE(replacement.undo());
    EXPECT_EQ(health_bits(replacement), 4);
    ASSERT_TRUE(replacement.redo());
    EXPECT_EQ(health_bits(replacement), 6);
}

TEST(EditorSession, DocumentsRetainIndependentHistoriesAndOutputs) {
    EditorSession editor;
    auto first{editor.from_source(source)};
    auto second{editor.from_source(source)};
    ASSERT_TRUE(first.valid()) << diagnostic_messages(editor);
    ASSERT_TRUE(second.valid()) << diagnostic_messages(editor);
    auto const second_source{second.sources()};
    auto first_edit{first.transaction()};
    first_edit.set_field_bits("game", "ShipState", "health", 4);
    ASSERT_TRUE(first.commit(first_edit)) << diagnostic_messages(editor);
    EXPECT_EQ(health_bits(second), 8);
    EXPECT_EQ(second.sources().front().text, second_source.front().text);
    EXPECT_FALSE(second.can_undo());
    auto second_edit{second.transaction()};
    second_edit.set_field_bits("game", "ShipState", "health", 6);
    ASSERT_TRUE(second.commit(second_edit)) << diagnostic_messages(editor);
    ASSERT_TRUE(first.undo());
    EXPECT_EQ(health_bits(first), 8);
    EXPECT_EQ(health_bits(second), 6);
    EXPECT_TRUE(second.can_undo());
    EXPECT_FALSE(second.can_redo());
}

TEST(EditorSession, DiagnosticsRemainReadableAfterDocumentDestruction) {
    EditorSession editor;
    std::string original;
    {
        auto invalid{editor.from_source("(module")};
        EXPECT_FALSE(invalid.valid());
        ASSERT_GT(editor.diagnostics().size(), 0);
        original = editor.format_diagnostic(lispb::DiagnosticId{0});
    }
    EXPECT_EQ(editor.format_diagnostic(lispb::DiagnosticId{0}), original);
    auto const errors{editor.diagnostics().size()};
    auto valid{editor.from_source(source)};
    ASSERT_TRUE(valid.valid()) << diagnostic_messages(editor);
    EXPECT_EQ(editor.diagnostics().size(), errors);
    EXPECT_EQ(editor.last_diagnostic_begin(), errors);
    EXPECT_EQ(editor.format_diagnostic(lispb::DiagnosticId{0}), original);
}

struct DeclarationCase {
    std::string_view name;
    std::string_view declaration;
};
class DocumentDeclaration : public testing::TestWithParam<DeclarationCase> {};

TEST_P(DocumentDeclaration, CreatesAndRoundTripsAuthoredDeclaration) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto doc{editor.from_source(R"((module game :header "Game.h" :backend standard-library
        (enum Kind std::uint8_t (value Small) (value Large))
        (integer-scalar Value :signed false :minimum 0 :maximum 100
            (code Invalid :value 127 :sentinel true))))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto tx{doc.transaction()};
    tx.create_declaration("game", test.declaration);
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    ASSERT_TRUE(doc.find_type("game", "Created"));
    auto const sources{doc.sources()};
    auto reloaded{editor.from_source(sources.front().text)};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(editor);
    EXPECT_TRUE(reloaded.find_type("game", "Created"));
    EXPECT_EQ(reloaded.declaration_source("game", "Created"),
              doc.declaration_source("game", "Created"));
}

INSTANTIATE_TEST_SUITE_P(
    DeclarationKinds,
    DocumentDeclaration,
    testing::Values(
        DeclarationCase{"Enum", "(enum Created std::uint8_t (value A) (value B :value 4))"},
        DeclarationCase{"Record", "(record Created (member values std::uint16_t :count 4))"},
        DeclarationCase{
            "Union",
            "(union Created (alternative small std::uint8_t) (alternative large std::uint32_t))"},
        DeclarationCase{"TaggedUnion",
                        "(tagged-union Created :discriminant Kind (alternative small std::uint8_t "
                        ":tag Small) (alternative large std::uint32_t :tag Large))"},
        DeclarationCase{
            "Packed",
            "(packed-value Created :storage std::uint16_t (field health std::uint16_t :bits 12))"},
        DeclarationCase{
            "Soa",
            "(struct Created (member positions array float) (member ids array std::uint32_t))"},
        DeclarationCase{"Scalar", "(integer-scalar Created :signed true :minimum -10 :maximum 10)"},
        DeclarationCase{
            "Quantized",
            "(linear-quantized Created :source Value :bits 8 :reserved-codes 1 :clipping clamp)"},
        DeclarationCase{"Varint", "(integer-varint Created :source Value :encoding unsigned)"},
        DeclarationCase{"FixedPoint",
                        "(fixed-point Created :signed true :total-bits 16 :fractional-bits 8 "
                        ":rounding nearest-even)"},
        DeclarationCase{
            "MiniFloat",
            "(mini-float Created :sign-bits 1 :exponent-bits 3 :significand-bits 4 :bias 3)"},
        DeclarationCase{"OptionalSentinel",
                        "(optional-sentinel Created :source Value :sentinel Invalid)"},
        DeclarationCase{"OptionalPresence", "(optional-presence-bit Created :source Value)"}),
    [](testing::TestParamInfo<DeclarationCase> const& info) {
        return std::string{info.param.name};
    });

TEST(DocumentEdit, EnumAndRecordChildrenUseNamedOperations) {
    EditorSession editor;
    auto doc{editor.from_source(R"((module game :header "Game.h"
        (enum State std::uint8_t (value Idle) (value Running))
        (record Position (member x float) (member y float))))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto tx{doc.transaction()};
    tx.add_enumerator("game", "State", "Done", "8");
    tx.rename_enumerator("game", "State", "Running", "Active");
    tx.set_enumerator_initializer("game", "State", "Active", "4");
    tx.reorder_enumerator("game", "State", "Done", 0);
    tx.delete_enumerator("game", "State", "Idle");
    tx.set_member_count("game", "Position", "x", 3);
    tx.set_member_type("game", "Position", "y", "double");
    tx.add_member("game", "Position", "z", "float");
    tx.rename_member("game", "Position", "z", "height");
    tx.reorder_member("game", "Position", "height", 0);
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    auto const enumeration{doc.declaration_source("game", "State").value()};
    EXPECT_LT(enumeration.find("Done"), enumeration.find("Active"));
    EXPECT_EQ(enumeration.find("Idle"), std::string::npos);
    EXPECT_NE(enumeration.find('4'), std::string::npos);
    auto const record{doc.declaration_source("game", "Position").value()};
    EXPECT_LT(record.find("height"), record.find("member x"));
    EXPECT_NE(record.find(":count 3"), std::string::npos);
    EXPECT_NE(record.find("double"), std::string::npos);
}

TEST(DocumentHistory, ReplaysCoordinatedEditsAcrossSeveralTransactions) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    auto const original_type{doc.find_type("game", "ShipState").value()};
    auto const declaration{doc.find_declaration(doc.types().identity(original_type)).value()};
    auto first{doc.transaction()};
    first.set_field_bits("game", "ShipState", "health", 12);
    first.set_field_bits("game", "ShipState", "armor", 4);
    ASSERT_TRUE(doc.commit(first)) << diagnostic_messages(editor);
    auto second{doc.transaction()};
    second.rename_declaration("game", "ShipState", "State");
    second.set_field_bits("game", "State", "health", 10);
    ASSERT_TRUE(doc.commit(second)) << diagnostic_messages(editor);
    auto const renamed_type{doc.find_type("game", "State").value()};
    EXPECT_EQ(doc.find_declaration(doc.types().identity(renamed_type)), declaration);
    auto third{doc.transaction()};
    third.delete_declaration("game", "State");
    ASSERT_TRUE(doc.commit(third)) << diagnostic_messages(editor);
    EXPECT_FALSE(doc.find_type("game", "State"));
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.query().module("game").packed_value("State").field("health").bits(), 10);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(health_bits(doc), 12);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(health_bits(doc), 8);
    EXPECT_EQ(
        doc.find_declaration(doc.types().identity(doc.find_type("game", "ShipState").value())),
        declaration);
    EXPECT_FALSE(doc.undo());
    ASSERT_TRUE(doc.redo());
    ASSERT_TRUE(doc.redo());
    ASSERT_TRUE(doc.redo());
    EXPECT_FALSE(doc.find_type("game", "State"));
    EXPECT_FALSE(doc.redo());
}

TEST(DocumentEdit, RenameRepairsReferencesAndRejectedDeleteIsAtomic) {
    EditorSession editor;
    auto doc{editor.from_source(R"((module game :header "Game.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 100)
        (optional-presence-bit Present :source Health)))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto rename{doc.transaction()};
    rename.rename_declaration("game", "Health", "HitPoints");
    ASSERT_TRUE(doc.commit(rename)) << diagnostic_messages(editor);
    EXPECT_NE(doc.declaration_source("game", "Present")->find("HitPoints"), std::string::npos);
    auto const revision{doc.revision()};
    auto invalid{doc.transaction()};
    invalid.delete_declaration("game", "HitPoints");
    EXPECT_FALSE(doc.commit(invalid));
    EXPECT_EQ(doc.revision(), revision);
    EXPECT_TRUE(doc.find_type("game", "HitPoints"));
    EXPECT_GT(editor.diagnostics().size(), editor.last_diagnostic_begin());
    auto remove_both{doc.transaction()};
    remove_both.delete_declaration("game", "HitPoints");
    remove_both.delete_declaration("game", "Present");
    EXPECT_TRUE(doc.commit(remove_both));
}

TEST(DocumentLoad, ResolvesNamedSourcesAndOwnsTheirContents) {
    EditorSession editor;
    Document doc{[&editor] {
        std::string first{R"((module first :header "First.h" (record Point (member x float))))"};
        std::string second{
            R"((module second :header "Second.h" (record Ship (member position Point))))"};
        std::array sources{DocumentSource{"first.lispb", first},
                           DocumentSource{"second.lispb", second}};
        return editor.from_sources(sources);
    }()};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    EXPECT_TRUE(doc.find_type("first", "Point"));
    EXPECT_TRUE(doc.find_type("second", "Ship"));
    auto tx{doc.transaction()};
    tx.rename_declaration("first", "Point", "Position");
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    EXPECT_NE(doc.declaration_source("second", "Ship")->find("Position"), std::string::npos);
    ASSERT_TRUE(doc.undo());
    EXPECT_TRUE(doc.find_type("first", "Point"));
}

TEST(DocumentEdit, RenameRespectsNamespaceLocalShadowing) {
    EditorSession editor;
    auto doc{editor.from_source(R"(
        (module global :header "Global.h"
            (record Value (member x float))
            (record Holder (member value Value)))
        (module local :header "Local.h" :namespace local
            (record Value (member y double))
            (record Holder (member value Value))))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto tx{doc.transaction()};
    tx.rename_declaration("global", "Value", "GlobalValue");
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    EXPECT_NE(doc.declaration_source("global", "Holder")->find("GlobalValue"), std::string::npos);
    EXPECT_EQ(doc.declaration_source("local", "Holder")->find("GlobalValue"), std::string::npos);
    EXPECT_TRUE(doc.find_type("local", "Value"));
}

TEST(DocumentEdit, NamespaceMoveRetainsOutgoingAndIncomingReferences) {
    EditorSession editor;
    auto doc{editor.from_source(R"(
        (module first :header "First.h" :namespace alpha
            (record Value (member x float))
            (record Holder (member value Value))
            (record User (member holder Holder)))
        (module second :header "Second.h" :namespace beta
            (record Value (member y double))))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto const holder{
        doc.find_declaration(doc.types().identity(*doc.find_type("first", "Holder")))};
    auto tx{doc.transaction()};
    tx.move_declaration("first", "Holder", "second");
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    EXPECT_FALSE(doc.find_type("first", "Holder"));
    EXPECT_TRUE(doc.find_type("second", "Holder"));
    EXPECT_EQ(doc.find_declaration(doc.types().identity(*doc.find_type("second", "Holder"))),
              holder);
    EXPECT_NE(doc.declaration_source("second", "Holder")->find("alpha::Value"), std::string::npos);
    EXPECT_NE(doc.declaration_source("first", "User")->find("beta::Holder"), std::string::npos);
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(doc.find_declaration(doc.types().identity(*doc.find_type("first", "Holder"))),
              holder);
    ASSERT_TRUE(doc.redo());
    EXPECT_EQ(doc.find_declaration(doc.types().identity(*doc.find_type("second", "Holder"))),
              holder);
}

TEST(DocumentEdit, ReplacementRenamesReferencedDeclaration) {
    EditorSession editor;
    auto doc{editor.from_source(R"((module game :header "Game.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 100)
        (optional-presence-bit Present :source Health)))")};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto tx{doc.transaction()};
    tx.replace_declaration(
        "game", "Health", "(integer-scalar HitPoints :signed false :minimum 0 :maximum 200)");
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    EXPECT_FALSE(doc.find_type("game", "Health"));
    EXPECT_TRUE(doc.find_type("game", "HitPoints"));
    EXPECT_NE(doc.declaration_source("game", "Present")->find("HitPoints"), std::string::npos);
}

TEST(DocumentEdit, WholeSourceReplacementSupportsAllModuleKindsAndRejectsInvalidInput) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto const id{
        doc.find_declaration(doc.types().identity(doc.find_type("game", "ShipState").value()))};
    auto tx{doc.transaction()};
    tx.replace_source("document.lispb", R"(
        (module game :header "Game.h"
            (packed-value ShipState :storage std::uint16_t (field health std::uint16_t :bits 12)))
        (settings-module settings :header "Settings.h" :source "Settings.cpp"
            :api-name SettingsAccess :state-name SettingsState
            (category video "Video")
            (setting vsync "VSync" :category video :value-type bool :backend engine :apply deferred
                (control toggle)))
        (umbrella-module all :header "All.h" :headers ("Game.h" "Settings.h")))");
    ASSERT_TRUE(doc.commit(tx)) << diagnostic_messages(editor);
    EXPECT_EQ(health_bits(doc), 12);
    EXPECT_EQ(
        doc.find_declaration(doc.types().identity(doc.find_type("game", "ShipState").value())), id);
    auto const rendered{std::string{doc.sources().front().text}};
    EXPECT_TRUE(rendered.contains("(settings-module settings"));
    EXPECT_TRUE(rendered.contains("(umbrella-module all"));
    auto invalid{doc.transaction()};
    invalid.replace_source("document.lispb", "(module broken");
    EXPECT_FALSE(doc.commit(invalid));
    EXPECT_EQ(doc.sources().front().text, rendered);
    EXPECT_GT(editor.diagnostics().size(), editor.last_diagnostic_begin());
    ASSERT_TRUE(doc.undo());
    EXPECT_EQ(health_bits(doc), 8);
}

TEST(DocumentEdit, RenderedSourcesOwnTheirTextAcrossSessionGrowthAndReplay) {
    EditorSession editor;
    auto doc{editor.from_source(source)};
    ASSERT_TRUE(doc.valid()) << diagnostic_messages(editor);
    auto const original{std::string{doc.sources().front().text}};
    auto tx{doc.transaction()};
    tx.set_field_bits("game", "ShipState", "health", 4);
    ASSERT_TRUE(doc.commit(tx));
    auto const edited{doc.sources()};
    EXPECT_NE(edited.front().text, original);
    EXPECT_TRUE(edited.front().text.contains(":bits 4"));

    auto other{editor.from_source(";" + std::string(32768, 'x') +
                                  "\n(module other :header \"Other.h\" (record Other))")};
    ASSERT_TRUE(other.valid()) << diagnostic_messages(editor);
    EXPECT_EQ(doc.sources().front().path, "document.lispb");
    EXPECT_EQ(doc.sources().front().text, edited.front().text);
    EXPECT_TRUE(doc.dirty());
    ASSERT_TRUE(doc.undo());
    EXPECT_FALSE(doc.dirty());
    EXPECT_EQ(doc.sources().front().text, original);
    EXPECT_TRUE(edited.front().text.contains(":bits 4"));
    EXPECT_EQ(health_bits(doc), 8);
}
} // namespace ioj::layout::document_tests
