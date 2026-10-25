#include <ioj/s7/interpreter.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>
#include <Mixed_reader.h>
#include <NestedSettings_reader.h>
#include <ReaderDependency_reader.h>
#include <Records_reader.h>

namespace lispb::tests {
using namespace ioj::s7;
using namespace codegen_compile_fixture;
using ioj::testing::structurally_equal;

template <typename T>
auto read_source(std::string_view const source) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read<T>(Reader{*ast}, ast->root, "record");
}

template <typename T>
auto override_source(std::string_view source, T initial = {}) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read_override(Reader{*ast}, ast->root, "record", std::move(initial));
}

TEST(RecordSettings, AppliesPartialLayersAndDistinguishesExplicitEmptyOptional) {
    ActorSettings target;
    auto const apply{[&](std::string_view source) {
        auto result{override_source(source, target)};
        if (result) {
            target = *result;
        }
        return result;
    }};
    ASSERT_TRUE(apply("'(:actor :max-health 200 :weapon (:weapon :damage 20))"));
    EXPECT_PRED_FORMAT2(structurally_equal, (ActorSettings{200, {20}}), target);
    ASSERT_TRUE(apply("'(:actor :max-health 100 :weapon (:weapon :turn-speed 4))"));
    EXPECT_PRED_FORMAT2(structurally_equal, (ActorSettings{100, {20, 4.f}}), target);
    ASSERT_TRUE(apply("'(:actor :optional-limit :none)"));
    ActorSettings const without_limit{100, {20, 4.f}, std::nullopt};
    EXPECT_PRED_FORMAT2(structurally_equal, without_limit, target);
    ASSERT_TRUE(apply("'(:actor)"));
    EXPECT_PRED_FORMAT2(structurally_equal, without_limit, target);
    ASSERT_TRUE(apply("'(:actor :optional-limit 3)"));
    EXPECT_PRED_FORMAT2(structurally_equal, (ActorSettings{100, {20, 4.f}, 3}), target);
}

TEST(RecordSettings, AppliesCrossNamespaceNestedOverridesAndEmptyPatches) {
    auto result{override_source<mixed::CrossSettings>(
        "'(:cross :actor (:actor :weapon (:weapon :damage 41)))")};
    ASSERT_TRUE(result);
    EXPECT_PRED_FORMAT2(structurally_equal, (ActorSettings{100, {41}}), result->actor);
    EXPECT_TRUE(override_source<EmptySettings>("'(:empty-settings)"));
}

TEST(GeneratedS7Reader, ConstrainedRecordsValidateAndOverrideAsWholeValues) {
    RangeSettings target;
    EXPECT_PRED_FORMAT2(structurally_equal, (RangeValue{0, 10}), target.range.get_ref());
    EXPECT_FALSE(Range::try_make({10, 0}));
    static_assert(std::same_as<decltype(target.range.get_ref()), RangeValue const&>);

    auto patch{
        override_source<RangeSettings>("'(:ranges :range (:range :minimum 20 :maximum 30))")};
    ASSERT_TRUE(patch);
    RangeSettings expected{*Range::try_make({20, 30})};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *patch);

    auto omitted{override_source("'(:ranges :enabled #f)", *patch)};
    ASSERT_TRUE(omitted);
    expected.enabled = false;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *omitted);

    auto defaults{
        override_source<RangeSettings>("'(:ranges :range (:range :minimum 0 :maximum 10))")};
    ASSERT_TRUE(defaults);
    EXPECT_PRED_FORMAT2(structurally_equal, RangeSettings{}, *defaults);

    auto missing{override_source<RangeSettings>("'(:ranges :range (:range :minimum 5))")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().front().node_path, "record.range.maximum");
    auto invalid{
        override_source<RangeSettings>("'(:ranges :range (:range :minimum 30 :maximum 20))")};
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().front().node_path, "record.range");
    EXPECT_EQ(invalid.error().front().message, "Expected minimum <= maximum");
}

