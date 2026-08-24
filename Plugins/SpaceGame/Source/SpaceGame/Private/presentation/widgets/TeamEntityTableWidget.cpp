#include "SpaceGame/presentation/widgets/TeamEntityTableWidget.h"

#include <SandboxGameShared/utilities/enums.h>

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
    auto font{text.GetFont()};
    font.Size = font_size;
    text.SetFont(font);
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
        slot->SetLayer(layer);
        return text;
    }};

    add_text(TEXT("entity_type_heading"), row_heading, 0, 1, entity_type_alignment)
        ->SetText(INVTEXT("Entity"));
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
        auto* border_slot{team_entity_grid->AddChildToGrid(border, 1, first_team_column + team)};
        border_slot->SetLayer(0);
        border_slot->SetRowSpan(n_data_rows);
    }

    for (int32 type{0}; type < n_types; ++type) {
        auto const type_value{static_cast<ETestEntityType>(type)};
        auto const row{type + 1};
        add_text(FString::Printf(TEXT("entity_type_%d"), type), row, 0, 1, entity_type_alignment)
            ->SetText(FText::FromString(ml::get_entity_short_name(type_value)));
        for (int32 team{0}; team < n_teams; ++team) {
            auto* count{add_text(FString::Printf(TEXT("entity_value_%d_%d"), type, team),
                                 row,
                                 first_team_column + team,
                                 1,
                                 data_alignment)};
            count->SetText(FText::AsNumber(values[team][type]));
        }
    }

    if (!show_team_totals) {
        return;
    }

    auto const total_row{n_types + 1};
    add_text(TEXT("total_heading"), total_row, 0, 1, entity_type_alignment)
        ->SetText(INVTEXT("Total"));
    for (int32 team{0}; team < n_teams; ++team) {
        int32 total{0};
        for (int32 type{0}; type < n_types; ++type) {
            total += values[team][type];
        }

        add_text(FString::Printf(TEXT("team_total_%d"), team),
                 total_row,
                 first_team_column + team,
                 1,
                 data_alignment)
            ->SetText(FText::AsNumber(total));
    }
}
