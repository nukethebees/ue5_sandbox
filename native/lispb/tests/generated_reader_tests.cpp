#include <ioj/s7/interpreter.h>

#include <gtest/gtest.h>
#include <ReaderDependency_reader.h>
#include <Records_reader.h>

namespace lispb::tests {
using namespace ioj::s7;
using namespace codegen_compile_fixture;

template <typename T>
auto read_source(std::string_view const source) -> ReadResult<T> {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(source)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    return read<T>(Reader{*ast}, ast->root, "record");
}

TEST(GeneratedS7Reader, ConstrainedRecordsValidateAndOverrideAsWholeValues) {
    RangeSettings target;
    EXPECT_EQ(target.range.get().minimum, 0);
    EXPECT_EQ(target.range.get().maximum, 10);
    EXPECT_FALSE(Range::try_make({10, 0}));
    static_assert(std::same_as<decltype(target.range.get_ref()), RangeValue const&>);
    static_assert(std::same_as<decltype(RangeSettingsOverrides::range), std::optional<Range>>);

    auto patch{
        read_source<RangeSettingsOverrides>("'(:ranges :range (:range :minimum 20 :maximum 30))")};
    ASSERT_TRUE(patch);
    apply_overrides(target, *patch);
    EXPECT_EQ(target.range.get().minimum, 20);
    EXPECT_EQ(target.range.get().maximum, 30);
    EXPECT_TRUE(target.enabled);

    auto omitted{read_source<RangeSettingsOverrides>("'(:ranges :enabled #f)")};
    ASSERT_TRUE(omitted);
    apply_overrides(target, *omitted);
    EXPECT_EQ(target.range.get().minimum, 20);
    EXPECT_FALSE(target.enabled);

    auto defaults{
        read_source<RangeSettingsOverrides>("'(:ranges :range (:range :minimum 0 :maximum 10))")};
    ASSERT_TRUE(defaults);
    apply_overrides(target, *defaults);
    EXPECT_EQ(target.range, Range{});

    auto missing{read_source<RangeSettingsOverrides>("'(:ranges :range (:range :minimum 5))")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().front().node_path, "record.range.maximum");
    auto invalid{
        read_source<RangeSettingsOverrides>("'(:ranges :range (:range :minimum 30 :maximum 20))")};
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().front().node_path, "record.range");
    EXPECT_EQ(invalid.error().front().message, "Expected minimum <= maximum");
}

TEST(GeneratedS7Reader, ReadsSparseTypedOverridesWithNestedLayersAndExplicitEmptyValues) {
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast(R"('(:settings-layers
      (:actor :max-health 200 :weapon (:weapon :damage 17 :turn-speed 9))
      (:actor :max-health 100 :weapon (:weapon :damage 5) :optional-limit :none)))")};
    ASSERT_TRUE(ast);
    auto patch{read_override(Reader{*ast}, ast->root, "actor", ActorSettingsOverrides{})};
    ASSERT_TRUE(patch);
    ASSERT_TRUE(patch->optional_limit);
    EXPECT_FALSE(*patch->optional_limit);
    ActorSettings target;
    target.weapon.asset_distance = 42;
    apply_overrides(target, *patch);
    EXPECT_EQ(target.max_health, 100);
    EXPECT_EQ(target.weapon.damage, 5);
    EXPECT_EQ(target.weapon.turn_rate, 9);
    EXPECT_EQ(target.weapon.asset_distance, 42);
    EXPECT_FALSE(target.optional_limit);
    EXPECT_FALSE(read_source<WeaponSettingsOverrides>("'(:weapon :asset-distance 3)"));
    EXPECT_FALSE(read_source<WeaponSettingsOverrides>("'(:weapon :damage -1)"));
    EXPECT_FALSE(read_source<WeaponSettingsOverrides>("'(:weapon :damage 1 :damage 2)"));
    EXPECT_TRUE(read_source<ActorSettingsOverrides>("'(:actor)"));
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
    ASSERT_EQ(recursive->children.size(), 1u);
    EXPECT_EQ(recursive->children.front().children.size(), 1u);
    auto const dependency{
        read_source<ReaderDependency>(R"('(:dependent :child (:child :name "nested")))")};
    ASSERT_TRUE(dependency);
    EXPECT_EQ(dependency->child.name.get_ref(), "nested");
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
    EXPECT_EQ(result->child.name.get_ref(), "root");
    EXPECT_EQ(result->child.count, 2);
    ASSERT_EQ(result->values.size(), 2u);
    EXPECT_EQ(result->values[0].count, 0);
    EXPECT_EQ(result->values[1].name.get_ref(), "second");
    EXPECT_EQ(result->points, (std::array{1.f, 2.f, 3.f}));
    EXPECT_FALSE(result->enabled);
    EXPECT_FALSE(result->maybe);
    EXPECT_EQ(result->mode, EPlainFixture::ReadableName);
    EXPECT_EQ(result->title, "untitled");
    EXPECT_EQ(result->scale, 1.f);
}

