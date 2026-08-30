#include "LuaLab/Interpreter.h"

#include <CQTest.h>

namespace {
void test_success(FAutomationTestBase& test,
                  LuaLab::FEvaluationResult const& result,
                  FString const& expected) {
    test.TestTrue(TEXT("Evaluation succeeds"), result.succeeded);
    test.TestEqual(TEXT("Evaluation returns the expected value"), result.value, expected);
    test.TestTrue(TEXT("Successful evaluation has no error"), result.error.IsEmpty());
}
}

TEST_CLASS(LuaInterpreter, "LuaLab.UnitTests")
{
    TEST_METHOD(EvaluatesLuaAndPreservesState)
    {
        LuaLab::FInterpreter interpreter;

        auto const definition{interpreter.evaluate(
            TEXT("answer_to_everything = 6; return answer_to_everything * 7"))};
        test_success(*TestRunner, definition, TEXT("42"));

        auto const reuse{interpreter.evaluate(TEXT("return answer_to_everything + 1"))};
        test_success(*TestRunner, reuse, TEXT("7"));
    }

    TEST_METHOD(CallsUnrealBindings)
    {
        LuaLab::FInterpreter interpreter;

        auto const vector_result{
            interpreter.evaluate(TEXT("return sbx_vector_length_squared(2, 3, 6)"))};
        test_success(*TestRunner, vector_result, TEXT("49.0"));

        auto const string_result{interpreter.evaluate(TEXT("return sbx_uppercase('Lang Lab')"))};
        test_success(*TestRunner, string_result, TEXT("LANG LAB"));
    }

    TEST_METHOD(ReportsErrorsAndRemainsUsable)
    {
        LuaLab::FInterpreter interpreter;

        auto const error{
            interpreter.evaluate(TEXT("return sbx_vector_length_squared(1, 'bad', 3)"))};
        TestRunner->TestFalse(TEXT("Invalid arguments fail evaluation"), error.succeeded);
        TestRunner->TestTrue(TEXT("Invalid arguments have no value"), error.value.IsEmpty());
        TestRunner->TestTrue(TEXT("The error identifies the binding"),
                             error.error.Contains(TEXT("sbx_vector_length_squared")));

        auto const recovery{interpreter.evaluate(TEXT("return 20 + 22"))};
        test_success(*TestRunner, recovery, TEXT("42"));
    }

    TEST_METHOD(InterpreterInstancesHaveIndependentState)
    {
        LuaLab::FInterpreter first;
        auto const definition{first.evaluate(TEXT("private_value = 17; return private_value"))};
        test_success(*TestRunner, definition, TEXT("17"));

        LuaLab::FInterpreter second;
        auto const lookup{second.evaluate(TEXT("return private_value + 1"))};
        TestRunner->TestFalse(TEXT("A second interpreter cannot see the first's state"),
                              lookup.succeeded);
        TestRunner->TestFalse(TEXT("Missing state produces an error"), lookup.error.IsEmpty());
    }
};
