#pragma once

#include <imgui.h>
#include <implot.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Visualizer {

// Structure to hold configuration for a single time series within a plot
struct TimeSeriesConfig {
    std::string name;                          // Display name for the series in the legend
    const std::vector<float>* data;            // Pointer to the data vector
    ImPlotMarker marker;                       // Marker style (e.g., ImPlotMarker_Circle)
    float markerSize;                          // Size of marker
    ImVec4 color;                              // Optional custom color (or ImVec4{0,0,0,0} for auto)
    bool visible;                              // Whether the series is visible
    
    TimeSeriesConfig() : 
        name("Series"), 
        data(nullptr),
        marker(ImPlotMarker_Circle),
        markerSize(2.0f),
        color(0, 0, 0, 0), // Default to auto color
        visible(true) {}
    
    TimeSeriesConfig(
        const std::string& seriesName,
        const std::vector<float>* seriesData,
        ImPlotMarker seriesMarker = ImPlotMarker_Circle,
        float seriesMarkerSize = 2.0f,
        ImVec4 seriesColor = ImVec4(0, 0, 0, 0),  // Default to auto color
        bool seriesVisible = true
    ) :
        name(seriesName),
        data(seriesData),
        marker(seriesMarker),
        markerSize(seriesMarkerSize),
        color(seriesColor),
        visible(seriesVisible)
    {}
};

// Structure to hold configuration for an entire plot window
struct TimeSeriesPlotConfig {
    std::string windowName;                    // Title of the ImGui window
    std::string plotTitle;                     // Title of the plot itself
    std::string xAxisLabel;                    // X-axis label (usually "Month")
    std::string yAxisLabel;                    // Y-axis label (e.g., "Value" or "Amount")
    std::vector<TimeSeriesConfig> series;      // All series to be displayed in this plot
    ImPlotLocation legendLocation;             // Location of the legend
    bool legendOutside;                        // Whether legend is outside the plot
    bool showControls;                         // Show line/marker style controls
    int ticksEvery;                            // Place X-axis ticks every N months (default 12)
    bool allowSeriesToggle;                    // Allow toggling series visibility
    bool showTooltips;                         // Show tooltips on hover
    ImPlotAxisFlags xAxisFlags;                // X-axis flags
    ImPlotAxisFlags yAxisFlags;                // Y-axis flags
    
    TimeSeriesPlotConfig() : 
        windowName("Time Series Plot"),
        plotTitle("##TimeSeries"), // Default to no visible title (## prefix hides it)
        xAxisLabel("Timestep"),
        yAxisLabel("Value"),
        legendLocation(ImPlotLocation_NorthWest),
        legendOutside(true),
        showControls(false),
        ticksEvery(12),
        allowSeriesToggle(false),
        showTooltips(true),
        xAxisFlags(ImPlotAxisFlags_None),
        yAxisFlags(ImPlotAxisFlags_AutoFit) {}

    // Helper method to add a series to this plot configuration
    void AddSeries(const TimeSeriesConfig& seriesConfig) {
        series.push_back(seriesConfig);
    }

    // Helper method to add a simple series with default styling
    void AddSeries(const std::string& name, const std::vector<float>* data, ImPlotMarker marker = ImPlotMarker_Circle) {
        TimeSeriesConfig seriesConfig;
        seriesConfig.name = name;
        seriesConfig.data = data;
        seriesConfig.marker = marker;
        series.push_back(seriesConfig);
    }
};

// Persistence state for a specific plot
struct PlotPersistentState {
    ImPlotLocation legendLocation = ImPlotLocation_NorthWest;  // Default legend location
    bool legendOutside = false;                               // Default legend outside status
    bool showLine = true;                                     // Default to show lines
    bool showMarkers = false;                                 // Default off to avoid 16-bit vertex overflow
    int markerSize = 2;                                       // Default marker size (0=auto, 1-5 for specific sizes)
};

// Plot manager to track and persist state across multiple plots
class TimeSeriesPlotManager {
public:
    // Get persistent state for a plot by name
    PlotPersistentState& GetState(const std::string& plotName);
    
    // Register a plot configuration for persistence
    void RegisterPlotConfig(const std::string& plotName, 
                            ImPlotLocation defaultLocation, 
                            bool defaultOutside);

    // Save persistent states to ini file (called via ImGui handler)
    void SaveToIni(ImGuiTextBuffer* buf, const char* handlerTypeName);
    
    // Load persistent states from ini file (called via ImGui handlers)
    void* GetStateForPlotName(const char* name);
    void LoadStateFromIni(void* entry, const char* line);
    // In Visualizer\TimeSeriesPlotTemplate.h, inside TimeSeriesPlotManager class
    void LogCurrentStates(); // For debugging - logs all current plot states
    // Export plot states to DataManager configuration
    void ExportPlotStatesToConfig();
    
    // Import plot states from DataManager configuration
    void ImportPlotStatesFromConfig();

    // Get singleton instance
    static TimeSeriesPlotManager& GetInstance();

private:
    std::unordered_map<std::string, PlotPersistentState> m_plotStates;
    TimeSeriesPlotManager() = default;
};

// Initialize ImGui settings handler for plot states
bool InitPlotStateHandler();

// Core function to show a plot with multiple time series
void ShowMultiSeriesPlot(
    const TimeSeriesPlotConfig& config,
    int maxMonths,
    bool showLine = true,
    bool showMarkers = true,
    float markerSize = 2.0f
);

// Utility function to create a color with specified hue (keeping saturation and value consistent)
inline ImVec4 ColorFromHue(float hue) {
    // Convert HSV to RGB with fixed saturation and value
    float h = hue;
    float s = 0.8f;
    float v = 0.8f;
    
    // HSV to RGB conversion
    int hi = static_cast<int>(h / 60.0f) % 6;
    float f = h / 60.0f - static_cast<float>(hi);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (hi) {
        case 0: return ImVec4(v, t, p, 1.0f);
        case 1: return ImVec4(q, v, p, 1.0f);
        case 2: return ImVec4(p, v, t, 1.0f);
        case 3: return ImVec4(p, q, v, 1.0f);
        case 4: return ImVec4(t, p, v, 1.0f);
        default: return ImVec4(v, p, q, 1.0f);
    }
}

} // namespace Visualizer