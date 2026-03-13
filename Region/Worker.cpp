#include <cstdlib>                    // For std::rand, RAND_MAX
#include <vector>
#include <string>
#include <algorithm>                  // Required for std::min/std::max
#include "Worker.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h" // For SAM access
#include "Region.h"                   // For Agent::s_region and Region methods
#include "Firm.h"                     // For dynamic_cast<Firm*> and SetOwner
#include "Agent.h"
#include "AgentFactory.h"             // Include the header file for AgentFactory
#include "CommercialBank.h"           // For banking interactions

namespace Region {

	// Initialize household consumption factor to 1.0
	double Worker::s_HouseholdConsumFactor = 2.0e-2;

	// Constructors
	Worker::Worker()
		: Agent(), m_employer(nullptr), m_lastMonthExpenditure(0.0)
	{
		m_agentType = -1;
		m_availableWorkTime = 1.0; // Full work time available at start
	}
	Worker::Worker(int id)
		: Agent(id), m_employer(nullptr), m_lastMonthExpenditure(0.0)
	{
		m_agentType = -1;
		m_availableWorkTime = 1.0; // Full work time available at start
	}
	// Destructor
	Worker::~Worker() = default;

	// Work time management
	double Worker::GetWorkTime() const
	{
		return m_availableWorkTime;
	}
	void Worker::SetWorkTime(double workTime)
	{
		// Ensure work time is within valid range (0.0 to 1.0)
		if (workTime < 0.0) {
			workTime = 0.0;
			Logger::Warning("Worker::SetWorkTime: Attempted to set negative work time, clamped to 0.0");
		}
		else if (workTime > 1.0) {
			workTime = 1.0;
			Logger::Warning("Worker::SetWorkTime: Attempted to set work time > 1.0, clamped to 1.0");
		}

		m_availableWorkTime = workTime;
	}
	void Worker::ResetWorkTimeForMonth()
	{
		// Reset work time to full capacity at the start of a new month
		m_availableWorkTime = 1.0;
	}

	// Employment management
	bool Worker::IsEmployed() const
	{
		return (m_employer != nullptr);
	}
	Agent* Worker::GetEmployer() const
	{
		return m_employer;
	}
	void Worker::SetEmployer(Agent* employer)
	{
		m_employer = employer;

		if (employer) {
			//   Logger::Info("Worker::SetEmployer: Worker " + std::to_string(GetID())
			//       + " is now employed by " + std::to_string(employer->GetID()));
		}
		else {
			//   Logger::Info("Worker::SetEmployer: Worker " + std::to_string(GetID()) + " is now unemployed");
		}
	}

	// Static getters and setters for household consumption factor
	double Worker::GetHouseholdConsumFactor()
	{
		return s_HouseholdConsumFactor;
	}
	void Worker::SetHouseholdConsumFactor(double factor)
	{
		if (factor < 0.0) {
			Logger::Warning("Worker::SetHouseholdConsumFactor: Cannot set negative factor, using 0.0");
			factor = 0.0;
		}
		s_HouseholdConsumFactor = factor;
		Logger::Info("Worker::SetHouseholdConsumFactor: Set to " + std::to_string(factor));
	}

	// Quantity coefficient management
	double Worker::GetGoodsPurchaseProportions(GoodType goodType) const
	{
		if (goodType < 0 || static_cast<size_t>(goodType) >= m_myGoodsPurchaseProportions.size()) {
			throw std::out_of_range("Worker::GetGoodsPurchaseProportions: GoodType out of range");
		}
		return m_myGoodsPurchaseProportions[static_cast<size_t>(goodType)];
	}
	void Worker::SetGoodsPurchaseProportions(GoodType goodType, double coef)
	{
		if (goodType < 0) {
			throw std::out_of_range("Worker::SetGoodsPurchaseProportions: GoodType is negative");
		}

		// Ensure vector is large enough
		if (static_cast<size_t>(goodType) >= m_myGoodsPurchaseProportions.size()) {
			m_myGoodsPurchaseProportions.resize(static_cast<size_t>(goodType) + 1, 0.0);
		}

		m_myGoodsPurchaseProportions[static_cast<size_t>(goodType)] = coef;
	}
	const std::vector<double>& Worker::GetAllGoodsPurchaseProportions() const
	{
		return m_myGoodsPurchaseProportions;
	}

