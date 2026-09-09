#include "SpaceGamePresentation/presentation/widgets/ForceStatusWidget.h"

#include <SandboxCoreEngine/enums.h>

#include <Styling/CoreStyle.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/SOverlay.h>
#include <Widgets/Text/STextBlock.h>

namespace {
FVector2f const default_bar_size{18.0f, 104.0f};
constexpr float default_bar_spacing{4.0f};

struct FForceCategory {
    ETestEntityType type;
    FText label;
};

auto force_categories() -> TConstArrayView<FForceCategory> {
    static FForceCategory const categories[]{
        {ETestEntityType::CapitalShipFighter, INVTEXT("F")},
        {ETestEntityType::CapitalShip, INVTEXT("CS")},
        {ETestEntityType::Turret, INVTEXT("T")},
    };
    return categories;
}
}

void UForceStatusWidget::set_entity_counts(FTestEntityRegistry::EntityCounts const& counts) {
    entity_counts_ = counts;
    refresh_content();
}

void UForceStatusWidget::set_team_colours(UTestTeamVisualData::FColourArray const& colours) {
    team_colours_ = colours;
    refresh_content();
}

void UForceStatusWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    refresh_content();
}

auto UForceStatusWidget::RebuildWidget() -> TSharedRef<SWidget> {
    SAssignNew(root_box_, SBox)[build_content()];
    return root_box_.ToSharedRef();
}

void UForceStatusWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    root_box_.Reset();
}

auto UForceStatusWidget::build_content() const -> TSharedRef<SWidget> {
    auto const* const white_brush{FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))};
    auto const* const text_style{
        hud_style_ ? &hud_style_->caption_text
                   : &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"))};
    auto const bar_size{hud_style_ ? hud_style_->force_status_bar_size : default_bar_size};
    auto const bar_spacing{hud_style_ ? hud_style_->force_status_bar_spacing : default_bar_spacing};
    auto const* const track_brush{hud_style_ ? &hud_style_->control_background : white_brush};
    auto const* const border_brush{hud_style_ ? &hud_style_->border : white_brush};

    auto bars{SNew(SHorizontalBox)};
    auto const categories{force_categories()};
    auto const category_count{categories.Num()};
    for (int32 category_index{}; category_index < category_count; ++category_index) {
        auto const& category{categories[category_index]};
        auto const type_index{std::to_underlying(category.type)};
        int32 total{};
        constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            total += entity_counts_[team_index][type_index];
        }

        auto segments{SNew(SVerticalBox)};
        if (total > 0) {
            for (int32 team_index{}; team_index < team_count; ++team_index) {
                auto const count{entity_counts_[team_index][type_index]};
                if (count <= 0) {
                    continue;
                }

                auto const team{static_cast<ETestTeam>(team_index)};
                segments->AddSlot().FillHeight(
                    static_cast<float>(count))[SNew(SBorder)
                                                   .BorderImage(white_brush)
                                                   .BorderBackgroundColor(team_colours_[team])];
            }
        }

        auto const right_padding{category_index + 1 < category_count ? bar_spacing : 0.0f};
        bars->AddSlot().AutoWidth().Padding(FMargin{0.0f, 0.0f, right_padding, 0.0f})
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight()
                 [SNew(SBox)
                      .WidthOverride(bar_size.X)
                      .HeightOverride(bar_size.Y)
                          [SNew(SBorder)
                               .BorderImage(border_brush)
                               .Padding(FMargin{1.0f})[SNew(SBorder)
                                                           .BorderImage(track_brush)
                                                           .Padding(FMargin{0.0f})[segments]]]] +
             SVerticalBox::Slot()
                 .AutoHeight()
                 .Padding(FMargin{0.0f, 4.0f, 0.0f, 0.0f})
                 .HAlign(
                     HAlign_Center)[SNew(STextBlock).Text(category.label).TextStyle(text_style)]];
    }

    auto content{StaticCastSharedRef<SWidget>(bars)};
    return hud_style_ ? ml::ioj::make_hud_panel(hud_style_.GetValue(), MoveTemp(content)) : content;
}

void UForceStatusWidget::refresh_content() {
    if (root_box_.IsValid()) {
        root_box_->SetContent(build_content());
    }
}
