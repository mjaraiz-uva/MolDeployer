#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <implot.h>

namespace DataManager {

    struct TimeSeriesDefinition {
        std::string id;                              // Unique identifier
        std::string displayName;                     // Display name for UI
        std::string yAxisLabel;                      // Y-axis label
        std::function<float()> valueGetter;          // Function to get current value
        std::vector<float> data;                     // Stored data

        // Plot configuration
        ImVec4 color;
        ImPlotMarker marker;
        float markerSize;
        bool enabledByDefault;

        // Optional: unit string for tooltips
        std::string unit;
    };

    struct PlotWindowDefinition {
        std::string windowName;
        std::string plotTitle;
        std::vector<std::string> seriesIds;
        ImPlotLocation legendLocation;
        bool legendOutside;
        int ticksEvery;
        bool showControls;
    };

    class TimeSeriesRegistry {
    public:
        static TimeSeriesRegistry& GetInstance();

        // Registration
        void RegisterTimeSeries(const TimeSeriesDefinition& def);
        void RegisterPlotWindow(const PlotWindowDefinition& def);

        // Data management
        void UpdateAllSeries(int currentMonth);
        void ClearAllSeries();
        void InitializeAllSeries(int maxMonths);
        void ExtendAllSeries(int newMaxSteps);

        // Accessors
        const std::vector<float>* GetSeriesData(const std::string& id) const;
        TimeSeriesDefinition* GetSeriesDefinition(const std::string& id);
        std::vector<std::string> GetAvailableSeriesIds() const;
        std::vector<PlotWindowDefinition> GetRegisteredPlotWindows() const;

        // Dynamic plot management
        void AddDynamicPlotWindow(const PlotWindowDefinition& def);
        void RemoveDynamicPlotWindow(const std::string& windowName);

        // Snapshot persistence: save/load all series data up to a given step
        nlohmann::json SaveAllSeriesData(int upToStep) const;
        void LoadAllSeriesData(const nlohmann::json& data);

    private:
        std::unordered_map<std::string, TimeSeriesDefinition> m_series;
        std::vector<PlotWindowDefinition> m_plotWindows;
        std::vector<PlotWindowDefinition> m_dynamicPlotWindows;
        TimeSeriesRegistry() = default;
    };

} // namespace DataManager