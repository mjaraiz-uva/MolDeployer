#pragma once

#include <string>
#include <vector>
#include "Agent.h"
#include "CommercialBank.h" // Added for GetEquity

namespace Region {

	// Forward declaration
	class Worker;

	// Firm class - represents a production agent that employs workers and produces goods
	class Firm : public Agent
	{
	private:

		Worker* m_owner; // Pointer to the owner of the firm

		// List of workers employed by this firm
		std::vector<Worker*> m_employees;

		// Current demand for the produced good
		GoodQtty m_myDemand;
		GoodQtty m_prevDemand; // Previous period demand

		// Desired stock level
		GoodQtty m_myDesiredSupply;

		// Amount to be produced in the current month
		GoodQtty m_toBeProduced;
		GoodQtty m_prevProduced;

		// Intermediate consumption coefficients from SAM
		std::vector<double> m_ICcoeficients;

		// New members for labor integration
		double m_laborCoefficient_CompEmp; // Labor cost (from CompEmployees) per unit of output

		double m_workedTime; // Total worked time by employees in the current month

		GoodQtty m_assistedProduction; // Quantity of goods produced with assistance

		// Quantity of getAgentType() goods available for sale
		GoodQtty m_forSale;

		double m_productionPrice; // Price minimum at which the firm will  sell its product
		void SetProductionPrice(double price) { m_productionPrice = price; };

		int m_WorkDay; // Day of the month when this firm performs its monthlyActivity()

		// Helper method to create a purchase list based on wealth and coefficients
		double makePurchaseList(); // Changed return type from void to double

		// Method to execute purchases in the market: Agent::BuyGoods()

		// Method to update stock reference and decide production quantity
		void updatemyDesiredSupplyAndToBeProduced();

	public:
		// Constructors and destructor
		Firm();
		explicit Firm(int id, GoodType agentType);
		virtual ~Firm();

		// SAM initialization method - overridden from Agent
		bool InitializeFromSAM(const DataManager::SAM* sam) override;

		void SetOwner(Worker* owner);
		Worker* GetOwner() const { return m_owner; }

		// Labor management methods
		double AssessLaborNeeds() const;
		bool NeedsWorkers() const;
		void DismissExcessWorkers();

		// Employee management
		bool AddEmployee(Worker* worker);
		bool RemoveEmployee(Worker* worker);
		size_t GetEmployeeCount() const;
		const std::vector<Worker*>& GetEmployees() const;

		double GetWorkedTime() const;
		void AddWorkedTime(double timeAmount);
		void ResetWorkedTimeForMonth();

		// Production management

		GoodQtty getassistedProduction() const;
		GoodQtty GetForSale() const;
		void SetForSale(GoodQtty quantity);
		void AddForSale(GoodQtty quantity);
		double GetProductionPrice() const { return m_productionPrice; };

		// IC coefficients management
		double GetICCoefficient(GoodType goodType) const;
		void SetICCoefficient(GoodType goodType, double coefficient);
		void LoadICCoefficientsFromSAM(const DataManager::SAM* sam);

		bool checkoperationalReserve(double operationalReserve);

		// New methods for demand and production planning
		GoodQtty GetLastMonthProduction() const;
		void RecordDemand(GoodQtty quantity);
		GoodQtty GetMyDemand() const;
		GoodQtty GetPrevDemand() const;
		void SetPrevDemand(GoodQtty prevDemand);

		void SetmyDesiredSupply(double stockRef);
		double GetmyDesiredSupply() const;
		void SetToBeProduced(GoodQtty toBeProduced);
		GoodQtty GetToBeProduced() const;

		// Override the Sell method to track demand
		bool Sell(GoodType goodType, GoodQtty quantity, double unitPrice) override;

		// Monthly activity implementation - override from Agent
		void monthlyActivity() override;

		// Production operations
		double Produce();
		void PaySalaries();
		double GetTotalSalaryCost() const;
		virtual double GetEquity() const override;

		int GetWorkDay() const;
	};

} // namespace Region