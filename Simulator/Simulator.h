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

} // namespace Simulator

#endif // SIMULATOR_H
