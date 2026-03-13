#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <unordered_map>
#include "../ThemeManager/ThemeManager.h"
#include "TimeSeriesRegistry.h"

using json = nlohmann::json;

namespace DataManager {

	// Structure to store plot state information
	struct PlotState {
		int legendLocation = 10;      // ImPlotLocation value
		bool legendOutside = false;
		bool showLine = true;
		bool showMarkers = true;
		int markerSize = 2;
	};

	using PlotStatesMap = std::map<std::string, PlotState>;

	// Configuration parameters for the MolecularDeployer chemistry simulation.
	// Replaces the economic ConfigParameters from DEPX/DEPLOYERS.
	struct ConfigParameters {
		std::string simulationName = "MolSim";

		// Simulation control
		int maxSteps = 100000;            // Total simulation timesteps
		double dt = 1.0;                  // Timestep size
		double temperature = 1000.0;      // Temperature in Kelvin

		// Simulation box dimensions
		double boxSizeX = 100.0;
		double boxSizeY = 100.0;
		double boxSizeZ = 100.0;

		// Atom counts
		int numCarbon = 100;
		int numHydrogen = 400;
		int numOxygen = 50;

		// Interaction and kinetic parameters
		double interactionCutoff = 3.0;       // Cutoff radius for neighbor detection
		double A_form = 1.0;                  // Pre-exponential factor for bond formation
		double A_break = 1.0e-3;              // Pre-exponential factor for bond breaking
		double activationFraction = 0.1;      // E_activation = activationFraction * E_bond

		// Boundary conditions: "reflective" or "periodic"
		std::string boundaryType = "reflective";

		// RNG
		unsigned int rngSeed = 42;

		// Display settings
		int displayPosX = 100;
		int displayPosY = 100;
		int displaySizeX = 1280;
		int displaySizeY = 720;
		bool logToConsole = true;
		float plotDelaySec = 0.01f;
		std::string uiTheme = "dark";

		ConfigParameters() = default;
	};

	const ConfigParameters& GetConfigParameters();
	ConfigParameters& GetMutableConfigParameters();

	bool Init();
	void Shutdown();

	// Unified time series update (delegates to TimeSeriesRegistry)
	void UpdateAllTimeSeriesData(int currentStep);
	void SetNewDataAvailable(bool available);

	// Returns the maximum number of steps
	int GetMaxSteps();

	// Checks if new data is available and resets the flag
	bool CheckAndResetNewDataFlag();

	// Clear all time series data (for simulation restart)
	void ClearAllTimeSeriesValues();

	// Configuration I/O
	bool LoadConfigParameters(const std::string& simulationName);
	const nlohmann::json& GetConfig_json();
	void UpdateDisplaySize(int width, int height);
	void UpdateDisplayPosition(int x, int y);
	bool SaveConfigParameters();

	// Random number generation
	std::mt19937& GetGlobalRandomEngine();
	double getRandom01();
	int getRandomInt(int min, int max);

	// Plot states management
	const PlotStatesMap& GetPlotStates();
	PlotStatesMap& GetMutablePlotStates();

} // namespace DataManager

#endif // DATAMANAGER_H
