#include "SpaceGamePresentation/presentation/widgets/TeamEntityTableWidget.h"

#include <SandboxCoreEngine/enums.h>

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/TextBlock.h>

void UTeamEntityTableWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    if (IsDesignTime()) {
        constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
        constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
        for (int32 team{0}; team < n_teams; ++team) {
            for (int32 type{0}; type < n_types; ++type) {
                values[team][type] = 10 + team + type;
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

auto UTeamEntityTableWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const content{Super::RebuildWidget()};
    return hud_style_ ? ml::ioj::make_hud_panel(hud_style_.GetValue(), content) : content;
}

void
    UTeamEntityTableWidget::set_entity_counts(FTestEntityRegistry::EntityCounts const& new_counts) {
    values = new_counts;
    rebuild_table();
}

void UTeamEntityTableWidget::set_team_kill_matrix(ml::ship_hud::FTeamKillMatrix const& new_matrix) {
    constexpr auto n_teams{ml::ship_hud::FTeamKillMatrix::team_count};
    constexpr auto n_types{ml::ship_hud::FTeamKillMatrix::entity_type_count};
    for (int32 team{0}; team < n_teams; ++team) {
        auto const team_value{static_cast<ETestTeam>(team)};
        for (int32 type{0}; type < n_types; ++type) {
            auto const type_value{static_cast<ETestEntityType>(type)};
            values[team][type] = new_matrix.get(team_value, type_value);
        }
    }

    rebuild_table();
}

void
    UTeamEntityTableWidget::set_team_colours(UTestTeamVisualData::FColourArray const& new_colours) {
    team_colours = new_colours;
    rebuild_table();
}

void UTeamEntityTableWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    rebuild_table();
}

void UTeamEntityTableWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    rebuild_table();
}

void UTeamEntityTableWidget::set_show_team_totals(bool const show_totals) {
    show_team_totals = show_totals;
    rebuild_table();
}

void UTeamEntityTableWidget::set_text_style(UTextBlock& text,
                                            ETextJustify::Type const alignment) const {
    if (hud_style_) {
        ml::ioj::apply_text_style(text, hud_style_->secondary_text);
    } else {
        auto font{text.GetFont()};
        font.Size = font_size;
        text.SetFont(font);
    }
    text.SetJustification(alignment);
}