TEST(GeneratedS7Reader, ReadsSparseTypedOverridesWithNestedLayersAndExplicitEmptyValues) {
    auto missing{read_source<WeaponSettings>("'(:weapon)")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().front().node_path, "record.damage");
    auto complete{read_source<WeaponSettings>("'(:weapon :damage 0)")};
    ASSERT_TRUE(complete);
    EXPECT_PRED_FORMAT2(structurally_equal, WeaponSettings{.damage = 0}, *complete);

    auto partial{override_source(
        "'(:child :count 0)", ReadableChild{.name = *ioj::NonEmptyString::try_make("original")})};
    ASSERT_TRUE(partial);
    EXPECT_PRED_FORMAT2(structurally_equal,
                        (ReadableChild{*ioj::NonEmptyString::try_make("original"), 0}),
                        *partial);

    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(R"('(:settings-layers
      (:actor :max-health 200 :weapon (:weapon :damage 17 :turn-speed 9))
      (:actor :max-health 100 :weapon (:weapon :damage 5) :optional-limit :none)))")};
    ASSERT_TRUE(ast);
    ActorSettings initial;
    initial.weapon.asset_distance = 42;
    auto patch{read_override(Reader{*ast}, ast->root, "actor", initial)};
    ASSERT_TRUE(patch);
    auto expected{initial};
    expected.weapon.turn_rate = 9;
    expected.optional_limit.reset();
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *patch);
    EXPECT_FALSE(override_source<WeaponSettings>("'(:weapon :asset-distance 3)"));
    EXPECT_FALSE(override_source<WeaponSettings>("'(:weapon :damage -1)"));
    EXPECT_FALSE(override_source<WeaponSettings>("'(:weapon :damage 1 :damage 2)"));
    EXPECT_TRUE(override_source<ActorSettings>("'(:actor)"));
}

auto read_record_source(std::string_view const source) -> ReadResult<ReadableRecord> {
    return read_source<ReadableRecord>(source);
}

TEST(GeneratedS7Reader, ReadsEmptyRecursiveAndCrossModuleRecords) {
    EXPECT_EQ(*read_source<ReaderEnum>("'MixedCase"), ReaderEnum::Mixed);
    EXPECT_EQ(*read_source<ReaderEnum>("'mixedcase"), ReaderEnum::Mixed);
    EXPECT_TRUE(read_source<EmptyReadable>("'(:empty)"));
    EXPECT_FALSE(read_source<EmptyReadable>("'(:empty :extra 1)"));
    auto const recursive{read_source<RecursiveReadable>(
        "'(:recursive :children ((:recursive :children ((:recursive)))))")};
    ASSERT_TRUE(recursive);
    RecursiveReadable const expected{{RecursiveReadable{{RecursiveReadable{}}}}};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *recursive);
    auto const dependency{
        read_source<ReaderDependency>(R"('(:dependent :child (:child :name "nested")))")};
    ASSERT_TRUE(dependency);
    EXPECT_PRED_FORMAT2(structurally_equal,
                        (ReadableChild{.name = *ioj::NonEmptyString::try_make("nested")}),
                        dependency->child);
}

TEST(GeneratedS7Reader, PreservesExactIntegerBounds) {
    auto const minimum{read_source<BoundedInteger>("'(:integer :value -9223372036854775808)")};
    ASSERT_TRUE(minimum);
    EXPECT_EQ(minimum->value, std::numeric_limits<std::int64_t>::min());
    auto const maximum{read_source<BoundedInteger>("'(:integer :value 9223372036854775806)")};
    ASSERT_TRUE(maximum);
    EXPECT_EQ(maximum->value, std::numeric_limits<std::int64_t>::max() - 1);
    EXPECT_FALSE(read_source<BoundedInteger>("'(:integer :value 9223372036854775807)"));
}

