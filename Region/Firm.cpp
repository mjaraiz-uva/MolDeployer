#include <algorithm>
#include <numeric>
#include <random> // Added for random number generation
#include "Firm.h"
#include "Worker.h"
#include "../DataManager/DataManager.h"
#include "../Logger/Logger.h"
#include "../Simulator/Simulator.h"
#include "CommercialBank.h" // For banking interactions

namespace Region {

	// Constructors
	Firm::Firm()
		: Agent(), m_forSale(0),
		m_myDemand(0),
		m_prevDemand(0), m_myDesiredSupply(0.0), m_toBeProduced(0),
		m_prevProduced(0), m_workedTime(0.0),
		m_assistedProduction(0), // Initialize m_assistedProduction
		m_laborCoefficient_CompEmp(0.0) // Initialize m_laborCoefficient_CompEmp
	{
		// Initialize IC coefficients vector
		size_t numTypes = CGoods::getNTypes();
		if (numTypes > 0) {
			m_ICcoeficients.resize(numTypes, 0.0);
		}
		m_WorkDay = DataManager::getRandomInt(0, 20); // Generates a random int in [0, 19]
	}
	Firm::Firm(int id, GoodType agentType)
		: Agent(id, agentType), m_owner(nullptr),
		m_forSale(0),
		m_myDemand(0),
		m_prevDemand(0), m_myDesiredSupply(0.0), m_toBeProduced(0),
		m_prevProduced(0), m_workedTime(0.0),
		m_assistedProduction(0), // Initialize m_assistedProduction
		m_laborCoefficient_CompEmp(0.0) // Initialize m_laborCoefficient_CompEmp
	{
		size_t numTypes = CGoods::getNTypes();
		if (numTypes > 0) {
			m_ICcoeficients.resize(numTypes, 0.0);
		}
		m_WorkDay = DataManager::getRandomInt(0, 20); // Generates a random int in [0, 19]
	}
	// Destructor
	Firm::~Firm() = default;

	// SAM initialization method - overridden from Agent
	bool Firm::InitializeFromSAM(const DataManager::SAM* sam)
	{
		// Call base class initialization first
		if (!Agent::InitializeFromSAM(sam)) {
			return false;
		}

		// Load IC coefficients from SAM
		LoadICCoefficientsFromSAM(sam);

		int laborIndex = sam->indexOfName("CompEmployees");
		m_laborCoefficient_CompEmp = sam->getCompleteColumnCoefficient(laborIndex, m_agentType);

		return true;
	}

	GoodQtty Firm::getassistedProduction() const
	{
		return m_assistedProduction;
	}
	GoodQtty Firm::GetLastMonthProduction() const
	{
		return m_prevProduced;
	}
	// Helper method to record demand for the firm's product
	void Firm::RecordDemand(GoodQtty quantity)
	{
		if (quantity > 0) {
			m_myDemand += quantity;
		}
	}

	// Override Sell method
	bool Firm::Sell(GoodType goodType, GoodQtty quantity, double unitPrice)
	{
		if (quantity <= 0) {
			// Nothing to sell
			return false;
		}

		// Check if agent has enough goods to sell
		if (!CanSell(this, quantity)) {
			return false;
		}

		// Update inventory and money
		double totalRevenue = static_cast<double>(quantity) * unitPrice;
		m_money += totalRevenue;
		this->AddForSale(-quantity); // Subtract sold quantity from inventory

		return true;
	}

