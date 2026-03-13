#pragma once

#include "Agent.h"
#include <vector> // Required for std::vector (if used for accounts, loans etc. later)
#include <string> // Required for std::string (if used for logging, etc. later)
#include <map>    // Required for std::map (if used for accounts, loans etc. later)

namespace Region {

    class CommercialBank; // Forward declaration for potential future interactions

    class CentralBank : public Agent {
    public:
        // Constructor
        CentralBank(int id, GoodType agentType = -2); // Default agentType for CentralBank, can be specific if needed

        // Destructor
        ~CentralBank() override;

        // Override Agent's monthly activity method
        void monthlyActivity() override;

        // Example method for interaction with Commercial Banks (Phase 4)
        void AcceptReserveDeposit(CommercialBank* depositingBank, double amount);
        
        // New method to get reserve balance for a specific bank
        double GetReserveBalance(int bankId) const;
        
        // New method to get total reserves held by the central bank
        double GetTotalReserves() const;
        
        // New method to record central bank metrics
        void RecordCentralBankMetrics(int currentMonth);

    private:
        // Example: Data structure to hold reserves from commercial banks (Phase 4)
        std::map<int, double> m_commercialBankReserves; 
    };

} // namespace Region