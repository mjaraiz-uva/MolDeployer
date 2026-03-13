// InterfaceGUI.cpp : Defines the functions for the static library.
//

#include <vector>
#include <string>
#include <algorithm> // For std::min/max if needed
#include <cstdio> // For sscanf

#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h"
#include "implot_internal.h" // For ImPlotPlot and ImPlotContext full definitions
#include "InterfaceGUI.h"
#include "../Visualizer/Visualizer.h" // For Visualizer::Init, Shutdown, NewFrame, Render
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h" // For accessing GDP data and parameters
#include "../Simulator/Simulator.h"   // For controlling simulation
#include "../ThemeManager/ThemeManager.h" // For applying themes
#include "../Visualizer/TimeSeriesAutoConfig.h"

namespace InterfaceGUI {

	// State for demo windows
	static bool m_show_imgui_demo = false;
	static bool m_show_implot_demo = false;
	static bool g_is_initialized = false;

	// Control variables for UI elements
	static float g_plot_delay_sec = 0.1f; // Default delay of 1 second max
	static bool g_first_frame = true;
	static bool g_show_plot_line = true;    // Default to show line
	static bool g_show_plot_markers = true; // Default to show markers
	static int g_marker_size_option = 2; // 0 for Auto, 1-5 for specific sizes
	const char* marker_size_items[] = { "Auto", "1", "2", "3", "4", "5" };

	// Buffer for simulation name input
	static char g_simulation_name_buffer[128] = "DefaultSim";

	// New log window states
	static ImGuiTextBuffer g_info_log_buffer;
	static ImGuiTextBuffer g_debug_log_buffer;
	static ImGuiTextBuffer g_warning_log_buffer;
	static ImGuiTextBuffer g_error_log_buffer;
	static bool g_info_log_auto_scroll = true;
	static bool g_debug_log_auto_scroll = true;
	static bool g_warning_log_auto_scroll = true;
	static bool g_error_log_auto_scroll = true;

	// New static booleans for controlling log output to GUI
	static bool g_log_info_to_gui = true;
	static bool g_log_debug_to_gui = true;
	static bool g_log_warning_to_gui = true;
	static bool g_log_error_to_gui = true;

	bool Init(GLFWwindow* window) {
		if (!Visualizer::Init(window)) { // Visualizer initializes ImGui
			Logger::Error("InterfaceGUI: Failed to initialize Visualizer.");
			return false;
		}
		Logger::Info("InterfaceGUI initialized successfully.");
		g_is_initialized = true;
		g_first_frame = true;

		// Load plot delay from config after DataManager has loaded it
		g_plot_delay_sec = DataManager::GetConfigParameters().plotDelaySec;
		// Set the initial calculation delay in the Simulator from the GUI's default or loaded config
		Simulator::SetCalculationDelay(g_plot_delay_sec);
		Logger::Info("InterfaceGUI: Initial plot delay set to " + std::to_string(g_plot_delay_sec) + "s.");

		// Enable auto-save of ImGui settings
		ImGuiIO& io = ImGui::GetIO();
		io.IniSavingRate = 5.0f;  // Save every 5 seconds when modified

		// Initialize Logger GUI output states
		Logger::SetEnableInfoGUIOutput(g_log_info_to_gui);
		Logger::SetEnableDebugGUIOutput(g_log_debug_to_gui);
		Logger::SetEnableWarningGUIOutput(g_log_warning_to_gui);
		Logger::SetEnableErrorGUIOutput(g_log_error_to_gui);

		return true;
	}

	void Shutdown() {
		// Save the current configuration before shutting down
		DataManager::SaveConfigParameters();

		// Force ImGui to save its settings
		ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);

