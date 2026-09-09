#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGameSimulation/simulation/LevelTelemetrySnapshot.h"

class SGraphPlot;

namespace ml::ioj::level_telemetry_presentation {
auto format_elapsed_time(double elapsed_seconds) -> FText;

void apply_activity_graph_style(SGraphPlot& graph,
                                FGameUiStyle const& style,
                                FText empty_text,
                                FVector2f desired_size);

void update_activity_graph(SGraphPlot& graph,
                           FLevelTelemetrySnapshot const& snapshot,
                           FLinearColor active_entity_color,
                           FLinearColor kills_color);
}
