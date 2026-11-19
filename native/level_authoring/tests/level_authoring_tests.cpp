#include "module_fixture.h"
#include "settings_diff.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_catalog_category.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/s7/interpreter.h>
#include <ioj/s7/source_format.h>
#include <ioj/settings/settings_values_writer.h>
#include <ioj/testing/files.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace ioj::levels::authoring::tests {
using ioj::testing::structurally_equal;
using ioj::testing::write_files;
namespace {
auto module_source(std::string_view name, std::string_view value) -> std::string {
    return std::format("(export (rename result {}))\n(define result {})\n", name, value);
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

class OverrideReader {
  public:
    template <typename T>
    auto assign(std::string_view const source, T& target, std::string_view const path = "value")
        -> bool {
        auto result{interpreter_.evaluate_ast(source).and_then([&](s7::Ast const& ast) {
            return s7::read_override(s7::Reader{ast}, ast.root, path, target);
        })};
        if (!result) {
            return false;
        }
        target = *result;
        return true;
    }
  private:
    s7::Interpreter interpreter_;
};

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
    OverrideReader reader;
    auto const maximum{std::numeric_limits<std::int64_t>::max()};
    std::int64_t integer{};
    ASSERT_TRUE(reader.assign(std::to_string(maximum), integer));
    EXPECT_EQ(integer, maximum);
    ASSERT_TRUE(reader.assign("9007199254740993", integer));
    EXPECT_EQ(integer, 9007199254740993);

    constexpr std::string_view invalid_sources[]{"9223372036854775808.0",
                                                 "-9223372036854777856.0",
                                                 "1.5",
                                                 s7::literals::positive_infinity,
                                                 s7::literals::nan};
    for (auto const invalid : invalid_sources) {
        SCOPED_TRACE(invalid);
        EXPECT_FALSE(reader.assign(invalid, integer));
        EXPECT_EQ(integer, 9007199254740993);
    }
    ASSERT_TRUE(reader.assign("-9223372036854775808", integer));
    EXPECT_EQ(integer, std::numeric_limits<std::int64_t>::min());

    std::uint64_t unsigned_integer{7};
    EXPECT_FALSE(reader.assign("18446744073709551616.0", unsigned_integer));
    EXPECT_FALSE(reader.assign("-1", unsigned_integer));
    EXPECT_EQ(unsigned_integer, 7u);
    PositiveFinite<float> real{3.f};
    EXPECT_FALSE(reader.assign("1e100", real));
    EXPECT_FALSE(reader.assign(s7::literals::positive_infinity, real));
    EXPECT_EQ(real.get(), 3.f);
}

TEST(NativeSettings, AssignsWholeVectorsOnlyAfterEveryComponentConverts) {
    Vector2d size{1., 2.};
    OverrideReader reader;
    ASSERT_TRUE(reader.assign("'(3 4)", size));
    EXPECT_EQ(size, (Vector2d{3., 4.}));
    EXPECT_FALSE(reader.assign("'(5 6 7)", size));
    EXPECT_EQ(size, (Vector2d{3., 4.}));

    Vector3f target{1.f, 2.f, 3.f};
    EXPECT_FALSE(reader.assign("'(4 5)", target, "vector"));
    EXPECT_FALSE(reader.assign("'(4 5 1e100)", target, "vector"));
    EXPECT_EQ(target, (Vector3f{1.f, 2.f, 3.f}));
    ASSERT_TRUE(reader.assign("'(4 5 6)", target, "vector"));
    EXPECT_EQ(target, (Vector3f{4.f, 5.f, 6.f}));
}

TEST(NativeSettings, DerivesStarfieldBindingsAndPreservesSparseLayers) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    ASSERT_TRUE(write_files({{path, module_source("game-settings", R"((settings-layers
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
      (settings :starfield (starfield :star (star :scale 1)))))")}}));
    auto const result{DefinitionReader{}.read_settings_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    starfield::Settings const expected{.generation = {42, -7},
                                       .star = {starfield::Settings{}.star.scale, 3, 4, 5, 6},
                                       .galactic_band = {7, 8},
                                       .stellar_cluster = {9, 10},
                                       .dust_lane = {11, 12, 13},
                                       .bright_star = {14, 15, 16, 17},
                                       .twinkle = {18, 19},
                                       .galactic_haze = {20, 21},
                                       .galactic_core = {22, 23},
                                       .nebular_knot = {24, 25}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, result->starfield);
}

TEST(NativeSettings, DecodesPresentationLayersIntoExistingNativeValues) {
    settings::Settings initial;
    initial.presentation.entity_overlay.maximum_range = 4000;
    auto const source{read_test_settings(DefinitionReader{},
                                         R"(
      (settings-layers
        (settings :presentation (presentation
          :player-ship (player-ship-presentation :boost-effect-colour-intensity 80)
          :entity-overlay (entity-overlay :enabled #f :bar-size-pixels '(32 6)
                           :soft-target (soft-target :opacity 0.5))
          :capital-ships (capital-ships-presentation :n-small-explosions 3)
          :radar (radar :combat-range 150000)))
        (settings :presentation (presentation
          :player-ship (player-ship-presentation :boost-effect-colour-intensity 75)
          :entity-overlay (entity-overlay :soft-target (soft-target :pulse-duration 0.3))))))",
                                         initial)};
    ASSERT_TRUE(source) << format_diagnostics(source.error());
    auto expected{initial};
    auto& presentation{expected.presentation};
    presentation.player_ship.boost_effect_colour_intensity = 75;
    presentation.entity_overlay.enabled = false;
    presentation.entity_overlay.bar_size_pixels = {32, 6};
    presentation.entity_overlay.soft_target.opacity = 0.5f;
    presentation.entity_overlay.soft_target.pulse_duration = ioj::NonNegativeFinite<float>{0.3f};
    presentation.capital_ships.n_small_explosions = 3;
    presentation.radar.combat_range = 150000;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *source);
}

TEST(NativeSettings, RecordTagsRegisterConstructorsWithoutHandwrittenBindings) {
    auto const source{R"(
      (settings :presentation (presentation
        :radar (radar :enabled #t :combat-range 150000)
        :entity-overlay (entity-overlay :enabled #f
          :soft-target (soft-target :opacity 0.5)))))"};
    DefinitionReader const reader;
    auto const settings{read_test_settings(reader, source)};
    ASSERT_TRUE(settings) << format_diagnostics(settings.error());
    settings::Settings const expected{
        .presentation = {.entity_overlay = {.enabled = false, .soft_target = {.opacity = 0.5f}},
                         .radar = {.combat_range = 150000}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *settings);

    auto const ast{read_test_value(reader, source)};
    ASSERT_TRUE(ast) << format_diagnostics(ast.error());
    auto const frozen{read_test_settings(reader, s7::serialize(*settings))};
    ASSERT_TRUE(frozen) << format_diagnostics(frozen.error());
    EXPECT_PRED_FORMAT2(structurally_equal, *settings, *frozen);
}

TEST(NativeSettings, AccumulatesStructuralAndNestedErrorsWithoutChangingTheBase) {
    settings::Settings base;
    base.fighters.speed = 7000.f;
    auto const original{base};
    auto const result{read_test_settings(DefinitionReader{},
                                         R"(
      (settings :unknown 1
        :fighters (fighters :speed 9000 :health #t :speed 1)
        :presentation (presentation :radar (radar :enabled 1))))",
                                         base)};
    ASSERT_FALSE(result);
    EXPECT_PRED_FORMAT2(structurally_equal, original, base);
    auto const& errors{result.error()};
    EXPECT_EQ(errors.size(), 4u) << format_diagnostics(errors);
    EXPECT_TRUE(has_code(errors, DiagnosticCode::UnknownProperty));
    EXPECT_TRUE(has_code(errors, DiagnosticCode::DuplicateProperty));
    EXPECT_TRUE(std::ranges::any_of(
        errors, [](auto const& error) { return error.node_path == "fighters.health"; }));
    EXPECT_TRUE(std::ranges::any_of(
        errors, [](auto const& error) { return error.node_path == "presentation.radar.enabled"; }));
}

TEST(NativeSettings, RejectsMalformedPresentationAtTheTypedDecodingBoundary) {
    for (auto const value : {"3", "#f", "'(1 2)", "'( :radar )"}) {
        EXPECT_FALSE(read_test_settings(DefinitionReader{},
                                        std::format("(settings :presentation {})", value)))
            << value;
    }

    struct Case {
        std::string_view fields;
        DiagnosticCode code;
        std::string_view path;
    };
    for (auto const& test :
         {Case{":radar (radar :unknown 3)",
               DiagnosticCode::UnknownProperty,
               "presentation.radar.unknown"},
          Case{":radar (radar :enabled 1)",
               DiagnosticCode::InvalidType,
               "presentation.radar.enabled"},
          Case{":radar (radar :enabled #t :enabled #f)",
               DiagnosticCode::DuplicateProperty,
               "presentation.radar[3]"},
          Case{":capital-ships (capital-ships-presentation :n-small-explosions 1.5)",
               DiagnosticCode::InvalidNumber,
               "presentation.capital-ships.n-small-explosions"},
          Case{
              ":capital-ships (capital-ships-presentation :min-small-explosion-range '(1 2 1e100))",
              DiagnosticCode::InvalidNumber,
              "presentation.capital-ships.min-small-explosion-range[2]"},
          Case{":entity-overlay (entity-overlay :bar-size-pixels '(1 +inf.0))",
               DiagnosticCode::InvalidNumber,
               "presentation.entity-overlay.bar-size-pixels"}}) {
        SCOPED_TRACE(test.fields);
        auto const result{read_test_settings(
            DefinitionReader{},
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
        ASSERT_TRUE(
            write_files({{path,
                          module_source("game-settings",
                                        std::format("(settings :starfield (starfield :star {}))",
                                                    test.section))}}));
        auto const result{DefinitionReader{}.read_settings_file(path)};
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().size(), 1u);
        EXPECT_EQ(result.error().front().code, test.code);
        EXPECT_EQ(result.error().front().node_path, std::format("starfield.{}", test.field));
        EXPECT_EQ(result.error().front().source_path, path);
    }
}

TEST(NativeSettings, ReadsShippedStarfieldSettingsAndValidatesEachNestedLayer) {
    auto shipped{DefinitionReader{}.read_settings_file(std::filesystem::path{IOJ_LEVEL_SCRIPTS} /
                                                       "Settings" / "base.scm")};
    ASSERT_TRUE(shipped) << format_diagnostics(shipped.error());
    EXPECT_EQ(shipped->starfield.generation.star_count, 10000);

    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "settings.scm"};
    ASSERT_TRUE(
        write_files({{path, module_source("game-settings", R"((settings :starfield (starfield
      :star (settings-layers
        (star :scale 2 :size-multiplier 3)
        (star :scale 1)))))")}}));
    auto result{DefinitionReader{}.read_settings_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    starfield::Settings expected;
    expected.star.size_multiplier = 3.f;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, result->starfield);

    ASSERT_TRUE(
        write_files({{path, module_source("game-settings", R"((settings :starfield (starfield
      :star (settings-layers (star :scale 0) (star :scale 1)))))")}}));
    result = DefinitionReader{}.read_settings_file(path);
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
        ASSERT_TRUE(write_files({{path, module_source("game-settings", source)}}));
        auto result{DefinitionReader{}.read_settings_file(path)};
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
        return read_test_level(DefinitionReader{},
                               std::format(
                                   R"((level :id 'integer :title "Integer" :player 'player
              :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                :position '(0 0 0) :health {}))))",
                                   value));
    }};

    for (auto const value : {"2147483647", "2147483647.0", "4294967294/2"}) {
        SCOPED_TRACE(value);
        auto const result{read(value)};
        ASSERT_TRUE(result);
        EXPECT_EQ(result->entities()[0].overrides.health->get().value(),
                  std::numeric_limits<std::int32_t>::max());
    }
    constexpr std::string_view invalid_sources[]{"2147483648",
                                                 "2147483648.0",
                                                 "-2147483649",
                                                 "3/2",
                                                 "1.5",
                                                 s7::literals::nan,
                                                 s7::literals::positive_infinity,
                                                 "\"123\""};
    for (auto const value : invalid_sources) {
        SCOPED_TRACE(value);
        auto const result{read(value)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(has_code(result.error(), DiagnosticCode::InvalidNumber));
    }
}

TEST(NativeLevelAuthoringReader, GeneratedRecordsCoexistWithLogicAndSparseSettings) {
    auto const level{read_test_level(DefinitionReader{}, R"(
      (let ((distance (* 2 500)))
        (level :id 'generated-records :title "Generated records"
          :camera (camera :look-at '(capital) :distance distance :offset-direction '(-1 0 1))
          :entities (list (entity :id 'capital :archetype 'capital-ship :team 'blue :position '(0 0 0)))
          :settings (settings-layers
            (settings :fighters (fighters :speed 9000)
                      :collision-grid (collision-grid :cell-size '(5000 5000 5000)))
            (settings :fighters (fighters :speed 2000)))
          :logic (logic
            (define delay (timer))
            (initially (start-timer delay 1))
            (on :when (timer-elapsed delay) :do (win 'blue))))))")};
    ASSERT_TRUE(level) << format_diagnostics(level.error());
    std::optional<LevelCameraDefinition> const camera{{{EntityId{"capital"}}, {-1, 0, 1}, 1000}};
    settings::Settings const expected{.collision_grid = {.cell_size = {5000, 5000, 5000}}};
    EXPECT_PRED_FORMAT2(structurally_equal, camera, level->camera());
    EXPECT_PRED_FORMAT2(structurally_equal, expected, level->settings());
    ASSERT_EQ(level->logic().rules.size(), 1u);
    EXPECT_EQ(std::get<WinEffect>(level->logic().rules[0].effects[0]).team, Team::Blue);
}

TEST(NativeLevelAuthoringReader, DecodesGraphInputsBeforeResolvingReferences) {
    s7::Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(R"('(:level :id typed :title "Typed"
      :entities ((:entity :archetype capital-ship :team blue :position (1 2 3)))
      :logic (:logic ((:entity-destroyed missing)) () 0 () () ((0 ((:win blue)))))))")};
    ASSERT_TRUE(ast);
    auto const input{s7::read<LevelInput>(s7::Reader{*ast}, ast->root, "level")};
    ASSERT_TRUE(input);
    EXPECT_EQ(std::get<EntityDestroyedExpr>(input->logic.expressions[0].payload).entity,
              EntityId{"missing"});
    auto const processed{parse_level(*ast, ast->root)};
    EXPECT_FALSE(processed);
}

TEST(NativeLevelAuthoringReader, RejectsForeignHandlesAndRestoresLogicScope) {
    DefinitionReader reader;
    for (auto const body :
         {"(define saved #f) (logic (set! saved (all-of))) (logic (on :when saved :do (stop)))",
          "(logic (on :when 0 :do (stop)))",
          "(logic (on :when (timer) :do (stop)))",
          "(entity-destroyed 'player)"}) {
        EXPECT_FALSE(read_test_level(reader, body));
    }
    EXPECT_TRUE(read_test_level(reader, minimal_source()));
}

TEST(NativeLevelAuthoringReader, FloatFieldsRejectNonfiniteAndUnrepresentableValues) {
    constexpr std::string_view sources[]{
        "0.5", "1e100", s7::literals::positive_infinity, s7::literals::nan};
    for (auto const value : sources) {
        SCOPED_TRACE(value);
        auto const result{read_test_level(DefinitionReader{},
                                          std::format(R"(
          (level :id 'floats :title "Floats" :player 'player
            :entities (list
              (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
              (entity :id 'capital :archetype 'capital-ship :team 'blue :position '(100 0 0)
                      :initial-spawn-delay {}))))",
                                                      value))};
        if (value == "0.5") {
            ASSERT_TRUE(result) << format_diagnostics(result.error());
            EXPECT_EQ(result->entities()[1].overrides.initial_spawn_delay, 0.5f);
        } else {
            ASSERT_FALSE(result);
            EXPECT_TRUE(has_code(result.error(), DiagnosticCode::InvalidNumber));
        }
    }
}

TEST(NativeLevelAuthoringReader, RejectsInvalidLogicNumbersAndReferences) {
    struct Case {
        std::string_view logic;
        std::string_view path;
    };
    Case const cases[]{
        {"(logic (define delay (timer)) (initially (start-timer delay -1)))",
         "level.logic.initial-effects[0].seconds"},
        {"(logic (entity-reached-position 'player '(0 +inf.0 0) 1))",
         "level.logic.expressions[0].position"},
        {"(logic (entity-reached-position 'player '(0 0 0) -1))",
         "level.logic.expressions[0].radius"},
        {"(logic (all-of) (entity-destroyed 'missing))", "level.logic.expressions[1].entity"},
        {"(logic (define t (timer)) (on :when (all-of) :do (stop)) "
         "(on :when (all-of) :do (list (win 'blue) (start-timer t -1))))",
         "level.logic.rules[1].effects[1].seconds"},
        {"(logic (spawn-group 'reserves '(player missing)))",
         "level.logic.spawn-groups[0].entities[1]"},
        {"'(:logic ((:all-of) (:any-of 0 bad)) () 0 () () ())",
         "level.logic.expressions[1].children[1]"},
        {"'(:logic ((:all-of) (:any-of 0 99)) () 0 () () ())",
         "level.logic.expressions[1].children[1]"},
        {"'(:logic ((:all-of)) ((0 bad)) 0 () () ())", "level.logic.latches[0].value"},
        {"'(:logic ((:all-of)) ((0 99)) 0 () () ())", "level.logic.latches[0].value"},
        {"'(:logic ((:all-of)) () 0 () () ((0 ()) (bad ())))", "level.logic.rules[1].when"},
        {"'(:logic ((:all-of)) () 0 () () ((0 ()) (99 ())))", "level.logic.rules[1].when"},
        {"'(:logic ((:all-of)) () 0 () () ((0 ((:stop) (:start-timer 99 1)))))",
         "level.logic.rules[0].effects[1].timer"},
    };
    for (auto const& test : cases) {
        SCOPED_TRACE(test.logic);
        auto source{minimal_source()};
        source.insert(source.size() - 1, std::format(" :logic {}", test.logic));
        auto const result{read_test_level(DefinitionReader{}, source)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(std::ranges::any_of(result.error(), [&](auto const& error) {
            return error.node_path.starts_with(test.path);
        })) << format_diagnostics(result.error());
    }
}

TEST(NativeLevelAuthoringReader, RejectsCombinationalCyclesAndAuthoredLatchFeedback) {
    struct Case {
        std::string_view graph;
        std::string_view path;
    };
    Case const cases[]{
        {"(:logic ((:all-of 1) (:not-of 0)) () 0 () () ())", "level.logic.expressions[1]"},
        {"(:logic ((:latched-true 0)) ((0 0)) 0 () () ())", "level.logic.expressions[0]"},
        {"(:logic ((:not-of)) () 0 () () ())", "level.logic.expressions[0].children"},
    };
    for (auto const& test : cases) {
        auto source{minimal_source()};
        source.insert(source.size() - 1, std::format(" :logic '{}", test.graph));
        auto const result{read_test_level(DefinitionReader{}, source)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(has_code(result.error(), DiagnosticCode::InvalidLogicStructure));
        EXPECT_EQ(result.error().front().node_path, test.path);
    }
}

TEST(NativeLevelAuthoringReader, ReadsFileWithRootRelativeLibrary) {
    TemporaryLevelDirectory directory;
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "benchmark-title");
    auto const path{directory.path() / "level.scm"};
    ASSERT_TRUE(write_files({
        {directory.path() / "Libraries" / "metadata.scm",
         module_source("benchmark-title", "\"Loaded From Library\"")},
        {path,
         "(define benchmark-title (module-ref (import-module \"Libraries/metadata.scm\") "
         "'benchmark-title))\n" +
             module_source("mission", source)},
    }));
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
    settings::Settings base;
    base.fighters.speed = 7300.f;
    base.collision_grid.cell_size = {1e8f, 1e8f, 1e8f};
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "shared-title");
    source.insert(source.size() - 1, R"(
      :settings (settings :fighters (fighters :speed 7300) :collision-grid
        (collision-grid :cell-size '(100000000 100000000 100000000) :level-size '(2000000000 2000000000 2000000000))))");
    auto const title_import{
        R"((define shared-title (module-ref (import-module "metadata.scm") 'title)))"};
    auto const mission{title_import + module_source("mission", source)};
    ASSERT_TRUE(write_files({
        {root / "metadata.scm", module_source("title", "\"Shared\"")},
        {root / "first.scm", mission},
        {root / "second.scm",
         title_import + module_source("mission", "(copy-with " + source + " :id 'second)")},
        {root / "broken.scm", "(undefined-function)"},
        {root / "ignored.scm", "(error 'ignored \"Must not be loaded\")"},
        {root / "campaign.scm",
         title_import +
             module_source("campaign",
                           "(campaign :id 'shared :title shared-title :levels '(test))")},
        {root / "catalog.scm",
         module_source(
             "catalog",
             R"((catalog :levels '("first.scm" "broken.scm" "second.scm" "../outside.scm" 42)
          :campaigns '("campaign.scm")))")},
    }));
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    auto const& levels{catalog->levels};
    ASSERT_EQ(levels.size(), 5u);
    ASSERT_EQ(catalog->campaigns.size(), 1u);
    ASSERT_TRUE(levels[0].definition);
    EXPECT_EQ(levels[0].source_text, mission);
    EXPECT_EQ(levels[0].definition->metadata().title, "Shared");
    ASSERT_FALSE(levels[1].definition);
    EXPECT_EQ(levels[1].source_text, "(undefined-function)");
    ASSERT_FALSE(levels[3].definition);
    EXPECT_TRUE(levels[3].source_text.empty());
    EXPECT_EQ(levels[3].definition.error().front().code, DiagnosticCode::InvalidType);
    ASSERT_FALSE(levels[4].definition);
    EXPECT_EQ(levels[4].definition.error().front().node_path, "catalog.levels[4]");
    EXPECT_EQ(levels[1].definition.error().front().source_path, root / "broken.scm");
    ASSERT_TRUE(levels[2].definition);
    EXPECT_EQ(levels[2].definition->metadata().title, "Shared");
    auto expected_settings{base};
    expected_settings.collision_grid.level_size = {2e9f, 2e9f, 2e9f};
    EXPECT_PRED_FORMAT2(structurally_equal, expected_settings, levels[0].definition->settings());
    EXPECT_PRED_FORMAT2(structurally_equal, expected_settings, levels[2].definition->settings());

    auto const& campaign{catalog->campaigns[0].definition};
    ASSERT_TRUE(campaign);
    CampaignDefinition const expected{CampaignId{"shared"}, "Shared", {LevelId{"test"}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *campaign);

    ASSERT_TRUE(
        write_files({{root / "variation.scm",
                      module_source("catalog", "(catalog :levels (list \"second.scm\"))")}}));
    auto const variation{DefinitionReader{}.read_root_file(root / "variation.scm")};
    ASSERT_TRUE(variation);
    ASSERT_EQ(variation->levels.size(), 1u);
    ASSERT_TRUE(variation->levels[0].definition);
    EXPECT_EQ(variation->levels[0].definition->metadata().title, "Shared");
}
TEST(NativeLevelAuthoringReader, CatalogAndNestedImportsShareOneFileBudget) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    auto const limit{s7::InterpreterOptions{}.max_loaded_files};
    auto const paths{std::format("(make-list {} \"level.scm\")", limit)};
    ASSERT_TRUE(write_files({
        {root / "catalog.scm", module_source("catalog", "(catalog :levels " + paths + ")")},
        {root / "level.scm",
         "(import-module \"helper.scm\")\n" + module_source("mission", minimal_source())},
        {root / "helper.scm", ""},
    }));
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), limit);
    auto const admitted{(limit - 1) / 2};
    ASSERT_FALSE(catalog->levels[admitted - 1].definition);
    EXPECT_TRUE(has_code(catalog->levels[admitted - 1].definition.error(),
                         DiagnosticCode::DuplicateLevelId));
    ASSERT_FALSE(catalog->levels[admitted].definition);
    EXPECT_TRUE(
        catalog->levels[admitted].definition.error().front().message.contains("file count limit"));
}

TEST(NativeLevelAuthoringReader, RootReportsEvaluationAndGrammarErrors) {
    TemporaryLevelDirectory directory;
    auto const root{directory.path() / "catalog.scm"};
    ASSERT_TRUE(write_files({{root, "(undefined-root-function)"}}));
    auto const evaluation{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(evaluation);
    EXPECT_EQ(evaluation.error().front().code, DiagnosticCode::ScriptEvaluationFailed);
    EXPECT_EQ(evaluation.error().front().source_path, root);
    ASSERT_TRUE(
        write_files({{root, module_source("catalog", "(catalog :levels 42 :unexpected #t)")}}));
    auto const grammar{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(grammar);
    EXPECT_GE(grammar.error().size(), 2u);
}
TEST(NativeLevelAuthoringFiles, ReadsEmptyBinaryAndLargeFilesWithExplicitErrors) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "binary.dat"};
    ASSERT_TRUE(write_files({{path, ""}}));
    EXPECT_EQ(read_file(path), "");
    std::string bytes(20000, 'x');
    bytes[100] = '\0';
    bytes[8191] = '\r';
    bytes[8192] = '\n';
    ASSERT_TRUE(write_files({{path, bytes}}));
    EXPECT_EQ(read_file(path), bytes);
    auto const missing{read_file(directory.path() / "missing.dat")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, FileReadErrorCode::OpenFailed);
    EXPECT_TRUE(missing.error().system_error);
}
TEST(NativeLevelAuthoringCampaignReader, DecodesAndValidatesCampaignData) {
    DefinitionReader reader;
    auto const result{read_test_campaign(
        reader, R"((campaign :id 'first-campaign :title "First Campaign" :levels '(alpha beta)))")};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    CampaignDefinition const expected{
        CampaignId{"first-campaign"}, "First Campaign", {LevelId{"alpha"}, LevelId{"beta"}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
    auto const duplicate{read_test_campaign(
        reader, R"((campaign :id 'campaign :title "Duplicate" :levels '(alpha alpha)))")};
    ASSERT_FALSE(duplicate);
    EXPECT_TRUE(has_code(duplicate.error(), DiagnosticCode::DuplicateCampaignLevel));
}
TEST(NativeLevelAuthoringReader, ParsesClosedVocabulariesWithoutTeamDeclarations) {
    auto source{minimal_source()};
    auto const result{read_test_level(DefinitionReader{}, source)};
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
        auto const result{read_test_level(DefinitionReader{}, source)};
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
        EXPECT_FALSE(read_test_level(DefinitionReader{}, source)) << properties;
    }
}
TEST(NativeLevelAuthoringReader, RestartsGeneratedIdsForEachDefinitionInOneRoot) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    for (auto const name : {"first", "second"}) {
        ASSERT_TRUE(write_files({{root / std::format("{}.scm", name),
                                  module_source("mission",
                                                std::format(R"(
          (level :id '{} :title "Generated" :player 'player
            :entities (list
              (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0))
              (entity :archetype 'static-turret :team 'red :position '(1 2 3)))))",
                                                            name))}}));
    }
    ASSERT_TRUE(write_files(
        {{root / "catalog.scm",
          module_source("catalog", R"((catalog :levels '("first.scm" "second.scm")))")}}));
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
        read_test_level(DefinitionReader{}, R"((level :id 'formation :title "Formation"
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
                              .spawn_time_seconds = NonNegativeFinite<double>{10.0}});
    definition.entities.emplace_back(EntitySpawnDefinition{
        .id = EntityId{"ally"}, .archetype = EntityArchetype::CapitalShip, .team = Team::Blue});
    EXPECT_TRUE(LevelDefinition::make(definition));
    EXPECT_EQ(participating_teams(LevelDefinition::make(definition).value()),
              (std::vector{Team::Red, Team::Blue}));
    definition.entities.back().team = static_cast<Team>(255);
    auto const invalid{LevelDefinition::make(definition)};
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(has_code(invalid.error(), DiagnosticCode::UnsupportedTeam));
}
TEST(NativeLevelAuthoringReader, RejectsRemovedTeamDeclarations) {
    auto source{minimal_source()};
    source.insert(source.size() - 1, " :teams '(blue)");
    auto const result{read_test_level(DefinitionReader{}, source)};
    ASSERT_FALSE(result);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnknownProperty));
}
TEST(NativeLevelAuthoringReader, RejectsUnknownTeamAndArchetypeIndependently) {
    auto source{minimal_source()};
    source.replace(source.find("'player-fighter"), 15, "'unknown-ship");
    source.replace(source.find(":team 'blue"), 11, ":team 'unknown-team");
    auto const result{read_test_level(DefinitionReader{}, source)};
    ASSERT_FALSE(result);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedArchetype));
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedTeam));
}
TEST(NativeLevelAuthoringReader, ReportsIndependentMalformedProperties) {
    auto const result{read_test_level(DefinitionReader{}, R"((level
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
        EXPECT_FALSE(read_test_level(DefinitionReader{}, source)) << source;
    }
    auto const missing{read_test_campaign(DefinitionReader{}, "(campaign :id 'x)")};
    ASSERT_FALSE(missing);
    EXPECT_TRUE(has_code(missing.error(), DiagnosticCode::MissingProperty));
}
TEST(NativeLevelAuthoringReader, DistinguishesEvaluationConversionAndParsingFailures) {
    auto const evaluation{read_test_level(DefinitionReader{}, "(missing-function)")};
    auto const conversion{read_test_level(DefinitionReader{}, "(lambda () 1)")};
    auto const parsing{read_test_level(DefinitionReader{}, "'(1 2)")};
    ASSERT_FALSE(evaluation);
    ASSERT_FALSE(conversion);
    ASSERT_FALSE(parsing);
    EXPECT_TRUE(has_code(evaluation.error(), DiagnosticCode::ScriptEvaluationFailed));
    EXPECT_TRUE(has_code(conversion.error(), DiagnosticCode::UnsupportedValue));
    EXPECT_TRUE(has_code(parsing.error(), DiagnosticCode::ExpectedRecord));
}
TEST(NativeLevelAuthoringCollisionGrid, PreservesIndependentOverrides) {
    settings::Settings base;
    base.fighters.speed = 7000.f;
    base.collision_grid.cell_size = {2500.f, 2500.f, 10000.f};
    for (int mask{}; mask < 4; ++mask) {
        auto expected{base};
        std::string fields;
        if ((mask & 1) != 0) {
            fields += " :level-size '(20000 20000 20000)";
            expected.collision_grid.level_size = {20000.f, 20000.f, 20000.f};
        }
        if ((mask & 2) != 0) {
            fields += " :cell-size '(5000 5000 20000)";
            expected.collision_grid.cell_size = sim::OverlapResponseConfig{}.cell_size;
        }
        auto authored{minimal_source()};
        authored.insert(authored.size() - 1,
                        std::format(R"( :settings (settings-layers
                (settings :fighters (fighters :speed 7000)
                  :collision-grid (collision-grid :cell-size '(2500 2500 10000)))
                (settings :collision-grid (collision-grid{}))))",
                                    fields));
        auto const definition{read_test_level(DefinitionReader{}, authored)};
        ASSERT_TRUE(definition) << format_diagnostics(definition.error());
        EXPECT_PRED_FORMAT2(structurally_equal, expected, definition->settings());
    }
}
TEST(NativeLevelAuthoringCollisionGrid, RejectsMalformedAndInvalidExplicitDimensions) {
    for (auto const grid :
         {"(collision-grid :cell-size '(0 1 1))",
          "(collision-grid :level-size '(+nan.0 1 1))",
          "(collision-grid :level-size '(2000 2000 2000) :cell-size '(0.00001 0.00001 0.00001))",
          "(collision-grid :cell-size '(1 1 1) :cell-size '(2 2 2))",
          "(collision-grid :unknown '(1 1 1))"}) {
        auto source{minimal_source()};
        source.insert(source.size() - 1,
                      std::format(" :settings (settings :collision-grid {})", grid));
        EXPECT_FALSE(read_test_level(DefinitionReader{}, source)) << grid;
    }
}

TEST(NativeLevelAuthoringCatalog, RejectsDuplicatesCyclesAndUnavailableDependencies) {
    DefinitionCatalog catalog{
        {{"a.scm", checked_level("a", {LevelId{"b"}})},
         {"b.scm", checked_level("b", {LevelId{"a"}})},
         {"c.scm", checked_level("c", {LevelId{"a"}})},
         {"d.scm", checked_level("d", {LevelId{"missing"}})}},
        {{"first.scm", CampaignDefinition{CampaignId{"same"}, "First", {LevelId{"a"}}}},
         {"second.scm", CampaignDefinition{CampaignId{"same"}, "Second", {LevelId{"missing"}}}}}};
    for (auto const& entry : catalog.levels) {
        ASSERT_FALSE(entry.definition);
    }
    EXPECT_TRUE(has_code(catalog.levels[0].definition.error(), DiagnosticCode::UnlockCycle));
    EXPECT_TRUE(has_code(catalog.levels[1].definition.error(), DiagnosticCode::UnlockCycle));
    EXPECT_TRUE(has_code(catalog.levels[2].definition.error(), DiagnosticCode::UnavailableLevel));
    EXPECT_TRUE(has_code(catalog.levels[3].definition.error(), DiagnosticCode::UnavailableLevel));
    for (auto const& entry : catalog.campaigns) {
        ASSERT_FALSE(entry.definition);
        EXPECT_TRUE(has_code(entry.definition.error(), DiagnosticCode::DuplicateCampaignId));
        EXPECT_TRUE(has_code(entry.definition.error(), DiagnosticCode::UnavailableLevel));
    }
}

TEST(NativeLevelAuthoringCatalog, ResolvesInvalidEntriesAndOrdersValidCampaigns) {
    DefinitionCatalog catalog{
        {{"first.scm", checked_level("same")},
         {"second.scm", checked_level("same")},
         {"valid.scm", checked_level("valid")}},
        {{"zulu.scm", CampaignDefinition{CampaignId{"zulu"}, "Zulu", {LevelId{"valid"}}}},
         {"invalid.scm", CampaignDefinition{CampaignId{"invalid"}, "Invalid", {LevelId{"same"}}}},
         {"second-alpha.scm", CampaignDefinition{CampaignId{"b"}, "alpha", {LevelId{"valid"}}}},
         {"first-alpha.scm", CampaignDefinition{CampaignId{"a"}, "Alpha", {LevelId{"valid"}}}}},
    };
    EXPECT_FALSE(catalog.levels[0].definition);
    EXPECT_FALSE(catalog.levels[1].definition);
    ASSERT_TRUE(catalog.levels[2].definition);
    EXPECT_EQ(catalog_category(*catalog.levels[2].definition), LevelCatalogCategory::Mission);
    EXPECT_EQ(catalog.campaigns[0].source_path, "first-alpha.scm");
    EXPECT_EQ(catalog.campaigns[1].source_path, "second-alpha.scm");
    EXPECT_EQ(catalog.campaigns[2].source_path, "zulu.scm");
    ASSERT_FALSE(catalog.campaigns[3].definition);
    EXPECT_TRUE(
        has_code(catalog.campaigns[3].definition.error(), DiagnosticCode::UnavailableLevel));

    auto camera{make_level("camera")};
    camera.player_entity_id = {};
    camera.entities[0].archetype = EntityArchetype::CapitalShip;
    camera.camera = LevelCameraDefinition{{EntityId{"player"}}, {-1., 0., 0.}, 1000.};
    auto const definition{LevelDefinition::make(std::move(camera))};
    ASSERT_TRUE(definition);
    EXPECT_EQ(catalog_category(*definition), LevelCatalogCategory::BattleViewer);
}

TEST(NativeLevelAuthoringFixtures, ReadsEveryCheckedInLevelAndCampaign) {
    std::filesystem::path const root{IOJ_LEVEL_SCRIPTS};
    auto catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    for (auto& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
    }
    for (auto& entry : catalog->campaigns) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
    }
    auto const benchmark{DefinitionReader{}.read_level_file(root / "Batch_benchmark.scm")};
    ASSERT_TRUE(benchmark) << format_diagnostics(benchmark.error());
    EXPECT_GT(catalog->levels.size(), 30u);
    EXPECT_GT(catalog->campaigns.size(), 5u);
}
}
