#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

#include "Simulator.h"
#include "../DataManager/DataManager.h"
#include "../DataManager/TimeSeriesRegistry.h"
#include "../Logger/Logger.h"
#include "../Chemistry/Reactor.h"
#include "../Chemistry/Particle.h"

namespace Simulator {

	static bool g_is_initialized = false;
	static std::thread worker_thread;
	static std::atomic<SimulationState> g_simulation_state{ IDLE };
	static std::mutex thread_management_mutex;
	static std::condition_variable cv_step;
	static std::atomic<int> g_current_step_calculated{ -1 };
	static std::atomic<float> g_calculation_delay_ms{ 10.0f }; // Default 10ms

	static std::unique_ptr<Chemistry::Reactor> g_reactor;
	static std::atomic<int> g_start_step{ 0 };  // Supports resume from snapshot

	static void PerformCalculationTask() {
		int startStep = g_start_step.load();
		Logger::Info("Simulator: Calculation task started from step " + std::to_string(startStep) + ".");

		int maxSteps = DataManager::GetMaxSteps();

		for (int step = startStep; step < maxSteps; ++step) {
			std::unique_lock<std::mutex> lock(thread_management_mutex);
			cv_step.wait(lock, [] {
				return g_simulation_state == RUNNING ||
				       g_simulation_state == STEPPING ||
				       g_simulation_state == IDLE;
			});

			if (g_simulation_state == IDLE) {
				Logger::Info("Simulator: Calculation task aborted due to state change to IDLE.");
				lock.unlock();
				break;
			}
			lock.unlock();

			// Perform one simulation timestep
			if (g_reactor) {
				g_reactor->RunTimestep(step);
			}

			// Update time series via the registry
			DataManager::UpdateAllTimeSeriesData(step);
			DataManager::SetNewDataAvailable(true);

			if (step % 1000 == 0 && g_reactor) {
				int bonds = g_reactor->GetBondCount();
				int mols = g_reactor->GetMoleculeCount();
				int freeAtoms = g_reactor->GetFreeAtomCount();
				int totalAtoms = static_cast<int>(g_reactor->GetAtomCount());
				double bondEnergy = g_reactor->GetTotalBondEnergy();
				int formed = g_reactor->GetFormationEventsThisStep();
				int broken = g_reactor->GetBreakingEventsThisStep();
				double avgSize = g_reactor->GetAverageMoleculeSize();
				int maxSize = g_reactor->GetMaxMoleculeSize();
				int freeC = g_reactor->GetFreeAtomCountByElement(Chemistry::Element::C);
				int freeH = g_reactor->GetFreeAtomCountByElement(Chemistry::Element::H);
				int freeO = g_reactor->GetFreeAtomCountByElement(Chemistry::Element::O);

				Logger::Info("=== Step " + std::to_string(step) + " report ===");
				Logger::Info("  Atoms: " + std::to_string(totalAtoms) + " total, "
					+ std::to_string(freeAtoms) + " free ("
					+ std::to_string(freeC) + "C + " + std::to_string(freeH) + "H + " + std::to_string(freeO) + "O), "
					+ std::to_string(totalAtoms - freeAtoms) + " bonded");
				Logger::Info("  Bonds: " + std::to_string(bonds) + ", Energy: "
					+ std::to_string(static_cast<int>(bondEnergy)) + " kJ/mol"
					+ " | this step: +" + std::to_string(formed) + " formed, -" + std::to_string(broken) + " broken");
				Logger::Info("  Molecules: " + std::to_string(mols)
					+ ", avg size: " + std::to_string(avgSize).substr(0, 4)
					+ ", max size: " + std::to_string(maxSize));

				// Species census — sorted by count descending
				const auto& census = g_reactor->GetMoleculeCensus();
				if (!census.empty()) {
					std::vector<std::pair<std::string, int>> sorted(census.begin(), census.end());
					std::sort(sorted.begin(), sorted.end(),
						[](const auto& a, const auto& b) { return a.second > b.second; });

					std::string speciesLine = "  Species: ";
					int shown = 0;
					for (const auto& [formula, count] : sorted) {
						if (shown > 0) speciesLine += ", ";
						speciesLine += formula + "(" + std::to_string(count) + ")";
						if (++shown >= 10) {
							if (static_cast<int>(sorted.size()) > 10)
								speciesLine += " ... +" + std::to_string(sorted.size() - 10) + " more";
							break;
						}
					}
					Logger::Info(speciesLine);
				}
			}

			g_current_step_calculated = step;

			// Apply delay
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(g_calculation_delay_ms.load())));

