#include "SbxUIExperiments/Radar3D/Radar3DShowcase.h"

#include <utility>

#include "Benchmarks/Radar3D/Radar3DBenchmark.h"
#include "SandboxUI/Radar/RadarTypes.h"
#include "SandboxUI/Radar/SRadarWidget.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#include "generated/URadar3DShowcase.slate.generated.h"

URadar3DShowcase::URadar3DShowcase() {
    TabDisplayName = NSLOCTEXT("Radar3D", "ShowcaseTabName", "RDG 3D Radar Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> URadar3DShowcase::RebuildWidget() {
    auto builder{SlateGenerated::URadar3DShowcaseBuilder{*this}};
    radar_widget_ = builder.BuildRadarWidget();
    radar_frame_store_ = MakeShared<FRadarFrameStore, ESPMode::ThreadSafe>();
    radar_widget_->set_frame_store(radar_frame_store_);
    populate_frame(19);
    radar_widget_->render();
    auto on_value_changed{[this](int32 const contact_count) { set_contact_count(contact_count); }};

    return builder.RebuildWidget(std::move(on_value_changed), radar_widget_.ToSharedRef());
}

void URadar3DShowcase::set_contact_count(int32 const contact_count) {
    populate_frame(FMath::Max(contact_count, 1));
    radar_widget_->render();
}

void URadar3DShowcase::populate_frame(int32 const contact_count) {
    auto& frame{radar_frame_store_->next()};
    frame.combat_display_radius = 0.45f;
    frame.tactical_display_radius = 0.8f;
    auto& instances{frame.instances};
    instances.Reset();
    instances.Reserve(contact_count);
    FLinearColor const colors[]{
        {0.12f, 0.72f, 1.0f, 1.0f},
        {1.0f, 0.08f, 0.035f, 1.0f},
        {0.78f, 0.84f, 0.86f, 1.0f},
    };

    auto const non_player_count{FMath::Max(contact_count - 1, 0)};
    for (int32 priority{0}; priority < 3; ++priority) {
        for (int32 index{0}; index < non_player_count; ++index) {
            auto const flag_variant{index % 9};
            auto const flags{flag_variant == 0   ? ERadarContactFlags::Selected
                             : flag_variant == 1 ? ERadarContactFlags::DefendObjective
                             : flag_variant == 2 ? ERadarContactFlags::DestroyObjective
                             : flag_variant == 3 ? ERadarContactFlags::Selected |
                                                       ERadarContactFlags::DestroyObjective
                                                 : ERadarContactFlags::None};
            auto const draw_priority{EnumHasAnyFlags(flags, ERadarContactFlags::Selected) ? 2
                                     : flags != ERadarContactFlags::None                  ? 1
                                                                                          : 0};
            if (draw_priority != priority) {
                continue;
            }
            auto const angle{static_cast<float>(index) * 2.399963f};
            FVector2f const direction{FMath::Cos(angle), FMath::Sin(angle)};
            auto const hex_denominator{
                FMath::Max(FMath::Abs(direction.X) * 0.8660254f + FMath::Abs(direction.Y) * 0.5f,
                           FMath::Abs(direction.Y))};
            auto const boundary_radius{0.8660254f / hex_denominator};
            auto const zone_fraction{FMath::Fmod(index * 0.618034f, 1.0f)};
            auto const radius_fraction{index % 3 == 0   ? 0.12f + 0.28f * zone_fraction
                                       : index % 3 == 1 ? 0.50f + 0.22f * zone_fraction
                                                        : 0.83f + 0.13f * zone_fraction};
            auto radar_position{FVector3f{direction.X * radius_fraction * boundary_radius,
                                          direction.Y * radius_fraction * boundary_radius,
                                          -0.72f + 0.72f * static_cast<float>(index % 3)}};
            if (index >= 12 && index < 15) {
                radar_position = {0.22f + 0.015f * static_cast<float>(index - 12),
                                  -0.12f,
                                  -0.35f + 0.35f * static_cast<float>(index - 12)};
            }
            instances.Add({
                .radar_position = radar_position,
                .size_scale = 1.0f,
                .packed_color = pack_radar_color(colors[index % UE_ARRAY_COUNT(colors)]),
                .packed_glyph_and_flags =
                    pack_radar_display(static_cast<ERadarGlyph>(1 + index % 6), flags),
            });
        }
    }
    instances.Add({
        .radar_position = FVector3f::ZeroVector,
        .size_scale = 1.0f,
        .packed_color = pack_radar_color({1.0f, 0.72f, 0.18f, 1.0f}),
        .packed_glyph_and_flags = pack_radar_display(ERadarGlyph::Player, ERadarContactFlags::None),
    });
    radar_frame_store_->publish();
}

auto URadar3DShowcase::run_benchmark() -> FReply {
    FRadar3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_radar_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
