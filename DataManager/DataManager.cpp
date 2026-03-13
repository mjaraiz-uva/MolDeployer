#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <random>
#include "DataManager.h"
#include "../Logger/Logger.h"
#include "../Chemistry/Reactor.h"

using json = nlohmann::json;

namespace DataManager {

	static std::mutex g_data_mutex;
	static std::atomic<bool> new_data_available_flag{ false };

	static json configJson;
	static ConfigParameters g_config_parameters;
	static std::mutex g_config_mutex;

	// Global Random Engine and Mutex
	static std::mt19937 g_random_engine;
	static std::mutex g_rng_mutex;

	// Plot states map
	static PlotStatesMap g_plot_states;

	// --- Core functions ---

	void UpdateAllTimeSeriesData(int currentStep) {
		TimeSeriesRegistry::GetInstance().UpdateAllSeries(currentStep);
	}

	void SetNewDataAvailable(bool available) {
		std::lock_guard<std::mutex> lock(g_data_mutex);
		new_data_available_flag = available;
	}

	std::mt19937& GetGlobalRandomEngine() {
		return g_random_engine;
	}

	double getRandom01() {
		std::lock_guard<std::mutex> lock(g_rng_mutex);
		std::uniform_real_distribution<double> dist(0.0, 1.0);
		return dist(g_random_engine);
	}

	int getRandomInt(int min, int max) {
		std::lock_guard<std::mutex> lock(g_rng_mutex);
		if (min >= max) {
			Logger::Error("DataManager::getRandomInt: min >= max. Returning min.");
			return min;
		}
		std::uniform_int_distribution<int> dist(min, max - 1);
		return dist(g_random_engine);
	}

	bool Init() {
		std::lock_guard<std::mutex> lock(g_data_mutex);
		std::lock_guard<std::mutex> config_lock(g_config_mutex);
		std::lock_guard<std::mutex> rng_lock(g_rng_mutex);

		new_data_available_flag = false;

		// Initialize RNG
		if (g_config_parameters.rngSeed == 0) {
			g_config_parameters.rngSeed = 42u;
			Logger::Warning("DataManager::Init: rngSeed was 0, setting to default: 42");
		}
		g_random_engine.seed(g_config_parameters.rngSeed);

		Logger::Info("DataManager initialized. Max steps: " + std::to_string(g_config_parameters.maxSteps));

		// Initialize time series registry
		TimeSeriesRegistry::GetInstance().InitializeAllSeries(g_config_parameters.maxSteps);

		// Ensure chemistry time series are registered
		Chemistry::EnsureTimeSeriesRegistration();

		return true;
	}

	void Shutdown() {
		Logger::Info("DataManager shutdown.");
	}

	int GetMaxSteps() {
		std::lock_guard<std::mutex> lock(g_config_mutex);
		return g_config_parameters.maxSteps;
	}

	bool CheckAndResetNewDataFlag() {
		bool expected = true;
		return new_data_available_flag.compare_exchange_strong(expected, false);
	}

	void ClearAllTimeSeriesValues() {
		std::lock_guard<std::mutex> lock(g_data_mutex);
		TimeSeriesRegistry::GetInstance().ClearAllSeries();
		new_data_available_flag = false;
	}

	const ConfigParameters& GetConfigParameters() {
		return g_config_parameters;
	}

	ConfigParameters& GetMutableConfigParameters() {
		return g_config_parameters;
	}

	const nlohmann::json& GetConfig_json() {
		return configJson;
	}

	void UpdateDisplaySize(int width, int height) {
		std::lock_guard<std::mutex> lock(g_config_mutex);
		g_config_parameters.displaySizeX = width;
		g_config_parameters.displaySizeY = height;
	}

	void UpdateDisplayPosition(int x, int y) {
		std::lock_guard<std::mutex> lock(g_config_mutex);
		g_config_parameters.displayPosX = x;
		g_config_parameters.displayPosY = y;
	}

	// --- Configuration loading ---

