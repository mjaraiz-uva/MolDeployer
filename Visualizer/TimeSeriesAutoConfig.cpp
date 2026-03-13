#include "TimeSeriesAutoConfig.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <cstring>

namespace Visualizer {

    // Static member definitions
    bool TimeSeriesAutoConfig::s_showPlotCreator = false;
    char TimeSeriesAutoConfig::s_newPlotName[256] = "New Plot";
    std::vector<std::string> TimeSeriesAutoConfig::s_selectedSeries;

    TimeSeriesPlotConfig TimeSeriesAutoConfig::GeneratePlotConfig(
        const DataManager::PlotWindowDefinition& plotDef) {

        TimeSeriesPlotConfig config;
        config.windowName = plotDef.windowName;
        config.plotTitle = plotDef.plotTitle;
        config.xAxisLabel = "Timestep";
        config.yAxisLabel = "Value";
        config.legendLocation = plotDef.legendLocation;
        config.legendOutside = plotDef.legendOutside;
        config.showControls = plotDef.showControls;
        config.ticksEvery = plotDef.ticksEvery;
        config.allowSeriesToggle = true;
        config.showTooltips = true;

        // Add series from registry
        auto& registry = DataManager::TimeSeriesRegistry::GetInstance();

        // Collect all unique y-axis labels to determine if we should use a common one
        std::vector<std::string> yAxisLabels;

        for (const auto& seriesId : plotDef.seriesIds) {
            auto seriesDef = registry.GetSeriesDefinition(seriesId);
            if (seriesDef) {
                TimeSeriesConfig series;
                series.name = seriesDef->displayName;
                series.data = &seriesDef->data;
                series.marker = seriesDef->marker;
                series.markerSize = seriesDef->markerSize;
                series.color = seriesDef->color;
                config.series.push_back(series);

                // Collect y-axis labels
                if (std::find(yAxisLabels.begin(), yAxisLabels.end(), seriesDef->yAxisLabel) == yAxisLabels.end()) {
                    yAxisLabels.push_back(seriesDef->yAxisLabel);
                }
            }
            else {
                Logger::Warning("TimeSeriesAutoConfig: Series '" + seriesId + "' not found in registry");
            }
        }

        // Set y-axis label based on collected labels
        if (yAxisLabels.size() == 1) {
            config.yAxisLabel = yAxisLabels[0];
        }
        else if (yAxisLabels.size() > 1) {
            // Multiple different labels, use generic "Value"
            config.yAxisLabel = "Value";
        }

        return config;
    }

    void TimeSeriesAutoConfig::ShowAllRegisteredPlots(int maxMonths) {
        auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
        auto plotWindows = registry.GetRegisteredPlotWindows();
        static bool firstRun = true;
        if (firstRun) {
            Logger::Info("TimeSeriesAutoConfig: Showing " + std::to_string(plotWindows.size()) + " registered plot windows");
            firstRun = false;
        }

        // Get global plot settings from interface
        bool showLine = true;
        bool showMarkers = false;
        float markerSize = 2.0f;

        auto& plotManager = TimeSeriesPlotManager::GetInstance();
        for (const auto& plotDef : plotWindows) {
            // REMOVED: Logger::Debug line that was spamming the console
            TimeSeriesPlotConfig config = GeneratePlotConfig(plotDef);
            if (!config.series.empty()) {
                auto& plotState = plotManager.GetState(config.windowName);
                ShowMultiSeriesPlot(config, maxMonths,
                    plotState.showLine,
                    plotState.showMarkers,
                    static_cast<float>(plotState.markerSize));
            }
        }
    }

    void TimeSeriesAutoConfig::ShowPlotCreatorDialog() {
        if (!s_showPlotCreator) return;

        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("Create Custom Plot", &s_showPlotCreator, ImGuiWindowFlags_NoCollapse);

        // Plot name input
        ImGui::Text("Plot Configuration");
        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Plot Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##PlotName", s_newPlotName, sizeof(s_newPlotName));

        // Validation for plot name
        bool validName = strlen(s_newPlotName) > 0;
        if (!validName) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Name required!");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Select Time Series to Display:");
        ImGui::Spacing();

        // Series selector
        ShowSeriesSelector("##series_selector", s_selectedSeries);

        // Show selected count
        ImGui::Text("Selected series: %d", static_cast<int>(s_selectedSeries.size()));

