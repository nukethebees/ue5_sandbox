#include "SpaceGame/presentation/widgets/TopKillersWidget.h"

#include <SandboxGameShared/utilities/enums.h>

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/TextBlock.h>

void UTopKillersWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    if (IsDesignTime()) {
        team_colours[ETestTeam::White] = FLinearColor::White;
        team_colours[ETestTeam::Red] = FLinearColor::Red;
        team_colours[ETestTeam::Green] = FLinearColor::Green;
        team_colours[ETestTeam::Blue] = FLinearColor::Blue;
        team_colours[ETestTeam::Orange] = FLinearColor(1.f, 0.35f, 0.f, 1.f);
        team_colours[ETestTeam::Yellow] = FLinearColor::Yellow;

        top_killers.reset();
        top_killers.add({.id = 14}, ETestEntityType::CapitalShipFighter, ETestTeam::Green, 18);
        top_killers.add({.id = 7}, ETestEntityType::Turret, ETestTeam::Red, 13);
        top_killers.add({.id = 2}, ETestEntityType::CapitalShip, ETestTeam::Blue, 9);
    }

    rebuild_table();
}

void UTopKillersWidget::set_top_killers(ml::ship_hud::FTopKillerEntries const& new_entries) {
    top_killers = new_entries;

    rebuild_table();
}

void UTopKillersWidget::set_team_colours(UTestTeamVisualData::FColourArray const& new_colours) {
    team_colours = new_colours;
    rebuild_table();
}

void UTopKillersWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    rebuild_table();
}

void UTopKillersWidget::set_text_style(UTextBlock& text, ETextJustify::Type const alignment) const {
    auto font{text.GetFont()};
    font.Size = font_size;
    text.SetFont(font);
    text.SetJustification(alignment);
}

void UTopKillersWidget::rebuild_table() {
    if (!WidgetTree) {
        return;
    }

    if (!top_killers_grid) {
        top_killers_grid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(),
                                                                   TEXT("top_killers_grid"));
        WidgetTree->RootWidget = top_killers_grid;
    }

    top_killers_grid->ClearChildren();

    constexpr auto row_heading{0};
    constexpr auto column_rank{0};
    constexpr auto column_entity{1};
    constexpr auto column_type{2};
    constexpr auto column_team{3};
    constexpr auto column_kills{4};

    for (int32 column{column_rank}; column <= column_kills; ++column) {
        top_killers_grid->SetColumnFill(column, 1.f);
    }

    auto add_text{[this](FString const& name,
                         int32 const row,
                         int32 const column,
                         int32 const layer,
                         ETextJustify::Type const alignment) {
        auto* text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *name)};
        set_text_style(*text, alignment);
        auto* slot{top_killers_grid->AddChildToGrid(text, row, column)};
        slot->SetLayer(layer);
        return text;
    }};

    add_text(TEXT("rank_heading"), row_heading, column_rank, 1, data_alignment)
        ->SetText(INVTEXT("Rank"));
    add_text(TEXT("entity_heading"), row_heading, column_entity, 1, entity_alignment)
        ->SetText(INVTEXT("Entity"));
    add_text(TEXT("type_heading"), row_heading, column_type, 1, entity_alignment)
        ->SetText(INVTEXT("Type"));
    add_text(TEXT("team_heading"), row_heading, column_team, 1, data_alignment)
        ->SetText(INVTEXT("Team"));
    add_text(TEXT("kills_heading"), row_heading, column_kills, 1, data_alignment)
        ->SetText(INVTEXT("Kills"));

    auto const n_entries{FMath::Min(top_killers.num(), max_entries)};
    for (int32 entry_index{0}; entry_index < n_entries; ++entry_index) {
        auto const row{entry_index + 1};

        add_text(FString::Printf(TEXT("rank_%d"), entry_index), row, column_rank, 1, data_alignment)
            ->SetText(FText::AsNumber(entry_index + 1));
        add_text(FString::Printf(TEXT("entity_%d"), entry_index),
                 row,
                 column_entity,
                 1,
                 entity_alignment)
            ->SetText(FText::Format(
                INVTEXT("{0} {1}"),
                FText::FromString(ml::get_entity_class_name(top_killers.entity_types[entry_index])),
                top_killers.entity_ids[entry_index].id));
        add_text(
            FString::Printf(TEXT("type_%d"), entry_index), row, column_type, 1, entity_alignment)
            ->SetText(FText::FromString(
                ml::get_entity_display_name(top_killers.entity_types[entry_index])));
        add_text(FString::Printf(TEXT("team_%d"), entry_index), row, column_team, 1, data_alignment)
            ->SetText(FText::FromString(
                ml::to_string_without_type_prefix(top_killers.teams[entry_index])));
        add_text(
            FString::Printf(TEXT("kills_%d"), entry_index), row, column_kills, 1, data_alignment)
            ->SetText(FText::AsNumber(top_killers.kills[entry_index]));

        auto* border{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), *FString::Printf(TEXT("team_border_%d"), entry_index))};
        border->SetBrushColor(
            team_colours[top_killers.teams[entry_index]].CopyWithNewOpacity(0.3f));
        auto* border_slot{top_killers_grid->AddChildToGrid(border, row, column_team)};
        border_slot->SetLayer(0);
    }
}
