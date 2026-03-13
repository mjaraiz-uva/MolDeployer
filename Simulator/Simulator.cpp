#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "Simulator.h"
#include "../DataManager/DataManager.h"
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

	static void PerformCalculationTask() {
		Logger::Info("Simulator: Calculation task started.");

		int maxSteps = DataManager::GetMaxSteps();

		for (int step = 0; step < maxSteps; ++step) {
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

			if (step % 1000 == 0) {
				Logger::Info("Simulator: Step " + std::to_string(step) +
					" - Bonds: " + std::to_string(g_reactor ? g_reactor->GetBondCount() : 0) +
					", Molecules: " + std::to_string(g_reactor ? g_reactor->GetMoleculeCount() : 0) +
					", Free atoms: " + std::to_string(g_reactor ? g_reactor->GetFreeAtomCount() : 0));
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
			Logger::Info("Simulator: Calculation task complete. " + std::to_string(maxSteps) + " steps calculated.");
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
			{
				std::lock_guard<std::mutex> lock(thread_management_mutex);
				g_simulation_state = RUNNING;
			}
			cv_step.notify_one();
			Logger::Info("Simulator: Calculation resumed.");
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
			{
				std::lock_guard<std::mutex> lock(thread_management_mutex);
				g_simulation_state = STEPPING;
			}
			cv_step.notify_one();
			Logger::Info("Simulator: Stepping calculation.");
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

} // namespace Simulator