TEST(GeneratedS7Reader, ComposesRecordsContainersEnumsAndConstrainedValues) {
    auto const result{read_record_source(R"('(:record :child (:child :name "root")
        :values ((:child :name "first" :count 0) (:child :name "second"))
        :samples (1 2 3) :enabled #f :maybe :none :mode readable_name))")};
    ASSERT_TRUE(result);
    ReadableRecord const expected{.child = {.name = *ioj::NonEmptyString::try_make("root")},
                                  .values = {{*ioj::NonEmptyString::try_make("first"), 0},
                                             {*ioj::NonEmptyString::try_make("second")}},
                                  .points = {1.f, 2.f, 3.f},
                                  .enabled = false,
                                  .maybe = std::nullopt,
                                  .mode = EPlainFixture::ReadableName};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
}

TEST(GeneratedS7Reader, PreservesDefaultsAndInclusiveConstraints) {
    for (auto const scale : {0.5f, 5.f}) {
        SCOPED_TRACE(scale);
        auto const result{read_record_source(
            std::format(R"('(:record :child (:child :name "root") :scale {}))", scale))};
        ASSERT_TRUE(result);
        ReadableRecord const expected{.child = {.name = *ioj::NonEmptyString::try_make("root")},
                                      .scale = scale};
        EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
    }
}

TEST(GeneratedS7Reader, ReportsStructuralAndValueErrorsWithFieldPaths) {
    struct Case {
        std::string_view fields;
        AstErrorCode code;
        std::string_view path;
    };
    for (auto const& test :
         {Case{":child (:child)", AstErrorCode::MissingProperty, "record.child.name"},
          Case{":maybe", AstErrorCode::MissingValue, "record[3]"},
          Case{":unknown 1", AstErrorCode::UnknownProperty, "record.unknown"},
          Case{":enabled #t :enabled #f", AstErrorCode::DuplicateProperty, "record[5]"},
          Case{":scale 0", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale +inf.0", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale 6", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":scale 0.25", AstErrorCode::InvalidNumber, "record.scale"},
          Case{":offset 0", AstErrorCode::InvalidNumber, "record.offset"},
          Case{":title \"\"", AstErrorCode::InvalidType, "record.title"},
          Case{":samples (1 2)", AstErrorCode::InvalidType, "record.samples"},
          Case{":values ((:child :name \"\"))", AstErrorCode::InvalidType, "record.values[0].name"},
          Case{":mode unknown", AstErrorCode::UnknownSymbol, "record.mode"}}) {
        SCOPED_TRACE(test.fields);
        auto const source{
            test.fields.starts_with(":child")
                ? std::format("'(:record {})", test.fields)
                : std::format(R"('(:record :child (:child :name "root") {}))", test.fields)};
        auto const result{read_record_source(source)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(std::ranges::any_of(result.error(), [&](AstDiagnostic const& error) {
            return error.code == test.code && error.node_path == test.path &&
                   !error.message.empty();
        }));
    }
    auto const absent{read_record_source("'(:record)")};
    ASSERT_FALSE(absent);
    EXPECT_EQ(absent.error().front().node_path, "record.child");
    EXPECT_FALSE(read_record_source("'(:wrong)"));
}

TEST(GeneratedS7Reader, KeepsValueErrorsWhenTheRecordAlsoHasStructuralErrors) {
    auto const result{read_record_source(
        R"('(:record :child (:child :name 42) :enabled 7 :extra #t :enabled #f :scale 0))")};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().size(), 6u);
    for (auto const path :
         {"record.child.name", "record.enabled", "record.extra", "record[7]", "record.scale"}) {
        EXPECT_TRUE(std::ranges::any_of(result.error(), [&](auto const& error) {
            return error.node_path == path;
        })) << path;
    }
}

TEST(GeneratedS7Reader, DecodesFixedPositionalRecordsAndChecksArityAndConstraints) {
    EXPECT_TRUE(read_source<EmptyPositional>("'(:empty-position)"));
    EXPECT_FALSE(read_source<EmptyPositional>("'(:empty-position 1)"));
    auto result{read_source<PositionalChild>(R"('(:position "name" 3))")};
    ASSERT_TRUE(result);
    EXPECT_PRED_FORMAT2(
        structurally_equal, (PositionalChild{*ioj::NonEmptyString::try_make("name"), 3}), *result);
    for (auto source : {R"('(:position "name"))",
                        R"('(:position "name" 3 4))",
                        R"('(:wrong "name" 3))",
                        R"('(:position "" -1))"}) {
        SCOPED_TRACE(source);
        EXPECT_FALSE(read_source<PositionalChild>(source));
    }
    auto invalid{read_source<PositionalChild>(R"('(:position "name" -1))")};
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().front().node_path, "record.count");
}

TEST(GeneratedS7Reader, RequiresFieldsEvenWhenTheRecordHasValidDefaults) {
    EXPECT_FALSE(read_source<DefaultReadable>("'(:default)"));
    EXPECT_FALSE(read_source<DefaultReadable>("'(:settings-layers)"));
    EXPECT_FALSE(read_source<DefaultReadable>("'(:settings-layers (:default :maybe 2))"));
    auto const layered{read_source<DefaultReadable>(
        "'(:settings-layers (:default :count 2) (:default :maybe 3))")};
    ASSERT_TRUE(layered);
    EXPECT_PRED_FORMAT2(structurally_equal, (DefaultReadable{2, 3}), *layered);
    auto result{read_source<DefaultReadable>("'(:default :count 2)")};
    ASSERT_TRUE(result);
    EXPECT_PRED_FORMAT2(structurally_equal, DefaultReadable{.count = 2}, *result);
    EXPECT_FALSE(read_source<DefaultReadable>("'(:default :count 2 :maybe 1 :maybe 2)"));
}

TEST(GeneratedS7Reader, OverridesACopyAndPreservesOmittedNestedFields) {
    auto initial{read_record_source(R"('(:record :child (:child :name "original" :count 5)))")};
    ASSERT_TRUE(initial);
    auto const original{*initial};
    auto const patch{[&](std::string_view source) {
        Interpreter interpreter;
        auto ast{interpreter.evaluate_ast(source).value()};
        return read_override(Reader{ast}, ast.root, "record", *initial);
    }};
    auto result{patch("'(:record :child (:child :count 2) :maybe :none)")};
    ASSERT_TRUE(result);
    auto expected{*initial};
    expected.child.count = 2;
    expected.maybe.reset();
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
    EXPECT_PRED_FORMAT2(structurally_equal, original, *initial);
    EXPECT_FALSE(patch("'(:record :child (:child :count -1) :enabled #f)"));
    EXPECT_FALSE(patch("'(:record :unknown 1)"));
    EXPECT_PRED_FORMAT2(structurally_equal, original, *initial);
}

TEST(GeneratedS7Reader, OverridesEngagedOptionalRecordsWithoutLosingPresence) {
    auto child{read_source<ReadableChild>(R"('(:child :name "original" :count 5))").value()};
    std::optional<ReadableChild> initial{child};
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast("'(:child :count 2)").value()};
    auto result{read_override(Reader{ast}, ast.root, "child", initial)};
    ASSERT_TRUE(result);
    auto expected{initial};
    expected->count = 2;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
    EXPECT_PRED_FORMAT2(structurally_equal, std::optional{child}, initial);
    EXPECT_FALSE(read_override(Reader{ast}, ast.root, "child", std::optional<ReadableChild>{}));
}

