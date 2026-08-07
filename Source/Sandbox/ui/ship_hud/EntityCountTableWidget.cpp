#include "Sandbox/ui/ship_hud/EntityCountTableWidget.h"

#include <Sandbox/utilities/enums.h>

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/TextBlock.h>

void UEntityCountTableWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    if (IsDesignTime()) {
        constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
        constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
        for (int32 team{0}; team < n_teams; ++team) {
            for (int32 type{0}; type < n_types; ++type) {
                entity_counts[team][type] = 10 + team + type;
            }
        }

        team_colours[ETestTeam::White] = FLinearColor::White;
        team_colours[ETestTeam::Red] = FLinearColor::Red;
        team_colours[ETestTeam::Green] = FLinearColor::Green;
        team_colours[ETestTeam::Blue] = FLinearColor::Blue;
        team_colours[ETestTeam::Orange] = FLinearColor(1.f, 0.35f, 0.f, 1.f);
        team_colours[ETestTeam::Yellow] = FLinearColor::Yellow;
    }

    rebuild_table();
}

void UEntityCountTableWidget::set_entity_counts(
    ATestEntityRegistry::EntityCounts const& new_counts) {
    entity_counts = new_counts;
    rebuild_table();
}

void UEntityCountTableWidget::set_team_colours(
    UTestTeamVisualData::FColourArray const& new_colours) {
    team_colours = new_colours;
    rebuild_table();
}

void UEntityCountTableWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    rebuild_table();
}

void UEntityCountTableWidget::set_text_style(UTextBlock& text,
                                             ETextJustify::Type const alignment) const {
    auto font{text.GetFont()};
    font.Size = font_size;
    text.SetFont(font);
    text.SetJustification(alignment);
}

void UEntityCountTableWidget::rebuild_table() {
    if (!WidgetTree) {
        return;
    }

    if (!entity_count_grid) {
        entity_count_grid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(),
                                                                    TEXT("entity_count_grid"));
        WidgetTree->RootWidget = entity_count_grid;
    }

    entity_count_grid->ClearChildren();

    constexpr auto row_heading{0};
    constexpr auto first_team_column{1};
    constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
    constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};

    entity_count_grid->SetColumnFill(0, 1.f);
    for (int32 team{0}; team < n_teams; ++team) {
        entity_count_grid->SetColumnFill(first_team_column + team, 1.f);
    }

    auto add_text{[this](FString const& name,
                         int32 row,
                         int32 column,
                         int32 layer,
                         ETextJustify::Type const alignment) {
        auto* text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *name)};
        set_text_style(*text, alignment);
        auto* slot{entity_count_grid->AddChildToGrid(text, row, column)};
        slot->SetLayer(layer);
        return text;
    }};

    add_text(TEXT("entity_type_heading"), row_heading, 0, 1, entity_type_alignment)
        ->SetText(FText::FromString(TEXT("Entity")));
    for (int32 team{0}; team < n_teams; ++team) {
        auto const team_value{static_cast<ETestTeam>(team)};
        auto* heading{add_text(FString::Printf(TEXT("team_heading_%d"), team),
                               row_heading,
                               first_team_column + team,
                               1,
                               data_alignment)};
        heading->SetText(FText::FromString(ml::to_string_without_type_prefix(team_value)));

        auto* border{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), *FString::Printf(TEXT("team_border_%d"), team))};
        border->SetBrushColor(team_colours[team_value].CopyWithNewOpacity(0.3f));
        auto* border_slot{entity_count_grid->AddChildToGrid(border, 1, first_team_column + team)};
        border_slot->SetLayer(0);
        border_slot->SetRowSpan(n_types);
    }

    for (int32 type{0}; type < n_types; ++type) {
        auto const type_value{static_cast<ETestEntityType>(type)};
        auto const row{type + 1};
        add_text(FString::Printf(TEXT("entity_type_%d"), type), row, 0, 1, entity_type_alignment)
            ->SetText(FText::FromString(ml::get_entity_short_name(type_value)));
        for (int32 team{0}; team < n_teams; ++team) {
            auto* count{add_text(FString::Printf(TEXT("entity_count_%d_%d"), type, team),
                                 row,
                                 first_team_column + team,
                                 1,
                                 data_alignment)};
            count->SetText(FText::AsNumber(entity_counts[team][type]));
        }
    }
}