	// Production management
	double Firm::GetWorkedTime() const
	{
		return m_workedTime;
	}
	void Firm::AddWorkedTime(double timeAmount)
	{
		if (timeAmount < 0.0) {
			Logger::Error("Firm::AddWorkedTime: Cannot add negative work time");
			return;
		}

		m_workedTime += timeAmount;
		//Logger::Info("Firm::AddWorkedTime: Firm " + std::to_string(GetID()) + " added " + 
		//            std::to_string(timeAmount) + " work time, now at " + std::to_string(m_workedTime));
	}
	void Firm::ResetWorkedTimeForMonth()
	{
		m_workedTime = 0.0;
		//Logger::Info("Firm::ResetWorkedTimeForMonth: Firm " + std::to_string(GetID()) + " work time reset to 0.0");
	}
	GoodQtty Firm::GetForSale() const
	{
		return m_forSale;
	}
	void Firm::SetForSale(GoodQtty quantity)
	{
		if (quantity < 0) {
			Logger::Warning("Firm::SetForSale: Cannot set negative quantity for sale");
			quantity = 0;
		}

		m_forSale = quantity;
	}
	void Firm::AddForSale(GoodQtty quantity)
	{
		if (quantity < 0 && m_forSale < static_cast<GoodQtty>(std::abs(quantity))) {
			Logger::Warning("Firm::AddForSale: Cannot subtract more than available for sale");
			m_forSale = 0;
		}
		else {
			m_forSale += quantity;
		}
	}
	void Firm::SetOwner(Worker* owner)
	{
		if (!owner) {
			Logger::Error("Firm::SetOwner: Attempt to set null owner for Firm ID: " + std::to_string(GetID())); // Added Firm ID to log
			return;
		}
		if (m_owner) {
			Logger::Warning("Firm::SetOwner: Firm ID: " + std::to_string(GetID()) + " overriding existing owner " + std::to_string(m_owner->GetID()) + " with new owner " + std::to_string(owner->GetID())); // Added Firm ID to log
		}
		m_owner = owner;
	}
	double Firm::AssessLaborNeeds() const
	{
		// Calculate labor time needed for planned production
		if (m_toBeProduced <= 0 || m_laborCoefficient_CompEmp <= 0.0) {
			return 0.0;
		}

		double laborCost = GetSalaryPrice() * static_cast<double>(m_toBeProduced * m_laborCoefficient_CompEmp);
		double laborTimeNeeded = laborCost / GetSalary_mu();

		// Calculate current available labor
		double currentLabor = 0.0;
		for (const Worker* worker : m_employees) {
			if (worker) {
				currentLabor += worker->GetWorkTime();
			}
		}

		// Return additional labor needed
		return std::max(0.0, laborTimeNeeded - currentLabor);
	}

	bool Firm::NeedsWorkers() const
	{
		return AssessLaborNeeds() > 0.1; // Need at least 0.1 work time units
	}

	void Firm::DismissExcessWorkers()
	{
		// Calculate how much labor we actually need
		double laborNeeded = 0.0;
		if (m_toBeProduced > 0 && m_laborCoefficient_CompEmp > 0.0) {
			double laborCost = GetSalaryPrice() * static_cast<double>(m_toBeProduced * m_laborCoefficient_CompEmp);
			laborNeeded = laborCost / GetSalary_mu();
		}

		// Calculate current labor and dismiss from the end of the list
		double currentLabor = 0.0;
		std::vector<Worker*> toKeep;

		for (Worker* worker : m_employees) {
			if (worker && currentLabor < laborNeeded * 1.2) { // Keep 20% buffer
				toKeep.push_back(worker);
				currentLabor += worker->GetWorkTime();
			}
			else if (worker) {
				// Dismiss this worker
				worker->SetEmployer(nullptr);
				Logger::Info("Firm " + std::to_string(GetID()) +
					" dismissed Worker " + std::to_string(worker->GetID()));
			}
		}

		m_employees = toKeep;
	}

