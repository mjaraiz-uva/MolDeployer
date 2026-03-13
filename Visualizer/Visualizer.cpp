// Visualizer.cpp : Defines the functions for the static library.
//

// ImGui and ImPlot headers
#include <imgui.h> // Main ImGui header
#include "implot.h" // ImPlot header

#include <vector> // For std::vector in ShowGDPPlot

// Backend headers for GLFW + OpenGL3
// These headers are for the ImGui backend implementations.
// The corresponding .cpp files (imgui_impl_glfw.cpp and imgui_impl_opengl3.cpp)
// must be compiled as part of the Visualizer project.
#include "GLFW/glfw3.h" // For GLFWwindow type (though often forward-declared)
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot_internal.h"

#include "Visualizer.h" // Corresponding header
#include "TimeSeriesPlotTemplate.h" // Include our template
#include "../Logger/Logger.h"
#include "../ThemeManager/ThemeManager.h"
#include "../DataManager/DataManager.h"

namespace Visualizer {

bool Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext(); // Initialize ImPlot context
    ImGuiIO& io = ImGui::GetIO();
    (void)io; // Suppress unused variable warning if io is not used further directly
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // OPTIONAL: Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // OPTIONAL: Enable Multi-Viewport / Platform Windows

    // Set the ini saving rate to be more responsive
    io.IniSavingRate = 5.0f; // Save every 5 seconds when modified

    // Initialize our plot state handler AFTER ImGui and ImPlot are initialized
    // but BEFORE any plots are created
    Logger::Debug("Initializing plot state handler");
    if (!InitPlotStateHandler()) {
        Logger::Error("Failed to initialize plot state handler");
    }


    // Prevent loading/saving of imgui.ini to avoid persistent layout issues during testing
    // io.IniFilename = NULL;

    // Load and apply theme from configuration
    const auto& config = DataManager::GetConfigParameters();
    ThemeManager::ThemeType savedTheme = ThemeManager::Theme::getInstance().stringToTheme(config.uiTheme);
    ThemeManager::Theme::getInstance().applyTheme(savedTheme);
    Logger::Info("Applied UI theme: " + config.uiTheme);

    // Determine GLSL version string for ImGui OpenGL3 backend
#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130"; // Default for GL 3.0
#endif

    // Initialize ImGui backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) { // true = install callbacks
        // Consider adding error logging here if <iostream> is included
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        // Consider adding error logging here
        ImGui_ImplGlfw_Shutdown(); // Cleanup partial initialization
        return false;
    }
    
    return true; 
}

void Shutdown() {
    // Shutdown ImGui backends
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    // Destroy ImPlot and ImGui contexts
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void NewFrame() {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame(); // Renderer backend new frame
    ImGui_ImplGlfw_NewFrame();    // Platform backend new frame
    ImGui::NewFrame();            // ImGui new frame
}

void Render() {
    // Rendering
    ImGui::Render(); // End ImGui frame and prepare draw data

    // Render ImGui draw data using the OpenGL3 backend
    // The main application (DEPX.cpp) should handle clearing the framebuffer (glClear)
    // and setting the viewport (glViewport) before this call.
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multi-viewports if enabled
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
    // Frame presentation (glfwSwapBuffers) is handled by DEPX.cpp after this.
}

void ShowMainControls(float& parameter_A, float /*framerate_param*/, float /*ms_per_frame_param*/, bool& should_recalculate_gdp) {
    ImGui::Begin("DEPX Simulator Control");
    ImGui::Text("GDP(month) = A * sin(month * PI/6)"); // Formula for GDP
    if (ImGui::SliderFloat("Parameter A", &parameter_A, 0.0f, 1000.0f)) {
        should_recalculate_gdp = true; // Flag for recalculation if parameter changes
    }
    
    // Display framerate and ms per frame using ImGui's IO data
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f, io.Framerate);
    ImGui::End();
}

void ShowGDPPlot(const std::vector<float>& gdp_values, int max_months) {
    ImGui::Begin("GDP Output");
    if (ImPlot::BeginPlot("GDP Time Series")) {
        ImPlot::SetupAxes("Month", "GDP"); // Set up axes labels

        if (!gdp_values.empty() && static_cast<int>(gdp_values.size()) == max_months) {
            // Prepare month data for the x-axis
            std::vector<float> months(static_cast<size_t>(max_months));
            for(int i = 0; i < max_months; ++i) {
                months[static_cast<size_t>(i)] = static_cast<float>(i);
            }
            
            ImPlot::PlotLine("GDP", months.data(), gdp_values.data(), max_months); // Plot GDP data
        } else if (gdp_values.empty()) {
            // Display a message if there's no data
            ImPlot::PlotText("No GDP data to display.", 0.5, 0.5, ImVec2(0,0), 0); 
        }
        // Optionally, handle cases where gdp_values.size() != max_months if that's a possible error state

        ImPlot::EndPlot();
    }
    ImGui::End();
}

