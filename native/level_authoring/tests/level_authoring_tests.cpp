#include <ioj/files.h>
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/levels/authoring/source_session.h>
#include <ioj/levels/authoring/starfield_reader.h>
#include <ioj/levels/catalog_validation.h>
#include <ioj/s7/interpreter.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace ioj::levels::authoring::tests {
namespace {
auto timer_mission() -> MissionDefinition {
    ObjectiveDefinition objective{.id = ioj::levels::ObjectiveName{"primary"}, .team = Team::Blue};
    objective.expressions = {
        {ioj::levels::TimeElapsedExpr{.seconds = ioj::NonNegativeFinite<double>::make(1.0)}}};
    objective.roots[ioj::ObjectiveOutcome::Passed] = 0;
    return {.objectives = {objective}};
}

auto emit_source(LevelDefinition const& level) -> std::expected<std::string, Diagnostics> {
    return emit_editor_level_source(level);
}

auto emit_source(LevelDefinitionComponents definition) -> std::expected<std::string, Diagnostics> {
    return LevelDefinition::make(std::move(definition)).and_then([](LevelDefinition const& level) {
        return emit_editor_level_source(level);
    });
}

auto make_level(std::string id, std::vector<LevelId> unlocks = {}) -> LevelDefinitionComponents {
    LevelDefinitionComponents result{
        .metadata = {.id = LevelId{std::move(id)}, .title = "Test Level"},
        .unlock_level_ids = std::move(unlocks),
        .player_entity_id = EntityId{"player"},
    };
    result.entities.push_back({.id = EntityId{"player"},
                               .archetype = EntityArchetype::PlayerFighter,
                               .team = Team::Blue});
    return result;
}
auto checked_level(std::string id, std::vector<LevelId> unlocks = {}) -> LevelDefinition {
    return LevelDefinition::make(make_level(std::move(id), std::move(unlocks))).value();
}

auto has_code(Diagnostics const& errors, DiagnosticCode const code) -> bool {
    return std::ranges::any_of(errors,
                               [code](Diagnostic const& error) { return error.code == code; });
}
auto minimal_source() -> std::string {
    return R"((level :id 'test :title "Test" :player 'player
      :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 0 0) :rotation '(0 0 0)))))";
}
class TemporaryLevelDirectory {
  public:
    TemporaryLevelDirectory() {
        path_ =
            std::filesystem::temp_directory_path() /
            std::filesystem::path{std::format(
                L"level-\u03bb-{}", std::chrono::steady_clock::now().time_since_epoch().count())};
        std::filesystem::create_directories(path_ / "Libraries");
    }
    ~TemporaryLevelDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_{};
};
}

static_assert(!std::is_convertible_v<LevelId, CampaignId>);
static_assert(!std::is_convertible_v<EntityId, Team>);
static_assert(!std::is_convertible_v<std::string, LevelId>);
static_assert(!std::is_default_constructible_v<LevelDefinition>);
static_assert(std::is_same_v<decltype(std::declval<LevelDefinition&>().entities()),
                             std::vector<EntitySpawnDefinition> const&>);

TEST(NativeSettings, PreservesExactIntegersAndRejectsOutOfRangeConversions) {
    s7::Interpreter interpreter;
    auto const assign{[&]<typename T>(std::string_view source, T& target) {
        auto ast{interpreter.evaluate_ast(source)};
        if (!ast) {
            return false;
        }
        auto result{s7::read_override(s7::Reader{*ast}, ast->root, "value", target)};
        if (!result) {
            return false;
        }
        target = *result;
        return true;
    }};
    auto const maximum{std::numeric_limits<std::int64_t>::max()};
    std::int64_t integer{};
    ASSERT_TRUE((assign(std::to_string(maximum), integer)));
    EXPECT_EQ(integer, maximum);
    ASSERT_TRUE((assign("9007199254740993", integer)));
    EXPECT_EQ(integer, 9007199254740993);

    for (auto const invalid :
         {"9223372036854775808.0", "-9223372036854777856.0", "1.5", "+inf.0", "+nan.0"}) {
        SCOPED_TRACE(invalid);
        EXPECT_FALSE((assign(invalid, integer)));
        EXPECT_EQ(integer, 9007199254740993);
    }
    ASSERT_TRUE((assign("-9223372036854775808", integer)));
    EXPECT_EQ(integer, std::numeric_limits<std::int64_t>::min());

    std::uint64_t unsigned_integer{7};
    EXPECT_FALSE((assign("18446744073709551616.0", unsigned_integer)));
    EXPECT_FALSE((assign("-1", unsigned_integer)));
    EXPECT_EQ(unsigned_integer, 7u);
    PositiveFinite<float> real{3.f};
    EXPECT_FALSE((assign("1e100", real)));
    EXPECT_FALSE((assign("+inf.0", real)));
    EXPECT_EQ(real.get(), 3.f);
}

TEST(NativeSettings, AssignsWholeVectorsOnlyAfterEveryComponentConverts) {
    Vector3f target{1.f, 2.f, 3.f};
    s7::Interpreter interpreter;
    auto assign{[&](std::string_view source) {
        auto ast{interpreter.evaluate_ast(source)};
        if (!ast) {
            return false;
        }
        auto result{s7::read_override(s7::Reader{*ast}, ast->root, "vector", target)};
        if (!result) {
            return false;
        }
        target = *result;
        return true;
    }};
    EXPECT_FALSE(assign("'(4 5)"));
    EXPECT_FALSE(assign("'(4 5 1e100)"));
    EXPECT_EQ(target, (Vector3f{1.f, 2.f, 3.f}));
    ASSERT_TRUE(assign("'(4 5 6)"));
    EXPECT_EQ(target, (Vector3f{4.f, 5.f, 6.f}));
}

