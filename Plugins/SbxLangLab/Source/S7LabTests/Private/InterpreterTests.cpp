#include "S7Lab/Interpreter.h"

#include <CQTest.h>

namespace {
void test_success(FAutomationTestBase& test,
                  S7Lab::FEvaluationResult const& result,
                  FString const& expected) {
    test.TestTrue(TEXT("Evaluation succeeds"), result.succeeded);
    test.TestEqual(TEXT("Evaluation returns the expected value"), result.value, expected);
    test.TestTrue(TEXT("Successful evaluation has no error"), result.error.IsEmpty());
}
}

TEST_CLASS(S7Interpreter, "S7Lab.UnitTests")
{
    TEST_METHOD(EvaluatesSchemeAndPreservesState)
    {
        S7Lab::FInterpreter interpreter;

        auto const definition{interpreter.evaluate(
            TEXT("(begin (define answer-to-everything 6) (* answer-to-everything 7))"))};
        test_success(*TestRunner, definition, TEXT("42"));

        auto const reuse{interpreter.evaluate(TEXT("(+ answer-to-everything 1)"))};
        test_success(*TestRunner, reuse, TEXT("7"));
    }

    TEST_METHOD(CallsUnrealBindings)
    {
        S7Lab::FInterpreter interpreter;

        auto const vector_result{
            interpreter.evaluate(TEXT("(= (sbx-vector-length-squared 2 3 6) 49.0)"))};
        test_success(*TestRunner, vector_result, TEXT("#t"));

        auto const string_result{
            interpreter.evaluate(TEXT("(string=? (sbx-uppercase \"Lang Lab\") \"LANG LAB\")"))};
        test_success(*TestRunner, string_result, TEXT("#t"));
    }

    TEST_METHOD(ReportsErrorsAndRemainsUsable)
    {
        S7Lab::FInterpreter interpreter;

        auto const error{interpreter.evaluate(TEXT("(sbx-vector-length-squared 1 \"bad\" 3)"))};
        TestRunner->TestFalse(TEXT("Invalid arguments fail evaluation"), error.succeeded);
        TestRunner->TestTrue(TEXT("Invalid arguments have no value"), error.value.IsEmpty());
        TestRunner->TestTrue(TEXT("The error identifies the binding"),
                             error.error.Contains(TEXT("sbx-vector-length-squared")));

        auto const recovery{interpreter.evaluate(TEXT("(+ 20 22)"))};
        test_success(*TestRunner, recovery, TEXT("42"));
    }

    TEST_METHOD(InterpreterInstancesHaveIndependentState)
    {
        S7Lab::FInterpreter first;
        auto const definition{
            first.evaluate(TEXT("(begin (define private-value 17) private-value)"))};
        test_success(*TestRunner, definition, TEXT("17"));

        S7Lab::FInterpreter second;
        auto const lookup{second.evaluate(TEXT("private-value"))};
        TestRunner->TestFalse(TEXT("A second interpreter cannot see the first's state"),
                              lookup.succeeded);
        TestRunner->TestFalse(TEXT("Missing state produces an error"), lookup.error.IsEmpty());
    }

    TEST_METHOD(RejectsUnsafeOperationsAndRemainsUsable)
    {
        S7Lab::FInterpreter interpreter;
        constexpr TCHAR const* expressions[]{
            TEXT("(exit)"),
            TEXT("(emergency-exit)"),
            TEXT("(load \"scenario.scm\")"),
            TEXT("(open-input-file \"scenario.scm\")"),
            TEXT("(open-output-file \"scenario.scm\")"),
            TEXT("(system \"echo unsafe\")"),
        };

        for (auto const* const expression : expressions) {
            auto const result{interpreter.evaluate(expression)};
            TestRunner->TestFalse(TEXT("Unsafe operation fails evaluation"), result.succeeded);
            TestRunner->TestTrue(TEXT("Unsafe operation reports the sandbox restriction"),
                                 result.error.Contains(TEXT("disabled by the S7Lab sandbox")));
        }

        auto const recovery{interpreter.evaluate(TEXT("(+ 20 22)"))};
        test_success(*TestRunner, recovery, TEXT("42"));
    }
};