	// Helper to load a JSON field with a default value
	template<typename T>
	static void LoadField(const json& j, const std::string& key, T& target, const std::string& typeName = "value") {
		if (j.contains(key)) {
			try {
				target = j[key].get<T>();
			}
			catch (...) {
				Logger::Warning("DataManager: Invalid type for '" + key + "'. Using default.");
			}
		}
		else {
			Logger::Warning("DataManager: '" + key + "' not found in config. Using default.");
		}
	}

	bool LoadConfigParameters(const std::string& simulationName) {
		std::lock_guard<std::mutex> lock(g_config_mutex);
		std::lock_guard<std::mutex> rng_lock(g_rng_mutex);
		std::string filename = simulationName + ".json";
		Logger::Info("DataManager: Attempting to load configuration from: " + filename);

		g_config_parameters.simulationName = simulationName;

		std::ifstream configFile(filename);
		if (!configFile.is_open()) {
			Logger::Error("DataManager: Failed to open configuration file: " + filename);
			Logger::Warning("DataManager: Using default configuration parameters.");
		}
		else {
			try {
				configFile >> configJson;

				// Simulation control
				LoadField(configJson, "maxSteps", g_config_parameters.maxSteps);
				LoadField(configJson, "dt", g_config_parameters.dt);
				LoadField(configJson, "temperature", g_config_parameters.temperature);

				// Box dimensions
				LoadField(configJson, "boxSizeX", g_config_parameters.boxSizeX);
				LoadField(configJson, "boxSizeY", g_config_parameters.boxSizeY);
				LoadField(configJson, "boxSizeZ", g_config_parameters.boxSizeZ);

				// Atom counts
				LoadField(configJson, "numCarbon", g_config_parameters.numCarbon);
				LoadField(configJson, "numHydrogen", g_config_parameters.numHydrogen);
				LoadField(configJson, "numOxygen", g_config_parameters.numOxygen);

				// Interaction parameters
				LoadField(configJson, "interactionCutoff", g_config_parameters.interactionCutoff);
				LoadField(configJson, "A_form", g_config_parameters.A_form);
				LoadField(configJson, "A_break", g_config_parameters.A_break);
				LoadField(configJson, "activationFraction", g_config_parameters.activationFraction);

				// Boundary
				LoadField(configJson, "boundaryType", g_config_parameters.boundaryType);

				// Display
				LoadField(configJson, "displayPosX", g_config_parameters.displayPosX);
				LoadField(configJson, "displayPosY", g_config_parameters.displayPosY);
				LoadField(configJson, "displaySizeX", g_config_parameters.displaySizeX);
				LoadField(configJson, "displaySizeY", g_config_parameters.displaySizeY);
				LoadField(configJson, "logToConsole", g_config_parameters.logToConsole);
				LoadField(configJson, "plotDelaySec", g_config_parameters.plotDelaySec);
				LoadField(configJson, "uiTheme", g_config_parameters.uiTheme);

				Logger::SetConsoleLogging(g_config_parameters.logToConsole);

				// RNG: try to restore state, otherwise use seed
				bool use_seed = true;
				if (configJson.contains("rngState") && configJson["rngState"].is_string()) {
					std::string rng_state_str = configJson["rngState"].get<std::string>();
					if (!rng_state_str.empty()) {
						std::istringstream rng_stream(rng_state_str);
						try {
							rng_stream >> g_random_engine;
							use_seed = false;
							Logger::Info("DataManager: RNG state restored from config.");
						}
						catch (...) {
							Logger::Warning("DataManager: Failed to restore RNG state. Will use seed.");
						}
					}
				}

				if (use_seed) {
					LoadField(configJson, "rngSeed", g_config_parameters.rngSeed);
					if (g_config_parameters.rngSeed == 0) {
						g_config_parameters.rngSeed = 42u;
					}
					g_random_engine.seed(g_config_parameters.rngSeed);
					Logger::Info("DataManager: RNG seeded with: " + std::to_string(g_config_parameters.rngSeed));
				}

				// Load plot states
				if (configJson.contains("plotStates") && configJson["plotStates"].is_object()) {
					for (auto& [key, val] : configJson["plotStates"].items()) {
						PlotState state;
						if (val.contains("legendLocation")) state.legendLocation = val["legendLocation"].get<int>();
						if (val.contains("legendOutside")) state.legendOutside = val["legendOutside"].get<bool>();
						if (val.contains("showLine")) state.showLine = val["showLine"].get<bool>();
						if (val.contains("showMarkers")) state.showMarkers = val["showMarkers"].get<bool>();
						if (val.contains("markerSize")) state.markerSize = val["markerSize"].get<int>();
						g_plot_states[key] = state;
					}
				}

				Logger::Info("DataManager: Configuration loaded successfully from " + filename);
			}
			catch (const json::parse_error& e) {
				Logger::Error("DataManager: JSON parse error in " + filename + ": " + e.what());
				Logger::Warning("DataManager: Using default configuration parameters.");
			}
		}

		return true;
	}