TEST(NativeSettings, DerivesStarfieldBindingsAndPreservesSparseLayers) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    std::ofstream{path} << R"((settings-layers
      (settings :starfield (starfield
        :generation (generation :star-count 42 :random-seed -7)
        :star (star :scale 2 :size-multiplier 3 :global-brightness 4
                    :colour-variation-strength 5 :parallax-strength 6)
        :galactic-band (galactic-band :strength 7 :width-degrees 8)
        :stellar-cluster (stellar-cluster :strength 9 :width-degrees 10)
        :dust-lane (dust-lane :strength 11 :width-degrees 12 :irregularity 13)
        :bright-star (bright-star :fraction 14 :size-multiplier 15
                                 :brightness-multiplier 16 :shape-strength 17)
        :twinkle (twinkle :strength 18 :speed 19)
        :galactic-haze (galactic-haze :strength 20 :width-degrees 21)
        :galactic-core (galactic-core :strength 22 :width-degrees 23)
        :nebular-knot (nebular-knot :strength 24 :size-degrees 25)))
      (settings :starfield (starfield :star (star :scale 1)))))";
    auto const result{read_starfield_settings_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->generation.star_count, 42);
    EXPECT_EQ(result->generation.random_seed, -7);
    EXPECT_EQ(result->star.scale, starfield::Settings{}.star.scale);
    EXPECT_EQ(result->star.size_multiplier, 3);
    EXPECT_EQ(result->star.global_brightness, 4);
    EXPECT_EQ(result->star.colour_variation_strength, 5);
    EXPECT_EQ(result->star.parallax_strength, 6);
    EXPECT_EQ(result->galactic_band.strength, 7);
    EXPECT_EQ(result->galactic_band.width_degrees, 8);
    EXPECT_EQ(result->stellar_cluster.strength, 9);
    EXPECT_EQ(result->stellar_cluster.width_degrees, 10);
    EXPECT_EQ(result->dust_lane.strength, 11);
    EXPECT_EQ(result->dust_lane.width_degrees, 12);
    EXPECT_EQ(result->dust_lane.irregularity, 13);
    EXPECT_EQ(result->bright_star.fraction, 14);
    EXPECT_EQ(result->bright_star.size_multiplier, 15);
    EXPECT_EQ(result->bright_star.brightness_multiplier, 16);
    EXPECT_EQ(result->bright_star.shape_strength, 17);
    EXPECT_EQ(result->twinkle.strength, 18);
    EXPECT_EQ(result->twinkle.speed, 19);
    EXPECT_EQ(result->galactic_haze.strength, 20);
    EXPECT_EQ(result->galactic_haze.width_degrees, 21);
    EXPECT_EQ(result->galactic_core.strength, 22);
    EXPECT_EQ(result->galactic_core.width_degrees, 23);
    EXPECT_EQ(result->nebular_knot.strength, 24);
    EXPECT_EQ(result->nebular_knot.size_degrees, 25);
}

TEST(NativeSettings, DecodesPresentationLayersIntoExistingNativeValues) {
    settings::Settings initial;
    initial.presentation.entity_overlay.maximum_range = 4000;
    auto const source{DefinitionReader{}.read_settings_source(R"(
      (settings-layers
        (settings :presentation (presentation
          :player-ship (player-ship :boost-effect-colour-intensity 80)
          :entity-overlay (settings-section 'entity-overlay :enabled #f :bar-size-pixels '(32 6)
                           :soft-target (settings-section 'soft-target :opacity 0.5))
          :capital-ships (capital-ships :n-small-explosions 3)
          :radar (settings-section 'radar :combat-range 150000)))
        (settings :presentation (presentation
          :player-ship (player-ship :boost-effect-colour-intensity 75)
          :entity-overlay (settings-section 'entity-overlay :soft-target (settings-section 'soft-target :pulse-duration 0.3))))))",
                                                              initial)};
    ASSERT_TRUE(source) << format_diagnostics(source.error());
    EXPECT_EQ(source->fighters.speed, initial.fighters.speed);
    auto const& target{source->presentation};
    EXPECT_EQ(target.laser_projectiles.max_cull_distance,
              initial.presentation.laser_projectiles.max_cull_distance);
    EXPECT_EQ(target.player_ship.boost_effect_colour_intensity, 75);
    EXPECT_FALSE(target.entity_overlay.enabled);
    EXPECT_EQ(target.entity_overlay.maximum_range, 4000);
    EXPECT_EQ(target.entity_overlay.bar_size_pixels, (Vector<double, 2>{32, 6}));
    EXPECT_EQ(target.entity_overlay.soft_target.opacity, 0.5f);
    EXPECT_EQ(target.entity_overlay.soft_target.pulse_duration, 0.3f);
    EXPECT_EQ(target.capital_ships.n_small_explosions, 3);
    EXPECT_EQ(target.radar.combat_range, 150000);
}

TEST(NativeSettings, RecordTagsRegisterConstructorsWithoutHandwrittenBindings) {
    auto const settings{DefinitionReader{}.read_settings_source(R"(
      (settings :presentation (presentation
        :radar (radar :combat-range 150000)
        :entity-overlay (entity-overlay :enabled #f
          :soft-target (soft-target :opacity 0.5)))))")};
    ASSERT_TRUE(settings) << format_diagnostics(settings.error());
    EXPECT_EQ(settings->presentation.radar.combat_range, 150000);
    EXPECT_FALSE(settings->presentation.entity_overlay.enabled);
    EXPECT_EQ(settings->presentation.entity_overlay.soft_target.opacity, 0.5f);
}

TEST(NativeSettings, RejectsMalformedPresentationAtTheTypedDecodingBoundary) {
    for (auto const value : {"3", "#f", "'(1 2)", "'( :radar )"}) {
        EXPECT_FALSE(DefinitionReader{}.read_settings_source(
            std::format("(settings :presentation {})", value)))
            << value;
    }

    struct Case {
        std::string_view fields;
        DiagnosticCode code;
        std::string_view path;
    };
    for (auto const& test :
         {Case{":radar (settings-section 'radar :unknown 3)",
               DiagnosticCode::UnknownProperty,
               "presentation.radar.unknown"},
          Case{":radar (settings-section 'radar :enabled 1)",
               DiagnosticCode::InvalidType,
               "presentation.radar.enabled"},
          Case{":radar (settings-section 'radar :enabled #t :enabled #f)",
               DiagnosticCode::DuplicateProperty,
               "presentation.radar[3]"},
          Case{":capital-ships (capital-ships :n-small-explosions 1.5)",
               DiagnosticCode::InvalidNumber,
               "presentation.capital-ships.n-small-explosions"},
          Case{":capital-ships (capital-ships :min-small-explosion-range '(1 2 1e100))",
               DiagnosticCode::InvalidNumber,
               "presentation.capital-ships.min-small-explosion-range[2]"},
          Case{":entity-overlay (settings-section 'entity-overlay :bar-size-pixels '(1 +inf.0))",
               DiagnosticCode::InvalidNumber,
               "presentation.entity-overlay.bar-size-pixels"}}) {
        SCOPED_TRACE(test.fields);
        auto const result{DefinitionReader{}.read_settings_source(
            std::format("(settings :presentation (presentation {}))", test.fields))};
        ASSERT_FALSE(result);
        ASSERT_FALSE(result.error().empty());
        EXPECT_EQ(result.error().front().code, test.code);
        EXPECT_EQ(result.error().front().node_path, test.path);
    }
}

TEST(NativeSettings, RetainsStarfieldConversionAndDomainDiagnostics) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    struct Case {
        std::string_view section;
        DiagnosticCode code;
        std::string_view field;
    };
    for (auto const& test :
         {Case{"(star :missing 1)", DiagnosticCode::UnknownProperty, "star.missing"},
          Case{"(star :scale 1e100)", DiagnosticCode::InvalidNumber, "star.scale"},
          Case{"(star :scale 0)", DiagnosticCode::InvalidNumber, "star.scale"}}) {
        SCOPED_TRACE(test.section);
        std::ofstream{path} << std::format("(settings :starfield (starfield :star {}))",
                                           test.section);
        auto const result{read_starfield_settings_file(path)};
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().size(), 1u);
        EXPECT_EQ(result.error().front().code, test.code);
        EXPECT_EQ(result.error().front().node_path, std::format("starfield.{}", test.field));
        EXPECT_EQ(result.error().front().source_path, path);
    }
}

TEST(NativeSettings, ReadsShippedStarfieldSettingsAndValidatesEachNestedLayer) {
    auto shipped{read_starfield_settings_file(std::filesystem::path{IOJ_LEVEL_SCRIPTS} /
                                              "Settings" / "base.scm")};
    ASSERT_TRUE(shipped) << format_diagnostics(shipped.error());
    EXPECT_EQ(shipped->generation.star_count, 10000);

    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    std::ofstream{path} << R"((settings :starfield (starfield
      :star (settings-layers
        (star :scale 2 :size-multiplier 3)
        (star :scale 1)))))";
    auto result{read_starfield_settings_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->star.scale, 1.f);
    EXPECT_EQ(result->star.size_multiplier, 3.f);

    std::ofstream{path} << R"((settings :starfield (starfield
      :star (settings-layers (star :scale 0) (star :scale 1)))))";
    result = read_starfield_settings_file(path);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().front().node_path, "starfield.star.scale");
}