	// Getter for last month's expenditure
	GoodQtty Worker::GetLastMonthExpenditure() const
	{
		return m_lastMonthExpenditure;
	}
	void Worker::AddToLastMonthExpenditure(GoodQtty amount)
	{
		if (amount < 0) {
			Logger::Error("Worker::AddToLastMonthExpenditure: Attempted to add negative expenditure, ignoring");
			return;
		}
		m_lastMonthExpenditure += amount;
	}
	void Worker::ResetLastMonthExpenditure()
	{
		m_lastMonthExpenditure = 0.0;
	}

	bool Worker::checkestimatedHouseholdPurchaseCost(double estimatedHouseholdPurchaseCost)
	{
		bool ok = true;
		CommercialBank* bank = GetDesignatedCommercialBank();
		if (bank) {
			// Calculate total cash needed for ALL purchases in the purchase list
			double totalCashNeeded = 0.0;

			// Get the purchase list
			const auto& purchaseList = GetPurchaseList();

			// Iterate through each good type
			for (size_t goodType = 0; goodType < purchaseList.size(); ++goodType) {
				GoodQtty quantity = purchaseList[goodType];
				if (quantity > 0) {
					double price = GetPrice(static_cast<GoodType>(goodType));
					totalCashNeeded += quantity * price;
				}
			}

			// Add a buffer for price variations (10% buffer)
			totalCashNeeded *= 1.1;

			// Only withdraw if we don't have enough cash
			if (GetMoney() < totalCashNeeded) {
				double withdrawAmount = totalCashNeeded - GetMoney();
				double currentBankBalance = bank->GetDepositBalance(GetID());

				if (currentBankBalance >= withdrawAmount) {
					Logger::Info("Worker " + std::to_string(GetID()) +
						" withdrawing " + std::to_string(withdrawAmount) +
						" for all purchases (total needed: " + std::to_string(totalCashNeeded) + ")");
					ok = WithdrawFromBank(withdrawAmount);
				}
				else if (currentBankBalance > 0) {
					// Withdraw what's available
					Logger::Info("Worker " + std::to_string(GetID()) +
						" withdrawing all available: " + std::to_string(currentBankBalance));
					ok = WithdrawFromBank(currentBankBalance);

					// Check if loan needed for the rest
					double stillNeeded = totalCashNeeded - GetMoney();
					if (stillNeeded > 0) {
						int termMonths = 12;
						Logger::Info("Worker " + std::to_string(GetID()) +
							" requesting loan for " + std::to_string(stillNeeded));
						ok = RequestLoan(stillNeeded, termMonths);
					}
				}
				else {
					// No bank balance, need full loan
					double loanAmount = totalCashNeeded - GetMoney();
					int termMonths = 12;
					Logger::Info("Worker " + std::to_string(GetID()) +
						" has no bank balance, requesting loan for " + std::to_string(loanAmount));
					ok = RequestLoan(loanAmount, termMonths);
				}
			}
		}
		else {
			ok = false;
			Logger::Error("Worker " + std::to_string(GetID()) +
				": No designated commercial bank for monthly activity.");
		}

		return ok;
	}

	double Worker::GetEquity() const
	{
		// Calculate total equity as cash + bank balance
		CommercialBank* bank = GetDesignatedCommercialBank();
		if (!bank) {
			Logger::Error("Worker " + std::to_string(GetID()) + ": No designated commercial bank for equity calculation.");
			return GetMoney(); // Return only cash if no bank
		}
		double bankBalance = bank->GetDepositBalance(GetID());
		return GetMoney() + bankBalance;
	}

	// Simplified fix for Worker::monthlyActivity()
	// Key changes: 
	// 1. Calculate optimal cash before making purchases
	// 2. Only deposit true excess, keeping adequate liquidity
	// 3. Withdraw if needed BEFORE making purchase list

