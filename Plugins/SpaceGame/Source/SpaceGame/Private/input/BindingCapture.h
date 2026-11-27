#pragma once

#include <ioj/input/physical_key.h>

#include <Delegates/Delegate.h>
#include <Types/ISlateMetaData.h>

namespace ioj {
struct FBindingCapture : ISlateMetaData {
    SLATE_METADATA_TYPE(FBindingCapture, ISlateMetaData)

    DECLARE_DELEGATE_OneParam(FOnKey, PhysicalKey);
    FOnKey on_key;
};
}
