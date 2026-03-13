#include "CentralBank.h"
#include "CommercialBank.h" // Added to resolve undefined type error
#include "../Logger/Logger.h" // For logging, if needed
#include "../DataManager/DataManager.h" // For storing time series data
#include "../Simulator/Simulator.h" // For Simulator::GetCurrentMonthCalculated

namespace Region {

    // Constructor
    CentralBank::CentralBank(int id, GoodType agentType)
        : Agent(id, agentType) { // Pass agentType to base Agent constructor
        Logger::Info("CentralBank created with ID: " + std::to_string(id) + ", Type: " + std::to_string(agentType));
    }

    // Destructor
    CentralBank::~CentralBank() {
        Logger::Info("CentralBank destroyed with ID: " + std::to_string(GetID()));
    }

    // monthlyActivity method
    void CentralBank::monthlyActivity() {
        Logger::Info("CentralBank " + std::to_string(GetID()) + " performing monthly activity.");
        
        // Record bank metrics for time series data - use current month from Simulator
        int currentMonth = Simulator::GetCurrentMonthCalculated();
        RecordCentralBankMetrics(currentMonth);
        
        // Future logic for monetary policy, etc.
    }

    // Example implementation for Phase 4
    void CentralBank::AcceptReserveDeposit(CommercialBank* depositingBank, double amount) {
        if (!depositingBank) {
            Logger::Error("CentralBank::AcceptReserveDeposit: Depositing bank is null.");
            return;
        }
        if (amount <= 0) { // Allow zero for logging consistency, but typically positive
            Logger::Warning("CentralBank::AcceptReserveDeposit: Deposit amount should be positive. Amount: " + std::to_string(amount));
            if (amount < 0) return; // Do not process negative amounts
        }
        m_commercialBankReserves[depositingBank->GetID()] += amount;
        AddMoney(amount); // Central bank's money increases
        Logger::Info("CentralBank " + std::to_string(GetID()) + " accepted reserve deposit of " + std::to_string(amount) +
                     " from CommercialBank " + std::to_string(depositingBank->GetID()) + 
                     ". New reserve balance for bank " + std::to_string(depositingBank->GetID()) + ": " + std::to_string(m_commercialBankReserves[depositingBank->GetID()]));
    }
    
    // New methods for central bank data
    double CentralBank::GetReserveBalance(int bankId) const {
        auto it = m_commercialBankReserves.find(bankId);
        if (it != m_commercialBankReserves.end()) {
            return it->second;
        }
        return 0.0; // No reserves found for this bank
    }
    
    double CentralBank::GetTotalReserves() const {
        double total = 0.0;
        for (const auto& reserve : m_commercialBankReserves) {
            total += reserve.second;
        }
        return total;
    }
    
    void CentralBank::RecordCentralBankMetrics(int currentMonth) {
        if (currentMonth < 0) return;
        
        // Record total reserves - this value will be used by commercial banks
        double totalReserves = GetTotalReserves();
        
        // Also record the central bank's total money, which includes reserves and potentially other assets
        double totalMoney = GetMoney();
        
        // Log central bank metrics
        Logger::Info("CentralBank " + std::to_string(GetID()) + 
                     ": Month " + std::to_string(currentMonth) + 
                     ", Total Reserves: " + std::to_string(totalReserves) + 
                     ", Total Money: " + std::to_string(totalMoney));

        // Update DataManager with these values
        DataManager::UpdateCentralBankTotalReservesValue(currentMonth, static_cast<float>(totalReserves));
        DataManager::UpdateCentralBankMoneyValue(currentMonth, static_cast<float>(totalMoney));
    }

} // namespace Region