TEST(NativeSettings, RejectsMalformedStarfieldRecordsAndInvalidEffectiveValues) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    for (auto source :
         {"(settings :starfield (starfield :star (star :scale 1 :scale 2)))",
          "(settings :starfield (starfield :star (star :scale)))",
          "(settings :starfield (starfield :star (twinkle :strength 1)))",
          "(settings :starfield (starfield :star (star :global-brightness +inf.0)))",
          "(settings :starfield (starfield :generation (generation :star-count 0)))",
          "(settings :starfield (starfield :generation (generation :star-count 1000001)))",
          "(settings :starfield (starfield :bright-star (bright-star :size-multiplier -1)))",
          "(settings :starfield (starfield :galactic-band (galactic-band :width-degrees 91)))",
          "(settings :starfield (starfield :nebular-knot (nebular-knot :size-degrees 0)))"}) {
        SCOPED_TRACE(source);
        std::ofstream{path} << source;
        auto result{read_starfield_settings_file(path)};
        ASSERT_FALSE(result);
        EXPECT_FALSE(result.error().empty());
        EXPECT_EQ(result.error().front().source_path, path);
    }
}

TEST(NativeLevelDefinition, VerifiesComponentsBeforeConstructionAndOwnsThem) {
    auto components{make_level("owned")};
    auto level{LevelDefinition::make(components)};
    ASSERT_TRUE(level);

    components.metadata.title.clear();
    components.entities.clear();
    auto invalid{LevelDefinition::make(components)};
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(has_code(invalid.error(), DiagnosticCode::MissingTitle));
    EXPECT_TRUE(has_code(invalid.error(), DiagnosticCode::PlayerEntityNotFound));
    EXPECT_EQ(level->metadata().title, "Test Level");
    EXPECT_EQ(level->entities().size(), 1u);
}

