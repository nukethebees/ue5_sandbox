#include <ioj/levels/authoring/identifiers.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/scene_reconciliation.h>
#include <ioj/levels/authoring/source_session.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <limits>

namespace ioj::levels::authoring::tests {
namespace {
auto timer_mission() -> MissionDefinition {
    ObjectiveDefinition objective{.id = ioj::levels::ObjectiveName{"primary"}, .team = Team::Blue};
    objective.expressions = {
        {ioj::levels::TimeElapsedExpression{.seconds = ioj::NonNegativeFinite<double>::make(1.0)}}};
    objective.roots[ioj::ObjectiveOutcome::Passed] = 0;
    return {.objectives = {objective}};
}

auto reconcile(SceneSnapshot const& observed, LevelDefinitionComponents incoming)
    -> std::expected<SyncPlan, Diagnostics> {
    return LevelDefinition::make(std::move(incoming)).transform([&](LevelDefinition const& level) {
        return reconcile_scene(observed, level);
    });
}

auto scene() -> SceneSnapshot {
    SceneSnapshot result;
    result.definition.metadata = {.id = LevelId{"fixture"}, .title = "Fixture"};
    result.definition.player_entity_id = EntityId{"player"};
    result.definition.entities = {
        {.id = EntityId{"player"}, .archetype = EntityArchetype::PlayerFighter, .team = Team::Blue},
        {.id = EntityId{"enemy"}, .archetype = EntityArchetype::CapitalShip, .team = Team::Red}};
    result.properties = {{.label = "player"}, {.label = "enemy"}};
    return result;
}

struct EntityCase {
    char const* name;
    std::function<void(SceneSnapshot&, LevelDefinitionComponents&)> edit;
    SyncAction action;
    bool configure;
    bool relabel;
};

class Reconciliation : public ::testing::TestWithParam<EntityCase> {};

TEST_P(Reconciliation, ProducesOnlyNecessaryOperations) {
    auto observed{scene()};
    auto incoming{observed.definition};
    auto const& test{GetParam()};
    test.edit(observed, incoming);
    auto const plan{reconcile(observed, incoming)};

    ASSERT_TRUE(plan);
    ASSERT_EQ(plan->changes.size(), 1);
    EXPECT_EQ(plan->changes[0].action, test.action);
    EXPECT_EQ(plan->changes[0].configure_actor, test.configure);
    EXPECT_EQ(plan->changes[0].relabel_actor, test.relabel);
}
INSTANTIATE_TEST_SUITE_P(
    Entities,
    Reconciliation,
    ::testing::Values(
        EntityCase{"add",
                   [](auto&, auto& d) {
                       d.entities.push_back({.id = EntityId{"new"},
                                             .archetype = EntityArchetype::StaticTurret,
                                             .team = Team::Red});
                   },
                   SyncAction::Add,
                   true,
                   true},
        EntityCase{"remove",
                   [](auto&, auto& d) { d.entities.pop_back(); },
                   SyncAction::Remove,
                   false,
                   false},
        EntityCase{"team",
                   [](auto&, auto& d) { d.entities[1].team = Team::Blue; },
                   SyncAction::Update,
                   true,
                   false},
        EntityCase{"position",
                   [](auto&, auto& d) { d.entities[1].position = {42.0, 0.0, 0.0}; },
                   SyncAction::Update,
                   true,
                   false},
        EntityCase{"rotation",
                   [](auto&, auto& d) { d.entities[1].rotation.yaw = 90.0; },
                   SyncAction::Update,
                   true,
                   false},
        EntityCase{"scale",
                   [](auto& s, auto&) { s.properties[1].scale = {2.0, 1.0, 1.0}; },
                   SyncAction::Update,
                   true,
                   false},
        EntityCase{"time",
                   [](auto&, auto& d) { d.entities[1].spawn_time_seconds = 4.0; },
                   SyncAction::Update,
                   false,
                   false},
        EntityCase{"label",
                   [](auto& s, auto&) { s.properties[1].label = "Enemy"; },
                   SyncAction::Update,
                   false,
                   true},
        EntityCase{"class",
                   [](auto& s, auto&) { s.properties[1].class_matches = false; },
                   SyncAction::Replace,
                   true,
                   true},
        EntityCase{"archetype",
                   [](auto&, auto& d) { d.entities[1].archetype = EntityArchetype::StaticTurret; },
                   SyncAction::Replace,
                   true,
                   true}),
    [](auto const& info) { return info.param.name; });

TEST(NativeEditorScene, UnchangedAndEquivalentRotationsNeedNoOperations) {
    auto observed{scene()};
    for (auto const yaw : {0.0, 180.0, 45.0}) {
        observed.definition.entities[1].rotation = {30.0, yaw, -60.0};
        auto incoming{observed.definition};
        incoming.entities[1].rotation.yaw = yaw - 360.0;
        auto const plan{reconcile(observed, incoming)};
        ASSERT_TRUE(plan);
        EXPECT_FALSE(plan->has_changes()) << yaw;
    }

    EXPECT_TRUE(observed == observed);
}

TEST(NativeEditorScene, CameraReferenceOrderDoesNotChangeThePlan) {
    auto observed{scene()};
    auto& definition{observed.definition};
    definition.player_entity_id = {};
    definition.entities[0].archetype = EntityArchetype::CapitalShip;
    definition.camera = {.target_entity_ids = {EntityId{"player"}, EntityId{"enemy"}},
                         .offset_direction = {-1.0, 0.0, 0.0},
                         .distance = 1000.0};
    definition.mission = timer_mission();
    auto incoming{definition};
    std::ranges::reverse(incoming.camera->target_entity_ids);
    auto const plan{reconcile(observed, incoming)};
    ASSERT_TRUE(plan);
    EXPECT_FALSE(plan->has_changes());
}

TEST(NativeEditorScene, InvalidObservedValuesCanBeComparedAndRepaired) {
    auto observed{scene()};
    auto const incoming{observed.definition};
    observed.definition.entities[1].spawn_time_seconds = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(observed == observed);
    auto const plan{reconcile(observed, incoming)};
    ASSERT_TRUE(plan);
    ASSERT_EQ(plan->changes.size(), 1);
    EXPECT_FALSE(plan->changes[0].configure_actor);

    auto const nan{std::numeric_limits<double>::quiet_NaN()};
    observed.definition.collision_grid = {.level_size = Vector3d{nan, 1.0, 1.0}};
    observed.definition.camera = {.offset_direction = {nan, 0.0, 0.0}, .distance = nan};
    observed.definition.mission = timer_mission();
    EXPECT_EQ(observed, observed);

    auto const repaired{reconcile(observed, incoming)};
    ASSERT_TRUE(repaired);
    EXPECT_TRUE(repaired->collision_grid_changed);
    EXPECT_TRUE(repaired->viewpoint_changed);
    EXPECT_TRUE(repaired->mission_changed);
}

TEST(NativeEditorScene, ComparesDocumentFieldsAndRepairsInvalidReferences) {
    auto observed{scene()};
    auto incoming{observed.definition};
    incoming.metadata.description = "Changed";
    incoming.collision_grid = {.level_size = Vector3d{100000.0, 100000.0, 100000.0}};
    incoming.mission = timer_mission();
    auto const plan{reconcile(observed, incoming)};
    ASSERT_TRUE(plan);
    EXPECT_TRUE(plan->changes.empty());
    EXPECT_TRUE(plan->metadata_changed);
    EXPECT_TRUE(plan->collision_grid_changed);
    EXPECT_TRUE(plan->mission_changed);
    EXPECT_FALSE(plan->viewpoint_changed);

    incoming.player_entity_id = {};
    incoming.entities[0].archetype = EntityArchetype::CapitalShip;
    incoming.camera = {.target_entity_ids = {EntityId{"enemy"}},
                       .offset_direction = {-1.0, 0.0, 0.0},
                       .distance = 1000.0};
    auto const camera_plan{reconcile(observed, incoming)};
    ASSERT_TRUE(camera_plan);
    EXPECT_TRUE(camera_plan->viewpoint_changed);
    observed.viewpoint_valid = false;
    observed.mission_references_valid = false;
    EXPECT_TRUE(observed == observed);
    auto const repaired{reconcile(observed, incoming)};
    ASSERT_TRUE(repaired);
    EXPECT_TRUE(repaired->mission_changed);
}

TEST(NativeEditorIdentifiers, UsesAsciiAndAllocatesDeterministicCollisions) {
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

    std::unordered_set<EntityId> used{EntityId{"enemy"}, EntityId{"enemy-2"}};
    EXPECT_EQ(allocate_entity_id("Enemy", "entity", used), EntityId{"enemy-3"});
    EXPECT_EQ(allocate_entity_id("enemy", "entity", used), EntityId{"enemy-4"});
}

TEST(NativeEditorSession, SeparatesSavedAppliedConflictedAndDetachedState) {
    SourceSessionState state;
    state.loaded({}, "", true);
    state.observe({});
    EXPECT_FALSE(state.disk_conflict);
    EXPECT_FALSE(state.dirty());

    state.preview = PreviewState::Valid;
    state.edited("edited");
    EXPECT_TRUE(state.dirty());
    EXPECT_EQ(state.preview, PreviewState::Stale);

    state.saved(false);
    EXPECT_FALSE(state.dirty());
    EXPECT_TRUE(state.unapplied("original"));

    state.preview = PreviewState::Valid;
    state.observe({true, "external"});
    EXPECT_TRUE(state.disk_conflict);
    EXPECT_EQ(state.preview, PreviewState::Valid);

    state.edited("unsaved");
    state.scene_dirty = true;
    state.document_changed();
    EXPECT_TRUE(state.recovery_required);
    EXPECT_FALSE(state.scene_dirty);
    EXPECT_EQ(state.preview, PreviewState::None);

    state.saved(true);
    EXPECT_TRUE(state.recovery_required);
    EXPECT_FALSE(state.dirty());

    state.loaded({true, "next"}, "next", true);
    EXPECT_FALSE(state.recovery_required);
    EXPECT_FALSE(state.disk_conflict);
}
}
}
