#pragma once

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "SpaceSaveSubsystem.generated.h"

class USpaceSaveGame;
struct FScoreRecord;

UCLASS()
class USpaceSaveSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    auto get_save() const -> USpaceSaveGame const*;
    auto get_mutable_save() -> USpaceSaveGame&;

    void append_score_record(FScoreRecord const& record);

    bool save_to_disk();
    bool load_or_create();
  private:
    static auto slot_name() -> FString;
    static constexpr int32 user_index{0};

    void migrate_if_needed();
  private:
    UPROPERTY()
    TObjectPtr<USpaceSaveGame> current_save{nullptr};
};
