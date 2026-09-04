#include "SandboxNiagaraShowcaseActor.h"

#include "NiagaraComponent.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxNiagaraShowcase, Log, All);

ASandboxNiagaraShowcaseActor::ASandboxNiagaraShowcaseActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;

    root_component_ = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = root_component_;
}

void ASandboxNiagaraShowcaseActor::configure(
    TArray<FSandboxNiagaraShowcaseEntry> entries,
    float const replay_interval) {
    entries_ = MoveTemp(entries);
    replay_interval_ = replay_interval;
    replay_elapsed_ = 0.0f;
    rebuild_components();
}

void ASandboxNiagaraShowcaseActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    replay_elapsed_ += delta_seconds;
    if (replay_elapsed_ < replay_interval_) {
        return;
    }
    replay_elapsed_ = FMath::Fmod(replay_elapsed_, replay_interval_);

    auto const component_count{effect_components_.Num()};
    for (int32 component_index{0}; component_index < component_count; ++component_index) {
        if (!entries_.IsValidIndex(component_index) ||
            !entries_[component_index].replay_burst) {
            continue;
        }

        auto* const component{effect_components_[component_index].Get()};
        if (IsValid(component)) {
            component->ReinitializeSystem();
        }
    }
}

auto ASandboxNiagaraShowcaseActor::ShouldTickIfViewportsOnly() const -> bool {
    return true;
}

void ASandboxNiagaraShowcaseActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    rebuild_components();
}

void ASandboxNiagaraShowcaseActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    if (effect_components_.IsEmpty() && !entries_.IsEmpty()) {
        rebuild_components();
    }
}

void ASandboxNiagaraShowcaseActor::rebuild_components() {
    for (auto const& component_pointer : effect_components_) {
        auto* const component{component_pointer.Get()};
        if (IsValid(component)) {
            component->DestroyComponent();
        }
    }
    for (auto const& component_pointer : label_components_) {
        auto* const component{component_pointer.Get()};
        if (IsValid(component)) {
            component->DestroyComponent();
        }
    }
    effect_components_.Reset();
    label_components_.Reset();

    for (FSandboxNiagaraShowcaseEntry const& entry : entries_) {
        if (!IsValid(entry.system)) {
            UE_LOG(LogSandboxNiagaraShowcase,
                   Warning,
                   TEXT("Showcase entry '%s' does not have a valid Niagara system."),
                   *entry.display_name);
            effect_components_.Add(nullptr);
            continue;
        }

        auto const effect_component_name{MakeUniqueObjectName(
            this, UNiagaraComponent::StaticClass(), entry.system->GetFName())};
        auto* const effect_component{NewObject<UNiagaraComponent>(
            this, effect_component_name, RF_Transactional)};
        effect_component->SetupAttachment(root_component_);
        effect_component->SetAsset(entry.system);
        effect_component->SetRelativeTransform(entry.effect_transform);
        effect_component->SetAutoActivate(true);
        AddInstanceComponent(effect_component);
        effect_component->RegisterComponent();
        effect_component->Activate(true);
        effect_components_.Add(effect_component);

        auto const label_name{MakeUniqueObjectName(
            this,
            UTextRenderComponent::StaticClass(),
            FName{FString::Printf(TEXT("%s_Label"), *entry.system->GetName())})};
        auto* const label_component{
            NewObject<UTextRenderComponent>(this, label_name, RF_Transactional)};
        label_component->SetupAttachment(root_component_);
        label_component->SetText(FText::FromString(entry.display_name));
        label_component->SetHorizontalAlignment(EHTA_Center);
        label_component->SetWorldSize(80.0f);
        label_component->SetTextRenderColor(FColor::White);
        label_component->SetRelativeTransform(entry.label_transform);
        AddInstanceComponent(label_component);
        label_component->RegisterComponent();
        label_components_.Add(label_component);
    }
}
