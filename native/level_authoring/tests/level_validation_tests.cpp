#include <ioj/levels/level_definition.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

namespace ioj::levels::validation_tests {
namespace {
auto camera_level() -> LevelDefinitionComponents {
    return {
        .metadata = {.id = LevelId{"camera"}, .title = "Camera"},
        .camera = LevelCameraDefinition{.target_entity_ids = {EntityId{"blue"}, EntityId{"red"}},
                                        .offset_direction = {-1., -1., .5},
                                        .distance = 10000.},
        .entities = {
            {.id = EntityId{"blue"}, .archetype = EntityArchetype::CapitalShip, .team = Team::Blue},
            {.id = EntityId{"red"}, .archetype = EntityArchetype::CapitalShip, .team = Team::Red}}};
}

void expect_error(LevelDefinitionComponents definition, DiagnosticCode const code) {
    auto const result{LevelDefinition::make(std::move(definition))};
    ASSERT_FALSE(result);
    EXPECT_TRUE(std::ranges::contains(result.error(), code, &Diagnostic::code))
        << format_diagnostics(result.error());
}
}

TEST(LevelDefinition, ParTimeRequiresAPositivePlayerMissionTarget) {
    auto definition{camera_level()};
    definition.camera.reset();
    definition.entities[0].archetype = EntityArchetype::PlayerFighter;
    definition.player_entity_id = EntityId{"blue"};
    definition.mission = MissionDefinition{};
    definition.metadata.par_time_seconds = 45.f;
    ASSERT_TRUE(LevelDefinition::make(definition));
    for (float const seconds : {0.f, -1.f, std::numeric_limits<float>::infinity()}) {
        SCOPED_TRACE(seconds);
        definition.metadata.par_time_seconds = seconds;
        expect_error(definition, DiagnosticCode::InvalidParTime);
    }
    definition = camera_level();
    definition.metadata.par_time_seconds = 45.f;
    expect_error(definition, DiagnosticCode::UnexpectedParTime);
}

TEST(LevelDefinition, CameraCanTargetOneEntity) {
    auto definition{camera_level()};
    definition.camera->target_entity_ids.resize(1);
    EXPECT_TRUE(LevelDefinition::make(std::move(definition)));
}

TEST(LevelDefinition, DuplicateAuthoredEntityIdFailsValidation) {
    auto definition{camera_level()};
    definition.entities[1].id = definition.entities[0].id;
    expect_error(definition, DiagnosticCode::DuplicateEntityId);
}

TEST(LevelDefinition, ObjectiveCyclesAreValidated) {
    auto definition{camera_level()};
    ObjectiveDefinition objective{.id = ObjectiveName{"primary"}, .team = Team::Blue};
    objective.expressions = {{AllExpr{.children = {0}}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    definition.mission = MissionDefinition{.objectives = {objective}};
    expect_error(definition, DiagnosticCode::InvalidMissionStructure);
}

TEST(LevelDefinition, MissionEntityReferencesAreValidated) {
    auto definition{camera_level()};
    ObjectiveDefinition objective{.id = ObjectiveName{"primary"}, .team = Team::Blue};
    objective.expressions = {{EntityDestroyedExpr{EntityId{"missing"}}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    definition.mission = MissionDefinition{.objectives = {objective}};
    expect_error(definition, DiagnosticCode::InvalidMissionStructure);
}

TEST(LevelDefinition, CameraAndPlayerAreMutuallyExclusive) {
    auto definition{camera_level()};
    definition.entities[0].archetype = EntityArchetype::PlayerFighter;
    definition.player_entity_id = EntityId{"blue"};
    expect_error(definition, DiagnosticCode::ConflictingViewpoints);
    definition.player_entity_id = {};
    definition.camera.reset();
    expect_error(definition, DiagnosticCode::MissingViewpoint);
}

TEST(LevelDefinition, CameraTargetsMustBeUniqueDeclaredEntities) {
    auto definition{camera_level()};
    definition.camera->target_entity_ids = {EntityId{"blue"}, EntityId{"blue"}};
    expect_error(definition, DiagnosticCode::DuplicateCameraTarget);
    definition.camera->target_entity_ids = {EntityId{"missing"}};
    expect_error(definition, DiagnosticCode::CameraTargetNotFound);
    definition.camera->target_entity_ids.clear();
    expect_error(definition, DiagnosticCode::MissingCameraTarget);
}

TEST(LevelDefinition, CameraPlacementMustBeFiniteAndNonZero) {
    for (double const distance : {0., std::numeric_limits<double>::infinity()}) {
        auto definition{camera_level()};
        definition.camera->distance = distance;
        expect_error(definition, DiagnosticCode::InvalidCameraDistance);
    }
    for (auto const direction :
         {Vector3d{}, Vector3d{std::numeric_limits<double>::quiet_NaN(), 0., 0.}}) {
        auto definition{camera_level()};
        definition.camera->offset_direction = direction;
        expect_error(definition, DiagnosticCode::InvalidCameraOffsetDirection);
    }
}
}
