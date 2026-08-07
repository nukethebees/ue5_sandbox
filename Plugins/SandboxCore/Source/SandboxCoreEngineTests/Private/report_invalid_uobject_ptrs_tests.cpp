#include <SandboxCore/error_msg.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <CQTest.h>
#include <UObject/Package.h>
#include <UObject/UObjectGlobals.h>

TEST_CLASS(ReportInvalidUObjectPtrs, "SandboxCoreEngine.UnitTests")
{
    TEST_METHOD(ValidPointersDoNotProduceAnError)
    {
        ml::FErrorMsg error_msg;
        auto const test_valid_object{GetTransientPackage()};

        auto const has_errors{ml::report_invalid_uobject_ptrs(
            {
                SANDBOX_NAMED_UOBJECT_PTR(test_valid_object),
            },
            error_msg)};

        TestRunner->TestFalse(TEXT("Valid pointers are accepted"), has_errors);
        TestRunner->TestTrue(TEXT("The error message remains empty"), error_msg.message.IsEmpty());
    }

    TEST_METHOD(InvalidPointersReportTheirNames)
    {
        ml::FErrorMsg error_msg;
        auto const test_valid_object{GetTransientPackage()};

        auto const has_errors{ml::report_invalid_uobject_ptrs(
            {
                SANDBOX_NAMED_UOBJECT_PTR(test_valid_object),
                SANDBOX_NAMED_UOBJECT_PTR(first_invalid_object),
            },
            error_msg)};

        TestRunner->TestTrue(TEXT("Invalid pointers are reported"), has_errors);
        TestRunner->TestTrue(TEXT("The invalid pointer name is included"),
                             error_msg.message.Contains(TEXT("first_invalid_object")));
        TestRunner->TestFalse(TEXT("Valid pointer names are omitted"),
                              error_msg.message.Contains(TEXT("test_valid_object")));
    }

    TEST_METHOD(StringOverloadReportsInvalidPointers)
    {
        auto const message{
            ml::report_invalid_uobject_ptrs({SANDBOX_NAMED_UOBJECT_PTR(first_invalid_object)})};

        TestRunner->TestTrue(TEXT("The string overload reports the invalid pointer"),
                             message.message.Contains(TEXT("first_invalid_object")));
    }

    TEST_METHOD(BatchedChecksStopAtTheFirstInvalidBatch)
    {
        ml::FErrorMsg error_msg;
        auto const test_valid_object{GetTransientPackage()};

        auto const has_errors{ml::report_invalid_uobject_ptrs(
            {
                {SANDBOX_NAMED_UOBJECT_PTR(test_valid_object)},
                {SANDBOX_NAMED_UOBJECT_PTR(first_invalid_object)},
                {SANDBOX_NAMED_UOBJECT_PTR(second_invalid_object)},
            },
            error_msg)};

        TestRunner->TestTrue(TEXT("The invalid batch is reported"), has_errors);
        TestRunner->TestTrue(TEXT("The first invalid batch is included"),
                             error_msg.message.Contains(TEXT("first_invalid_object")));
        TestRunner->TestFalse(TEXT("Later batches are not checked"),
                              error_msg.message.Contains(TEXT("second_invalid_object")));
    }
  private:
    UObject const* first_invalid_object{nullptr};
    UObject const* second_invalid_object{nullptr};
};
