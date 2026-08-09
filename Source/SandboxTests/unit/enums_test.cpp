#include "Sandbox/utilities/enums.h"

#include "Misc/AutomationTest.h"

#include "Sandbox/utilities/tests/LocalTestEnum.h"

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
                   auto const result{ml::to_string_without_type_prefix(input)};
                   TestEqual(TEXT("Stripped output"), *result, expected);
               });
        }
    });
}
