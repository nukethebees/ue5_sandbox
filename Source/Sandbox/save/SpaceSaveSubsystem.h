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

    // Accessors
    auto get_save() const -> USpaceSaveGame const*;
    auto get_mutable_save() -> USpaceSaveGame&;

    // Appending
    void save_score_record(FScoreRecord const& record);

    // Loading
    bool load_or_create();

    // Saving
    bool save_to_disk();

    // Displaying
    void log_save_data() const;
  private:
    static auto slot_name() -> FString;
    static constexpr int32 user_index{0};

    void migrate_if_needed();
    void migrate_to_v2();

    UPROPERTY()
    TObjectPtr<USpaceSaveGame> current_save{nullptr};
};
