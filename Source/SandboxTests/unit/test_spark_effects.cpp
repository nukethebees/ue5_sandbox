#include <SpaceGameRendering/SparkEffects.h>
#include <SpaceGameRendering/SparkRendererComponent.h>

#include <CQTest.h>

TEST_CLASS(SparkEffects, "Sandbox.UnitTests")
{
    TEST_METHOD(QueuedBurstsCommitToRendererAndClearResetsPresentation)
    {
        auto* const renderer{NewObject<USparkRendererComponent>()};
        FSparkRendererSettings settings;
        settings.capacity = 2;
        renderer->initialise(settings);
        FSparkEffects effects{*renderer};
        ioj::presentation::SparkBurst burst{.emission = {.seed = 12345},
                                            .style = {.count = 1, .lifetime = {10.f, 10.f}}};
        effects.queue_burst(burst);
        ++burst.emission.seed;
        effects.queue_burst(burst);
        effects.commit(1.f);

        TestRunner->TestEqual(TEXT("Commit submits both compact records"),
                              renderer->get_last_submission_upload_bytes(),
                              2 * static_cast<int32>(sizeof(ioj::presentation::GpuSparkBurst)));
        TestRunner->TestEqual(
            TEXT("Native effect time reaches the renderer"), renderer->get_effect_time(), 1.f);
        ioj::presentation::SparkBurst const replacement[]{burst};
        TestRunner->TestEqual(TEXT("Both pre-commit bursts occupy the ring"),
                              renderer->submit_bursts(replacement, 1.f).replaced_slots,
                              1);
        effects.clear();
        TestRunner->TestEqual(TEXT("Clear removes live ring entries"),
                              renderer->submit_bursts(replacement, 0.f).replaced_slots,
                              0);
        TestRunner->TestEqual(
            TEXT("Clear resets presentation time"), renderer->get_effect_time(), 0.f);
    }
};
