#include <nlohmann/json.hpp> // Add this include for JSON parsing
#include <atomic> // Required for std::atomic
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h" // Added for GetConfigParameters
#include "AgentFactory.h"
#include "Worker.h"
#include "Firm.h"
#include "CentralBank.h"    // Added
#include "CommercialBank.h"

namespace Region {

// Initialize static members
bool AgentFactory::s_isInitialized = false;
const DataManager::SAM* AgentFactory::s_samData = nullptr;
std::atomic<int> AgentFactory::s_nextWorkerId{1000000}; // Start from a large number to avoid collision with numeric string IDs
int AgentFactory::s_nextAgentID = 0; // Added

bool AgentFactory::Initialize()
{
    // Get the SAM data from DataManager
    s_samData = DataManager::GetCurrentSAM();

    if (!s_samData) {
        Logger::Error("AgentFactory::Initialize: No SAM data available");
        return false;
    }

    // Initialize agents' goods system
    int numTypes = s_samData->getnPproducerTypes();
    if (numTypes <= 0) {
        Logger::Error("AgentFactory::Initialize: SAM has no producer types");
        return false;
    }

    // Create a temporary agent to initialize common resources
    Agent tempAgent(0); // Use a default int ID
    if (!tempAgent.InitializeFromSAM(s_samData)) {
        Logger::Error("AgentFactory::Initialize: Failed to initialize goods system");
        return false;
    }

    Logger::Info("AgentFactory::Initialize: Successfully initialized with " +
        std::to_string(numTypes) + " good types");

    s_isInitialized = true;
    s_nextAgentID = 0; // Reset ID counter on initialization
    Logger::Info("AgentFactory::Initialize: AgentFactory initialized.");
    return true;
}

void AgentFactory::Cleanup()
{
    s_samData = nullptr;
    s_isInitialized = false;
    s_nextAgentID = 0; // Reset ID counter on cleanup
    Logger::Info("AgentFactory::Cleanup: Resources released");
    Logger::Info("AgentFactory::Cleanup: AgentFactory cleaned up.");
}

int AgentFactory::GetNextAgentID() {
    return s_nextAgentID++;
}

std::unique_ptr<Worker> AgentFactory::CreateWorker(int id)
{
    if (!s_isInitialized) {
        if (!Initialize()) {
            Logger::Error("AgentFactory::CreateWorker: Factory not initialized");
            return nullptr;
        }
    }
    
    if (id >= s_nextAgentID) s_nextAgentID = id + 1; // Ensure next ID is always fresh

    // Create a new worker
    auto worker = std::make_unique<Worker>(id);
    
    // Initialize it with SAM data
    if (s_samData && !worker->InitializeFromSAM(s_samData)) {
        Logger::Error("AgentFactory::CreateWorker: Failed to initialize worker from SAM data");
        return nullptr;
    }
    
    // Set up default values for the agent and worker-specific values
    SetupAgentDefaults(*worker);
    SetupWorkerDefaults(*worker);
 
	// Adding it to neighborhood will be done in Region CreateWorkers

    //Logger::Info("AgentFactory: Created Worker with ID: " + std::to_string(id));
    return worker;
}

// Added method
std::unique_ptr<CentralBank> AgentFactory::CreateCentralBank(int id, GoodType agentType) {
    if (!s_isInitialized) {
        if (!Initialize()) {
            Logger::Error("AgentFactory::CreateCentralBank: Factory not initialized");
            return nullptr;
        }
    }
    if (id >= s_nextAgentID) s_nextAgentID = id + 1;

    auto bank = std::make_unique<CentralBank>(id, agentType);
    // Central Banks might not need SAM-based goods/price initialization like Firms/Workers.
    // If they do, their InitializeFromSAM should be called.
    // For now, we assume they primarily manage money and don't produce/consume SAM goods.
    // Agent::InitializeFromSAM might still be relevant for CGoods initialization if they hold any 'goods'
    // but their AgentType is distinct.
    
    // Call base Agent SAM initialization if it's generally needed for all agents (e.g. CGoods setup)
    // but be mindful of what it does. For banks, it might not be fully applicable.
    // if (s_samData && !bank->InitializeFromSAM(s_samData)) {
    //     Logger::Error("AgentFactory::CreateCentralBank: Failed to initialize CentralBank from SAM data for ID: " + std::to_string(id));
    //     // return nullptr; // Decide if this is fatal
    // }


    SetupAgentDefaults(*bank); // Setup basic agent defaults (e.g. money, potentially dummy prices if Agent requires)
    // No SetupCentralBankDefaults yet, can be added if specific defaults are needed.

    Logger::Info("AgentFactory: Created CentralBank with ID: " + std::to_string(id) + " of type " + std::to_string(agentType));
    return bank;
}

// Added method
std::unique_ptr<CommercialBank> AgentFactory::CreateCommercialBank(int id, GoodType agentType) {
    if (!s_isInitialized) {
        if (!Initialize()) {
            Logger::Error("AgentFactory::CreateCommercialBank: Factory not initialized");
            return nullptr;
        }
    }
    if (id >= s_nextAgentID) s_nextAgentID = id + 1;
    
    auto bank = std::make_unique<CommercialBank>(id, agentType);
    // Similar to CentralBank, CommercialBanks might not need full SAM-based goods/price init.
    // if (s_samData && !bank->InitializeFromSAM(s_samData)) {
    //    Logger::Error("AgentFactory::CreateCommercialBank: Failed to initialize CommercialBank from SAM data for ID: " + std::to_string(id));
    //    // return nullptr; // Decide if this is fatal
    // }

    SetupAgentDefaults(*bank); // Setup basic agent defaults
    // No SetupCommercialBankDefaults yet.

    Logger::Info("AgentFactory: Created CommercialBank with ID: " + std::to_string(id) + " of type " + std::to_string(agentType));
    return bank;
}

void AgentFactory::SetupAgentDefaults(Agent& agent)
{
    // Here you can set up default prices and quantity coefficients
    // based on the type of agent or other factors
    
    size_t numTypes = CGoods::getNTypes();
    for (size_t i = 0; i < numTypes; ++i) {
        GoodType goodType = static_cast<GoodType>(i);
        
        // Default price set to 1.0 * (i+1)
        double defaultPrice = 1.0;
        agent.SetPrice(goodType, defaultPrice);
    }
}

void AgentFactory::SetupWorkerDefaults(Worker& worker)
{
    // Initialize worker-specific defaults
    worker.SetWorkTime(1.0);           // Start with full work time
    worker.SetSalaryPrice(1.0);        // Default salary price
    worker.SetEmployer(nullptr);       // Start unemployed

    size_t numTypes = CGoods::getNTypes();

    if (!s_samData) {
        Logger::Error("AgentFactory::SetupWorkerDefaults: SAM data is not available");
    } else {
        for (size_t i = 0; i < numTypes; ++i) {
            GoodType goodType = static_cast<GoodType>(i);
            // Assuming household category 0 for all workers for now
            auto householdsIndex = s_samData->getHouseholdsIndex();
            double coef = s_samData->getHouseholdsCoefficient(static_cast<int>(goodType));
            
            worker.SetGoodsPurchaseProportions(goodType, coef);
        }
    }
}

void AgentFactory::SetupFirmDefaults(Firm& firm)
{
    // Initialize firm-specific defaults
    firm.SetSalaryPrice(1.0);         // Default salary price
    firm.ResetWorkedTimeForMonth();   // Start with zero worked time
    firm.SetForSale(0);               // Start with no goods for sale
    
    // Additional setup based on the firm's agent type
    GoodType agentType = firm.GetAgentType();
    
    // Ensure agent type is valid
    if (agentType >= 0 && static_cast<size_t>(agentType) < CGoods::getNTypes()) {
        // Set price for the produced good type to a default value
        firm.SetPrice(agentType, 1.0);
    }
    
   // Logger::Info("AgentFactory::SetupFirmDefaults: Firm " + std::to_string(firm.GetID()) + 
   //             " initialized with default values for agent type " + std::to_string(agentType));
}

} // namespace Region