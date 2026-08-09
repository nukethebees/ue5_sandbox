#pragma once

#include <CoreMinimal.h>
#include <Engine/DataTable.h>
#include <Misc/Paths.h>

#include "ExampleDataTable.generated.h"

UENUM(BlueprintType)
enum class EExampleDataTableLongFormExampleState : uint8 {
    FirstExample,
    SecondExample,
};

USTRUCT(BlueprintType)
struct FExampleDataTableRow : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    float example_float{0.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    int32 example_int{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    bool example_bool{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    EExampleDataTableLongFormExampleState example_state{
        EExampleDataTableLongFormExampleState::FirstExample};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    FVector example_vector{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    FDirectoryPath example_directory_path{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    FFilePath example_file_path{};

    // UFUNCTION is not valid on a FTableRowBase USTRUCT. This is the equivalent native example.
    [[nodiscard]]
    auto example_row_function() const -> float;
};

UCLASS(BlueprintType)
class SANDBOX_API UExampleDataTable : public UDataTable {
    GENERATED_BODY()
  public:
    UExampleDataTable();

    UFUNCTION(BlueprintCallable, Category = "Example Data Table")
    FVector example_table_function(FName row_name) const;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Example Data Table")
    void example_call_in_editor() const;
};
