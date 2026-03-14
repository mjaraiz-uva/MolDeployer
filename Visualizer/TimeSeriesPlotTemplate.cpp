#include "TimeSeriesPlotTemplate.h"
#include <imgui_internal.h> // for ImGuiSettingsHandler
#include <implot_internal.h> // for ImPlotContext
#include <cstdio> // for sscanf_s
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h"
#include "../Simulator/Simulator.h"

namespace Visualizer {

	// Forward declarations for INI handler functions
	static void* PlotStateHandler_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name);
	static void PlotStateHandler_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line);
	static void PlotStateHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf);

	// Implementation of TimeSeriesPlotManager
	PlotPersistentState& TimeSeriesPlotManager::GetState(const std::string& plotName) {
		return m_plotStates[plotName];
	}

	// Modified to strictly preserve existing settings
	void TimeSeriesPlotManager::RegisterPlotConfig(const std::string& plotName,
		ImPlotLocation defaultLocation,
		bool defaultOutside) {
		// Only set default values if the state doesn't already exist in the map
		auto it = m_plotStates.find(plotName);
		if (it == m_plotStates.end()) {
			Logger::Debug("Registering new plot config: " + plotName);
			PlotPersistentState state;
			state.legendLocation = defaultLocation;
			state.legendOutside = defaultOutside;
			state.showLine = true;
			state.showMarkers = false;
			state.markerSize = 2;
			m_plotStates[plotName] = state;
		}
		else {
			// Plot already exists in our state map, don't modify existing settings
			//Logger::Debug("Preserving existing plot config: " + plotName +
			//	" (Legend: " + std::to_string(static_cast<int>(it->second.legendLocation)) +
			//	", Outside: " + std::to_string(it->second.legendOutside) + 
			//	", Line: " + std::to_string(it->second.showLine) + 
			//	", Markers: " + std::to_string(it->second.showMarkers) + 
			//	", MarkerSize: " + std::to_string(it->second.markerSize) + ")");
		}
	}

	void TimeSeriesPlotManager::SaveToIni(ImGuiTextBuffer* buf, const char* handlerTypeName) {
		for (const auto& [plotName, state] : m_plotStates) {
			buf->appendf("[%s][%s]\n", handlerTypeName, plotName.c_str());
			buf->appendf("LegendLocation=%d\n", static_cast<int>(state.legendLocation));
			buf->appendf("LegendOutside=%d\n", state.legendOutside ? 1 : 0);
			buf->appendf("ShowLine=%d\n", state.showLine ? 1 : 0);
			buf->appendf("ShowMarkers=%d\n", state.showMarkers ? 1 : 0);
			buf->appendf("MarkerSize=%d\n", state.markerSize);
			buf->appendf("LogScale=%d\n", state.useLogScale ? 1 : 0);
			buf->appendf("\n");
		}
	}

	void* TimeSeriesPlotManager::GetStateForPlotName(const char* name) {
		std::string plotName(name);

		// Check if the state already exists
		auto it = m_plotStates.find(plotName);
		if (it != m_plotStates.end()) {
			return &it->second;
		}

		// Create a new state entry if it doesn't exist
		// This is crucial for INI loading to work properly
		Logger::Debug("Creating new plot state entry for INI loading: " + plotName);
		PlotPersistentState& newState = m_plotStates[plotName];

		// Initialize with sensible defaults that will be overwritten by INI values
		newState.legendLocation = ImPlotLocation_NorthWest;
		newState.legendOutside = false;
		newState.showLine = true;
		newState.showMarkers = false;
		newState.markerSize = 2;
		newState.useLogScale = false;

		return &newState;
	}

	void TimeSeriesPlotManager::LoadStateFromIni(void* entry, const char* line) {
		if (!entry) return;

		PlotPersistentState* state = static_cast<PlotPersistentState*>(entry);
		int loc_val;
		int outside_val;
		int line_val;
		int markers_val;
		int marker_size;

		// Fixed: replaced sscanf with sscanf_s for Windows security
		if (sscanf_s(line, "LegendLocation=%d", &loc_val) == 1) {
			state->legendLocation = static_cast<ImPlotLocation>(loc_val);
		}
		else if (sscanf_s(line, "LegendOutside=%d", &outside_val) == 1) {
			state->legendOutside = (outside_val == 1);
		}
		else if (sscanf_s(line, "ShowLine=%d", &line_val) == 1) {
			state->showLine = (line_val == 1);
		}
		else if (sscanf_s(line, "ShowMarkers=%d", &markers_val) == 1) {
			state->showMarkers = (markers_val == 1);
		}
		else if (sscanf_s(line, "MarkerSize=%d", &marker_size) == 1) {
			state->markerSize = marker_size;
		}
		else {
			int log_val;
			if (sscanf_s(line, "LogScale=%d", &log_val) == 1) {
				state->useLogScale = (log_val == 1);
			}
		}
	}

	TimeSeriesPlotManager& TimeSeriesPlotManager::GetInstance() {
		static TimeSeriesPlotManager instance;
		return instance;
	}

	void TimeSeriesPlotManager::ExportPlotStatesToConfig() {
		 // Plot settings are now managed by ImGui's ini system
		 // This function is kept as a no-op for compatibility
		Logger::Info("TimeSeriesPlotManager: Plot settings are now stored in ImGui's ini file");
	}

	void TimeSeriesPlotManager::ImportPlotStatesFromConfig() {
		// Plot settings are now loaded from ImGui's ini system
		// This function is kept as a no-op for compatibility
	}

	// INI handler implementations
	static void* PlotStateHandler_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) {
		return TimeSeriesPlotManager::GetInstance().GetStateForPlotName(name);
	}

	static void PlotStateHandler_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) {
		TimeSeriesPlotManager::GetInstance().LoadStateFromIni(entry, line);
	}

	static void PlotStateHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf) {
		TimeSeriesPlotManager::GetInstance().SaveToIni(out_buf, handler->TypeName);
	}

	// First, add a debug function to log the current state
	void TimeSeriesPlotManager::LogCurrentStates() {
		for (const auto& [plotName, state] : m_plotStates) {
			Logger::Debug("Plot '" + plotName + "' settings - Legend: " + std::to_string(static_cast<int>(state.legendLocation)) +
				", Outside: " + std::to_string(state.legendOutside) +
				", ShowLine: " + std::to_string(state.showLine) +
				", ShowMarkers: " + std::to_string(state.showMarkers) +
				", MarkerSize: " + std::to_string(state.markerSize));
		}
	}

	// Improved InitPlotStateHandler to ensure settings are loaded before plots are created
	bool InitPlotStateHandler() {
		// Check if a handler with this name already exists to avoid duplicates
		ImGuiContext* ctx = ImGui::GetCurrentContext();
		if (!ctx) {
			Logger::Error("InitPlotStateHandler: ImGui context is null!");
			return false;
		}

		for (int i = 0; i < ctx->SettingsHandlers.Size; i++) {
			if (strcmp(ctx->SettingsHandlers[i].TypeName, "TimeSeries") == 0) {
				// Handler already exists, don't add again
				Logger::Debug("Plot state handler already registered");
				return true;
			}
		}

		// Setup the handler as before
		ImGuiSettingsHandler ini_handler;
		ini_handler.TypeName = "TimeSeries";
		ini_handler.TypeHash = ImHashStr("TimeSeries");
		ini_handler.ReadOpenFn = PlotStateHandler_ReadOpen;
		ini_handler.ReadLineFn = PlotStateHandler_ReadLine;
		ini_handler.WriteAllFn = PlotStateHandler_WriteAll;
		ini_handler.UserData = nullptr;
		ImGui::AddSettingsHandler(&ini_handler);

		// Load the ini settings - this will populate m_plotStates through the ReadOpen/ReadLine handlers
		if (ImGui::GetIO().IniFilename)
			ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);

		Logger::Info("Plot state handler initialized and ini settings loaded");
		TimeSeriesPlotManager::GetInstance().LogCurrentStates();

		return true;
	}

	// Modified ShowMultiSeriesPlot function to preserve settings when recreating windows
	void ShowMultiSeriesPlot(
		const TimeSeriesPlotConfig& config,
		int maxMonths,
		bool showLine,
		bool showMarkers,
		float markerSize)
	{
		// Register the plot for persistence - but don't override existing settings
		TimeSeriesPlotManager::GetInstance().RegisterPlotConfig(
			config.windowName,
			config.legendLocation,
			config.legendOutside
		);

		// Get persistent state for this plot
		PlotPersistentState& state = TimeSeriesPlotManager::GetInstance().GetState(config.windowName);

		// Begin the window
		ImGui::Begin(config.windowName.c_str());

		// Add controls if enabled - make these instance-specific by keying off window name
		if (config.showControls) {
			// REMOVED: Block that would reset settings to defaults
			// Always use the persistent state values, never override them with parameters

			const char* markerSizeItems[] = { "Auto", "1", "2", "3", "4", "5" };

			// Use UI controls to modify the persistent state
			if (ImGui::Checkbox("Show Line", &state.showLine)) {
				ImGui::MarkIniSettingsDirty();
				// Force immediate save to avoid losing settings on recreation
				if (ImGui::GetIO().IniFilename) {
					ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
				}
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Show Markers", &state.showMarkers)) {
				ImGui::MarkIniSettingsDirty();
				// Force immediate save to avoid losing settings on recreation
				if (ImGui::GetIO().IniFilename) {
					ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
				}
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Log Y", &state.useLogScale)) {
				ImGui::MarkIniSettingsDirty();
				if (ImGui::GetIO().IniFilename) {
					ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50);
			if (ImGui::Combo("Marker Size", &state.markerSize, markerSizeItems, IM_ARRAYSIZE(markerSizeItems))) {
				ImGui::MarkIniSettingsDirty();
				// Force immediate save to avoid losing settings on recreation
				if (ImGui::GetIO().IniFilename) {
					ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
				}
			}

			// Always use the state values for rendering, ignore the parameters passed in
			showLine = state.showLine;
			showMarkers = state.showMarkers;
			// markerSize will be determined later using state.markerSize
		}

		// Begin the plot
		if (ImPlot::BeginPlot(config.plotTitle.c_str(), ImVec2(-1, -1), ImPlotFlags_None)) {
			// Apply persisted legend settings
			ImPlot::SetupLegend(
				state.legendLocation,
				state.legendOutside ? ImPlotLegendFlags_Outside : ImPlotLegendFlags_None
			);

			// Setup axes
			ImPlot::SetupAxes(config.xAxisLabel.c_str(), config.yAxisLabel.c_str(),
				ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
			if (state.useLogScale) {
				ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
			}
			ImPlot::SetupAxisLimits(ImAxis_X1, 0, maxMonths, ImGuiCond_Always);

			// Setup custom X-axis ticks at specified intervals
			std::vector<double> x_ticks;
			for (int tick_val = 0; tick_val <= maxMonths; tick_val += config.ticksEvery) {
				x_ticks.push_back(static_cast<double>(tick_val));
			}
			// Ensure at least one tick
			if (x_ticks.empty() && maxMonths > 0) {
				x_ticks.push_back(0);
			}
			else if (x_ticks.empty() && maxMonths == 0) {
				x_ticks.push_back(0);
			}
			if (!x_ticks.empty()) {
				ImPlot::SetupAxisTicks(ImAxis_X1, x_ticks.data(), static_cast<int>(x_ticks.size()));
			}

			// Get the current calculated month to limit plotting
			int currentCalculatedMonth = Simulator::GetCurrentStepCalculated();

			// Only plot data if any month has been calculated
			if (currentCalculatedMonth >= 0) {
				// Prepare X-axis values (months) once and reuse for all series
				// Only plot up to the current calculated month
				int plotMonths = std::min(currentCalculatedMonth + 1, maxMonths);
				std::vector<float> x_values(plotMonths);
				for (int i = 0; i < plotMonths; ++i) {
					x_values[i] = static_cast<float>(i);
				}

				// Plot each series
				for (const auto& series : config.series) {
					// Skip if data is null or empty
					if (!series.data || series.data->empty()) {
						continue;
					}

					// Limit data points to what has been calculated
					int count = std::min(static_cast<int>(series.data->size()), plotMonths);
					if (count <= 0) continue;

					// Set custom color if specified (non-zero alpha)
					if (series.color.w > 0) {
						ImPlot::PushStyleColor(ImPlotCol_Line, series.color);
						ImPlot::PushStyleColor(ImPlotCol_MarkerFill, series.color);
						ImPlot::PushStyleColor(ImPlotCol_MarkerOutline, series.color);
					}

					// Calculate actual marker size to use
					float actualMarkerSize;
					if (state.markerSize == 0) {
						// Auto size (use the default or series-specific size)
						actualMarkerSize = series.markerSize > 0 ? series.markerSize : 2.0f;
					}
					else {
						// Use the user-selected size from state
						actualMarkerSize = static_cast<float>(state.markerSize);
					}

					// Plot based on line/marker visibility
					if (showLine && showMarkers) {
						ImPlot::SetNextMarkerStyle(series.marker, actualMarkerSize);
						ImPlot::PlotLine(series.name.c_str(), x_values.data(), series.data->data(), count);
					}
					else if (showLine) {
						ImPlot::SetNextMarkerStyle(ImPlotMarker_None);
						ImPlot::PlotLine(series.name.c_str(), x_values.data(), series.data->data(), count);
					}
					else if (showMarkers) {
						ImPlot::SetNextMarkerStyle(series.marker, actualMarkerSize);
						ImPlot::PlotScatter(series.name.c_str(), x_values.data(), series.data->data(), count);
					}

					// Pop custom colors if we pushed them
					if (series.color.w > 0) {
						ImPlot::PopStyleColor(3);
					}
				}
			}
			else {
				 // FIX: Set explicit Y-axis limits when no data is present to prevent extreme scaling
				ImPlot::SetupAxisLimits(ImAxis_Y1, -10, 10, ImGuiCond_Always);
                
				// Display a message if no data has been calculated yet
				// Use ImPlotTextFlags_None instead of 0 for clarity
				ImPlot::PlotText("No data calculated yet", maxMonths / 2.0f, 0.0f, ImVec2(0, 0), ImPlotTextFlags_None);
			}

			// After plotting, retrieve and store the current legend state for persistence
			ImPlotPlot* currentPlot = ImPlot::GetCurrentContext()->CurrentPlot;
			if (currentPlot && !(currentPlot->Flags & ImPlotFlags_NoLegend)) {
				ImPlotLocation current_legend_loc = currentPlot->Items.Legend.Location;
				bool current_legend_outside = (currentPlot->Items.Legend.Flags & ImPlotLegendFlags_Outside) != 0;

				// If legend state changed, update and mark settings as dirty
				if (current_legend_loc != state.legendLocation || current_legend_outside != state.legendOutside) {
					state.legendLocation = current_legend_loc;
					state.legendOutside = current_legend_outside;
					ImGui::MarkIniSettingsDirty();

					// Force immediate save to ini file when settings change to prevent loss on unexpected exit
					if (ImGui::GetIO().IniFilename) {
						ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
					}
				}
			}

			ImPlot::EndPlot();
		}

		ImGui::End();
	}

} // namespace Visualizer