	GoodQtty Firm::GetMyDemand() const
	{
		return m_myDemand;
	}
	GoodQtty Firm::GetPrevDemand() const
	{
		return m_prevDemand;
	}
	double Firm::GetmyDesiredSupply() const
	{
		return m_myDesiredSupply;
	}
	GoodQtty Firm::GetToBeProduced() const
	{
		return m_toBeProduced;
	}
	void Firm::SetPrevDemand(GoodQtty prevDemand)
	{
		if (prevDemand < 0) {
			Logger::Warning("Firm::SetPrevDemand: Cannot set negative previous demand");
			prevDemand = 0;
		}
		m_prevDemand = prevDemand;
	}
	void Firm::SetmyDesiredSupply(double stockRef)
	{
		if (stockRef < 0.0) {
			Logger::Warning("Firm::SetmyDesiredSupply: Cannot set negative stock reference");
			stockRef = 0.0;
		}
		m_myDesiredSupply = stockRef;
	}
	void Firm::SetToBeProduced(GoodQtty toBeProduced)
	{
		if (toBeProduced < 0) {
			Logger::Warning("Firm::SetToBeProduced: Cannot set negative production quantity");
			toBeProduced = 0;
		}
		m_toBeProduced = toBeProduced;
	}

	// Employee management
	bool Firm::AddEmployee(Worker* worker)
	{
		if (!worker) {
			Logger::Error("Firm::AddEmployee: Attempt to add null worker");
			return false;
		}

		// Check if worker already employed by another firm
		if (worker->IsEmployed()) {
			Logger::Error("Firm::AddEmployee: Worker " + std::to_string(worker->GetID()) +
				" is already employed");
			return false;
		}

		// Add worker to employees
		m_employees.push_back(worker);

		// Set this firm as worker's employer
		worker->SetEmployer(this);

		//Logger::Info("Firm::AddEmployee: Firm " + std::to_string(GetID()) + " hired worker " + std::to_string(worker->GetID()));
		return true;
	}
	bool Firm::RemoveEmployee(Worker* worker)
	{
		if (!worker) {
			Logger::Warning("Firm::RemoveEmployee: Attempt to remove null worker");
			return false;
		}

		auto it = std::find(m_employees.begin(), m_employees.end(), worker);
		if (it == m_employees.end()) {
			Logger::Warning("Firm::RemoveEmployee: Worker " + std::to_string(worker->GetID()) +
				" is not employed by this firm");
			return false;
		}

		// Remove worker from employees
		m_employees.erase(it);

		// Reset worker's employer if it's still this firm
		if (worker->GetEmployer() == this) {
			worker->SetEmployer(nullptr);
		}

		Logger::Info("Firm::RemoveEmployee: Firm " + std::to_string(GetID()) + " removed worker " + std::to_string(worker->GetID()));
		return true;
	}
	size_t Firm::GetEmployeeCount() const
	{
		return m_employees.size();
	}
	const std::vector<Worker*>& Firm::GetEmployees() const
	{
		return m_employees;
	}

	// IC coefficients management
	double Firm::GetICCoefficient(GoodType goodType) const
	{
		if (goodType < 0 || static_cast<size_t>(goodType) >= m_ICcoeficients.size()) {
			Logger::Warning("Firm::GetICCoefficient: Invalid good type " + std::to_string(goodType));
			return 0.0;
		}

		// Return the value from our IC coefficients vector, which was populated from the SAM
		return m_ICcoeficients[static_cast<size_t>(goodType)];
	}
	void Firm::SetICCoefficient(GoodType goodType, double coefficient)
	{
		if (goodType < 0) {
			Logger::Warning("Firm::SetICCoefficient: Invalid negative good type");
			return;
		}

		if (coefficient < 0.0) {
			Logger::Warning("Firm::SetICCoefficient: Cannot set negative coefficient");
			coefficient = 0.0;
		}

		// Ensure vector is large enough
		if (static_cast<size_t>(goodType) >= m_ICcoeficients.size()) {
			m_ICcoeficients.resize(static_cast<size_t>(goodType) + 1, 0.0);
		}

		m_ICcoeficients[static_cast<size_t>(goodType)] = coefficient;
	}
	void Firm::LoadICCoefficientsFromSAM(const DataManager::SAM* sam)
	{
		if (!sam) {
			Logger::Error("Firm::LoadICCoefficientsFromSAM: SAM pointer is null");
			return;
		}

		int numTypes = sam->getnPproducerTypes();
		if (numTypes <= 0) {
			Logger::Error("Firm::LoadICCoefficientsFromSAM: SAM has no producer types");
			return;
		}

		// Resize the IC coefficients vector
		m_ICcoeficients.resize(static_cast<size_t>(numTypes), 0.0);

		// Load IC coefficients from SAM for this firm's agent type
		for (int i = 0; i < numTypes; ++i) {
			m_ICcoeficients[i] = sam->getICCoefficient(m_agentType, i);

			// Log the loaded coefficient values
			if (m_ICcoeficients[i] > 0.0) {
				//    Logger::Info("Firm::LoadICCoefficientsFromSAM: Firm " + std::to_string(GetID()) + 
				//                " - IC coefficient for input " + std::to_string(i) + 
				//                " set to " + std::to_string(m_ICcoeficients[i]));
			}
		}

		//Logger::Info("Firm::LoadICCoefficientsFromSAM: Loaded IC coefficients from SAM for firm " + std::to_string(GetID()));
	}