TEST(GeneratedS7Reader, PreservesDefaultsAndInclusiveConstraints) {
    for (auto const scale : {"0.5", "5"}) {
        SCOPED_TRACE(scale);
        auto const result{read_record_source(
            std::format(R"('(:record :child (:child :name "root") :scale {}))", scale))};
        ASSERT_TRUE(result);
        EXPECT_EQ(result->maybe, 7);
        EXPECT_TRUE(result->values.empty());
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
    EXPECT_EQ(result->name.get_ref(), "name");
    EXPECT_EQ(result->count, 3);
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
    auto result{read_source<DefaultReadable>("'(:default :count 2)")};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->count, 2);
    EXPECT_EQ(result->maybe, 7);
    EXPECT_FALSE(read_source<DefaultReadable>("'(:default :count 2 :maybe 1 :maybe 2)"));
}

TEST(GeneratedS7Reader, OverridesACopyAndPreservesOmittedNestedFields) {
    auto initial{read_record_source(R"('(:record :child (:child :name "original" :count 5)))")};
    ASSERT_TRUE(initial);
    auto const patch{[&](std::string_view source) {
        Interpreter interpreter;
        auto ast{interpreter.evaluate_ast(source).value()};
        return read_override(Reader{ast}, ast.root, "record", *initial);
    }};
    auto result{patch("'(:record :child (:child :count 2) :maybe :none)")};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->child.name.get_ref(), "original");
    EXPECT_EQ(result->child.count, 2);
    EXPECT_FALSE(result->maybe);
    EXPECT_EQ(result->scale, 1.f);
    EXPECT_EQ(initial->child.count, 5);
    EXPECT_EQ(initial->maybe, 7);
    EXPECT_FALSE(patch("'(:record :child (:child :count -1) :enabled #f)"));
    EXPECT_FALSE(patch("'(:record :unknown 1)"));
    EXPECT_TRUE(initial->enabled);
    EXPECT_EQ(initial->child.count, 5);
}

TEST(GeneratedS7Reader, OverridesEngagedOptionalRecordsWithoutLosingPresence) {
    auto child{read_source<ReadableChild>(R"('(:child :name "original" :count 5))").value()};
    std::optional<ReadableChild> initial{child};
    Interpreter interpreter;
    auto ast{interpreter.evaluate_ast("'(:child :count 2)").value()};
    auto result{read_override(Reader{ast}, ast.root, "child", initial)};
    ASSERT_TRUE(result);
    ASSERT_TRUE(*result);
    EXPECT_EQ((*result)->name.get_ref(), "original");
    EXPECT_EQ((*result)->count, 2);
    EXPECT_EQ(initial->count, 5);
    EXPECT_FALSE(read_override(Reader{ast}, ast.root, "child", std::optional<ReadableChild>{}));
}

TEST(GeneratedS7Reader, UsesCompactMasksAndSupportsMoreThan64Fields) {
    enum class Small : std::uint8_t { Required, Optional, Count };
    static_assert(std::same_as<RecordMask<Small>, ioj::FieldMask<Small>>);
    std::array<std::string_view, 2> constexpr names{"required", ""};
    auto constexpr required{required_fields<Small>(names)};
    RecordMask<Small> seen;
    AstDiagnostics errors;
    require_fields<Small>(seen, required, names, "record", errors);
    ASSERT_EQ(errors.size(), 1u);
    seen.set(Small::Required);
    errors.clear();
    require_fields<Small>(seen, required, names, "record", errors);
    EXPECT_TRUE(errors.empty());

    enum class Large : std::uint8_t { Last = 64, Count };
    auto constexpr large_names{[] {
        std::array<std::string_view, 65> result{};
        result.back() = "last";
        return result;
    }()};
    auto constexpr large_required{required_fields<Large>(large_names)};
    RecordMask<Large> large_seen;
    require_fields<Large>(large_seen, large_required, large_names, "record", errors);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors.front().node_path, "record.last");
    large_seen.set(Large::Last);
    errors.clear();
    require_fields<Large>(large_seen, large_required, large_names, "record", errors);
    EXPECT_TRUE(errors.empty());
}
}
