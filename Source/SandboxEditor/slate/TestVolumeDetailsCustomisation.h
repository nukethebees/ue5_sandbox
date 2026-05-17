#pragma once

#include <IDetailCustomization.h>
#include <Templates/SharedPointer.h>
#include <UObject/WeakObjectPtr.h>

class IDetailLayoutBuilder;
class UBoxComponent;

class ATestVolume;

class FTestVolumeDetailsCustomisation final : public IDetailCustomization {
  public:
    static auto MakeInstance() -> TSharedRef<IDetailCustomization>;

    void CustomizeDetails(IDetailLayoutBuilder& detail_builder) override;
  private:
    void get_actor(IDetailLayoutBuilder& detail_builder);
    void build_custom_rows(IDetailLayoutBuilder& detail_builder);

    auto get_box() -> UBoxComponent*;

    TWeakObjectPtr<ATestVolume> selected_actor{nullptr};
};