	bool Firm::checkoperationalReserve(double operationalReserve)
	{
		bool operationalReserveCheck = true; // Assume we comply the reserve
		CommercialBank* bank = GetDesignatedCommercialBank();
		if (bank) {

			// Withdraw funds if cash is below operational reserve and bank balance is available
			if (GetMoney() < operationalReserve) {
				double neededAmount = operationalReserve - GetMoney();
				double currentBankBalance = bank->GetDepositBalance(GetID());
				double withdrawAmount = std::min(neededAmount, currentBankBalance);
				if (withdrawAmount > 0) {
					//Logger::Info("Firm " + std::to_string(GetID()) + " needs cash for operations. Current money: "
					//	+ std::to_string(GetMoney()) + ". Attempting to withdraw: " + std::to_string(withdrawAmount));
					operationalReserveCheck = WithdrawFromBank(withdrawAmount);
				}
			}

			// if still money < operationalReserve request a loan
			if (GetMoney() < operationalReserve) {
				double loanAmount = operationalReserve - GetMoney();
				int termMonths = 24;
				Logger::Info("Firm " + std::to_string(GetID()) + " has low funds: "
					+ std::to_string(GetMoney()) + ". Requesting loan of " + std::to_string(loanAmount));
				operationalReserveCheck = RequestLoan(loanAmount, termMonths);
			}

		}
		else {
			Logger::Error("Firm " + std::to_string(GetID()) + ": No designated commercial bank for monthly activity.");

			operationalReserveCheck = false;

			return operationalReserveCheck;
		}

		return operationalReserveCheck;
	}

	void Firm::monthlyActivity()
	{
		int currMonth = s_region->getCurrentMonth();

		// Reset worked time for the new month
		ResetWorkedTimeForMonth();

		// First, dismiss excess workers if any
		DismissExcessWorkers();

		// Update production plans
		double estimatedFirmPurchaseCost = makePurchaseList();
		double estimatedLaborCost = GetTotalSalaryCost() * 1.5; // Buffer for potential new hires
		double operationalReserve = 4.0 * (estimatedFirmPurchaseCost + estimatedLaborCost);

		// Check operational reserve
		bool operationalReserveCheck = checkoperationalReserve(operationalReserve);

		if (operationalReserveCheck) {
			// Hire workers if needed (before buying goods and producing)
			HireWorkers();

			// Buy intermediate goods
			BuyGoods();

			Produce();

			// Handle surplus money
			double money = GetMoney();
			double depositAmount = std::max(0.0, money - operationalReserve);

			if (depositAmount > 100.0) {
				DepositToBank(depositAmount);
			}
		}
		else {
			Logger::Error("Firm " + std::to_string(GetID()) + ": Insufficient operational reserve");
			return;
		}

		Logger::Info("Firm " + std::to_string(GetID()) + " completed monthly activity. Money: " +
			std::to_string(GetMoney()) + ", For Sale: " + std::to_string(GetForSale()) +
			", Employees: " + std::to_string(GetEmployeeCount()));
	}

