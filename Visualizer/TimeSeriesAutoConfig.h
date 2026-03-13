#pragma once
#include "../DataManager/TimeSeriesRegistry.h"
#include "TimeSeriesPlotTemplate.h"
#include <string>
#include <vector>

namespace Visualizer {

    class TimeSeriesAutoConfig {
    public:
        // Generate plot configuration from registry
        static TimeSeriesPlotConfig GeneratePlotConfig(
            const DataManager::PlotWindowDefinition& plotDef);

        // Show all registered plot windows
        static void ShowAllRegisteredPlots(int maxMonths);

        // Show dynamic plot creator dialog
        static void ShowPlotCreatorDialog();

        // Show series selector for dynamic plots
        static void ShowSeriesSelector(const std::string& label,
            std::vector<std::string>& selectedSeries);
        static bool s_showPlotCreator;

    private:
        static char s_newPlotName[256];
        static std::vector<std::string> s_selectedSeries;
    };

} // namespace Visualizer