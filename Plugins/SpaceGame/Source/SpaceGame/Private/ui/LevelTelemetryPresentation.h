#pragma once

#include "ioj/sim/level_telemetry_snapshot.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

class SGraphPlot;

namespace ml::ioj::level_telemetry_presentation {
auto format_elapsed_time(double elapsed_seconds) -> FText;

void apply_activity_graph_style(SGraphPlot& graph,
                                FGameUiStyle const& style,
                                FText empty_text,
                                FVector2f desired_size);

void update_activity_graph(SGraphPlot& graph,
                           ::ioj::sim::LevelTelemetrySnapshot const& snapshot,
                           FLinearColor active_entity_color,
                           FLinearColor kills_color);
}
