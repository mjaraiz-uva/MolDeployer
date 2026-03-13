#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

namespace Simulator {

    // Enum for simulation state
    enum SimulationState {
        IDLE,      // Not started or after a full stop/reset
        RUNNING,   // Actively calculating
        PAUSED,    // Calculation paused, can be resumed or stepped
        STEPPING,  // Executing a single step then returning to PAUSED
        COMPLETED  // Calculation finished for all steps
    };

    bool Init();
    void Shutdown();

    // Get a pointer to the Reactor object (as void* for decoupling)
    void* GetReactor();

    void StartAsyncCalculation();
    void PauseCalculation();
    void ResumeCalculation();
    void StepCalculation();
    void RestartCalculation();

    void SetCalculationDelay(float delay_seconds);

    bool IsCalculating();
    SimulationState GetSimulationState();

    // Gets the last timestep that has been calculated (-1 if none)
    int GetCurrentStepCalculated();

    std::string StateToString(SimulationState state);

    // Snapshot persistence: save/load full simulation state (reactor + RNG) to/from file
    bool SaveSnapshot(const std::string& filename);
    bool LoadSnapshot(const std::string& filename);

    // In-process persistence test: verifies that save/load produces identical results
    // Returns true if the test passes. Logs results.
    bool TestPersistence(int saveAtStep = 7000, int verifyAtStep = 8000);

} // namespace Simulator

#endif // SIMULATOR_H
