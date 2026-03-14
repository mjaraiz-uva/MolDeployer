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
		bool showMarkers = false;
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
		double temperature = 288.0;       // Temperature in Kelvin (Earth surface)

		// Simulation box dimensions
		double boxSizeX = 100.0;
		double boxSizeY = 100.0;
		double boxSizeZ = 100.0;

		// Atom counts (budget includes atoms consumed by initial molecules)
		int numCarbon = 20;
		int numHydrogen = 400;
		int numOxygen = 640;

		// Initial molecules (formed at startup from atom pool)
		int initialH2O = 200;             // 200 O + 400 H
		int initialCO2 = 20;              // 20 C + 40 O
		int initialO2 = 200;              // 400 O

		// Interaction and kinetic parameters
		double interactionCutoff = 3.0;       // Cutoff radius for neighbor detection
		double A_form = 1.0;                  // Pre-exponential factor for bond formation
		double A_break = 1.0e-3;              // Pre-exponential factor for bond breaking
		double activationFraction = 0.1;      // E_activation = activationFraction * E_bond

		// Darwinian daemon parameters (riding daemons — every atom has one)
		bool enableDaemons = true;                  // Enable Darwinian assembly daemons
		bool enableStochasticBonds = true;          // Keep existing random bond formation
		int daemonTimeout = 500;                    // Steps without success before timeout
		int atomResupplyInterval = 0;               // Inject fresh atoms every N steps (0=disabled)

		// Radiation parameters (ionizing radiation on unshielded planet surface)
		double radiationFlux = 0.0003;        // Photon flux intensity (pre-ozone Earth UV)
		double radiationMaxEnergy = 600.0;    // Max photon energy kJ/mol (~6.2 eV, 200nm UV)

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
		bool autoStart = false;
		bool testPersistence = false;  // Run persistence test on startup
		float plotDelaySec = 0.01f;
		std::string uiTheme = "system";
		int snapshotInterval = 0;     // Auto-save snapshot every N steps (0=disabled)
		int censusSortMode = 0;       // 0=size (atoms), 1=count
		float uiScale = 1.0f;

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