	double Firm::makePurchaseList()
	{
		// Clear any previous purchase plans
		ClearPurchaseList();
		double totalEstimatedCost = 0.0;

		// Update stock reference and determine how much to produce
		updatemyDesiredSupplyAndToBeProduced();

		// If nothing to produce, no need to purchase anything
		if (m_toBeProduced == 0) {
			//Logger::Info("Firm::makePurchaseList: Firm " + std::to_string(GetID()) + " has nothing to produce, no purchases needed");
			return 0.0;
		}

		// Calculate needed intermediate consumption goods based on the IC coefficients
		size_t numTypes = CGoods::getNTypes();
		for (size_t i = 0; i < numTypes; ++i) {
			GoodType goodType = static_cast<GoodType>(i);
			double icCoefficient = GetICCoefficient(goodType);

			// Skip goods with zero coefficient
			if (icCoefficient <= 0.0) continue;

			// Calculate quantity needed for planned production
			GoodQtty quantityToBuy = static_cast<GoodQtty>(static_cast<double>(m_toBeProduced) * icCoefficient);

			if (quantityToBuy > 0) {
				// Add to purchase list
				AddToPurchaseList(goodType, quantityToBuy);
				//Logger::Info("Firm::makePurchaseList: Firm " + std::to_string(GetID()) + 
				//            " added to purchase list: " + std::to_string(quantityToBuy) + 
				//            " units of good " + std::to_string(goodType));

				// Estimate cost for this good type
				double price = GetPrice(goodType); // Assuming GetPrice provides a reasonable estimate
				if (price > 0) { // Avoid issues with zero or negative prices in estimation
					totalEstimatedCost += static_cast<double>(quantityToBuy) * price;
				}
			}
		}
		return totalEstimatedCost;
	}

	void Firm::updatemyDesiredSupplyAndToBeProduced()
	{
		// Get the global config parameters for production decisions
		const auto& config = DataManager::GetConfigParameters();

		// Default values if not found in config
		double productionChangeFraction = 0.1; // 10% change allowed per period
		double desiredStockLevelFactor = 1.5;  // 1.5x current demand

		// Log the demand before updating
		//Logger::Info("Firm::updatemyDesiredSupplyAndToBeProduced: Firm " + std::to_string(GetID()) +
		//            " processing demand of " + std::to_string(m_myDemand) + " units");

		// Decide desired Supply level (myDesiredSupply)
		double prevmyDesiredSupply = m_myDesiredSupply;
		double newStockRef = static_cast<double>(m_myDemand) * desiredStockLevelFactor;

		// Calculate the change in stock reference, limited by the allowed change percentage
		double deltamyDesiredSupply = newStockRef - prevmyDesiredSupply;
		double maxDeltamyDesiredSupply = m_myDesiredSupply * productionChangeFraction;
		if (m_myDesiredSupply <= 0)
			maxDeltamyDesiredSupply = m_myDemand;

		// Limit the change to maxDeltamyDesiredSupply in either direction
		deltamyDesiredSupply = std::min(maxDeltamyDesiredSupply, std::max(-maxDeltamyDesiredSupply, deltamyDesiredSupply));

		// Update stock reference
		m_myDesiredSupply = m_myDesiredSupply + deltamyDesiredSupply;

		// Calculate how much to produce this month (stock reference minus current inventory)
		GoodQtty unsold = GetForSale(); // GetGoodQuantity(m_agentType) is IC purchased to own sector firms
		m_toBeProduced = m_assistedProduction + static_cast<GoodQtty>(std::max(0.0, m_myDesiredSupply - static_cast<double>(unsold)));

		if (m_prevProduced > 0)
			m_toBeProduced = std::min<GoodQtty>(m_toBeProduced, m_prevProduced * 1.1); // Limit production to max allowed

		// Keep a copy of these values for next iteration
		m_prevDemand = m_myDemand;
		m_myDemand = 0; // Reset for the new period
		m_assistedProduction = 0;
	}