TEST(GeneratedS7Reader, AppliesOverrideChainsFromDistinctSourcesWithoutMutatingTheBase) {
    auto const base{
        read_record_source(R"('(:record :child (:child :name "original" :count 5)))").value()};
    auto const original{base};
    Interpreter interpreter;
    auto first{
        interpreter.evaluate_ast("'(:record :child (:child :count 2) :maybe :none)").value()};
    auto second{
        interpreter.evaluate_ast("'(:record :child (:child :count 3) :enabled #f)").value()};
    Reader const first_reader{first};
    Reader const second_reader{second};
    std::array const overrides{Override{first_reader, first.root, "base.scm"},
                               Override{second_reader, second.root, "level.scm"}};

    auto expected{base};
    expected.child.count = 3;
    expected.maybe.reset();
    expected.enabled = false;
    auto const result{apply_overrides(base, overrides)};
    ASSERT_TRUE(result);
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *result);
    EXPECT_PRED_FORMAT2(structurally_equal, original, base);

    auto const unchanged{apply_overrides(base, {})};
    ASSERT_TRUE(unchanged);
    EXPECT_PRED_FORMAT2(structurally_equal, original, *unchanged);

    auto invalid{
        interpreter.evaluate_ast("'(:record :child (:child :count -1) :enabled #t)").value()};
    Reader const invalid_reader{invalid};
    auto const failed{apply_overrides(
        base, {overrides[0], {invalid_reader, invalid.root, "broken.scm"}, overrides[1]})};
    ASSERT_FALSE(failed);
    ASSERT_EQ(failed.error().size(), 1u);
    EXPECT_EQ(failed.error().front().node_path, "broken.scm.child.count");
    EXPECT_PRED_FORMAT2(structurally_equal, original, base);
}

