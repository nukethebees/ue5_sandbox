#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "SbxMeshGenLabWidget.generated.h"

class STextBlock;

UCLASS()
class SBXMESHGENLAB_API USbxMeshGenLabWidget final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    USbxMeshGenLabWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto generate_cube() -> FReply;

    TSharedPtr<STextBlock> status_text_;
};