	// Production operations

	double Firm::Produce()
	{
		GoodQtty quantityToProduce = m_toBeProduced;
		if (quantityToProduce <= 0) {
			return 0;
		}

		// Step 1: Check and constrain by intermediate goods
		GoodQtty actualProduction = quantityToProduce;

		for (GoodType inputGoodType = 0; static_cast<size_t>(inputGoodType) < CGoods::getNTypes(); ++inputGoodType) {
			double icCoefficient = GetICCoefficient(inputGoodType);
			if (icCoefficient > 0.0) {
				GoodQtty neededInput = static_cast<GoodQtty>(std::ceil(static_cast<double>(quantityToProduce) * icCoefficient));
				GoodQtty availableInput = GetGoodQuantity(inputGoodType);

				if (availableInput < neededInput) {
					if (icCoefficient > 1.e-9) {
						actualProduction = std::min(actualProduction,
							static_cast<GoodQtty>(std::floor(static_cast<double>(availableInput) / icCoefficient)));
					}
				}
			}
		}

		// Step 2: Check and constrain by available labor
		double laborCoefficient = m_laborCoefficient_CompEmp;
		if (laborCoefficient > 0.0 && actualProduction > 0) {
			// Calculate total labor time needed for current production level
			double laborCost = GetSalaryPrice() * static_cast<double>(actualProduction * laborCoefficient);
			double laborTimeNeeded = laborCost / GetSalary_mu();

			// Calculate total available labor time from employees
			double totalAvailableLabor = 0.0;
			for (const Worker* worker : m_employees) {
				if (worker) {
					totalAvailableLabor += worker->GetWorkTime();
				}
			}

			// Constrain production by available labor
			if (totalAvailableLabor < laborTimeNeeded && laborTimeNeeded > 0.0) {
				// Scale down production to match available labor
				double laborConstrainedProduction = (totalAvailableLabor / laborTimeNeeded) * actualProduction;
				actualProduction = static_cast<GoodQtty>(std::floor(laborConstrainedProduction));

				Logger::Info("Firm " + std::to_string(GetID()) +
					" production constrained by labor. Needed: " + std::to_string(laborTimeNeeded) +
					" Available: " + std::to_string(totalAvailableLabor) +
					" Reduced production from " + std::to_string(quantityToProduce) +
					" to " + std::to_string(actualProduction));
			}
		}

		// If no labor available, no production
		if (m_employees.empty() && laborCoefficient > 0.0) {
			Logger::Warning("Firm::Produce: Firm " + std::to_string(GetID()) +
				" has no employees, cannot produce");
			actualProduction = 0;
		}

		// Step 3: Consume intermediate inputs (scaled to actual production)
		for (GoodType inputGoodType = 0; static_cast<size_t>(inputGoodType) < CGoods::getNTypes(); ++inputGoodType) {
			double icCoefficient = GetICCoefficient(inputGoodType);
			if (icCoefficient > 0.0 && actualProduction > 0) {
				GoodQtty consumedInput = static_cast<GoodQtty>(std::ceil(static_cast<double>(actualProduction) * icCoefficient));
				if (consumedInput > 0) {
					AddGoodQuantity(inputGoodType, -consumedInput);
					Logger::Info("Firm::Produce: Firm " + std::to_string(GetID()) + " consumed " +
						std::to_string(consumedInput) + " units of input " + CGoods::GoodTypeToName(inputGoodType));
				}
			}
		}

		// Step 4: Use labor and pay salaries (only for actual production)
		double actualLaborCost = GetSalaryPrice() * static_cast<double>(actualProduction * laborCoefficient);
		double actualWorkTime = actualLaborCost / GetSalary_mu();
		AddWorkedTime(actualWorkTime);
		PaySalaries();

		// Step 5: Pay gross operating surplus to owners
		const auto& sam = DataManager::GetCurrentSAM();
		int GrossOpSurplusIndex = sam->indexOfName("GrossOpSurplus");
		double grossOperatingSurplusRate = sam->getCompleteColumnCoefficient(GrossOpSurplusIndex, GetAgentType());
		double grossOperatingSurplus = actualProduction * grossOperatingSurplusRate;
		if (grossOperatingSurplus > 0) {
			AddMoney(-grossOperatingSurplus);
			if (m_owner) {
				m_owner->AddMoney(grossOperatingSurplus);
			}
		}

		// Step 6: Add assisted production
		m_prevProduced = actualProduction;
		double totalProduction = actualProduction;
		double AssistedProductionUpto = DataManager::GetConfigParameters().AssistedProductionUpto;
		double assistedRange = AssistedProductionUpto;
		GoodQtty delta = 0;
		auto currentStep = 1 + Simulator::GetCurrentMonthCalculated();
		if (currentStep <= AssistedProductionUpto) {
			delta = std::max<GoodQtty>(0, GetToBeProduced() - actualProduction)
				* std::max<double>(0, AssistedProductionUpto - currentStep) / assistedRange;
			if (delta > 0) {
				totalProduction += delta;
				m_assistedProduction = delta;
			}
		}

		// Step 7: Add produced goods to inventory
		AddForSale(totalProduction);

		return actualWorkTime;
	}