TEST(GeneratedS7Reader, UsesCompactMasksAndSupportsMoreThan64Fields) {
    enum class Small : std::uint8_t { Required, Optional };
    static_assert(std::same_as<RecordMask<Small, 2>, ioj::FieldMask<Small, 2>>);
    std::array<std::string_view, 2> constexpr names{"required", "optional"};
    auto constexpr required{required_fields<Small, 2>({Small::Required})};
    RecordMask<Small, 2> seen;
    AstDiagnostics errors;
    require_fields<Small>(seen, required, names, "record", errors);
    ASSERT_EQ(errors.size(), 1u);
    seen.set(Small::Required);
    errors.clear();
    require_fields<Small>(seen, required, names, "record", errors);
    EXPECT_TRUE(errors.empty());

    enum class Large : std::uint8_t { Last = 64 };
    auto constexpr large_names{[] {
        std::array<std::string_view, 65> result{};
        result.back() = "last";
        return result;
    }()};
    auto constexpr large_required{required_fields<Large, 65>({Large::Last})};
    RecordMask<Large, 65> large_seen;
    require_fields<Large>(large_seen, large_required, large_names, "record", errors);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors.front().node_path, "record.last");
    large_seen.set(Large::Last);
    errors.clear();
    require_fields<Large>(large_seen, large_required, large_names, "record", errors);
    EXPECT_TRUE(errors.empty());
}
}

namespace lispb::tests {
TEST(RecordSettings, DiscoversNestedFieldsAndReusesRecordsAtDifferentPaths) {
    auto patch{override_source<SettingsTarget>(R"('(:settings
      :first (:actor :max-health 17)
      :second (:actor :weapon (:weapon :damage 17 :turn-speed 4))
      :external-value 17))")};
    ASSERT_TRUE(patch);
    SettingsTarget const expected{.first = {.max_health = 17},
                                  .second = {.weapon = {.damage = 17, .turn_rate = 4}},
                                  .external_value = 17};
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *patch);
    EXPECT_FALSE(override_source<WeaponSettings>("'(:weapon :turn-rate 1)"));
    EXPECT_FALSE(override_source<WeaponSettings>("'(:weapon :asset-distance 1)"));
    EXPECT_FALSE(read_source<WeaponSettings>("'(:weapon :asset-distance 1)"));
    EXPECT_FALSE(override_source<SettingsTarget>("'(:settings :unknown 1)"));
}
}