        if (s_selectedSeries.empty()) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Select at least one time series!");
        }

        ImGui::Separator();

        // Action buttons
        bool canCreate = validName && !s_selectedSeries.empty();

        if (!canCreate) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create Plot", ImVec2(120, 0))) {
            // Create the plot definition
            DataManager::PlotWindowDefinition def;
            def.windowName = s_newPlotName;
            def.plotTitle = std::string("##") + s_newPlotName;
            def.seriesIds = s_selectedSeries;
            def.legendLocation = ImPlotLocation_NorthWest;
            def.legendOutside = true;
            def.ticksEvery = 12;
            def.showControls = true;

            auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
            registry.AddDynamicPlotWindow(def);

            Logger::Info("Created custom plot: " + def.windowName + " with " +
                std::to_string(s_selectedSeries.size()) + " series");

            // Reset dialog
            strcpy_s(s_newPlotName, sizeof(s_newPlotName), "New Plot");
            s_selectedSeries.clear();
            s_showPlotCreator = false;
        }

        if (!canCreate) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            s_showPlotCreator = false;
        }

        ImGui::End();
    }

    void TimeSeriesAutoConfig::ShowSeriesSelector(const std::string& label,
        std::vector<std::string>& selectedSeries) {
        auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
        auto availableSeries = registry.GetAvailableSeriesIds();

        // Create a child window for the list to have a border and scrollbar
        ImGui::BeginChild("SeriesListFrame", ImVec2(-1, 250), true);

        // Add "Select All" and "Clear All" buttons
        if (ImGui::Button("Select All")) {
            selectedSeries.clear();
            for (const auto& seriesId : availableSeries) {
                selectedSeries.push_back(seriesId);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) {
            selectedSeries.clear();
        }

        ImGui::Separator();

        // List all available series
        for (const auto& seriesId : availableSeries) {
            bool isSelected = std::find(selectedSeries.begin(),
                selectedSeries.end(),
                seriesId) != selectedSeries.end();

            auto seriesDef = registry.GetSeriesDefinition(seriesId);
            if (!seriesDef) continue;

            // Create a selectable with custom styling
            ImGui::PushID(seriesId.c_str());

            // Show a colored rectangle representing the series color
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            // Draw color indicator
            ImVec4 color = seriesDef->color;
            draw_list->AddRectFilled(p, ImVec2(p.x + 12, p.y + 12),
                ImColor(color.x, color.y, color.z, color.w));
            draw_list->AddRect(p, ImVec2(p.x + 12, p.y + 12),
                ImColor(0.5f, 0.5f, 0.5f, 1.0f));

            ImGui::Dummy(ImVec2(12, 12));
            ImGui::SameLine();

            // Create selectable item
            std::string displayText = seriesDef->displayName + " (" + seriesDef->yAxisLabel + ")";
            if (ImGui::Selectable(displayText.c_str(), isSelected)) {
                if (isSelected) {
                    // Remove from selection
                    selectedSeries.erase(
                        std::remove(selectedSeries.begin(),
                            selectedSeries.end(), seriesId),
                        selectedSeries.end());
                }
                else {
                    // Add to selection
                    selectedSeries.push_back(seriesId);
                }
            }

            // Tooltip with more information
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("ID: %s", seriesId.c_str());
                ImGui::Text("Display Name: %s", seriesDef->displayName.c_str());
                ImGui::Text("Y-Axis Label: %s", seriesDef->yAxisLabel.c_str());
                if (!seriesDef->unit.empty()) {
                    ImGui::Text("Unit: %s", seriesDef->unit.c_str());
                }
                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    // Public method to show the plot creator (can be called from menu)
    void ShowPlotCreator() {
        Visualizer::TimeSeriesAutoConfig::s_showPlotCreator = true;
    }

    // Helper method to remove a dynamic plot
    void RemoveDynamicPlot(const std::string& windowName) {
        auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
        registry.RemoveDynamicPlotWindow(windowName);
        Logger::Info("Removed dynamic plot: " + windowName);
    }

    // Method to export plot configuration to JSON
    void ExportPlotConfiguration(const std::string& filename) {
        auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
        auto plotWindows = registry.GetRegisteredPlotWindows();

        // Create JSON representation
        nlohmann::json plotsJson;
        plotsJson["plots"] = nlohmann::json::array();

        for (const auto& plot : plotWindows) {
            nlohmann::json plotJson;
            plotJson["windowName"] = plot.windowName;
            plotJson["plotTitle"] = plot.plotTitle;
            plotJson["seriesIds"] = plot.seriesIds;
            plotJson["legendLocation"] = static_cast<int>(plot.legendLocation);
            plotJson["legendOutside"] = plot.legendOutside;
            plotJson["ticksEvery"] = plot.ticksEvery;
            plotJson["showControls"] = plot.showControls;
            plotsJson["plots"].push_back(plotJson);
        }

        // Write to file
        std::ofstream file(filename);
        if (file.is_open()) {
            file << plotsJson.dump(4);
            file.close();
            Logger::Info("Exported plot configuration to: " + filename);
        }
        else {
            Logger::Error("Failed to export plot configuration to: " + filename);
        }
    }

    // Method to import plot configuration from JSON
    void ImportPlotConfiguration(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            Logger::Error("Failed to open plot configuration file: " + filename);
            return;
        }

        try {
            nlohmann::json plotsJson;
            file >> plotsJson;
            file.close();

            if (plotsJson.contains("plots") && plotsJson["plots"].is_array()) {
                auto& registry = DataManager::TimeSeriesRegistry::GetInstance();

                for (const auto& plotJson : plotsJson["plots"]) {
                    DataManager::PlotWindowDefinition def;
                    def.windowName = plotJson["windowName"].get<std::string>();
                    def.plotTitle = plotJson["plotTitle"].get<std::string>();
                    def.seriesIds = plotJson["seriesIds"].get<std::vector<std::string>>();
                    def.legendLocation = static_cast<ImPlotLocation>(plotJson["legendLocation"].get<int>());
                    def.legendOutside = plotJson["legendOutside"].get<bool>();
                    def.ticksEvery = plotJson["ticksEvery"].get<int>();
                    def.showControls = plotJson["showControls"].get<bool>();

                    registry.AddDynamicPlotWindow(def);
                }

                Logger::Info("Imported plot configuration from: " + filename);
            }
        }
        catch (const std::exception& e) {
            Logger::Error("Failed to parse plot configuration file: " + std::string(e.what()));
        }
    }

} // namespace Visualizer