// Helper function to create EconomicTimeSeries config using the template
TimeSeriesPlotConfig CreateEconomicTimeSeriesConfig(
    const std::vector<float>& gdp_values,
    const std::vector<float>& household_expenditure_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Economic Time Series";
    config.plotTitle = "##EconomicTimeSeries";
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;
    
    // Add GDP series
    TimeSeriesConfig gdpSeries;
    gdpSeries.name = "GDP";
    gdpSeries.data = &gdp_values;
    gdpSeries.marker = ImPlotMarker_Circle;
    gdpSeries.markerSize = 2.0f;
    gdpSeries.color = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Green color for GDP
    config.series.push_back(gdpSeries);
    
    // Add Household Expenditure series
    TimeSeriesConfig expenditureSeries;
    expenditureSeries.name = "House Expend";
    expenditureSeries.data = &household_expenditure_values;
    expenditureSeries.marker = ImPlotMarker_Square;
    expenditureSeries.markerSize = 2.0f;
    expenditureSeries.color = ImVec4(0.8f, 0.4f, 0.2f, 1.0f); // Orange color for Expenditure
    config.series.push_back(expenditureSeries);
    
    return config;
}

// Helper function to create Commercial Bank plot config using the template
TimeSeriesPlotConfig CreateCommercialBankPlotConfig(
    const std::vector<float>& commercial_bank_deposit_values,
    const std::vector<float>& commercial_bank_loan_values,
    const std::vector<float>& commercial_bank_cash_values,
    const std::vector<float>& commercial_bank_reserves_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Commercial Bank Metrics";
    config.plotTitle = "##CommercialBankPlot";
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;
    
    // Add Deposits series
    TimeSeriesConfig depositsSeries;
    depositsSeries.name = "Deposits";
    depositsSeries.data = &commercial_bank_deposit_values;
    depositsSeries.marker = ImPlotMarker_Diamond;
    depositsSeries.markerSize = 2.0f;
    depositsSeries.color = ImVec4(0.2f, 0.4f, 0.8f, 1.0f); // Blue color for Deposits
    config.series.push_back(depositsSeries);
    
    // Add Loans series
    TimeSeriesConfig loansSeries;
    loansSeries.name = "Loans";
    loansSeries.data = &commercial_bank_loan_values;
    loansSeries.marker = ImPlotMarker_Up;
    loansSeries.markerSize = 2.0f;
    loansSeries.color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // Red color for Loans
    config.series.push_back(loansSeries);
    
    // Add Cash series
    TimeSeriesConfig cashSeries;
    cashSeries.name = "Cash";
    cashSeries.data = &commercial_bank_cash_values;
    cashSeries.marker = ImPlotMarker_Circle;
    cashSeries.markerSize = 2.0f;
    cashSeries.color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green color for Cash
    config.series.push_back(cashSeries);
    
    // Add Reserves series
    TimeSeriesConfig reservesSeries;
    reservesSeries.name = "ReserveDepos";
    reservesSeries.data = &commercial_bank_reserves_values;
    reservesSeries.marker = ImPlotMarker_Cross;
    reservesSeries.markerSize = 2.0f;
    reservesSeries.color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); // Yellow color for Reserves
    config.series.push_back(reservesSeries);
    
    return config;
}

