#pragma once

#include <vector>
#include <memory>
#include <string>
#include "Worker.h"
#include "Firm.h"
#include "CentralBank.h" // Added
#include "CommercialBank.h" // Added
#include "CircularNeighborhood.h" // Assuming it's in the same directory or accessible via include paths
#include "../DataManager/DataManager.h"

namespace Region {

	// Forward declaration
	class Agent; // Added forward declaration
	class CentralBank; // Added forward declaration
	class CommercialBank; // Added forward declaration

	void EnsureTimeSeriesRegistration();

	// Region class - represents an economic region (country) with workers and firms
	class Region
	{
	private:
		// Name of this region
		std::string m_name;

		// Collection of workers in this region
		std::vector<std::unique_ptr<Worker>> m_workers;

		// Collection of firms in this region
		std::vector<std::unique_ptr<Firm>> m_firms;

		 // The central bank in this region
		std::unique_ptr<CentralBank> m_centralBank;

		// Collection of commercial banks in this region
		std::vector<std::unique_ptr<CommercialBank>> m_commercialBanks;

		CircularNeighborhood m_circularNeighborhoods;

		// Flag indicating if the region has been initialized
		bool m_initialized;

		// Calculated economic stats after monthly activity  -----------------

		// Total GDP calculated for the current month
		double m_currentGDP;

		// Total unemployment rate
		double m_unemploymentRate;
		double m_prevUnemploymentRate;

		// Total annualized household expenditure
		double m_currentTotalAnnualizedHouseholdExpenditure;

		void AdjustParameters();
		std::vector<double> CalculateTotalHouseholdConsumptionBySector() const;
		std::vector<double> GetTargetHouseholdConsumptionFromSAM() const;

		// Private helper methods  -----------------

		// Helper method to create workers
		bool CreateWorkers(size_t count);

		// Helper method to create a central bank
		bool CreateCentralBank();

		// Helper method to create commercial banks
		bool CreateCommercialBanks(size_t count);

		void RemoveFirmByID(int firmId); // Added method declaration

	public:
		// Constructor and destructor
		Region(const std::string& name = "DefaultRegion");
		~Region();

		// Initialization method
		bool Initialize();

		void initializeMonthlyActivity();

		// Run monthly activities for all agents
		void RunMonthlyActivity(int month);

		// Calculate economic stats after monthly activity
		double GetSimulationUpscaleFactor() const;
		void CalculateStats();
		void SetGDP(double gdp);

		// Getters for region statistics
		double GetGDP() const;
		double GetUnemploymentRate() const;
		double GetTotalAnnualizedHouseholdExpenditure() const;

		// Add these if they don't exist:
	// Fix naming consistency
		double getGDP() const { return GetGDP(); }
		double getTotalHouseholdExpenditure() const { return GetTotalAnnualizedHouseholdExpenditure(); }
		double getTotalCommercialBankDeposits() const;
		double getTotalCommercialBankLoans() const;
		double getTotalCommercialBankCash() const;
		double getTotalCommercialBankReserves() const;
		double getCentralBankTotalReserves() const;
		double getCentralBankTotalMoney() const;
		double getTotalProduction() const;
		double getTotalAssistedProduction() const;
		double getTotalActualProduction() const;

		// Agent management
		size_t GetWorkerCount() const;
		size_t GetFirmCount() const;
		size_t GetCentralBankCount() const; // Returns 1 if a central bank exists, 0 otherwise
		size_t GetCommercialBankCount() const;
		const Worker* GetWorkerByIndex(size_t index) const;
		Worker* GetWorkerByID(int id);
		const Firm* GetFirmByIndex(size_t index) const;
		Firm* GetFirmByID(int agentID) const; // Signature remains the same
		const CentralBank* GetCentralBank() const; // Changed from GetCentralBankByIndex
		const CommercialBank* GetCommercialBankByIndex(size_t index) const;

		// Method to dynamically add a new producer (Firm)
		Firm* createFirm(const std::string& sectorLabel, Worker* firstOwner);

		// Cleanup resources
		void Cleanup();

		const CircularNeighborhood& getCircularNeighborhoods();

		// Method to remove firms with negative equity
		bool RemoveFirmWithNegativeEquity(int firmId);

		// Static method to get the current simulation month
		static int getCurrentMonth();
	};

} // namespace Region