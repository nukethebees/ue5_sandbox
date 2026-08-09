#include "Sandbox/ui/ship_hud/TeamKillMatrixWidget.h"

#include <Blueprint/WidgetTree.h>
#include <Components/Border.h>
#include <Components/GridPanel.h>
#include <Components/GridSlot.h>
#include <Components/TextBlock.h>

void UTeamKillMatrixWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    if (IsDesignTime()) {
        team_colours[ETestTeam::White] = FLinearColor::White;
        team_colours[ETestTeam::Red] = FLinearColor::Red;
        team_colours[ETestTeam::Green] = FLinearColor::Green;
        team_colours[ETestTeam::Blue] = FLinearColor::Blue;
        team_colours[ETestTeam::Orange] = FLinearColor(1.f, 0.35f, 0.f, 1.f);
        team_colours[ETestTeam::Yellow] = FLinearColor::Yellow;

        team_kill_matrix = {
            {ETestTeam::Green, {45, 7, 1, 21, 3}},
            {ETestTeam::Red, {39, 12, 0, 18, 2}},
        };
    }

    rebuild_table();
}

void UTeamKillMatrixWidget::set_team_kill_matrix(
    TConstArrayView<FTeamKillMatrixRow> const new_rows) {
    team_kill_matrix.Reset(new_rows.Num());

    auto const n_rows{new_rows.Num()};
    for (int32 row_index{0}; row_index < n_rows; ++row_index) {
        team_kill_matrix.Add(new_rows[row_index]);
    }

    rebuild_table();
}

void UTeamKillMatrixWidget::set_team_colours(UTestTeamVisualData::FColourArray const& new_colours) {
    team_colours = new_colours;
    rebuild_table();
}

void UTeamKillMatrixWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    rebuild_table();
}

void UTeamKillMatrixWidget::set_text_style(UTextBlock& text,
                                           ETextJustify::Type const alignment) const {
    auto font{text.GetFont()};
    font.Size = font_size;
    text.SetFont(font);
    text.SetJustification(alignment);
}

void UTeamKillMatrixWidget::rebuild_table() {
    if (!WidgetTree) {
        return;
    }

    if (!team_kill_matrix_grid) {
        team_kill_matrix_grid = WidgetTree->ConstructWidget<UGridPanel>(
            UGridPanel::StaticClass(), TEXT("team_kill_matrix_grid"));
        WidgetTree->RootWidget = team_kill_matrix_grid;
    }

    team_kill_matrix_grid->ClearChildren();

    constexpr auto row_heading{0};
    constexpr auto column_team{0};
    constexpr auto first_victim_type_column{1};
    constexpr auto n_types{FTeamKillMatrixRow::entity_type_count};
    auto const column_total{first_victim_type_column + n_types};

    for (int32 column{column_team}; column <= column_total; ++column) {
        team_kill_matrix_grid->SetColumnFill(column, 1.f);
    }

    auto add_text{[this](FString const& name,
                         int32 const row,
                         int32 const column,
                         int32 const layer,
                         ETextJustify::Type const alignment) {
        auto* text{WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *name)};
        set_text_style(*text, alignment);
        auto* slot{team_kill_matrix_grid->AddChildToGrid(text, row, column)};
        slot->SetLayer(layer);
        return text;
    }};

    add_text(TEXT("team_heading"), row_heading, column_team, 1, team_alignment)
        ->SetText(INVTEXT("Team"));
    for (int32 type_index{0}; type_index < n_types; ++type_index) {
        auto const type{static_cast<ETestEntityType>(type_index)};
        add_text(FString::Printf(TEXT("victim_type_heading_%d"), type_index),
                 row_heading,
                 first_victim_type_column + type_index,
                 1,
                 data_alignment)
            ->SetText(FText::FromString(ml::get_entity_short_name(type)));
    }
    add_text(TEXT("total_heading"), row_heading, column_total, 1, data_alignment)
        ->SetText(INVTEXT("Total"));

    auto const n_rows{team_kill_matrix.Num()};
    for (int32 row_index{0}; row_index < n_rows; ++row_index) {
        auto const& row_data{team_kill_matrix[row_index]};
        auto const row{row_index + 1};

        add_text(FString::Printf(TEXT("team_%d"), row_index), row, column_team, 1, team_alignment)
            ->SetText(FText::FromString(ml::to_string_without_type_prefix(row_data.killer_team)));

        int32 total_kills{0};
        for (int32 type_index{0}; type_index < n_types; ++type_index) {
            auto const kills{row_data.kills_by_victim_type[type_index]};
            total_kills += kills;
            add_text(FString::Printf(TEXT("kill_count_%d_%d"), row_index, type_index),
                     row,
                     first_victim_type_column + type_index,
                     1,
                     data_alignment)
                ->SetText(FText::AsNumber(kills));
        }
        add_text(FString::Printf(TEXT("total_%d"), row_index), row, column_total, 1, data_alignment)
            ->SetText(FText::AsNumber(total_kills));

        auto* border{WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), *FString::Printf(TEXT("team_border_%d"), row_index))};
        border->SetBrushColor(team_colours[row_data.killer_team].CopyWithNewOpacity(0.3f));
        auto* border_slot{team_kill_matrix_grid->AddChildToGrid(border, row, column_team)};
        border_slot->SetLayer(0);
    }
}
