#include "Sandbox/utilities/tests/LocalTestEnum.h"
#include "SpaceGamePresentation/ui/style/GameUiStyleTypes.h"

#include "SandboxCoreEngine/enums.h"

#include "Misc/AutomationTest.h"

#include <array>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackedEnumStringsTest,
                                 "Sandbox.enum.packed_strings",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FPackedEnumStringsTest::RunTest(FString const&) {
    std::array<TCHAR const*, 9> const names{TEXT("Hive"),
                                            TEXT("Video"),
                                            TEXT("Gameplay"),
                                            TEXT("Audio"),
                                            TEXT("Controls"),
                                            TEXT("Accessibility"),
                                            TEXT("System"),
                                            TEXT("LevelIncomplete"),
                                            TEXT("LevelComplete")};
    auto const count{names.size()};
    for (std::size_t index{}; index < count; ++index) {
        TestEqual(TEXT("Packed enum export preserves icon names"),
                  FString{LexToString(static_cast<EGameUiIcon>(index))},
                  FString{names[index]});
    }
    return true;
}

BEGIN_DEFINE_SPEC(FWithoutEnumTypePrefixSpec,
                  "Sandbox.enum.without_class_prefix",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TTuple<ELocalTestEnum, FString>> test_cases;
END_DEFINE_SPEC(FWithoutEnumTypePrefixSpec)

void FWithoutEnumTypePrefixSpec::Define() {
    test_cases = {
        MakeTuple(ELocalTestEnum::Foo, FString(TEXT("Foo"))),
        MakeTuple(ELocalTestEnum::Bar, FString(TEXT("Bar"))),
        MakeTuple(ELocalTestEnum::Baz, FString(TEXT("Baz"))),
    };

    Describe("without_class_prefix", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& input = test_case.Get<0>();
            auto const& expected = test_case.Get<1>();

            It(*FString::Printf(
                   TEXT("should format '%s' as '%s'"), *UEnum::GetValueAsString(input), *expected),
               [this, input, expected]() {
                   auto const result{ioj::to_string_without_type_prefix(input)};
                   TestEqual(TEXT("Stripped output"), *result, expected);
               });
        }
    });
}
