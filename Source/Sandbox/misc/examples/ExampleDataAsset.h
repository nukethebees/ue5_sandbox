#pragma once

#include "ExampleDataTable.h"

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Misc/Paths.h>

#include "ExampleDataAsset.generated.h"

class UDataTable;

USTRUCT(BlueprintType)
struct FExampleDataAssetValues {
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example", meta = (ContentDir))
    FDirectoryPath example_directory_path_content_dir{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    FFilePath example_file_path{};
};

UCLASS(BlueprintType)
class SANDBOX_API UExampleDataAsset : public UDataAsset {
    GENERATED_BODY()
  public:
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Example Data Asset")
    void example_call_in_editor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    FExampleDataAssetValues values{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    TArray<FExampleDataAssetValues> values_array{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Example")
    TObjectPtr<UDataTable> table{nullptr};
};
