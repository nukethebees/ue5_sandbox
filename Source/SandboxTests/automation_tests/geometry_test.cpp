#include "Sandbox/utilities/geometry.h"

#include "Misc/AutomationTest.h"

struct TestCase {
    float start;
    float arc;
    int32 segs;
    TArray<float> expected;
};

BEGIN_DEFINE_SPEC(FArcSplitSpec,
                  "Sandbox.geometry.subdivide_arc_into_segments",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TestCase> test_cases;
END_DEFINE_SPEC(FArcSplitSpec)

void FArcSplitSpec::Define() {
    using TC = TestCase;
    test_cases = {
        {0.f, 180.f, 1, {0.f, 180.f}},
        {-90.f, 180.f, 1, {-90.f, 90.f}},
        {-90.f, 180.f, 2, {-90.f, 0.f, 90.f}},
    };

    Describe("get_angle_between_norm_lines", [this]() {
        for (auto const& tc : test_cases) {
            auto name{
                FString::Printf(TEXT("Start=%.1f, Arc=%.1f, Segs=%d"), tc.start, tc.arc, tc.segs)};
            name.ReplaceCharInline(TEXT('.'), TEXT('p'));
            It(*name, [&]() {
                auto const result{ml::subdivide_arc_into_segments(tc.start, tc.arc, tc.segs)};
                TestEqual(TEXT("Split arc"), result, tc.expected);
            });
        }
    });
}