void UTeamEntityTableWidget::rebuild_table() {
    if (!WidgetTree) {
        return;
    }

    if (!team_entity_grid) {
        team_entity_grid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(),
                                                                   TEXT("team_entity_grid"));
        WidgetTree->RootWidget = team_entity_grid;
    }

    team_entity_grid->ClearChildren();

    constexpr auto row_heading{0};
    constexpr auto first_team_column{1};
    constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
    constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
    auto const n_data_rows{n_types + (show_team_totals ? 1 : 0)};

    team_entity_grid->SetColumnFill(0, 1.f);
    for (int32 team{0}; team < n_teams; ++team) {
        team_entity_grid->SetColumnFill(first_team_column + team, 1.f);
    }

    auto add_text{[this](FString const& name,
                         int32 const row,
                         int32 const column,
                         int32 const layer,
                         ETextJustify::Type const alignment) {
        auto* text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *name)};
        set_text_style(*text, alignment);
        auto* slot{team_entity_grid->AddChildToGrid(text, row, column)};
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
        auto* const slot{team_entity_grid->AddChildToGrid(header_background, row_heading, 0)};
        slot->SetColumnSpan(first_team_column + n_teams);
        slot->SetLayer(0);
    }

    auto* const entity_heading{
        add_text(TEXT("entity_type_heading"), row_heading, 0, 1, entity_type_alignment)};
    entity_heading->SetText(INVTEXT("Entity"));
    if (hud_style_) {
        ml::ioj::apply_text_style(*entity_heading, hud_style_->caption_text);
    }
    for (int32 team{0}; team < n_teams; ++team) {
        auto const team_value{static_cast<ETestTeam>(team)};
        auto* heading{add_text(FString::Printf(TEXT("team_heading_%d"), team),
                               row_heading,
                               first_team_column + team,
                               1,
                               data_alignment)};
        heading->SetText(FText::FromString(ml::to_string_without_type_prefix(team_value)));
        if (hud_style_) {
            auto heading_style{hud_style_->caption_text};
            auto const base_colour{heading_style.ColorAndOpacity.GetSpecifiedColor()};
            auto const colour{FLinearColor::LerpUsingHSV(
                base_colour, team_colours[team_value], hud_style_->team_text_blend)};
            heading_style.SetColorAndOpacity(FSlateColor{colour});
            ml::ioj::apply_text_style(*heading, heading_style);
        }

        auto* border{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), *FString::Printf(TEXT("team_border_%d"), team))};
        auto const wash_opacity{hud_style_ ? hud_style_->team_wash_opacity : 0.3f};
        border->SetBrushColor(team_colours[team_value].CopyWithNewOpacity(wash_opacity));
        auto* border_slot{team_entity_grid->AddChildToGrid(border, 1, first_team_column + team)};
        border_slot->SetLayer(1);
        border_slot->SetRowSpan(n_data_rows);
    }

    for (int32 type{0}; type < n_types; ++type) {
        auto const type_value{static_cast<ETestEntityType>(type)};
        auto const row{type + 1};
        if (hud_style_) {
            auto* const row_background{WidgetTree->ConstructWidget<UBorder>(
                UBorder::StaticClass(), *FString::Printf(TEXT("row_background_%d"), type))};
            row_background->SetBrush(type % 2 == 0 ? hud_style_->table_row_background
                                                   : hud_style_->table_alternate_row_background);
            auto* const row_slot{team_entity_grid->AddChildToGrid(row_background, row, 0)};
            row_slot->SetColumnSpan(first_team_column + n_teams);
            row_slot->SetLayer(0);
        }
        auto* const label{add_text(
            FString::Printf(TEXT("entity_type_%d"), type), row, 0, 1, entity_type_alignment)};
        label->SetText(FText::FromString(ml::get_entity_short_name(type_value)));
        if (hud_style_) {
            ml::ioj::apply_text_style(*label, hud_style_->caption_text);
        }
        for (int32 team{0}; team < n_teams; ++team) {
            auto* count{add_text(FString::Printf(TEXT("entity_value_%d_%d"), type, team),
                                 row,
                                 first_team_column + team,
                                 1,
                                 data_alignment)};
            count->SetText(FText::AsNumber(values[team][type]));
            if (hud_style_) {
                ml::ioj::apply_text_style(*count,
                                          values[team][type] == 0 ? hud_style_->caption_text
                                                                  : hud_style_->data_text);
            }
        }
    }

    if (!show_team_totals) {
        return;
    }

    auto const total_row{n_types + 1};
    if (hud_style_) {
        auto* const total_background{
            WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("total_background"))};
        total_background->SetBrush(hud_style_->table_header_background);
        auto* const total_slot{team_entity_grid->AddChildToGrid(total_background, total_row, 0)};
        total_slot->SetColumnSpan(first_team_column + n_teams);
        total_slot->SetLayer(0);
    }
    auto* const total_heading{
        add_text(TEXT("total_heading"), total_row, 0, 1, entity_type_alignment)};
    total_heading->SetText(INVTEXT("Total"));
    if (hud_style_) {
        ml::ioj::apply_text_style(*total_heading, hud_style_->data_accent_text);
    }
    for (int32 team{0}; team < n_teams; ++team) {
        int32 total{0};
        for (int32 type{0}; type < n_types; ++type) {
            total += values[team][type];
        }

        auto* const total_text{add_text(FString::Printf(TEXT("team_total_%d"), team),
                                        total_row,
                                        first_team_column + team,
                                        1,
                                        data_alignment)};
        total_text->SetText(FText::AsNumber(total));
        if (hud_style_) {
            ml::ioj::apply_text_style(*total_text, hud_style_->data_accent_text);
        }
    }
}
