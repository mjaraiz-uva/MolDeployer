#pragma once

// Forward declare ImDrawData to avoid including imgui.h in the public header if possible,
// though for rendering, it's often needed. For simplicity now, we'll include.
#include "imgui.h" // Provides ImDrawData
#include <vector> // For gdp_values
#include <string> // For window titles, labels etc.

// Include our new time series plot template definitions
#include "TimeSeriesPlotTemplate.h"

// Forward declaration for GLFWwindow
struct GLFWwindow;

namespace Visualizer {

    // Initialization and Shutdown
    bool Init(GLFWwindow* window); // New signature for GLFW

    void Shutdown();

    // Frame Management
    void NewFrame(); // Call at the start of your main loop's frame
    void Render();   // Call to render ImGui draw data

    // UI Component Drawing Functions
    // These functions will be called by InterfaceGUI or directly by DEPX initially
    void ShowMainControls(float& parameter_A, float framerate, float ms_per_frame, bool& should_recalculate_gdp);
    
    // Updated Economic Time Series without Commercial Bank data
    void ShowEconomicTimeSeries(
        const std::vector<float>& gdp_values, 
        const std::vector<float>& household_expenditure_values,
        int max_months
    );
    
    // New function for Commercial Bank metrics in a separate window
    void ShowCommercialBankPlot(
        const std::vector<float>& commercial_bank_deposit_values,
        const std::vector<float>& commercial_bank_loan_values,
        const std::vector<float>& commercial_bank_cash_values,
        const std::vector<float>& commercial_bank_reserves_values,
        int max_months,
        ImPlotLocation legend_loc = ImPlotLocation_NorthWest,
        bool legend_outside = false
    );
    
    // New helper function to create Economic Time Series using the template
    TimeSeriesPlotConfig CreateEconomicTimeSeriesConfig(
        const std::vector<float>& gdp_values,
        const std::vector<float>& household_expenditure_values
    );

    // New helper function to create Commercial Bank plot using the template
    TimeSeriesPlotConfig CreateCommercialBankPlotConfig(
        const std::vector<float>& commercial_bank_deposit_values,
        const std::vector<float>& commercial_bank_loan_values,
        const std::vector<float>& commercial_bank_cash_values,
        const std::vector<float>& commercial_bank_reserves_values
    );

    // New helper function to create Central Bank plot using the template
    TimeSeriesPlotConfig CreateCentralBankPlotConfig(
        const std::vector<float>& total_reserves_values,
        const std::vector<float>& total_money_values
    );

    TimeSeriesPlotConfig CreateWorkerMetricsPlotConfig(
        const std::vector<float>& worker_money_values,
        const std::vector<float>& worker_bank_balance_values,
        const std::vector<float>& worker_last_month_expenditure_values,
        int workerID
    );

    TimeSeriesPlotConfig CreateHouseholdWealthPlotConfig(
        const std::vector<float>& household_cash_values,
        const std::vector<float>& household_bank_balance_values,
        const std::vector<float>& household_wealth_values);

    TimeSeriesPlotConfig CreateUnemploymentPlotConfig(const std::vector<float>& unemployment_values);

    TimeSeriesPlotConfig CreateFirmMetricsPlotConfig(
        const std::vector<float>& firm_cash_values,
        const std::vector<float>& firm_bank_balance_values,
        const std::vector<float>& firm_revenue_values,
        const std::vector<float>& firm_production_values,
        int firmID
    );

    TimeSeriesPlotConfig CreateProductionMetricsPlotConfig(
        const std::vector<float>& total_assisted_production_values,
        const std::vector<float>& total_actual_production_values,
        const std::vector<float>& total_production_values
    );

    // New factory functions for creating various common plot types
    TimeSeriesPlotConfig CreateEmptyPlotConfig(const std::string& windowName, const std::string& yAxisLabel = "Value");
    
    // Generic function to create any custom plot from a collection of data series
    TimeSeriesPlotConfig CreateCustomPlotConfig(
        const std::string& windowName,
        const std::vector<std::pair<std::string, const std::vector<float>*>>& dataSeries,
        const std::string& yAxisLabel = "Value",
        bool showControls = true
    );
    
    // Keep the old function for backward compatibility if needed
    void ShowGDPPlot(const std::vector<float>& gdp_values, int max_months);
    
    // Optional: Show ImGui/ImPlot Demo Windows
    void ShowDemoWindows(bool show_imgui_demo, bool show_implot_demo);

} // namespace Visualizer