	void Firm::PaySalaries() {
		double totalworkTimeDue = GetWorkedTime();
		for (Worker* worker : m_employees) {
			if (worker) {
				double initWorkerTime = worker->GetWorkTime();
				double workPerformed = std::min(initWorkerTime, totalworkTimeDue); // Units of work (e.g., 0.0 to 1.0)
				if (workPerformed > 1e-3) {
					double paymentForWorker = workPerformed * worker->GetSalary_mu();

					AddMoney(-paymentForWorker);
					worker->AddMoney(paymentForWorker);
					worker->SetWorkTime(initWorkerTime - workPerformed);
					totalworkTimeDue -= workPerformed;

					// Dismiss surplus employees if no more needed
					if (totalworkTimeDue <= 0.0)
					{
						while (true)
						{
							if (worker == m_employees.back())
								break;
							else
								m_employees.pop_back();
						}
					}

					if (worker == m_employees.back())
						break;
				}
				else {
					// This worker didn't work here
				}
			}
		}
	}

	double Firm::GetTotalSalaryCost() const {
		double totalCost = 0.0;
		for (const auto* worker : m_employees) {
			if (worker) {
				// Assuming GetSalary_mu() is accessible and returns the monthly salary expectation or similar
				// If GetSalary_mu() is a rate per unit of work time, and worker->GetWorkTime() is max time,
				// then this needs to be adjusted based on how salaries are structured.
				// For now, let's assume GetSalary_mu() is the target monthly salary for a full-time worker.
				// This might need refinement based on actual salary mechanics.
				totalCost += worker->GetSalary_mu();
			}
		}
		return totalCost;
	}

	// Equity calculation
	double Firm::GetEquity() const
	{
		double cash = GetMoney();
		double valueOfGoodsForSale = static_cast<double>(GetForSale()) * GetPrice(GetAgentType());
		double totalOutstandingLoanBalance = 0.0;

		CommercialBank* bank = GetDesignatedCommercialBank();
		if (bank) {
			totalOutstandingLoanBalance = bank->GetTotalOutstandingLoanBalanceForAgent(this);
		}

		double equity = cash + valueOfGoodsForSale - totalOutstandingLoanBalance;
		// Logger::Info("Firm::GetEquity: Firm ID " + std::to_string(GetID()) + " Equity = " + std::to_string(cash)
		// + " (cash) + " + std::to_string(valueOfGoodsForSale) + " (goods) - 
		// " + std::to_string(totalOutstandingLoanBalance) + " (loans) = " + std::to_string(equity));
		return equity;
	}

	int Firm::GetWorkDay() const
	{
		return m_WorkDay;
	}

} // namespace Region