	void Worker::monthlyActivity()
	{
		int currMonth = s_region->getCurrentMonth();

		// Reset work time for the new month
		ResetWorkTimeForMonth();
		ResetLastMonthExpenditure();

		CommercialBank* bank = GetDesignatedCommercialBank();
		if (!bank) {
			Logger::Error("Worker " + std::to_string(GetID()) + ": No designated commercial bank for monthly activity.");
			return;
		}

		// NEW: Pre-purchase banking adjustments
		double currentCash = GetMoney();
		double currentBankBalance = bank->GetDepositBalance(GetID());
		double totalWealth = GetEquity();

		// Estimate needed cash (based on typical consumption pattern)
		double expectedConsumption = s_HouseholdConsumFactor * 0.70 * totalWealth;
		double neededCash = expectedConsumption * 1.5; // 50% buffer for price variations

		// Adjust cash position if needed
		if (currentCash < neededCash && currentBankBalance > 0) {
			// Need to withdraw
			double withdrawAmount = std::min(neededCash - currentCash, currentBankBalance);
			if (withdrawAmount > 0) {
			//	Logger::Info("Worker " + std::to_string(GetID()) + " withdrawing " +
			//		std::to_string(withdrawAmount) + " for purchases");
				WithdrawFromBank(withdrawAmount);
			}
		}

		// Make purchase list based on current wealth and coefficients
		double estimatedHouseholdPurchaseCost = makePurchaseList();
		bool ok = checkestimatedHouseholdPurchaseCost(estimatedHouseholdPurchaseCost);

		// Go to market to execute planned purchases
		if (ok)
			BuyGoods();
		else
			Logger::Error("Worker " + std::to_string(GetID()) + " could not prepare for market purchases due to insufficient funds.");

		// MODIFIED: More conservative deposit strategy
		// Only deposit if we have significantly more than needed
		double remainingCash = GetMoney();
		double safetyBuffer = estimatedHouseholdPurchaseCost * 2.0; // Keep 2x last purchase cost

		// Only deposit if we have more than 3x the safety buffer
		if (remainingCash > safetyBuffer * 3.0) {
			double depositAmount = remainingCash - safetyBuffer;
			if (depositAmount > 50.0) { // Only deposit if significant
				Logger::Info("Worker " + std::to_string(GetID()) + " depositing surplus: " +
					std::to_string(depositAmount));
				DepositToBank(depositAmount);
			}
		}

		// Log the monthly activity completion for only the worker ID specifyed in the input parameter json file
		if (DataManager::GetConfigParameters().PlotWorkerID == GetID())
			Logger::Info("Worker " + std::to_string(GetID()) + " completed monthly activity. Money: " +
				std::to_string(GetMoney()));
	}

	// Helper method to create a purchase list based on wealth and coefficients
	double Worker::makePurchaseList()
	{
		// Clear any previous purchase plans
		ClearPurchaseList();
		double totalEstimatedCost = 0.0;

		// Skip if worker has no money
		if (GetMoney() <= 0.0) {
			Logger::Error("Worker::makePurchaseList: Worker " + std::to_string(GetID()) + " has no money to make purchases");
			return 0.0;
		}

		// Calculate how much money will be spent on purchases
		CommercialBank* bank = GetDesignatedCommercialBank();
		double currentBankBalance = bank->GetDepositBalance(GetID());
		double wealth = GetMoney() + currentBankBalance;
		double PropToConsume = 0.70; // 70% of disposable is spent on consumption
		double totalSpendingMoney = s_HouseholdConsumFactor * PropToConsume * wealth;

		// Plan purchases based on quantity coefficients
		size_t numTypes = CGoods::getNTypes();
		for (size_t i = 0; i < numTypes; ++i) {
			GoodType goodType = static_cast<GoodType>(i);
			double coef = GetGoodsPurchaseProportions(goodType); // they are normalized to 1 already

			// Skip goods with zero coefficient
			if (coef <= 0.0) continue;

			// Calculate proportion of money to spend on this good
			double spendingForGood = coef * totalSpendingMoney;
			double price = GetPrice(goodType); // Use current price for planning

			// Calculate quantity to potentially buy
			GoodQtty quantityToBuy = 0;
			if (price > 0) { // Avoid division by zero if price is not set or invalid
				quantityToBuy = static_cast<GoodQtty>(spendingForGood / price);
			}

			if (quantityToBuy > 0) {
				AddToPurchaseList(goodType, quantityToBuy);
				totalEstimatedCost += spendingForGood; // spendingForGood is the estimated cost for this item
				//Logger::Info("Worker::makePurchaseList: Worker " + std::to_string(GetID()) +
				//	" added to purchase list: " + std::to_string(quantityToBuy) +
				//	" units of good " + std::to_string(goodType));
			}
		}
		return totalEstimatedCost;
	}