// Helper function to create Central Bank plot config using the template
TimeSeriesPlotConfig CreateCentralBankPlotConfig(
    const std::vector<float>& total_reserves_values,
    const std::vector<float>& total_money_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Central Bank Metrics";
    config.plotTitle = "##CentralBankPlot"; // Unique ID for the plot
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Value";
    config.legendLocation = ImPlotLocation_NorthWest; // Default, can be overridden
    config.legendOutside = true;                      // Default, can be overridden
    config.showControls = true;                       // Show plot controls (like legend options)
    config.ticksEvery = 12;                           // X-axis ticks every 12 months
    config.allowSeriesToggle = true;                  // Allow toggling series visibility
    config.showTooltips = true;                       // Show tooltips on hover

    // Add Total Reserves series
    TimeSeriesConfig totalReservesSeries;
    totalReservesSeries.name = "Total Reserves";
    totalReservesSeries.data = &total_reserves_values;
    totalReservesSeries.marker = ImPlotMarker_Circle;
    totalReservesSeries.markerSize = 2.0f;
    totalReservesSeries.color = ImVec4(0.1f, 0.5f, 0.9f, 1.0f); // A distinct blue
    config.series.push_back(totalReservesSeries);

    // Add Total Money series
    TimeSeriesConfig totalMoneySeries;
    totalMoneySeries.name = "Total Money";
    totalMoneySeries.data = &total_money_values;
    totalMoneySeries.marker = ImPlotMarker_Square;
    totalMoneySeries.markerSize = 2.0f;
    totalMoneySeries.color = ImVec4(0.9f, 0.5f, 0.1f, 1.0f); // A distinct orange
    config.series.push_back(totalMoneySeries);

    return config;
}

// Helper function to create Worker Metrics plot config using the template
TimeSeriesPlotConfig CreateWorkerMetricsPlotConfig(
    const std::vector<float>& worker_money_values,
    const std::vector<float>& worker_bank_balance_values,
    const std::vector<float>& worker_last_month_expenditure_values,
    int workerID)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Worker Financial Metrics"; // Static window name
    config.plotTitle = "##WorkerMetricsPlot_" + std::to_string(workerID); // Unique ImPlot ID, can include workerID
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;

    // Add Worker Money series
    TimeSeriesConfig moneySeries;
    moneySeries.name = "Cash";
    moneySeries.data = &worker_money_values;
    moneySeries.marker = ImPlotMarker_Circle;
    moneySeries.markerSize = 2.0f;
    moneySeries.color = ImVec4(0.1f, 0.8f, 0.1f, 1.0f); // Bright Green
    config.series.push_back(moneySeries);

    // Add Worker Bank Balance series
    TimeSeriesConfig bankBalanceSeries;
    bankBalanceSeries.name = "Bank Balance ID " + std::to_string(workerID);
    bankBalanceSeries.data = &worker_bank_balance_values;
    bankBalanceSeries.marker = ImPlotMarker_Square;
    bankBalanceSeries.markerSize = 2.0f;
    bankBalanceSeries.color = ImVec4(0.1f, 0.1f, 0.8f, 1.0f); // Bright Blue
    config.series.push_back(bankBalanceSeries);

    // Add Worker Last Month Expenditure series
    TimeSeriesConfig expenditureSeries;
    expenditureSeries.name = "Expenditure (Prev M)";
    expenditureSeries.data = &worker_last_month_expenditure_values;
    expenditureSeries.marker = ImPlotMarker_Diamond;
    expenditureSeries.markerSize = 2.0f;
    expenditureSeries.color = ImVec4(0.8f, 0.1f, 0.1f, 1.0f); // Bright Red
    config.series.push_back(expenditureSeries);

    return config;
}

TimeSeriesPlotConfig CreateHouseholdWealthPlotConfig(
    const std::vector<float>& household_cash_values,
    const std::vector<float>& household_bank_balance_values,
    const std::vector<float>& household_wealth_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Household Wealth Metrics";
    config.plotTitle = "##HouseholdWealthPlot";
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Annualized Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;

    // Add Total Wealth series (should be displayed first/on top)
    TimeSeriesConfig wealthSeries;
    wealthSeries.name = "Total Wealth";
    wealthSeries.data = &household_wealth_values;
    wealthSeries.marker = ImPlotMarker_Circle;
    wealthSeries.markerSize = 2.0f;
    wealthSeries.color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
    config.series.push_back(wealthSeries);

    // Add Cash series
    TimeSeriesConfig cashSeries;
    cashSeries.name = "Cash Holdings";
    cashSeries.data = &household_cash_values;
    cashSeries.marker = ImPlotMarker_Square;
    cashSeries.markerSize = 2.0f;
    cashSeries.color = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); // Orange
    config.series.push_back(cashSeries);

    // Add Bank Balance series
    TimeSeriesConfig bankBalanceSeries;
    bankBalanceSeries.name = "Bank Deposits";
    bankBalanceSeries.data = &household_bank_balance_values;
    bankBalanceSeries.marker = ImPlotMarker_Diamond;
    bankBalanceSeries.markerSize = 2.0f;
    bankBalanceSeries.color = ImVec4(0.2f, 0.4f, 0.8f, 1.0f); // Blue
    config.series.push_back(bankBalanceSeries);

    return config;
}