		Visualizer::Shutdown(); // Visualizer shuts down ImGui
		g_is_initialized = false;
		g_info_log_buffer.clear();
		g_debug_log_buffer.clear();
		g_warning_log_buffer.clear();
		g_error_log_buffer.clear();
		Logger::Info("InterfaceGUI shutdown.");
	}

	void BeginFrame() {
		if (!g_is_initialized) {
			Logger::Warning("InterfaceGUI: BeginFrame called before initialization.");
			return;
		}
		Visualizer::NewFrame(); // Visualizer calls ImGui_ImplOpenGL3_NewFrame, ImGui_ImplGlfw_NewFrame, ImGui::NewFrame
	}

    void DrawUI(
        float& WorkersPerMillionActivePerSector,
        float framerate,
        float ms_per_frame,
        bool& should_recalculate_gdp
    ) {
        if (!g_is_initialized) {
            Logger::Warning("InterfaceGUI: DrawUI called before initialization.");
            return;
        }

        // Get the current theme from DataManager
        std::string currentTheme = DataManager::GetConfigParameters().uiTheme;

        // Apply theme
        if (currentTheme == "dark") {
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
        }
        else if (currentTheme == "light") {
            ImGui::StyleColorsLight();
            ImPlot::StyleColorsLight();
        }
        else {
            // Default to dark if unknown theme
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
        }

        // Main Menu Bar for plot management
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Plots")) {
                if (ImGui::MenuItem("Create Custom Plot...")) {
                    Visualizer::TimeSeriesAutoConfig::ShowPlotCreatorDialog();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset All Plot Settings")) {
                    Visualizer::TimeSeriesPlotManager::GetInstance().LogCurrentStates();
                    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("ImGui Demo", nullptr, &m_show_imgui_demo);
                ImGui::MenuItem("ImPlot Demo", nullptr, &m_show_implot_demo);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Show demos if requested
        if (m_show_imgui_demo) {
            ImGui::ShowDemoWindow(&m_show_imgui_demo);
        }
        if (m_show_implot_demo) {
            ImPlot::ShowDemoWindow(&m_show_implot_demo);
        }

        // --- Theme Selection Window ---
        if (ImGui::Begin("Theme Settings")) {
            ImGui::Text("Select UI Theme:");

            const char* themes[] = { "dark", "light" };
            int currentThemeIndex = (currentTheme == "light") ? 1 : 0;

            if (ImGui::Combo("Theme", &currentThemeIndex, themes, IM_ARRAYSIZE(themes))) {
                std::string newTheme = themes[currentThemeIndex];
                DataManager::GetMutableConfigParameters().uiTheme = newTheme;
                DataManager::SaveConfigParameters();
                Logger::Info("Theme changed to: " + newTheme);
            }

            ImGui::Separator();
            ImGui::TextWrapped("Theme changes are applied immediately and saved to configuration.");
        }
        ImGui::End();

        // --- DEPX Simulator Control Window ---
        ImGui::Begin("DEPX Simulator Control");
        {
            Simulator::SimulationState current_sim_state_for_slider = Simulator::GetSimulationState();
            bool slider_disabled = (current_sim_state_for_slider == Simulator::RUNNING ||
                current_sim_state_for_slider == Simulator::PAUSED ||
                current_sim_state_for_slider == Simulator::STEPPING);

            // Set the collapsing header to be open by default
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("Simulation Parameters")) {
                // Disable all parameters when simulation is running
                if (slider_disabled) {
                    ImGui::BeginDisabled();
                }

                // Workers per million
                ImGui::Text("Workers Per Million Active Per Sector:");
                ImGui::SetNextItemWidth(50);
                float oldWorkersValue = WorkersPerMillionActivePerSector;
                ImGui::InputScalar("##WorkersPerMillionInput", ImGuiDataType_Float, &WorkersPerMillionActivePerSector,
                    NULL, NULL, "%.2f", ImGuiInputTextFlags_CharsDecimal);

                // Only save if the value actually changed and input was deactivated
                if (ImGui::IsItemDeactivatedAfterEdit() && oldWorkersValue != WorkersPerMillionActivePerSector) {
                    if (WorkersPerMillionActivePerSector < 0.1f) WorkersPerMillionActivePerSector = 0.1f;
                    if (WorkersPerMillionActivePerSector > 100.0f) WorkersPerMillionActivePerSector = 100.0f;
                    // Parameter update removed (chemistry mode)
                    DataManager::SaveConfigParameters();
                }

                ImGui::SameLine();
                ImGui::Text("= %d workers, ", static_cast<int>(DataManager::GetNworkers()));
                ImGui::SameLine();

                // Max months
                ImGui::Text("Max. months:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                int oldMaxMonths = DataManager::GetMaxSteps();
                int newMaxMonths = oldMaxMonths;
                ImGui::InputScalar("##MaxMonthsInput", ImGuiDataType_S32, &newMaxMonths,
                    NULL, NULL, "%d", ImGuiInputTextFlags_CharsDecimal);

                // Only save if the value actually changed and input was deactivated
                if (ImGui::IsItemDeactivatedAfterEdit() && newMaxMonths != oldMaxMonths) {
                    if (newMaxMonths < 1) newMaxMonths = 1;
                    if (newMaxMonths > 10000) newMaxMonths = 10000;
                    DataManager::GetMutableConfigParameters().maxSteps = newMaxMonths;
                    DataManager::SaveConfigParameters();
                    Logger::Info("Max months updated to: " + std::to_string(newMaxMonths));
                }

                // Plot delay control
                ImGui::Text("Calculation delay (seconds):");
                ImGui::SetNextItemWidth(100);
                if (ImGui::SliderFloat("##PlotDelay", &g_plot_delay_sec, 0.01f, 2.0f, "%.2f")) {
                    DataManager::GetMutableConfigParameters().plotDelaySec = g_plot_delay_sec;
                    Simulator::SetCalculationDelay(g_plot_delay_sec);
                    DataManager::SaveConfigParameters();
                }

                if (slider_disabled) {
                    ImGui::EndDisabled();
                }
            }

            ImGui::Separator();

            // Simulation control buttons
// Based on existing InterfaceGUI.cpp pattern
            Simulator::SimulationState sim_state = Simulator::GetSimulationState();
            const char* run_pause_label = "Run";

            if (sim_state == Simulator::RUNNING) {
                run_pause_label = "Pause";
            }
            else if (sim_state == Simulator::PAUSED) {
                run_pause_label = "Resume";
            }

            if (ImGui::Button(run_pause_label)) {
                if (sim_state == Simulator::RUNNING) {
                    Simulator::PauseCalculation();
                }
                else if (sim_state == Simulator::PAUSED) {
                    Simulator::ResumeCalculation();
                }
                else { // IDLE or COMPLETED
                    Simulator::RestartCalculation();
                    // Parameter update removed (chemistry mode)
                    Simulator::StartAsyncCalculation();
                }
            }

            ImGui::SameLine();
            bool step_disabled = !(sim_state == Simulator::PAUSED);
            if (step_disabled) ImGui::BeginDisabled();
            if (ImGui::Button("Step")) {
                if (sim_state == Simulator::PAUSED) {
                    Simulator::StepCalculation();
                }
            }
            if (step_disabled) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                Simulator::RestartCalculation();
            }

            // Simulation status
            ImGui::Separator();
            int currentMonth = Simulator::GetCurrentStepCalculated();
            ImGui::Text("Simulation State: %s", Simulator::StateToString(sim_state).c_str());
            ImGui::Text("Current Month: %d, ", currentMonth);
            ImGui::SameLine();
            ImGui::Text(" Year: %.1f", currentMonth / 12.0f);
        }
        ImGui::End();

        // --- Show all registered plots using the new auto-configuration system ---
        Visualizer::TimeSeriesAutoConfig::ShowAllRegisteredPlots(DataManager::GetMaxSteps());

        // --- Worker Metrics Window (Plot) - Special handling for dynamic ID ---
        {
            int plotWorkerID = DataManager::GetConfigParameters().PlotWorkerID;

            // Add a control to select which worker to plot
            ImGui::Begin("Worker Plot Controls");
            {
                ImGui::Text("Worker Selection:");
                if (ImGui::InputInt("Worker ID", &plotWorkerID)) {
                    if (plotWorkerID < 0) plotWorkerID = 0; // Ensure non-negative
                    DataManager::GetMutableConfigParameters().PlotWorkerID = plotWorkerID;
                    DataManager::SaveConfigParameters(); // Save when worker ID changes
                }
                ImGui::Text("Currently tracking Worker ID: %d", plotWorkerID);
            }
            ImGui::End();

            // Get worker-specific data
            const std::vector<float>& worker_money = DataManager::GetWorkerMoneyValues();
            const std::vector<float>& worker_bank_balance = DataManager::GetWorkerBankBalanceValues();
            const std::vector<float>& worker_expenditure = DataManager::GetWorkerLastMonthExpenditureValues();
            int max_months = DataManager::GetMaxSteps();

            Visualizer::TimeSeriesPlotConfig config = Visualizer::CreateWorkerMetricsPlotConfig(
                worker_money,
                worker_bank_balance,
                worker_expenditure,
                plotWorkerID);

            Visualizer::ShowMultiSeriesPlot(config, max_months, g_show_plot_line, g_show_plot_markers,
                g_marker_size_option > 0 ? static_cast<float>(g_marker_size_option) : 2.0f);
        }

        // --- Firm Metrics Window (Plot) - Special handling for dynamic ID ---
        {
            int plotFirmID = DataManager::GetConfigParameters().PlotFirmID;

            // Add a control to select which firm to plot
            ImGui::Begin("Firm Plot Controls");
            {
                ImGui::Text("Firm Selection:");
                if (ImGui::InputInt("Firm ID", &plotFirmID)) {
                    if (plotFirmID < 0) plotFirmID = 0; // Ensure non-negative
                    DataManager::GetMutableConfigParameters().PlotFirmID = plotFirmID;
                    DataManager::SaveConfigParameters(); // Save when firm ID changes
                }
                ImGui::Text("Currently tracking Firm ID: %d", plotFirmID);
            }
            ImGui::End();

            const std::vector<float>& firm_cash = DataManager::GetFirmCashValues();
            const std::vector<float>& firm_bank_balance = DataManager::GetFirmBankBalanceValues();
            const std::vector<float>& firm_revenue = DataManager::GetFirmRevenueValues();
            const std::vector<float>& firm_production = DataManager::GetFirmProductionValues();
            int max_months = DataManager::GetMaxSteps();

            Visualizer::TimeSeriesPlotConfig config = Visualizer::CreateFirmMetricsPlotConfig(
                firm_cash,
                firm_bank_balance,
                firm_revenue,
                firm_production,
                plotFirmID);

            Visualizer::ShowMultiSeriesPlot(config, max_months, g_show_plot_line, g_show_plot_markers,
                g_marker_size_option > 0 ? static_cast<float>(g_marker_size_option) : 2.0f);
        }

        // --- Info Log Output Window ---
        ImGui::Begin("Info Output");
        {
            if (ImGui::Button("Clear##Info")) {
                g_info_log_buffer.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll##Info", &g_info_log_auto_scroll);
            ImGui::SameLine();
            if (ImGui::Checkbox("Output to GUI##Info", &g_log_info_to_gui)) {
                Logger::SetEnableInfoGUIOutput(g_log_info_to_gui);
            }
            ImGui::Separator();
            ImGui::BeginChild("InfoLogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::TextUnformatted(g_info_log_buffer.begin(), g_info_log_buffer.end());
                if (g_info_log_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // --- Debug Log Output Window ---
        ImGui::Begin("Debug Output");
        {
            if (ImGui::Button("Clear##Debug")) {
                g_debug_log_buffer.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll##Debug", &g_debug_log_auto_scroll);
            ImGui::SameLine();
            if (ImGui::Checkbox("Output to GUI##Debug", &g_log_debug_to_gui)) {
                Logger::SetEnableDebugGUIOutput(g_log_debug_to_gui);
            }
            ImGui::Separator();
            ImGui::BeginChild("DebugLogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::TextUnformatted(g_debug_log_buffer.begin(), g_debug_log_buffer.end());
                if (g_debug_log_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // --- Warning Log Output Window ---
        ImGui::Begin("Warning Output");
        {
            if (ImGui::Button("Clear##Warning")) {
                g_warning_log_buffer.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll##Warning", &g_warning_log_auto_scroll);
            ImGui::SameLine();
            if (ImGui::Checkbox("Output to GUI##Warning", &g_log_warning_to_gui)) {
                Logger::SetEnableWarningGUIOutput(g_log_warning_to_gui);
            }
            ImGui::Separator();
            ImGui::BeginChild("WarningLogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::TextUnformatted(g_warning_log_buffer.begin(), g_warning_log_buffer.end());
                if (g_warning_log_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // --- Error Log Output Window ---
        ImGui::Begin("Error Output");
        {
            if (ImGui::Button("Clear##Error")) {
                g_error_log_buffer.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll##Error", &g_error_log_auto_scroll);
            ImGui::SameLine();
            if (ImGui::Checkbox("Output to GUI##Error", &g_log_error_to_gui)) {
                Logger::SetEnableErrorGUIOutput(g_log_error_to_gui);
            }
            ImGui::Separator();
            ImGui::BeginChild("ErrorLogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::TextUnformatted(g_error_log_buffer.begin(), g_error_log_buffer.end());
                if (g_error_log_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // Framerate display window
        ImGui::Begin("Performance Metrics");
        {
            ImGui::Text("Framerate: %.1f FPS (%.3f ms/frame)", framerate, ms_per_frame);

            // Optional: Add a framerate graph
            static float fps_history[120] = { 0 };
            static int fps_history_offset = 0;
            fps_history[fps_history_offset] = framerate;
            fps_history_offset = (fps_history_offset + 1) % IM_ARRAYSIZE(fps_history);

            float fps_min = *std::min_element(fps_history, fps_history + IM_ARRAYSIZE(fps_history));
            float fps_max = *std::max_element(fps_history, fps_history + IM_ARRAYSIZE(fps_history));

            ImGui::PlotLines("##FPS", fps_history, IM_ARRAYSIZE(fps_history), fps_history_offset,
                "FPS History", fps_min - 10.0f, fps_max + 10.0f, ImVec2(0, 80));
        }
        ImGui::End();

        // Handle first frame logic
        if (g_first_frame) {
            g_first_frame = false;
            ImGui::SetWindowFocus("DEPX Simulator Control");
        }
    }

	void EndFrame() {
		if (!g_is_initialized) {
			Logger::Warning("InterfaceGUI: EndFrame called before initialization.");
			return;
		}
		Visualizer::Render(); // Visualizer calls ImGui::Render and ImGui_ImplOpenGL3_RenderDrawData
	}

	// Implementations for categorized log messages
	void AddInfoLog(const std::string& message) {
		g_info_log_buffer.appendf("%s\n", message.c_str());
	}

	void AddDebugLog(const std::string& message) {
		g_debug_log_buffer.appendf("%s\n", message.c_str());
	}

	void AddWarningLog(const std::string& message) {
		g_warning_log_buffer.appendf("%s\n", message.c_str());
	}

	void AddErrorLog(const std::string& message) {
		g_error_log_buffer.appendf("%s\n", message.c_str());
	}

} // namespace InterfaceGUI
