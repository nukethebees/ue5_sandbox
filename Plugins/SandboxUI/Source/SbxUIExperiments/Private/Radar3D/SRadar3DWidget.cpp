#include "SRadar3DWidget.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadar3DWidget, Log, All);

void SRadar3DWidget::Construct(FArguments const&) {
    initialise_contacts(initial_contact_count);

    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
    brush_.ImageSize =
        FVector2D{radar_3d_output_texture_dimension, radar_3d_output_texture_dimension};

    output_ready_ = initialise_output_texture();
    SetCanTick(true);

    ChildSlot[SAssignNew(image_, SImage).Image(&brush_)];

    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
    }
}

void SRadar3DWidget::set_contact_count(int32 const contact_count) {
    check(contact_count > 0);
    initialise_contacts(contact_count);

    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

SRadar3DWidget::~SRadar3DWidget() {
    brush_.SetResourceObject(nullptr);
    image_.Reset();
    output_texture_.Reset();
}

void SRadar3DWidget::Tick(FGeometry const& allotted_geometry,
                          double const current_time,
                          float const delta_time) {
    constexpr float moving_contact_orbit_radius{0.72f};
    constexpr float moving_contact_orbit_speed{0.8f};
    constexpr float moving_contact_height{0.32f};
    constexpr float moving_contact_height_amplitude{0.22f};
    constexpr float moving_contact_height_speed{1.3f};

    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);

    elapsed_time_ += delta_time;
    auto const orbit_angle{elapsed_time_ * moving_contact_orbit_speed};
    auto const height_angle{elapsed_time_ * moving_contact_height_speed};
    auto& moving_contact{contacts_[0]};
    moving_contact.position = FVector3f{moving_contact_orbit_radius * FMath::Cos(orbit_angle),
                                        moving_contact_orbit_radius * FMath::Sin(orbit_angle),
                                        moving_contact_height + moving_contact_height_amplitude *
                                                                    FMath::Sin(height_angle)};

    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SRadar3DWidget::initialise_contacts(int32 const contact_count) {
    constexpr float minimum_contact_radius{0.18f};
    constexpr float contact_radius_range{0.68f};
    constexpr float contact_radius_sequence_step{0.618034f};
    constexpr float contact_angle_range{17.0f};
    constexpr float contact_angle_sequence_step{0.37f};
    constexpr float minimum_contact_height{-0.8f};
    constexpr float contact_height_range{1.6f};
    constexpr float contact_height_sequence_step{0.414214f};
    constexpr float minimum_contact_size{4.5f};
    constexpr int32 contact_size_variant_count{4};

    FVector4f const colors[]{
        FVector4f{1.0f, 0.28f, 0.12f, 1.0f},
        FVector4f{0.2f, 0.85f, 1.0f, 1.0f},
        FVector4f{0.42f, 1.0f, 0.38f, 1.0f},
        FVector4f{1.0f, 0.88f, 0.22f, 1.0f},
        FVector4f{0.75f, 0.38f, 1.0f, 1.0f},
    };

    contacts_.SetNumUninitialized(contact_count);
    for (int32 index{0}; index < contact_count; ++index) {
        auto const sequence_index{static_cast<float>(index)};
        auto const fraction{sequence_index / static_cast<float>(contact_count)};
        auto const radius_fraction{
            FMath::Fmod(sequence_index * contact_radius_sequence_step, 1.0f)};
        auto const height_fraction{
            FMath::Fmod(sequence_index * contact_height_sequence_step, 1.0f)};
        auto const radius{minimum_contact_radius + contact_radius_range * radius_fraction};
        auto const angle{fraction * contact_angle_range +
                         sequence_index * contact_angle_sequence_step};
        contacts_[index] = {
            .position = FVector3f{radius * FMath::Cos(angle),
                                  radius * FMath::Sin(angle),
                                  minimum_contact_height + contact_height_range * height_fraction},
            .size = minimum_contact_size + static_cast<float>(index % contact_size_variant_count),
            .color = colors[index % UE_ARRAY_COUNT(colors)],
        };
    }
}

auto SRadar3DWidget::initialise_output_texture() -> bool {
    FLinearColor const render_target_clear_color{0.003f, 0.008f, 0.012f, 1.0f};

    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogRadar3DWidget, Error, TEXT("Failed to allocate the 3D radar render target."));
        return false;
    }

    output_texture->bSupportsUAV = true;
    output_texture->ClearColor = render_target_clear_color;
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(
        radar_3d_output_texture_dimension, radar_3d_output_texture_dimension, PF_R8G8B8A8, true);

    output_texture_.Reset(output_texture);
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}

void SRadar3DWidget::submit_render() {
    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogRadar3DWidget,
               Error,
               TEXT("Failed to acquire the 3D radar render-target resource."));
        output_ready_ = false;
        return;
    }

    renderer_.render(contacts_, output_resource);
}