TimeSeriesPlotConfig CreateUnemploymentPlotConfig(const std::vector<float>& unemployment_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Unemployment Rate";
    config.plotTitle = "##UnemploymentPlot";
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Unemployment (%)";
    config.legendLocation = ImPlotLocation_NorthEast;
    config.legendOutside = false;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = false; // Single series, no need to toggle
    config.showTooltips = true;

    // Add unemployment series
    TimeSeriesConfig unemploymentSeries;
    unemploymentSeries.name = "Unemployment Rate";
    unemploymentSeries.data = &unemployment_values;
    unemploymentSeries.marker = ImPlotMarker_Circle;
    unemploymentSeries.markerSize = 3.0f;
    unemploymentSeries.color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red color for unemployment
    config.series.push_back(unemploymentSeries);

    return config;
}

// Helper function to create Firm Metrics plot config using the template
TimeSeriesPlotConfig CreateFirmMetricsPlotConfig(
    const std::vector<float>& firm_cash_values,
    const std::vector<float>& firm_bank_balance_values,
    const std::vector<float>& firm_revenue_values,
    const std::vector<float>& firm_production_values,
    int firmID)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Firm Financial Metrics"; // Static window name
    config.plotTitle = "##FirmMetricsPlot_" + std::to_string(firmID); // Unique ImPlot ID, can include firmID
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;

    // Add Firm Cash series
    TimeSeriesConfig cashSeries;
    cashSeries.name = "Cash";
    cashSeries.data = &firm_cash_values;
    cashSeries.marker = ImPlotMarker_Circle;
    cashSeries.markerSize = 2.0f;
    cashSeries.color = ImVec4(0.1f, 0.8f, 0.1f, 1.0f); // Bright Green
    config.series.push_back(cashSeries);

    // Add Firm Bank Balance series
    TimeSeriesConfig bankBalanceSeries;
    bankBalanceSeries.name = "Bank Balance ID " + std::to_string(firmID);
    bankBalanceSeries.data = &firm_bank_balance_values;
    bankBalanceSeries.marker = ImPlotMarker_Square;
    bankBalanceSeries.markerSize = 2.0f;
    bankBalanceSeries.color = ImVec4(0.1f, 0.1f, 0.8f, 1.0f); // Bright Blue
    config.series.push_back(bankBalanceSeries);

    // Add Firm Revenue series
    TimeSeriesConfig revenueSeries;
    revenueSeries.name = "Revenue (Prev M)";
    revenueSeries.data = &firm_revenue_values;
    revenueSeries.marker = ImPlotMarker_Diamond;
    revenueSeries.markerSize = 2.0f;
    revenueSeries.color = ImVec4(0.8f, 0.1f, 0.1f, 1.0f); // Bright Red
    config.series.push_back(revenueSeries);

    // Add Firm Production series
    TimeSeriesConfig productionSeries;
    productionSeries.name = "Production";
    productionSeries.data = &firm_production_values;
    productionSeries.marker = ImPlotMarker_Up;
    productionSeries.markerSize = 2.0f;
    productionSeries.color = ImVec4(0.8f, 0.6f, 0.1f, 1.0f); // Orange
    config.series.push_back(productionSeries);

    return config;
}


TimeSeriesPlotConfig CreateProductionMetricsPlotConfig(
    const std::vector<float>& total_production_values,
    const std::vector<float>& assisted_production_values,
    const std::vector<float>& actual_production_values)
{
    TimeSeriesPlotConfig config;
    config.windowName = "Production Metrics";
    config.plotTitle = "##ProductionMetricsPlot";
    config.xAxisLabel = "Month";
    config.yAxisLabel = "Production Value";
    config.legendLocation = ImPlotLocation_NorthWest;
    config.legendOutside = true;
    config.showControls = true;
    config.ticksEvery = 12;
    config.allowSeriesToggle = true;
    config.showTooltips = true;

    // Add Actual Production series (Total - Assisted)
    TimeSeriesConfig actualProdSeries;
    actualProdSeries.name = "Actual Production";
    actualProdSeries.data = &actual_production_values;
    actualProdSeries.marker = ImPlotMarker_Diamond;
    actualProdSeries.markerSize = 2.0f;
    actualProdSeries.color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
    config.series.push_back(actualProdSeries);

    // Add Total Production series
    TimeSeriesConfig totalProdSeries;
    totalProdSeries.name = "Total Production";
    totalProdSeries.data = &total_production_values;
    totalProdSeries.marker = ImPlotMarker_Circle;
    totalProdSeries.markerSize = 2.0f;
    totalProdSeries.color = ImVec4(0.2f, 0.6f, 0.9f, 1.0f); // Blue
    config.series.push_back(totalProdSeries);

    // Add Assisted Production series
    TimeSeriesConfig assistedProdSeries;
    assistedProdSeries.name = "Assisted Production";
    assistedProdSeries.data = &assisted_production_values;
    assistedProdSeries.marker = ImPlotMarker_Square;
    assistedProdSeries.markerSize = 2.0f;
    assistedProdSeries.color = ImVec4(0.9f, 0.6f, 0.2f, 1.0f); // Orange
    config.series.push_back(assistedProdSeries);

    return config;
}

