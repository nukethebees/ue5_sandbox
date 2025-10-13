#include "Sandbox/mass_entity/processors/MassBulletDestructionProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mass_entity/processors/BulletProcessorGroups.h"
#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletDestructionExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletDestructionExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    TRY_INIT_PTR(world, context.GetWorld());
    TRY_INIT_PTR(bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());

    auto executor{[bullet_subsystem](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const& bullet_data_frag{context.GetConstSharedFragment<FMassBulletDataFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (!state_fragments[i].hit_occurred) {
                continue;
            }

            auto entity{context.GetEntity(i)};
            bullet_subsystem->destroy_bullet(entity, bullet_data_frag.bullet_type);
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletDestructionProcessor::UMassBulletDestructionProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletDestructionExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionVisualization);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
