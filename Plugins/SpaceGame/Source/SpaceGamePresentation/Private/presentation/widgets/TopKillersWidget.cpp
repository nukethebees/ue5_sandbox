#include "SpaceGamePresentation/presentation/widgets/TopKillersWidget.h"

#include <SandboxCoreEngine/enums.h>

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

auto UTopKillersWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const content{Super::RebuildWidget()};
    return hud_style_ ? ml::ioj::make_hud_panel(hud_style_.GetValue(), content) : content;
}

void UTopKillersWidget::set_top_killers(ml::ship_hud::FTopKillerEntries const& new_entries) {
    top_killers = new_entries;

    rebuild_table();
}

void UTopKillersWidget::set_team_colours(UTestTeamVisualData::FColourArray const& new_colours) {
    team_colours = new_colours;
    rebuild_table();
}

void UTopKillersWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    rebuild_table();
}

void UTopKillersWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    rebuild_table();
}

void UTopKillersWidget::set_text_style(UTextBlock& text, ETextJustify::Type const alignment) const {
    if (hud_style_) {
        ml::ioj::apply_text_style(text, hud_style_->secondary_text);
    } else {
        auto font{text.GetFont()};
        font.Size = font_size;
        text.SetFont(font);
    }
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
        slot->SetLayer(hud_style_ ? layer + 1 : layer);
        if (hud_style_) {
            slot->SetPadding(hud_style_->table_cell_padding);
        }
        return text;
    }};

    if (hud_style_) {
        auto* const header_background{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), TEXT("header_background"))};
        header_background->SetBrush(hud_style_->table_header_background);
        auto* const slot{top_killers_grid->AddChildToGrid(header_background, row_heading, 0)};
        slot->SetColumnSpan(column_kills + 1);
        slot->SetLayer(0);
    }

    auto add_heading{[&](FString const& name,
                         int32 const column,
                         ETextJustify::Type const alignment,
                         FText text) {
        auto* const heading{add_text(name, row_heading, column, 1, alignment)};
        heading->SetText(MoveTemp(text));
        if (hud_style_) {
            ml::ioj::apply_text_style(*heading, hud_style_->caption_text);
        }
    }};
    add_heading(TEXT("rank_heading"), column_rank, data_alignment, INVTEXT("Rank"));
    add_heading(TEXT("entity_heading"), column_entity, entity_alignment, INVTEXT("Entity"));
    add_heading(TEXT("type_heading"), column_type, entity_alignment, INVTEXT("Type"));
    add_heading(TEXT("team_heading"), column_team, data_alignment, INVTEXT("Team"));
    add_heading(TEXT("kills_heading"), column_kills, data_alignment, INVTEXT("Kills"));

    auto const n_entries{FMath::Min(top_killers.num(), max_entries)};
    for (int32 entry_index{0}; entry_index < n_entries; ++entry_index) {
        auto const row{entry_index + 1};

        if (hud_style_) {
            auto* const row_background{WidgetTree->ConstructWidget<UBorder>(
                UBorder::StaticClass(), *FString::Printf(TEXT("row_background_%d"), entry_index))};
            row_background->SetBrush(entry_index % 2 == 0
                                         ? hud_style_->table_row_background
                                         : hud_style_->table_alternate_row_background);
            auto* const row_slot{top_killers_grid->AddChildToGrid(row_background, row, 0)};
            row_slot->SetColumnSpan(column_kills + 1);
            row_slot->SetLayer(0);
        }

        auto add_value{[&](FString const& name,
                           int32 const column,
                           ETextJustify::Type const alignment,
                           FText text) -> UTextBlock* {
            auto* const value{add_text(name, row, column, 1, alignment)};
            value->SetText(MoveTemp(text));
            if (hud_style_) {
                ml::ioj::apply_text_style(*value, hud_style_->data_text);
            }
            return value;
        }};
        add_value(FString::Printf(TEXT("rank_%d"), entry_index),
                  column_rank,
                  data_alignment,
                  FText::AsNumber(entry_index + 1));
        add_value(FString::Printf(TEXT("entity_%d"), entry_index),
                  column_entity,
                  entity_alignment,
                  FText::Format(INVTEXT("{0} {1}"),
                                FText::FromString(ml::get_entity_class_name(
                                    top_killers.entity_types[entry_index])),
                                top_killers.entity_ids[entry_index].id));
        add_value(
            FString::Printf(TEXT("type_%d"), entry_index),
            column_type,
            entity_alignment,
            FText::FromString(ml::get_entity_display_name(top_killers.entity_types[entry_index])));
        auto* const team_text{add_value(
            FString::Printf(TEXT("team_%d"), entry_index),
            column_team,
            data_alignment,
            FText::FromString(ml::to_string_without_type_prefix(top_killers.teams[entry_index])))};
        add_value(FString::Printf(TEXT("kills_%d"), entry_index),
                  column_kills,
                  data_alignment,
                  FText::AsNumber(top_killers.kills[entry_index]));

        auto* border{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), *FString::Printf(TEXT("team_border_%d"), entry_index))};
        auto const team_colour{team_colours[top_killers.teams[entry_index]]};
        auto const wash_opacity{hud_style_ ? hud_style_->team_wash_opacity : 0.3f};
        border->SetBrushColor(team_colour.CopyWithNewOpacity(wash_opacity));
        auto* border_slot{top_killers_grid->AddChildToGrid(border, row, column_team)};
        border_slot->SetLayer(1);

        if (hud_style_) {
            auto team_text_style{hud_style_->data_text};
            team_text_style.SetColorAndOpacity(FLinearColor::LerpUsingHSV(
                hud_style_->data_text.ColorAndOpacity.GetSpecifiedColor(),
                team_colour,
                hud_style_->team_text_blend));
            ml::ioj::apply_text_style(*team_text, team_text_style);
        }
    }
}