	bool SaveConfigParameters() {
		std::lock_guard<std::mutex> lock(g_config_mutex);
		std::lock_guard<std::mutex> rng_lock(g_rng_mutex);
		std::string filename = g_config_parameters.simulationName + ".json";
		Logger::Info("DataManager: Attempting to save configuration to: " + filename);

		// Build JSON from current parameters
		configJson["simulationName"] = g_config_parameters.simulationName;
		configJson["maxSteps"] = g_config_parameters.maxSteps;
		configJson["dt"] = g_config_parameters.dt;
		configJson["temperature"] = g_config_parameters.temperature;
		configJson["boxSizeX"] = g_config_parameters.boxSizeX;
		configJson["boxSizeY"] = g_config_parameters.boxSizeY;
		configJson["boxSizeZ"] = g_config_parameters.boxSizeZ;
		configJson["numCarbon"] = g_config_parameters.numCarbon;
		configJson["numHydrogen"] = g_config_parameters.numHydrogen;
		configJson["numOxygen"] = g_config_parameters.numOxygen;
		configJson["interactionCutoff"] = g_config_parameters.interactionCutoff;
		configJson["A_form"] = g_config_parameters.A_form;
		configJson["A_break"] = g_config_parameters.A_break;
		configJson["activationFraction"] = g_config_parameters.activationFraction;
		configJson["boundaryType"] = g_config_parameters.boundaryType;
		configJson["rngSeed"] = g_config_parameters.rngSeed;
		configJson["displayPosX"] = g_config_parameters.displayPosX;
		configJson["displayPosY"] = g_config_parameters.displayPosY;
		configJson["displaySizeX"] = g_config_parameters.displaySizeX;
		configJson["displaySizeY"] = g_config_parameters.displaySizeY;
		configJson["logToConsole"] = g_config_parameters.logToConsole;
		configJson["plotDelaySec"] = g_config_parameters.plotDelaySec;
		configJson["uiTheme"] = g_config_parameters.uiTheme;

		// Save plot states
		if (!g_plot_states.empty()) {
			configJson["plotStates"] = nlohmann::json::object();
			for (const auto& [key, state] : g_plot_states) {
				configJson["plotStates"][key] = {
					{"legendLocation", state.legendLocation},
					{"legendOutside", state.legendOutside},
					{"showLine", state.showLine},
					{"showMarkers", state.showMarkers},
					{"markerSize", state.markerSize}
				};
			}
		}

		// Save RNG state
		std::ostringstream rngStream;
		rngStream << g_random_engine;
		configJson["rngState"] = rngStream.str();

		std::ofstream configFile(filename);
		if (!configFile.is_open()) {
			Logger::Error("DataManager: Failed to open configuration file for saving: " + filename);
			return false;
		}

		try {
			configFile << configJson.dump(4);
			Logger::Info("DataManager: Configuration saved successfully to " + filename);
			return true;
		}
		catch (const std::exception& e) {
			Logger::Error("DataManager: Failed to write JSON: " + std::string(e.what()));
			return false;
		}
	}

	// Plot states management
	const PlotStatesMap& GetPlotStates() {
		return g_plot_states;
	}

	PlotStatesMap& GetMutablePlotStates() {
		return g_plot_states;
	}

} // namespace DataManager
