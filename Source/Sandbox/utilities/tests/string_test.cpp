#include "Sandbox/utilities/string.h"

#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FWithoutClassPrefixSpec,
                  "Sandbox.string.without_class_prefix",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TTuple<FString, FString>> test_cases;
END_DEFINE_SPEC(FWithoutClassPrefixSpec)

void FWithoutClassPrefixSpec::Define() {
    test_cases = {
        MakeTuple(FString(TEXT("Foo::Bar")), FString(TEXT("Bar"))),
        MakeTuple(FString(TEXT("Bar")), FString(TEXT("Bar"))),
    };

    Describe("without_class_prefix", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& input = test_case.Get<0>();
            auto const& expected = test_case.Get<1>();

            It(*FString::Printf(TEXT("should format '%s' as '%s'"), *input, *expected),
               [this, input, expected]() {
                   auto const result{ml::without_class_prefix(input)};
                   TestEqual(TEXT("Stripped output"), *result, expected);
               });
        }
    });
}