// New factory functions for creating plot configs

TimeSeriesPlotConfig CreateEmptyPlotConfig(const std::string& windowName, const std::string& yAxisLabel) {
    TimeSeriesPlotConfig config;
    config.windowName = windowName;
    config.plotTitle = "##" + windowName; // Default hidden title with unique ID
    config.yAxisLabel = yAxisLabel;
    config.showControls = true;
    config.ticksEvery = 12;
    return config;
}

TimeSeriesPlotConfig CreateCustomPlotConfig(
    const std::string& windowName,
    const std::vector<std::pair<std::string, const std::vector<float>*>>& dataSeries,
    const std::string& yAxisLabel,
    bool showControls)
{
    TimeSeriesPlotConfig config = CreateEmptyPlotConfig(windowName, yAxisLabel);
    config.showControls = showControls;
    
    // Add data series with different markers and colors
    const ImPlotMarker markers[] = { 
        ImPlotMarker_Circle, ImPlotMarker_Square, ImPlotMarker_Diamond, 
        ImPlotMarker_Up, ImPlotMarker_Down, ImPlotMarker_Left, 
        ImPlotMarker_Right, ImPlotMarker_Cross, ImPlotMarker_Plus 
    };
    const int numMarkers = IM_ARRAYSIZE(markers);
    
    int seriesIndex = 0;
    for (const auto& [seriesName, seriesData] : dataSeries) {
        TimeSeriesConfig seriesConfig;
        seriesConfig.name = seriesName;
        seriesConfig.data = seriesData;
        seriesConfig.marker = markers[seriesIndex % numMarkers];
        seriesConfig.markerSize = 2.0f;
        
        // Generate a color based on hue to get visually distinct colors
        float hue = 360.0f * (static_cast<float>(seriesIndex) / static_cast<float>(dataSeries.size()));
        seriesConfig.color = ColorFromHue(hue);
        
        config.series.push_back(seriesConfig);
        seriesIndex++;
    }
    
    return config;
}

void ShowEconomicTimeSeries(
    const std::vector<float>& gdp_values, 
    const std::vector<float>& household_expenditure_values,
    int max_months) 
{
    // Create the config
    TimeSeriesPlotConfig config = CreateEconomicTimeSeriesConfig(
        gdp_values, household_expenditure_values);
    
    // Show the plot using the templated function
    ShowMultiSeriesPlot(config, max_months, true, true, 2.0f);
}

// Updated function to display Commercial Bank metrics using the template
void ShowCommercialBankPlot(
    const std::vector<float>& commercial_bank_deposit_values,
    const std::vector<float>& commercial_bank_loan_values,
    const std::vector<float>& commercial_bank_cash_values,
    const std::vector<float>& commercial_bank_reserves_values,
    int max_months,
    ImPlotLocation legend_loc,
    bool legend_outside) 
{
    // Create the config
    TimeSeriesPlotConfig config = CreateCommercialBankPlotConfig(
        commercial_bank_deposit_values,
        commercial_bank_loan_values,
        commercial_bank_cash_values,
        commercial_bank_reserves_values);
    
    // Override legend location/outside from the parameters
    config.legendLocation = legend_loc;
    config.legendOutside = legend_outside;
    
    // Show the plot using the templated function
    ShowMultiSeriesPlot(config, max_months, true, true, 2.0f);
}

void ShowDemoWindows(bool show_imgui_demo, bool show_implot_demo) {
    // These functions show the built-in demo windows for ImGui and ImPlot.
    // Passing nullptr means the window's own close button will manage its visibility
    // if the demo window supports that. If InterfaceGUI is managing the bools,
    // this just triggers the display based on InterfaceGUI's state.
    if (show_imgui_demo) {
        ImGui::ShowDemoWindow(nullptr); 
    }
    if (show_implot_demo) {
        ImPlot::ShowDemoWindow(nullptr);
    }
}

} // namespace Visualizer
