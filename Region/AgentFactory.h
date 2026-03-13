#pragma once

#include <string>
#include <memory>
#include <atomic>
#include "Agent.h"
#include "Worker.h"
#include "Firm.h"
#include "CentralBank.h"    // Added
#include "CommercialBank.h" // Added
#include "../DataManager/DataManager.h"

namespace Region {

// Agent Factory class - Creates and manages agents based on SAM data
class AgentFactory 
{
public:
    
    // Factory method for creating workers
    static std::unique_ptr<Worker> CreateWorker(int id);
    
    // Factory method for creating central banks
    static std::unique_ptr<CentralBank> CreateCentralBank(int id, GoodType agentType = -2); // Added, -2 default type
    
    // Factory method for creating commercial banks
    static std::unique_ptr<CommercialBank> CreateCommercialBank(int id, GoodType agentType = -3); // Added, -3 default type
    
    // Initialize the factory with SAM data
    static bool Initialize();
    
    // Cleanup the factory resources
    static void Cleanup();
    
    // Utility to get the next available agent ID
    static int GetNextAgentID(); // Ensure this is public if Region needs it directly
    // Helper method to set up agent with default values
    static void SetupAgentDefaults(Agent& agent);
    
    // Helper method to set up worker with default values
    static void SetupWorkerDefaults(Worker& worker);
    
    // Helper method to set up firm with default values
    static void SetupFirmDefaults(Firm& firm);
    
private:
    
    // Tracks if the factory has been initialized
    static bool s_isInitialized;
    static std::atomic<int> s_nextWorkerId; // For generating unique IDs for workers from JSON if string ID fails
    static int s_nextAgentID; // Static counter for generating unique IDs // Added
    
    // Reference to the SAM data
    static const DataManager::SAM* s_samData;
};

} // namespace Region