TEST(NativeLevelAuthoringReader, IntegerFieldsRequireExactInt32Values) {
    auto const read{[](std::string_view const value) {
        return DefinitionReader{}.read_level_source(std::format(
            R"((level :id 'integer :title "Integer" :player 'player
              :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                :position '(0 0 0) :health {}))))",
            value));
    }};

    for (auto const value : {"2147483647", "2147483647.0", "4294967294/2"}) {
        SCOPED_TRACE(value);
        auto const result{read(value)};
        ASSERT_TRUE(result);
        EXPECT_EQ(result->entities()[0].overrides.health, std::numeric_limits<std::int32_t>::max());
    }
    for (auto const value : {"2147483648",
                             "2147483648.0",
                             "-2147483649",
                             "3/2",
                             "1.5",
                             "+nan.0",
                             "+inf.0",
                             "\"123\""}) {
        SCOPED_TRACE(value);
        auto const result{read(value)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(has_code(result.error(), DiagnosticCode::InvalidNumber));
    }
}

TEST(NativeLevelAuthoringReader, GeneratedRecordsCoexistWithMissionsAndSparseSettings) {
    auto const level{DefinitionReader{}.read_level_source(R"(
      (let ((distance (* 2 500)))
        (level :id 'generated-records :title "Generated records"
          :camera (camera :look-at '(capital) :distance distance :offset-direction '(-1 0 1))
          :collision-grid (collision-grid :cell-size '(5000 5000 5000))
          :entities (list (entity :id 'capital :archetype 'capital-ship :team 'blue :position '(0 0 0)))
          :settings (settings-layers
            (settings :fighters (fighters :speed 9000))
            (settings :fighters (fighters :speed 2000)))
          :mission (mission :objectives (list
            (objective :id 'wait :team 'blue :pass (time-elapsed 1)))))))")};
    ASSERT_TRUE(level) << format_diagnostics(level.error());
    ASSERT_TRUE(level->camera());
    EXPECT_EQ(level->camera()->target_entity_ids, (std::vector{EntityId{"capital"}}));
    EXPECT_EQ(level->camera()->distance, 1000);
    ASSERT_TRUE(level->collision_grid());
    EXPECT_FALSE(level->collision_grid()->level_size);
    ASSERT_TRUE(level->collision_grid()->cell_size);
    EXPECT_EQ(level->collision_grid()->cell_size->x(), 5000);
    ASSERT_TRUE(level->mission());
    ASSERT_EQ(level->mission()->objectives.size(), 1u);
    EXPECT_EQ(level->mission()->objectives.front().id, ObjectiveName{"wait"});
    ASSERT_TRUE(level->settings());
    auto const& settings{*level->settings()};
    EXPECT_EQ(settings.fighters.speed, 2000);
    EXPECT_EQ(settings.fighters.health, settings::Settings{}.fighters.health);
}

TEST(NativeLevelAuthoringReader, DecodesTypedInputsBeforeResolvingReferencesOrGeneratingIds) {
    s7::Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(R"('(:level :id typed :title "Typed"
      :entities ((:entity :archetype capital-ship :team blue :position (1 2 3)))
      :mission (:mission :objectives ((:objective :id wait :team blue
        :pass (:expr-ref later)
        :nodes ((:expr later (:time-elapsed 1))))))))")};
    ASSERT_TRUE(ast);
    auto const input{s7::read<LevelInput>(s7::Reader{*ast}, ast->root, "level")};
    ASSERT_TRUE(input);
    ASSERT_EQ(input->entities.size(), 1u);
    EXPECT_TRUE(input->entities[0].id.empty());
    EXPECT_EQ(input->entities[0].position.x(), 1);
    auto const& objective{input->mission->objectives[0]};
    EXPECT_EQ(objective.pass->reference, ObjectiveName{"later"});
    ASSERT_EQ(objective.nodes.size(), 1u);
    EXPECT_EQ(std::get<TimeElapsedExpr>(objective.nodes[0].value.expression.payload).seconds.get(),
              1);

    // Decoding does not require a viewpoint or resolve even an unknown expression name.
    auto unresolved{interpreter.evaluate_ast(R"('(:level :id unknown :title "Unknown"
      :mission (:mission :objectives ((:objective :id wait :team blue
        :pass (:expr-ref missing))))))")};
    ASSERT_TRUE(unresolved);
    EXPECT_TRUE(s7::read<LevelInput>(s7::Reader{*unresolved}, unresolved->root, "level"));
    auto const processed{parse_level(*unresolved, unresolved->root)};
    ASSERT_FALSE(processed);
    EXPECT_TRUE(has_code(processed.error(), DiagnosticCode::UnknownSymbol));
}

TEST(NativeLevelAuthoringReader, RejectsMalformedInputBeforeProcessingSymbolicReferences) {
    auto const result{DefinitionReader{}.read_level_source(R"(
      (level :id 'invalid :title "Invalid"
        :mission (mission :objectives (list
          (objective :id 'wait :team 'unknown :pass (expr-ref 'missing))))))")};
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().size(), 1u);
    EXPECT_EQ(result.error()[0].code, DiagnosticCode::UnsupportedTeam);
    EXPECT_EQ(result.error()[0].node_path, "level.mission.objectives[0].team");
}

TEST(NativeLevelAuthoringReader, FloatFieldsRejectNonfiniteAndUnrepresentableValues) {
    for (auto const value : {"0.5", "1e100", "+inf.0", "+nan.0"}) {
        SCOPED_TRACE(value);
        auto const result{DefinitionReader{}.read_level_source(std::format(R"(
          (level :id 'floats :title "Floats" :player 'player
            :entities (list
              (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
              (entity :id 'capital :archetype 'capital-ship :team 'blue :position '(100 0 0)
                      :initial-spawn-delay {}))))",
                                                                           value))};
        if (std::string_view{value} == "0.5") {
            ASSERT_TRUE(result) << format_diagnostics(result.error());
            EXPECT_EQ(result->entities()[1].overrides.initial_spawn_delay, 0.5f);
        } else {
            ASSERT_FALSE(result);
            EXPECT_TRUE(has_code(result.error(), DiagnosticCode::InvalidNumber));
        }
    }
}

TEST(NativeSourceSession, PreservesTextAndSeparatesSavedFromAppliedState) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "source.scm"};
    SourceSession session;
    ASSERT_TRUE(session.attach(path));
    EXPECT_FALSE(session.state().disk_conflict);
    auto const source{minimal_source() + "\r\n;; exact \xcf\x80 text\n"};
    session.set_buffer(source);
    auto const revision{session.state().revision};
    session.set_buffer(source);
    EXPECT_EQ(session.state().revision, revision);

    ASSERT_TRUE(session.save());
    EXPECT_EQ(read_file(path).value(), source);
    EXPECT_FALSE(session.state().dirty());
    EXPECT_TRUE(session.state().unapplied("original"));
    EXPECT_FALSE(session.save_replacement("canonical", "original"));
    EXPECT_EQ(read_file(path).value(), source);

    session.state().preview = PreviewState::Valid;
    ASSERT_TRUE(session.save_replacement("canonical", session.state().buffer_text));
    EXPECT_EQ(session.buffer(), "canonical");
    EXPECT_EQ(session.state().preview, PreviewState::Stale);
    EXPECT_GT(session.state().revision, revision);

    std::ofstream{path} << "external";
    ASSERT_TRUE(session.refresh_external_conflict());
    EXPECT_TRUE(session.state().disk_conflict);
    EXPECT_FALSE(session.save());
    ASSERT_TRUE(session.reload());
    EXPECT_EQ(session.buffer(), "external");
    EXPECT_FALSE(session.state().dirty());
    EXPECT_FALSE(session.state().disk_conflict);
}

TEST(NativeSourceSession, ProtectsDiskChangesAndRecoversDetachedBuffers) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "source.scm"};
    auto const recovery{directory.path() / "recovery.scm"};
    auto const next{directory.path() / "next.scm"};
    for (auto const change : {"edit", "delete", "create"}) {
        SCOPED_TRACE(change);
        std::filesystem::remove(path);
        if (std::string_view{change} != "create") {
            std::ofstream{path} << "original";
        }
        SourceSession session;
        ASSERT_TRUE(session.attach(path));
        session.set_buffer("unsaved");
        session.state().preview = PreviewState::Valid;
        if (std::string_view{change} == "delete") {
            std::filesystem::remove(path);
        } else {
            std::ofstream{path} << "external";
        }
        ASSERT_TRUE(session.refresh_external_conflict());
        EXPECT_TRUE(session.state().disk_conflict);
        EXPECT_EQ(session.state().preview, PreviewState::Valid);
        EXPECT_FALSE(session.save());

        session.state().scene_dirty = true;
        session.detach();
        EXPECT_FALSE(session.attach(next));
        EXPECT_EQ(session.buffer(), "unsaved");
        EXPECT_FALSE(session.state().scene_dirty);
        EXPECT_EQ(session.state().preview, PreviewState::None);
        std::ofstream{recovery} << "unrelated";
        EXPECT_FALSE(session.save_as(recovery, SourceOverwritePolicy::RefuseExisting));
        EXPECT_EQ(read_file(recovery).value(), "unrelated");

        auto const revision{session.state().revision};
        ASSERT_TRUE(session.save_as(recovery, SourceOverwritePolicy::ReplaceExisting));
        EXPECT_GT(session.state().revision, revision);
        EXPECT_EQ(read_file(recovery).value(), "unsaved");
        EXPECT_FALSE(session.is_attached());
        EXPECT_FALSE(session.attach(next));
        ASSERT_TRUE(session.discard_and_attach(next));
        EXPECT_TRUE(session.is_attached());
        EXPECT_FALSE(session.state().recovery_required);
    }
}

TEST(NativeSourceSession, ReadsUtf8BomText) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "encoding.scm"};
    std::string const bytes{"\xef\xbb\xbf"
                            "A\xcf\x80"};
    std::ofstream{path, std::ios::binary}.write(bytes.data(),
                                                static_cast<std::streamsize>(bytes.size()));
    SourceSession session;
    ASSERT_TRUE(session.attach(path));
    EXPECT_EQ(session.buffer(), "A\xcf\x80");
    ASSERT_TRUE(session.save());
    EXPECT_EQ(read_file(path).value(), session.buffer());
}

TEST(NativeLevelAuthoringReader, ReadsFileWithRootRelativeLibrary) {
    TemporaryLevelDirectory directory;
    std::ofstream{directory.path() / "Libraries" / "metadata.scm"}
        << "(define benchmark-title \"Loaded From Library\")";
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "benchmark-title");
    auto const path{directory.path() / "level.scm"};
    std::ofstream{path} << "(load-script \"Libraries/metadata.scm\")\n" << source;
    auto const result{DefinitionReader{}.read_level_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->metadata().title, "Loaded From Library");
}
TEST(NativeLevelAuthoringReader, ReportsMissingFile) {
    auto const result{DefinitionReader{}.read_level_file("missing-level.scm")};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().front().code, DiagnosticCode::FileOpenFailed);
    EXPECT_EQ(result.error().front().source_path, "missing-level.scm");
}
TEST(NativeLevelAuthoringReader, RootControlsImportsAndPreservesFailuresAndSourcePaths) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "shared-title");
    std::ofstream{root / "first.scm"} << "(define shared-title \"Shared\")\n" << source;
    std::ofstream{root / "second.scm"} << source;
    std::ofstream{root / "broken.scm"} << "(undefined-function)";
    std::ofstream{root / "ignored.scm"} << "(error 'ignored \"Must not be loaded\")";
    std::ofstream{root / "campaign.scm"}
        << "(campaign :id 'shared :title shared-title :levels '(test))";
    std::ofstream{root / "catalog.scm"} << R"(
        (catalog
          :levels (list "first.scm"
                        "broken.scm"
                        "second.scm")
          :campaigns (list "campaign.scm")))";
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), 3u);
    ASSERT_EQ(catalog->campaigns.size(), 1u);
    ASSERT_TRUE(catalog->levels[0].definition);
    EXPECT_EQ(catalog->levels[0].definition->metadata().title, "Shared");
    ASSERT_FALSE(catalog->levels[1].definition);
    EXPECT_EQ(catalog->levels[1].definition.error().front().source_path, root / "broken.scm");
    ASSERT_TRUE(catalog->levels[2].definition);
    EXPECT_EQ(catalog->levels[2].definition->metadata().title, "Shared");
    ASSERT_TRUE(catalog->campaigns[0].definition);
    EXPECT_EQ(catalog->campaigns[0].definition->title, "Shared");

    std::ofstream{root / "variation.scm"} << "(catalog :levels (list \"second.scm\"))";
    auto const variation{DefinitionReader{}.read_root_file(root / "variation.scm")};
    ASSERT_TRUE(variation);
    ASSERT_FALSE(variation->levels[0].definition);
    EXPECT_EQ(variation->levels[0].definition.error().front().code,
              DiagnosticCode::ScriptEvaluationFailed);
    EXPECT_EQ(catalog->levels[0].definition->metadata().title, "Shared");
}
TEST(NativeLevelAuthoringReader, RootLoadsSharedLibrariesOnce) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    std::ofstream{root / "Libraries" / "count.scm"}
        << "(set! library-load-count (+ library-load-count 1))";
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "(number->string library-load-count)");
    std::ofstream{root / "level.scm"} << "(load-script \"Libraries/count.scm\")\n" << source;
    std::ofstream{root / "catalog.scm"} << R"(
        (define library-load-count 0)
        (load-script "Libraries/count.scm")
        (catalog :levels (list "level.scm"
                               "level.scm")))";
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), 2u);
    for (auto const& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        EXPECT_EQ(entry.definition->metadata().title, "1");
    }
}
TEST(NativeLevelAuthoringReader, RootReportsEvaluationAndGrammarErrors) {
    TemporaryLevelDirectory directory;
    auto const root{directory.path() / "catalog.scm"};
    std::ofstream{root} << "(undefined-root-function)";
    auto const evaluation{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(evaluation);
    EXPECT_EQ(evaluation.error().front().code, DiagnosticCode::ScriptEvaluationFailed);
    EXPECT_EQ(evaluation.error().front().source_path, root);
    std::ofstream{root} << "(catalog :levels 42 :unexpected #t)";
    auto const grammar{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(grammar);
    EXPECT_GE(grammar.error().size(), 2u);
}
TEST(NativeLevelAuthoringFiles, ReadsEmptyBinaryAndLargeFilesWithExplicitErrors) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "binary.dat"};
    {
        std::ofstream empty_file{path, std::ios::binary};
        ASSERT_TRUE(empty_file);
    }
    EXPECT_EQ(read_file(path), "");
    std::string bytes(20000, 'x');
    bytes[100] = '\0';
    bytes[8191] = '\r';
    bytes[8192] = '\n';
    {
        std::ofstream output{path, std::ios::binary};
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    EXPECT_EQ(read_file(path), bytes);
    auto const missing{read_file(directory.path() / "missing.dat")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, FileReadErrorCode::OpenFailed);
    EXPECT_TRUE(missing.error().system_error);
}
TEST(NativeLevelAuthoringCampaignReader, DecodesAndValidatesCampaignData) {
    DefinitionReader reader;
    auto const result{reader.read_campaign_source(
        R"((campaign :id 'first-campaign :title "First Campaign" :levels '(alpha beta)))")};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->id, CampaignId{"first-campaign"});
    EXPECT_EQ(result->level_ids, (std::vector{LevelId{"alpha"}, LevelId{"beta"}}));
    auto const duplicate{reader.read_campaign_source(
        R"((campaign :id 'campaign :title "Duplicate" :levels '(alpha alpha)))")};
    ASSERT_FALSE(duplicate);
    EXPECT_TRUE(has_code(duplicate.error(), DiagnosticCode::DuplicateCampaignLevel));
}
TEST(NativeLevelAuthoringReader, ParsesClosedVocabulariesWithoutTeamDeclarations) {
    auto source{minimal_source()};
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(participating_teams(*result), (std::vector{Team::Blue}));
    ASSERT_EQ(result->entities().size(), 1u);
    EXPECT_EQ(result->entities().front().team, Team::Blue);
    EXPECT_EQ(result->entities().front().archetype, EntityArchetype::PlayerFighter);
}
TEST(NativeLevelAuthoringReader, GeneratesLevelLocalIdsAndDefaultsRotations) {
    auto const source{R"((level :id 'defaults :title "Defaults" :player 'player
      :entities (list
        (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
        (entity :archetype 'capital-ship :team 'red :position '(1 2 3))
        (entity :id 'red-capital-ship-0 :archetype 'capital-ship :team 'red :position '(4 5 6))
        (entity :archetype 'static-turret :team 'red :position '(7 8 9))
        (entity :archetype 'capital-ship :team 'red :position '(10 11 12) :spawn-at 3)
        (entity :archetype 'static-turret :team 'blue :position '(13 14 15)))))"};
    std::vector<EntityId> const expected{EntityId{"player"},
                                         EntityId{"red-capital-ship-1"},
                                         EntityId{"red-capital-ship-0"},
                                         EntityId{"red-static-turret-0"},
                                         EntityId{"red-capital-ship-2"},
                                         EntityId{"blue-static-turret-0"}};
    for (int iteration{}; iteration < 2; ++iteration) {
        auto const result{DefinitionReader{}.read_level_source(source)};
        ASSERT_TRUE(result) << format_diagnostics(result.error());
        ASSERT_EQ(result->entities().size(), expected.size());
        auto const count{expected.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& entity{result->entities()[index]};
            EXPECT_EQ(entity.id, expected[index]);
            EXPECT_DOUBLE_EQ(entity.rotation.pitch, 0.0);
            EXPECT_DOUBLE_EQ(entity.rotation.yaw, 0.0);
            EXPECT_DOUBLE_EQ(entity.rotation.roll, 0.0);
        }

        auto const emitted{emit_source(*result)};
        ASSERT_TRUE(emitted) << format_diagnostics(emitted.error());
        EXPECT_FALSE(emitted->contains(":rotation"));
        EXPECT_TRUE(emitted->contains(":id 'red-capital-ship-1"));
        auto const round_trip{DefinitionReader{}.read_level_source(*emitted)};
        ASSERT_TRUE(round_trip) << format_diagnostics(round_trip.error());
        for (auto const& original : result->entities()) {
            auto const found{
                std::ranges::find(round_trip->entities(), original.id, &EntitySpawnDefinition::id)};
            ASSERT_NE(found, round_trip->entities().end());
            EXPECT_DOUBLE_EQ(found->position.x(), original.position.x());
            EXPECT_DOUBLE_EQ(found->spawn_time_seconds, original.spawn_time_seconds);
        }
    }
}
TEST(NativeLevelAuthoringReader, OptionalPropertiesStillRejectMalformedValuesAndDuplicateIds) {
    for (auto const properties : {":id #f",
                                  ":id 17",
                                  ":id (string->symbol \"\")",
                                  ":id 'player",
                                  ":rotation #f",
                                  ":rotation '(0 0)",
                                  ":rotation '(0 bad 0)",
                                  ":rotation '(0 +inf.0 0)"}) {
        auto const source{std::format(R"((level :id 'invalid :title "Invalid" :player 'player
          :entities (list
            (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
            (entity :archetype 'static-turret :team 'red :position '(1 2 3) {}))))",
                                      properties)};
        EXPECT_FALSE(DefinitionReader{}.read_level_source(source)) << properties;
    }
}
TEST(NativeLevelAuthoringReader, RestartsGeneratedIdsForEachDefinitionInOneRoot) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    for (auto const name : {"first", "second"}) {
        std::ofstream{root / std::format("{}.scm", name)} << std::format(R"(
          (level :id '{} :title "Generated" :player 'player
            :entities (list
              (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
              (entity :archetype 'static-turret :team 'red :position '(1 2 3)))))",
                                                                         name);
    }
    std::ofstream{root / "catalog.scm"} << R"((catalog :levels '("first.scm" "second.scm")))";
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), 2u);
    for (auto const& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        ASSERT_EQ(entry.definition->entities().size(), 2u);
        EXPECT_EQ(entry.definition->entities().back().id, EntityId{"red-static-turret-0"});
    }
}
TEST(NativeLevelAuthoringReader, MapsPositionsAndConstructsExplicitIdsInScheme) {
    auto const result{
        DefinitionReader{}.read_level_source(R"((level :id 'formation :title "Formation"
      :camera (camera :look-at '(flagship-0 flagship-1) :distance 1000 :offset-direction '(-1 0 0))
      :entities (append
        (map (lambda (index)
               (entity :id (string->symbol (format #f "flagship-~A" index))
                 :archetype 'capital-ship :team 'blue :position (list (* index 100) 0 0)))
             '(0 1))
        (map (lambda (position)
               (entity :archetype 'static-turret :team 'red :position position))
             '((200 0 0) (300 10 20))))))")};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    ASSERT_EQ(result->entities().size(), 4u);
    EXPECT_EQ(result->entities()[0].id, EntityId{"flagship-0"});
    EXPECT_EQ(result->entities()[1].id, EntityId{"flagship-1"});
    EXPECT_DOUBLE_EQ(result->entities()[1].position.x(), 100.0);
    EXPECT_EQ(result->entities()[2].id, EntityId{"red-static-turret-0"});
    EXPECT_EQ(result->entities()[3].id, EntityId{"red-static-turret-1"});
    EXPECT_DOUBLE_EQ(result->entities()[3].position.y(), 10.0);
    EXPECT_DOUBLE_EQ(result->entities()[3].position.z(), 20.0);
}
TEST(NativeLevelAuthoringReader, DerivesDistinctTeamsIncludingScheduledSpawns) {
    auto definition{make_level("teams")};
    definition.entities.emplace_back(
        EntitySpawnDefinition{.id = EntityId{"reinforcement"},
                              .archetype = EntityArchetype::CapitalShip,
                              .team = Team::Red,
                              .spawn_time_seconds = 10.0});
    definition.entities.emplace_back(EntitySpawnDefinition{
        .id = EntityId{"ally"}, .archetype = EntityArchetype::CapitalShip, .team = Team::Blue});
    EXPECT_TRUE(LevelDefinition::make(definition));
    EXPECT_EQ(participating_teams(LevelDefinition::make(definition).value()),
              (std::vector{Team::Red, Team::Blue}));
    auto const source{emit_source(definition)};
    ASSERT_TRUE(source) << format_diagnostics(source.error());
    EXPECT_FALSE(source->contains(":teams"));
    auto const parsed{DefinitionReader{}.read_level_source(*source)};
    ASSERT_TRUE(parsed);
    EXPECT_EQ(participating_teams(*parsed),
              participating_teams(LevelDefinition::make(definition).value()));
    definition.entities.back().team = static_cast<Team>(255);
    auto const invalid{LevelDefinition::make(definition)};
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(has_code(invalid.error(), DiagnosticCode::UnsupportedTeam));
}
TEST(NativeLevelAuthoringReader, RejectsRemovedTeamDeclarations) {
    auto source{minimal_source()};
    source.insert(source.size() - 1, " :teams '(blue)");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_FALSE(result);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnknownProperty));
}
TEST(NativeLevelAuthoringReader, RejectsUnknownTeamAndArchetypeIndependently) {
    auto source{minimal_source()};
    source.replace(source.find("'player-fighter"), 15, "'unknown-ship");
    source.replace(source.find(":team 'blue"), 11, ":team 'unknown-team");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_FALSE(result);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedArchetype));
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedTeam));
}
TEST(NativeLevelAuthoringReader, ReportsIndependentMalformedProperties) {
    auto const result{DefinitionReader{}.read_level_source(R"((level
      :id 17 :title 'not-text :title "duplicate" :unknown 4
      :entities (list (entity :id 4 :team 6 :archetype 7
        :position '(a b c) :rotation '(x y z) :spawn-at 'bad))))")};
    ASSERT_FALSE(result);
    EXPECT_GE(result.error().size(), 12u);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::DuplicateProperty));
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnknownProperty));
    EXPECT_TRUE(std::ranges::any_of(result.error(), [](Diagnostic const& error) {
        return error.node_path == "level.entities[0].spawn-at";
    }));
}
TEST(NativeLevelAuthoringReader, RejectsMissingPropertiesOddListsAndWrongTags) {
    for (auto const source : {"(level)",
                              "(level :id)",
                              "'(level :id x :title \"X\")",
                              "(level 'id 'x :title \"X\")",
                              "(campaign :id 'x :title \"X\")"}) {
        EXPECT_FALSE(DefinitionReader{}.read_level_source(source)) << source;
    }
    auto const missing{DefinitionReader{}.read_campaign_source("(campaign :id 'x)")};
    ASSERT_FALSE(missing);
    EXPECT_TRUE(has_code(missing.error(), DiagnosticCode::MissingProperty));
}
TEST(NativeLevelAuthoringReader, DistinguishesEvaluationConversionAndParsingFailures) {
    auto const evaluation{DefinitionReader{}.read_level_source("(missing-function)")};
    auto const conversion{DefinitionReader{}.read_level_source("(lambda () 1)")};
    auto const parsing{DefinitionReader{}.read_level_source("'(1 2)")};
    ASSERT_FALSE(evaluation);
    ASSERT_FALSE(conversion);
    ASSERT_FALSE(parsing);
    EXPECT_TRUE(has_code(evaluation.error(), DiagnosticCode::ScriptEvaluationFailed));
    EXPECT_TRUE(has_code(conversion.error(), DiagnosticCode::UnsupportedValue));
    EXPECT_TRUE(has_code(parsing.error(), DiagnosticCode::ExpectedRecord));
}
TEST(NativeLevelAuthoringWriter, EmitsDeterministicReadableSource) {
    auto first{make_level("writer-example")};
    first.metadata.title = "Writer \"Example\"";
    first.metadata.description = "Line one\nLine two.";
    first.entities.push_back({.id = EntityId{"red-capital"},
                              .archetype = EntityArchetype::CapitalShip,
                              .team = Team::Red,
                              .position = {1000.0, 200.12349, -300.5},
                              .rotation = {0.0, 90.0, 0.0},
                              .spawn_time_seconds = 2.5});
    auto second{first};
    std::ranges::reverse(second.entities);
    auto const a{emit_source(first)};
    auto const b{emit_source(second)};
    ASSERT_TRUE(a) << format_diagnostics(a.error());
    ASSERT_TRUE(b);
    EXPECT_EQ(*a, *b);
    EXPECT_TRUE(a->contains(":position '(1000 200.123 -300.5)"));
    EXPECT_TRUE(a->contains(":spawn-at 2.5"));
    auto const loaded{DefinitionReader{}.read_level_source(*a)};
    ASSERT_TRUE(loaded) << format_diagnostics(loaded.error());
    EXPECT_EQ(loaded->metadata().title, first.metadata.title);
    EXPECT_EQ(loaded->metadata().description, first.metadata.description);
    EXPECT_EQ(loaded->entities().size(), 2u);
}
TEST(NativeLevelAuthoringWriter, HandlesEmptyCollectionsEscapesAndNumericBoundaries) {
    auto definition{make_level("numbers")};
    definition.metadata.description = std::string{"a\0b\t\r\\\"", 7};
    definition.entities[0].position = {-0.0001, 0.0005, 1.0e100};
    auto source{emit_source(definition)};
    ASSERT_TRUE(source) << format_diagnostics(source.error());
    EXPECT_TRUE(source->contains(":position '(0 0.001 "));
    auto parsed{DefinitionReader{}.read_level_source(*source)};
    ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
    EXPECT_EQ(parsed->metadata().description, definition.metadata.description);
    EXPECT_DOUBLE_EQ(parsed->entities()[0].position.x(), 0.0);
}
TEST(NativeLevelAuthoringCollisionGrid, PreservesIndependentOverridesAcrossSourceRoundTrip) {
    for (int mask{}; mask < 4; ++mask) {
        auto definition{make_level("grid")};
        if (mask != 0) {
            definition.collision_grid.emplace();
        }
        if ((mask & 1) != 0) {
            definition.collision_grid->level_size = Vector3d{20000, 20000, 20000};
        }
        if ((mask & 2) != 0) {
            definition.collision_grid->cell_size = Vector3d{5000, 5000, 5000};
        }
        auto const source{emit_source(definition)};
        ASSERT_TRUE(source) << format_diagnostics(source.error());
        auto const parsed{DefinitionReader{}.read_level_source(*source)};
        ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
        ASSERT_EQ(parsed->collision_grid().has_value(), mask != 0);
        if (mask != 0) {
            EXPECT_EQ(parsed->collision_grid()->level_size.has_value(), (mask & 1) != 0);
            EXPECT_EQ(parsed->collision_grid()->cell_size.has_value(), (mask & 2) != 0);
        }
    }
}
TEST(NativeLevelAuthoringCollisionGrid, RejectsMalformedAndInvalidExplicitDimensions) {
    for (auto const grid :
         {"(collision-grid)",
          "(collision-grid :cell-size '(0 1 1))",
          "(collision-grid :level-size '(+nan.0 1 1))",
          "(collision-grid :level-size '(2000 2000 2000) :cell-size '(0.00001 0.00001 0.00001))",
          "(collision-grid :cell-size '(1 1 1) :cell-size '(2 2 2))",
          "(collision-grid :unknown '(1 1 1))"}) {
        auto source{minimal_source()};
        source.insert(source.size() - 1, std::format(" :collision-grid {}", grid));
        EXPECT_FALSE(DefinitionReader{}.read_level_source(source)) << grid;
    }
}
TEST(NativeLevelAuthoringWriter, RejectsObjectiveEditingWithoutDiscardingMission) {
    auto definition{make_level("mission")};
    definition.mission = timer_mission();
    auto const source{emit_source(definition)};
    ASSERT_FALSE(source);
    EXPECT_EQ(source.error()[0].code, DiagnosticCode::UnsupportedEditorFeature);
    EXPECT_EQ(definition.mission->objectives[0].id, ObjectiveName{"primary"});
}
TEST(NativeLevelAuthoringWriter, ReportsUnsupportedEditorFeatures) {
    auto definition{make_level("unsupported")};
    definition.metadata.par_time_seconds = 10;
    definition.unlock_level_ids.emplace_back("other");
    definition.mission = timer_mission();
    auto const result{emit_source(std::move(definition))};
    ASSERT_FALSE(result);
    EXPECT_EQ(std::ranges::count(
                  result.error(), DiagnosticCode::UnsupportedEditorFeature, &Diagnostic::code),
              3);
}
TEST(NativeLevelAuthoringCatalog, RejectsDuplicatesCyclesAndUnavailableDependencies) {
    std::vector<LevelCatalogEntry> duplicates{{7, "a.scm", checked_level("same")},
                                              {31, "b.scm", checked_level("same")}};
    auto const duplicate_issues{validate_level_catalog(duplicates)};
    ASSERT_EQ(duplicate_issues.size(), 2u);
    EXPECT_EQ(duplicate_issues[0].entry_index, 7u);
    EXPECT_EQ(duplicate_issues[1].entry_index, 31u);
    std::vector<LevelCatalogEntry> cycles{{2, "a.scm", checked_level("a", {LevelId{"b"}})},
                                          {4, "b.scm", checked_level("b", {LevelId{"a"}})},
                                          {9, "c.scm", checked_level("c", {LevelId{"a"}})}};
    auto const cycle_issues{validate_level_catalog(cycles)};
    EXPECT_EQ(cycle_issues.size(), 3u);
    std::vector<LevelCatalogEntry> missing{{2, "a.scm", checked_level("a", {LevelId{"missing"}})},
                                           {8, "b.scm", checked_level("b", {LevelId{"a"}})}};
    EXPECT_EQ(validate_level_catalog(missing).size(), 2u);
}
TEST(NativeLevelAuthoringCatalog, ValidatesCampaignIdsAndLevelReferences) {
    std::vector<LevelCatalogEntry> levels{{3, "a.scm", checked_level("a")}};
    std::vector<CampaignCatalogEntry> campaigns{
        {5, "first.scm", {CampaignId{"same"}, "First", {LevelId{"a"}}}},
        {8, "second.scm", {CampaignId{"same"}, "Second", {LevelId{"missing"}}}}};
    auto const issues{validate_campaign_catalog(campaigns, levels)};
    EXPECT_EQ(issues.size(), 3u);
    campaigns.resize(1);
    campaigns[0].definition.level_ids = {LevelId{"missing"}};
    auto const missing{validate_campaign_catalog(campaigns, levels)};
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0].entry_index, 5u);
    EXPECT_EQ(missing[0].diagnostic.code, DiagnosticCode::UnavailableLevel);
}
TEST(NativeLevelAuthoringFixtures, ReadsEveryCheckedInLevelAndCampaign) {
    std::filesystem::path const root{IOJ_LEVEL_SCRIPTS};
    std::vector<LevelCatalogEntry> levels;
    std::vector<CampaignCatalogEntry> campaigns;
    auto catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    for (auto& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        levels.emplace_back(levels.size(), entry.source_path, std::move(*entry.definition));
    }
    for (auto& entry : catalog->campaigns) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        campaigns.emplace_back(campaigns.size(), entry.source_path, std::move(*entry.definition));
    }
    for (auto const& entry : std::filesystem::directory_iterator(root / "Benchmarks")) {
        if (entry.path().extension() == ".scm") {
            auto const definition{DefinitionReader{root}.read_level_file(entry.path())};
            ASSERT_TRUE(definition) << format_diagnostics(definition.error());
        }
    }
    EXPECT_GT(levels.size(), 30u);
    EXPECT_GT(campaigns.size(), 5u);
    EXPECT_TRUE(validate_level_catalog(levels).empty());
    EXPECT_TRUE(validate_campaign_catalog(campaigns, levels).empty());
}
}
