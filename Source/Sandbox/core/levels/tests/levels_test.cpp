#include "Sandbox/core/levels/levels.h"

BEGIN_DEFINE_SPEC(FFormatLevelDisplayNameSpec,
                  "Sandbox.levels.format_level_display_name 1",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TTuple<FName, FString>> test_cases;
END_DEFINE_SPEC(FFormatLevelDisplayNameSpec)

void FFormatLevelDisplayNameSpec::Define() {
    // Setup test data
    test_cases = {
        MakeTuple(FName("a_b_c"), FString("A B C")),
        MakeTuple(FName("level_one"), FString("Level One")),
        MakeTuple(FName("test_level_42"), FString("Test Level 42")),
        MakeTuple(FName(""), FString("")),
        MakeTuple(FName("singleword"), FString("Singleword")),
        MakeTuple(FName("__42"), FString("42")),
        MakeTuple(FName("Level1"), FString("Level 1")),
        MakeTuple(FName("Level123"), FString("Level 123")),
    };

    Describe("format_level_display_name", [this]() {
        for (auto const& test_case : test_cases) {
            FName const input = test_case.Get<0>();
            FString const expected = test_case.Get<1>();

            It(*FString::Printf(TEXT("should format '%s' as '%s'"), *input.ToString(), *expected),
               [this, input, expected]() {
                   auto const actual{ml::format_level_display_name(input)};
                   TestEqual(TEXT("Formatted output"), actual, expected);
               });
        }
    });
}