			// If stepping, change state back to PAUSED
			if (g_simulation_state == STEPPING) {
				g_simulation_state = PAUSED;
			}
		}

		// Check completion
		if (g_simulation_state != IDLE && g_current_step_calculated == maxSteps - 1) {
			g_simulation_state = COMPLETED;
			Logger::Info("========================================");
			Logger::Info("  SIMULATION COMPLETE — " + std::to_string(maxSteps) + " steps");
			Logger::Info("========================================");

			if (g_reactor) {
				int totalAtoms = static_cast<int>(g_reactor->GetAtomCount());
				int freeAtoms = g_reactor->GetFreeAtomCount();
				int bonds = g_reactor->GetBondCount();
				int mols = g_reactor->GetMoleculeCount();
				double bondEnergy = g_reactor->GetTotalBondEnergy();

				Logger::Info("  Final state:");
				Logger::Info("    Atoms: " + std::to_string(totalAtoms) + " total, "
					+ std::to_string(freeAtoms) + " free, "
					+ std::to_string(totalAtoms - freeAtoms) + " bonded ("
					+ std::to_string((totalAtoms - freeAtoms) * 100 / std::max(totalAtoms, 1)) + "%)");
				Logger::Info("    Bonds: " + std::to_string(bonds)
					+ ", Total energy: " + std::to_string(static_cast<int>(bondEnergy)) + " kJ/mol");
				Logger::Info("    Molecules: " + std::to_string(mols)
					+ ", avg size: " + std::to_string(g_reactor->GetAverageMoleculeSize()).substr(0, 4)
					+ ", max size: " + std::to_string(g_reactor->GetMaxMoleculeSize()));

				// Full species census at end
				const auto& census = g_reactor->GetMoleculeCensus();
				if (!census.empty()) {
					std::vector<std::pair<std::string, int>> sorted(census.begin(), census.end());
					std::sort(sorted.begin(), sorted.end(),
						[](const auto& a, const auto& b) { return a.second > b.second; });
					Logger::Info("    All species (" + std::to_string(sorted.size()) + " unique):");
					for (const auto& [formula, count] : sorted) {
						Logger::Info("      " + formula + ": " + std::to_string(count));
					}
				}
			}
			Logger::Info("========================================");

			// Auto-save final snapshot
			int finalStep = g_current_step_calculated.load();
			const auto& cfg = DataManager::GetConfigParameters();
			std::string autoFile = cfg.simulationName + "_" + std::to_string(finalStep) + ".json";
			SaveSnapshot(autoFile);
			Logger::Info("Simulator: Auto-saved final snapshot: " + autoFile);
		}
		else if (g_simulation_state != IDLE && g_simulation_state != COMPLETED) {
			Logger::Warning("Simulator: Calculation task exited prematurely at step " + std::to_string(g_current_step_calculated.load()));
			g_simulation_state = IDLE;
		}
	}

	bool Init() {
		g_simulation_state = IDLE;
		g_current_step_calculated = -1;
		g_calculation_delay_ms = 10.0f;

		// Create and initialize the reactor
		g_reactor = std::make_unique<Chemistry::Reactor>("MolecularReactor");
		if (!g_reactor->Initialize()) {
			Logger::Error("Simulator: Failed to initialize reactor.");
			g_reactor.reset();
			return false;
		}

		// Set the static reactor pointer for all particles
		Chemistry::Particle::SetReactor(g_reactor.get());
		Logger::Info("Simulator: Static reactor pointer set for Particles.");

		g_is_initialized = true;
		Logger::Info("Simulator initialized.");
		return true;
	}

	void Shutdown() {
		{
			std::lock_guard<std::mutex> lock(thread_management_mutex);
			if (g_simulation_state != IDLE && g_simulation_state != COMPLETED) {
				g_simulation_state = IDLE;
			}
		}
		cv_step.notify_all();

		if (worker_thread.joinable()) {
			worker_thread.join();
		}

		if (g_reactor) {
			g_reactor->Cleanup();
			g_reactor.reset();
		}

		g_is_initialized = false;
		Logger::Info("Simulator shutdown.");
	}

	void* GetReactor() {
		return static_cast<void*>(g_reactor.get());
	}

	void StartAsyncCalculation() {
		if (!g_is_initialized) {
			Logger::Error("Simulator not initialized. Cannot start calculation.");
			return;
		}

		SimulationState current_state = g_simulation_state.load();
		if (current_state == RUNNING || current_state == STEPPING) {
			Logger::Warning("Simulator: Calculation already in progress or stepping.");
			return;
		}

		if (worker_thread.joinable()) {
			worker_thread.join();
		}

		if (current_state != PAUSED) {
			g_current_step_calculated = -1;
			g_start_step = 0;
		}

		g_simulation_state = RUNNING;
		worker_thread = std::thread(PerformCalculationTask);
		Logger::Info("Simulator: Asynchronous calculation started/resumed.");
		cv_step.notify_one();
	}

	void PauseCalculation() {
		if (g_simulation_state == RUNNING) {
			g_simulation_state = PAUSED;
			Logger::Info("Simulator: Calculation paused.");
		}
		else {
			Logger::Warning("Simulator: Cannot pause, not in RUNNING state.");
		}
	}

	void ResumeCalculation() {
		if (g_simulation_state == PAUSED) {
			if (!worker_thread.joinable()) {
				// No worker thread (e.g. after loading a snapshot) — start a new one
				g_simulation_state = RUNNING;
				worker_thread = std::thread(PerformCalculationTask);
				Logger::Info("Simulator: Calculation started from loaded snapshot.");
			}
			else {
				// Worker thread exists and is waiting — just wake it up
				{
					std::lock_guard<std::mutex> lock(thread_management_mutex);
					g_simulation_state = RUNNING;
				}
				cv_step.notify_one();
				Logger::Info("Simulator: Calculation resumed.");
			}
		}
		else if (g_simulation_state == IDLE || g_simulation_state == COMPLETED) {
			Logger::Info("Simulator: Resuming from IDLE/COMPLETED, treating as new run.");
			RestartCalculation();
			StartAsyncCalculation();
		}
		else {
			Logger::Warning("Simulator: Cannot resume, not in PAUSED state.");
		}
	}

	void StepCalculation() {
		if (g_simulation_state == PAUSED) {
			if (!worker_thread.joinable()) {
				// No worker thread (e.g. after loading a snapshot) — start one in STEPPING mode
				g_simulation_state = STEPPING;
				worker_thread = std::thread(PerformCalculationTask);
				Logger::Info("Simulator: Stepping from loaded snapshot.");
			}
			else {
				{
					std::lock_guard<std::mutex> lock(thread_management_mutex);
					g_simulation_state = STEPPING;
				}
				cv_step.notify_one();
				Logger::Info("Simulator: Stepping calculation.");
			}
		}
		else {
			Logger::Warning("Simulator: Cannot step, not in PAUSED state.");
		}
	}

	void RestartCalculation() {
		if (!g_is_initialized) {
			Logger::Error("Simulator: Not initialized. Cannot restart.");
			return;
		}

		{
			std::lock_guard<std::mutex> lock(thread_management_mutex);
			if (g_simulation_state != IDLE && g_simulation_state != COMPLETED) {
				g_simulation_state = IDLE;
			}
		}
		cv_step.notify_all();

		if (worker_thread.joinable()) {
			worker_thread.join();
		}

		g_current_step_calculated = -1;
		g_start_step = 0;
		DataManager::ClearAllTimeSeriesValues();

		// Re-initialize the reactor for a fresh start
		if (g_reactor) {
			g_reactor->Cleanup();
			g_reactor->Initialize();
		}

		Logger::Info("Simulator: Calculation reset. Ready for new task.");
		g_simulation_state = IDLE;
	}

	void SetCalculationDelay(float delay_seconds) {
		if (delay_seconds < 0.0f) delay_seconds = 0.0f;
		g_calculation_delay_ms = delay_seconds * 1000.0f;
		Logger::Info("Simulator: Calculation delay set to " + std::to_string(delay_seconds) + "s.");
	}

	bool IsCalculating() {
		SimulationState current_state = g_simulation_state.load();
		return current_state == RUNNING || current_state == STEPPING;
	}

	SimulationState GetSimulationState() {
		return g_simulation_state.load();
	}

	int GetCurrentStepCalculated() {
		return g_current_step_calculated.load();
	}

	std::string StateToString(SimulationState state) {
		switch (state) {
		case IDLE: return "IDLE";
		case RUNNING: return "RUNNING";
		case PAUSED: return "PAUSED";
		case STEPPING: return "STEPPING";
		case COMPLETED: return "COMPLETED";
		default: return "UNKNOWN";
		}
	}

	// --- Snapshot persistence ---

	// Custom JSON serializer: objects indented, arrays on single lines
	static void WriteCompactJson(std::ostream& os, const nlohmann::json& j, int indent = 0) {
		const std::string pad(indent, ' ');
		const std::string pad2(indent + 4, ' ');

		if (j.is_object()) {
			os << "{\n";
			bool first = true;
			for (auto it = j.begin(); it != j.end(); ++it) {
				if (!first) os << ",\n";
				first = false;
				os << pad2 << "\"" << it.key() << "\": ";
				WriteCompactJson(os, it.value(), indent + 4);
			}
			os << "\n" << pad << "}";
		}
		else if (j.is_array() && !j.empty() && !j[0].is_object()) {
			// Flat arrays (numbers, strings): single line
			os << j.dump();
		}
		else if (j.is_array()) {
			// Arrays of objects: indented
			os << "[\n";
			for (size_t i = 0; i < j.size(); ++i) {
				if (i > 0) os << ",\n";
				os << pad2;
				WriteCompactJson(os, j[i], indent + 4);
			}
			os << "\n" << pad << "]";
		}
		else {
			os << j.dump();
		}
	}

	bool SaveSnapshot(const std::string& filename) {
		if (!g_reactor) {
			Logger::Error("Simulator::SaveSnapshot: No reactor to save.");
			return false;
		}

		nlohmann::json snap;
		snap["snapshotVersion"] = 1;

		// Save configuration parameters
		const auto& config = DataManager::GetConfigParameters();
		nlohmann::json cfgJson;
		cfgJson["simulationName"] = config.simulationName;
		cfgJson["maxSteps"] = config.maxSteps;
		cfgJson["dt"] = config.dt;
		cfgJson["temperature"] = config.temperature;
		cfgJson["boxSizeX"] = config.boxSizeX;
		cfgJson["boxSizeY"] = config.boxSizeY;
		cfgJson["boxSizeZ"] = config.boxSizeZ;
		cfgJson["numCarbon"] = config.numCarbon;
		cfgJson["numHydrogen"] = config.numHydrogen;
		cfgJson["numOxygen"] = config.numOxygen;
		cfgJson["interactionCutoff"] = config.interactionCutoff;
		cfgJson["A_form"] = config.A_form;
		cfgJson["A_break"] = config.A_break;
		cfgJson["activationFraction"] = config.activationFraction;
		cfgJson["enableDaemons"] = config.enableDaemons;
		cfgJson["enableStochasticBonds"] = config.enableStochasticBonds;
		cfgJson["daemonTimeout"] = config.daemonTimeout;
		cfgJson["boundaryType"] = config.boundaryType;
		cfgJson["rngSeed"] = config.rngSeed;
		cfgJson["plotDelaySec"] = config.plotDelaySec;
		cfgJson["displayPosX"] = config.displayPosX;
		cfgJson["displayPosY"] = config.displayPosY;
		cfgJson["displaySizeX"] = config.displaySizeX;
		cfgJson["displaySizeY"] = config.displaySizeY;
		cfgJson["logToConsole"] = config.logToConsole;
		cfgJson["autoStart"] = config.autoStart;
		cfgJson["uiTheme"] = config.uiTheme;
		snap["config"] = cfgJson;

		snap["reactor"] = g_reactor->SaveSnapshot();

		// Save RNG state
		std::ostringstream rngStream;
		rngStream << DataManager::GetGlobalRandomEngine();
		snap["rngState"] = rngStream.str();

		// Save time series data for plot continuity
		int currentStep = g_reactor->GetCurrentStep();
		snap["timeSeries"] = DataManager::TimeSeriesRegistry::GetInstance().SaveAllSeriesData(currentStep);

		std::ofstream file(filename);
		if (!file.is_open()) {
			Logger::Error("Simulator::SaveSnapshot: Failed to open file: " + filename);
			return false;
		}
		WriteCompactJson(file, snap);
		Logger::Info("Simulator: Snapshot saved to " + filename);
		return true;
	}

	bool LoadSnapshot(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			Logger::Error("Simulator::LoadSnapshot: Failed to open file: " + filename);
			return false;
		}

		nlohmann::json snap;
		try {
			file >> snap;
		}
		catch (const nlohmann::json::parse_error& e) {
			Logger::Error("Simulator::LoadSnapshot: JSON parse error: " + std::string(e.what()));
			return false;
		}

		// Restore configuration parameters before loading reactor (reactor reads from DataManager config)
		if (snap.contains("config") && snap["config"].is_object()) {
			const auto& cfgJson = snap["config"];
			auto& cfg = DataManager::GetMutableConfigParameters();
			if (cfgJson.contains("simulationName")) cfg.simulationName = cfgJson["simulationName"].get<std::string>();
			if (cfgJson.contains("maxSteps")) cfg.maxSteps = cfgJson["maxSteps"].get<int>();
			if (cfgJson.contains("dt")) cfg.dt = cfgJson["dt"].get<double>();
			if (cfgJson.contains("temperature")) cfg.temperature = cfgJson["temperature"].get<double>();
			if (cfgJson.contains("boxSizeX")) cfg.boxSizeX = cfgJson["boxSizeX"].get<double>();
			if (cfgJson.contains("boxSizeY")) cfg.boxSizeY = cfgJson["boxSizeY"].get<double>();
			if (cfgJson.contains("boxSizeZ")) cfg.boxSizeZ = cfgJson["boxSizeZ"].get<double>();
			if (cfgJson.contains("numCarbon")) cfg.numCarbon = cfgJson["numCarbon"].get<int>();
			if (cfgJson.contains("numHydrogen")) cfg.numHydrogen = cfgJson["numHydrogen"].get<int>();
			if (cfgJson.contains("numOxygen")) cfg.numOxygen = cfgJson["numOxygen"].get<int>();
			if (cfgJson.contains("interactionCutoff")) cfg.interactionCutoff = cfgJson["interactionCutoff"].get<double>();
			if (cfgJson.contains("A_form")) cfg.A_form = cfgJson["A_form"].get<double>();
			if (cfgJson.contains("A_break")) cfg.A_break = cfgJson["A_break"].get<double>();
			if (cfgJson.contains("activationFraction")) cfg.activationFraction = cfgJson["activationFraction"].get<double>();
			if (cfgJson.contains("enableDaemons")) cfg.enableDaemons = cfgJson["enableDaemons"].get<bool>();
			if (cfgJson.contains("enableStochasticBonds")) cfg.enableStochasticBonds = cfgJson["enableStochasticBonds"].get<bool>();
			if (cfgJson.contains("daemonTimeout")) cfg.daemonTimeout = cfgJson["daemonTimeout"].get<int>();
			if (cfgJson.contains("boundaryType")) cfg.boundaryType = cfgJson["boundaryType"].get<std::string>();
			if (cfgJson.contains("rngSeed")) cfg.rngSeed = cfgJson["rngSeed"].get<unsigned int>();
			if (cfgJson.contains("plotDelaySec")) cfg.plotDelaySec = cfgJson["plotDelaySec"].get<float>();
			if (cfgJson.contains("displayPosX")) cfg.displayPosX = cfgJson["displayPosX"].get<int>();
			if (cfgJson.contains("displayPosY")) cfg.displayPosY = cfgJson["displayPosY"].get<int>();
			if (cfgJson.contains("displaySizeX")) cfg.displaySizeX = cfgJson["displaySizeX"].get<int>();
			if (cfgJson.contains("displaySizeY")) cfg.displaySizeY = cfgJson["displaySizeY"].get<int>();
			if (cfgJson.contains("logToConsole")) cfg.logToConsole = cfgJson["logToConsole"].get<bool>();
			if (cfgJson.contains("autoStart")) cfg.autoStart = cfgJson["autoStart"].get<bool>();
			if (cfgJson.contains("uiTheme")) cfg.uiTheme = cfgJson["uiTheme"].get<std::string>();

			// Re-initialize time series registry for potentially new maxSteps
			DataManager::TimeSeriesRegistry::GetInstance().InitializeAllSeries(cfg.maxSteps);
		}

		if (!g_reactor) {
			g_reactor = std::make_unique<Chemistry::Reactor>("MolecularReactor");
		}

		if (!g_reactor->LoadSnapshot(snap["reactor"])) {
			Logger::Error("Simulator::LoadSnapshot: Reactor failed to load snapshot.");
			return false;
		}

		Chemistry::Particle::SetReactor(g_reactor.get());

		// Restore RNG state
		if (snap.contains("rngState") && snap["rngState"].is_string()) {
			std::istringstream rngStream(snap["rngState"].get<std::string>());
			rngStream >> DataManager::GetGlobalRandomEngine();
		}

		// Restore time series data for plot continuity
		if (snap.contains("timeSeries") && snap["timeSeries"].is_object()) {
			DataManager::TimeSeriesRegistry::GetInstance().LoadAllSeriesData(snap["timeSeries"]);
		}

		// Set start step so PerformCalculationTask resumes from the right place
		int savedStep = snap["reactor"]["currentStep"].get<int>();
		g_start_step = savedStep + 1;
		g_current_step_calculated = savedStep;

		// Set state to PAUSED so UI shows Resume button and correct step
		g_simulation_state = PAUSED;

		// Signal that plot data is available for rendering
		DataManager::SetNewDataAvailable(true);

		Logger::Info("Simulator: Snapshot loaded from " + filename + " at step " + std::to_string(savedStep));
		return true;
	}

	// --- Persistence test ---

	// Helper: log a brief reactor state summary and return it as a string for comparison
	static std::string ReactorSummary(const Chemistry::Reactor& reactor) {
		int totalAtoms = static_cast<int>(reactor.GetAtomCount());
		int freeAtoms = reactor.GetFreeAtomCount();
		int bonds = reactor.GetBondCount();
		int mols = reactor.GetMoleculeCount();
		double energy = reactor.GetTotalBondEnergy();
		double avgSize = reactor.GetAverageMoleculeSize();
		int maxSize = reactor.GetMaxMoleculeSize();

		std::ostringstream ss;
		ss << "atoms=" << totalAtoms
			<< " free=" << freeAtoms
			<< " bonds=" << bonds
			<< " mols=" << mols
			<< " energy=" << static_cast<int>(energy)
			<< " avgSize=" << avgSize
			<< " maxSize=" << maxSize;

		// Add species census
		const auto& census = reactor.GetMoleculeCensus();
		if (!census.empty()) {
			std::vector<std::pair<std::string, int>> sorted(census.begin(), census.end());
			std::sort(sorted.begin(), sorted.end(),
				[](const auto& a, const auto& b) { return a.first < b.first; });
			for (const auto& [formula, count] : sorted) {
				ss << " " << formula << "=" << count;
			}
		}

		return ss.str();
	}

	// Helper: compare positions of first N atoms between two reactors
	static bool CompareAtomPositions(const Chemistry::Reactor& a, const Chemistry::Reactor& b, int count) {
		int n = std::min(count, static_cast<int>(std::min(a.GetAtomCount(), b.GetAtomCount())));
		for (int i = 0; i < n; ++i) {
			const Chemistry::Atom* atomA = a.GetAtomByIndex(i);
			const Chemistry::Atom* atomB = b.GetAtomByIndex(i);
			if (!atomA || !atomB) return false;
			const auto& pA = atomA->GetPosition();
			const auto& pB = atomB->GetPosition();
			if (pA.x != pB.x || pA.y != pB.y || pA.z != pB.z) return false;
			const auto& vA = atomA->GetVelocity();
			const auto& vB = atomB->GetVelocity();
			if (vA.x != vB.x || vA.y != vB.y || vA.z != vB.z) return false;
		}
		return true;
	}

	bool TestPersistence(int saveAtStep, int verifyAtStep) {
		Logger::Info("========================================");
		Logger::Info("  PERSISTENCE TEST BEGIN");
		Logger::Info("  Save at step " + std::to_string(saveAtStep)
			+ ", verify at step " + std::to_string(verifyAtStep));
		Logger::Info("========================================");

		auto& rng = DataManager::GetGlobalRandomEngine();
		const auto& config = DataManager::GetConfigParameters();

		// --- Phase 1: Reference run (fresh, seed 42 → step verifyAtStep) ---
		Logger::Info("Phase 1: Reference run from seed to step " + std::to_string(verifyAtStep));
		rng.seed(config.rngSeed);

		Chemistry::Reactor refReactor("RefReactor");
		Chemistry::Particle::SetReactor(&refReactor);
		refReactor.Initialize();

		for (int step = 0; step < verifyAtStep; ++step) {
			refReactor.RunTimestep(step);
		}
		std::string refSummary = ReactorSummary(refReactor);
		Logger::Info("  Reference state at step " + std::to_string(verifyAtStep) + ": " + refSummary);

		// --- Phase 2: Run to saveAtStep, save snapshot + RNG ---
		Logger::Info("Phase 2: Run from seed to step " + std::to_string(saveAtStep) + " and save snapshot");
		rng.seed(config.rngSeed);

		Chemistry::Reactor saveReactor("SaveReactor");
		Chemistry::Particle::SetReactor(&saveReactor);
		saveReactor.Initialize();

		for (int step = 0; step < saveAtStep; ++step) {
			saveReactor.RunTimestep(step);
		}

		// Save reactor snapshot
		nlohmann::json snapshot = saveReactor.SaveSnapshot();

		// Save RNG state
		std::ostringstream rngStream;
		rngStream << rng;
		std::string savedRng = rngStream.str();

		Logger::Info("  Snapshot saved at step " + std::to_string(saveAtStep)
			+ ": " + ReactorSummary(saveReactor));

		// --- Phase 3: Load snapshot, restore RNG, continue to verifyAtStep ---
		Logger::Info("Phase 3: Load snapshot and continue to step " + std::to_string(verifyAtStep));

		// Restore RNG state
		std::istringstream rngIn(savedRng);
		rngIn >> rng;

		Chemistry::Reactor loadedReactor("LoadedReactor");
		Chemistry::Particle::SetReactor(&loadedReactor);
		loadedReactor.LoadSnapshot(snapshot);

		for (int step = saveAtStep; step < verifyAtStep; ++step) {
			loadedReactor.RunTimestep(step);
		}
		std::string loadedSummary = ReactorSummary(loadedReactor);
		Logger::Info("  Loaded+continued state at step " + std::to_string(verifyAtStep) + ": " + loadedSummary);

		// --- Phase 4: Compare ---
		Logger::Info("========================================");
		bool summaryMatch = (refSummary == loadedSummary);
		bool positionsMatch = CompareAtomPositions(refReactor, loadedReactor,
			static_cast<int>(refReactor.GetAtomCount()));

		if (summaryMatch && positionsMatch) {
			Logger::Info("  PERSISTENCE TEST PASSED");
			Logger::Info("  All statistics and atom positions match exactly.");
		}
		else {
			Logger::Error("  PERSISTENCE TEST FAILED");
			if (!summaryMatch) {
				Logger::Error("  Summary mismatch!");
				Logger::Error("    Reference: " + refSummary);
				Logger::Error("    Loaded:    " + loadedSummary);
			}
			if (!positionsMatch) {
				Logger::Error("  Atom positions/velocities mismatch!");
			}
		}
		Logger::Info("========================================");

		// Restore the main reactor pointer
		if (g_reactor) {
			Chemistry::Particle::SetReactor(g_reactor.get());
		}

		return summaryMatch && positionsMatch;
	}

} // namespace Simulator