	// Helper method to check if the worker can own more producers
	bool Worker::canOwnMoreProducers() const
	{
		// For now, let's assume a worker can own at most one producer
		return m_ownedProducers.size() < 1;
	}

	// Helper method to add a producer to the worker's owned list
	void Worker::addOwnedProducer(Firm* producer)
	{
		m_ownedProducers.push_back(producer);
	}

	// Business startup
	bool Worker::tryStartNewBusiness() {
		// Conditions for starting a new business
		if (IsEmployed() || !m_ownedProducers.empty()) {
			return false; // Already employed or owns a business
		}

		double probability = DataManager::GetConfigParameters().ProducerStartupProbab; // 0.007
		std::uniform_real_distribution<> dist(0.0, 1.0);
		if (dist(DataManager::GetGlobalRandomEngine()) > probability) {
			return false; // Didn't meet probability threshold
		}

		double initialWorkerMoney = DataManager::GetConfigParameters().nInitSalaries
			* DataManager::GetCurrentSAM()->getInitSalary_mu();
		if (GetEquity() < 0.5 * initialWorkerMoney)
			return false; // Not enough capital

		// Randomly select a sector for the new business
		const DataManager::SAM* sam = DataManager::GetCurrentSAM();
		int nProducerTypes = sam->getnPproducerTypes();
		std::uniform_int_distribution<> sector_dist(0, nProducerTypes - 1);
		GoodType firmType = static_cast<GoodType>(sector_dist(DataManager::GetGlobalRandomEngine()));
		std::string sectorLabel = sam->labelOfIndex(firmType); // Get label for logging/potential use

		// The ID for the new firm will be its future index in the m_firms vector.
		// Region::AddFirm will then push it, making the ID match the index.
		int newFirmId = static_cast<int>(s_region->GetFirmCount());

		Logger::Info("Worker ID " + std::to_string(GetID()) + " is attempting to start a new business in sector: " +
			sectorLabel + " (Type: " + std::to_string(firmType) + ") with proposed Firm ID: " + std::to_string(newFirmId));

		// Add the firm to the region and complete its setup
		// The AddFirm method now handles setting owner, initial money, bank, and adding to collections.
		Firm* addedFirm = s_region->createFirm(sectorLabel, this);

		if (addedFirm) {
			// Money for starting the firm is implicitly handled by AddFirm setting initial capital
			// and potentially the worker investing personal money if that logic were added.
			// For now, the firm gets its initial capital from Region::AddFirm.
			// The worker's money is not directly reduced here unless we add specific investment logic.
			Logger::Info("Worker ID " + std::to_string(GetID()) + " successfully started a new business: Firm ID " +
				std::to_string(addedFirm->GetID()) + " in sector " + sectorLabel);

			return true;
		}
		else {
			Logger::Error("Worker ID " + std::to_string(GetID()) +
				" created a firm with AgentFactory, but Region::AddFirm failed for Firm ID " + std::to_string(newFirmId));
			// The unique_ptr newFirm was moved, so if AddFirm fails, the memory is still managed correctly (deleted).

			return false;
		}
	}

	void Worker::RemoveOwnedProducer(Firm* producerToRemove) {
		if (!producerToRemove) {
			Logger::Error("Worker::RemoveOwnedProducer: Attempted to remove a null producer from worker " + std::to_string(GetID()));
			return;
		}

		auto it = std::find_if(m_ownedProducers.begin(), m_ownedProducers.end(),
			[&](const Firm* ownedProducer) {
				return ownedProducer && ownedProducer->GetID() == producerToRemove->GetID();
			});

		if (it != m_ownedProducers.end()) {
			m_ownedProducers.erase(it);
			Logger::Info("Worker " + std::to_string(GetID()) + " removed owned producer with ID " + std::to_string(producerToRemove->GetID()));
		}
		else {
			Logger::Warning("Worker " + std::to_string(GetID()) + " did not find producer with ID " + std::to_string(producerToRemove->GetID()) + " in its owned list.");
		}
	}

} // namespace Region