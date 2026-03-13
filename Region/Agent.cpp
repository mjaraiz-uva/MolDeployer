#include <stdexcept>
#include <algorithm> // For std::shuffle
#include <cassert>
#include <random>    // Still needed for std::shuffle, std::uniform_int_distribution etc.
#include <map>
#include "../Simulator/Simulator.h"
#include "Region.h"
#include "Agent.h"
#include "Firm.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h" // Added for global RNG
#include "CommercialBank.h" // Required for bank interactions

namespace Region {

	// Initialize static member
	Region* Agent::s_region = nullptr;

	void Agent::SetRegion(Region* region) {
		s_region = region;
	}

	Region* Agent::GetRegion() {
		return s_region;
	}

	// Constructors
	Agent::Agent()
		: m_id(0), m_money(0.0), m_myPriceOfSalary(1.0), m_designatedCommercialBank(nullptr)
	{
		EnsureVectorsSize();
	}
	Agent::Agent(int id)
		: m_id(id), m_money(0.0), m_myPriceOfSalary(1.0), m_designatedCommercialBank(nullptr)
	{
		EnsureVectorsSize();
	}
	Agent::Agent(int id, GoodType agentType)
		: m_id(id), m_agentType(agentType), m_myPriceOfSalary(1.0), m_designatedCommercialBank(nullptr)
	{
		EnsureVectorsSize();
	}
	// Destructor
	Agent::~Agent() = default;

	// Agent type management
	GoodType Agent::GetAgentType() const
	{
		return m_agentType;
	}
	void Agent::SetAgentType(GoodType agentType)
	{
		if (agentType < 0 || static_cast<size_t>(agentType) >= CGoods::getNTypes()) {
			Logger::Warning("Agent::SetAgentType: Invalid agent type " + std::to_string(agentType) +
				", must be between 0 and " + std::to_string(CGoods::getNTypes() - 1));
			return;
		}

		m_agentType = agentType;
		Logger::Info("Agent::SetAgentType: Agent " + std::to_string(GetID()) + " now produces good type " +
			std::to_string(agentType) + " (" + CGoods::GoodTypeToName(agentType) + ")");
	}

	// SAM initialization method
	bool Agent::InitializeFromSAM(const DataManager::SAM* sam)
	{
		if (!sam) {
			Logger::Error("Agent::InitializeFromSAM: SAM pointer is null");
			return false;
		}

		// Initialize good types if they haven't been initialized yet
		if (CGoods::getNTypes() == 0) {
			if (!InitializeGoodTypesFromSAM(sam)) {
				return false;
			}
		}

		// Ensure vectors are properly sized now that good types are initialized
		EnsureVectorsSize();

		return true;
	}
	// Helper method to initialize good types from SAM
	bool Agent::InitializeGoodTypesFromSAM(const DataManager::SAM* sam)
	{
		if (!sam) {
			Logger::Error("Agent::InitializeGoodTypesFromSAM: SAM pointer is null");
			return false;
		}

		int numTypes = sam->getnPproducerTypes();
		if (numTypes <= 0) {
			Logger::Error("Agent::InitializeGoodTypesFromSAM: SAM has no producer types");
			return false;
		}

		// Clear existing good definitions
		CGoods::clearGoodsDefinitions();

		// Define good types based on SAM labels
		try {
			const auto& header = sam->getHeaderData();
			//Logger::Info("Agent::InitializeGoodTypesFromSAM: Initializing " + std::to_string(numTypes) +
			//	" good types from SAM for country: " + header.country_name);

			// Get account labels for producers
			for (int i = 0; i < numTypes; ++i) {
				// Get label from SAM (e.g., "P003_Construc")
				std::string label = sam->labelOfIndex(i);

				// Extract the good name (everything after the underscore)
				std::string goodName;
				size_t underscorePos = label.find('_');

				if (underscorePos != std::string::npos && underscorePos + 1 < label.length()) {
					// Extract substring after underscore
					goodName = label.substr(underscorePos + 1);
				}
				else {
					// Fallback if no underscore or nothing after underscore
					goodName = "Good" + std::to_string(i);
					Logger::Warning("Agent::InitializeGoodTypesFromSAM: Could not extract good name from label '" +
						label + "', using default '" + goodName + "'");
				}

				// Define the good type
				CGoods::defineNewGoodType(goodName);
				//Logger::Info("Agent::InitializeGoodTypesFromSAM: Defined good type " +
				//	std::to_string(i) + ": '" + goodName + "' from label '" + label + "'");
			}

			return true;
		}
		catch (const std::exception& e) {
			Logger::Error("Agent::InitializeGoodTypesFromSAM: Exception while initializing good types: " + std::string(e.what()));
			return false;
		}
	}
	// Helper method to ensure vectors are properly sized
	void Agent::EnsureVectorsSize()
	{
		// Get the current number of globally defined good types.
		size_t numTypes = CGoods::getNTypes();

		// If good types are not yet initialized globally, attempt to initialize them.
		if (numTypes == 0) {
			const DataManager::SAM* sam = DataManager::GetCurrentSAM();
			if (sam) {
				try {
					int samProducerTypes = sam->getnPproducerTypes();
					if (samProducerTypes > 0) {
						// Logger::Info("Agent::EnsureVectorsSize: CGoods not initialized, attempting to initialize from SAM.");
						if (InitializeGoodTypesFromSAM(sam)) {
							numTypes = CGoods::getNTypes(); // Re-fetch the number of types after successful initialization.
						} else {
							Logger::Error("Agent::EnsureVectorsSize: Failed to initialize good types from SAM, though SAM and producer types were available. Vectors may be empty.");
							// numTypes remains 0.
						}
					} else {
						Logger::Warning("Agent::EnsureVectorsSize: SAM available but has no producer types. CGoods remains uninitialized. Vectors may be empty.");
						// numTypes remains 0.
					}
				} catch (const std::exception& e) {
					Logger::Error("Agent::EnsureVectorsSize: Exception while accessing SAM for good type initialization: " + std::string(e.what()) + ". Vectors may be empty.");
					// numTypes remains 0.
				}
			} else {
				Logger::Warning("Agent::EnsureVectorsSize: Current SAM is null. CGoods remains uninitialized. Vectors may be empty.");
				// numTypes remains 0.
			}
		}

		// If, after all attempts, no good types are defined, log a warning.
		if (numTypes == 0) {
			Logger::Warning("Agent::EnsureVectorsSize: No good types defined (CGoods::getNTypes() is 0). Agent-specific vectors will be sized to zero or remain empty.");
		}

		// Resize vectors to match the number of good types.
		// If numTypes is 0, vectors will be cleared or sized to 0.
		if (m_myPrice.size() != numTypes) {
			m_myPrice.resize(numTypes, 0.0);
		}

		if (m_purchaseList.size() != numTypes) {
			m_purchaseList.resize(numTypes, 0);
		}

		if (m_lastProviderIDs.size() != numTypes) {
			m_lastProviderIDs.resize(numTypes, -1); // Initialize with -1 (no known provider)
		}
	}

	// Basic getters and setters
	int Agent::GetID() const
	{
		return m_id;
	}
	double Agent::GetMoney() const
	{
		return m_money;
	}
	void Agent::SetMoney(double amount)
	{
		m_money = amount;
	}
	void Agent::AddMoney(double amount)
	{
		try
		{
			if (m_money + amount < 0.0) {
				throw std::runtime_error("Negative money value encountered");
			}
			else {
				m_money += amount;
			}
		}
		catch (const std::exception& e)
		{
			Logger::Error("Agent::AddMoney: Exception while adding money: " + std::string(e.what()));
			exit(-1);
		}
	}

	double Agent::GetEquity() const
	{
		return m_money;
	}

	// Inventory management
	const CGoods& Agent::GetInventory() const
	{
		return m_inventory;
	}
	CGoods& Agent::GetInventoryMutable()
	{
		return m_inventory;
	}
	GoodQtty Agent::GetGoodQuantity(GoodType goodType) const
	{
		return m_inventory(goodType);
	}
	void Agent::SetGoodQuantity(GoodType goodType, GoodQtty quantity)
	{
		m_inventory[goodType] = quantity;
		if (quantity < 0)
		{
			Logger::Warning("Agent::AddGoodQuantity: Attempted to set negative quantity for good type " +
				std::to_string(goodType) + ", setting to 1.e99 instead.");
			m_inventory[goodType] = 1.e18;
		}
	}
	void Agent::AddGoodQuantity(GoodType goodType, GoodQtty quantity)
	{
		m_inventory[goodType] += quantity;
		if (m_inventory[goodType] < 0)
		{
			Logger::Warning("Agent::AddGoodQuantity: Attempted to set negative quantity for good type " +
				std::to_string(goodType) + ", setting to 1.e99 instead.");
			m_inventory[goodType] = 1.e18;
		}
	}

	// Price management
	double Agent::GetPrice(GoodType goodType) const
	{
		return m_myPrice[static_cast<size_t>(goodType)];
	}
	void Agent::SetPrice(GoodType goodType, double price)
	{
		if (goodType < 0) {
			throw std::out_of_range("Agent::SetPrice: GoodType is negative");
		}

		m_myPrice[static_cast<size_t>(goodType)] = price;
	}
	const std::vector<double>& Agent::GetAllPrices() const
	{
		return m_myPrice;
	}

	// Salary management
	double Agent::GetSalaryPrice() const
	{
		return m_myPriceOfSalary;
	}
	double Agent::GetSalary_mu() const
	{
		const DataManager::SAM* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			Logger::Error("Agent::GetSalary_mu: No SAM data available");
			return 0.0;
		}

		double calibFactor = DataManager::GetInitSalaryCalibFactor();
		return m_myPriceOfSalary * sam->getInitSalary_mu() * calibFactor;
	}

	void Agent::SetSalaryPrice(double price)
	{
		if (price < 0.0) {
			Logger::Error("Agent::SetSalaryPrice: Attempted to set negative salary price, using 0.0 instead");
			price = 0.0;
		}

		m_myPriceOfSalary = price;
	}

	// Trading operations
	bool Agent::CanPurchase(GoodType goodType, GoodQtty quantity, double unitPrice) const
	{
		// Check if the agent has enough money to make the purchase
		return (m_money >= (static_cast<double>(quantity) * unitPrice));
	}
	bool Agent::Purchase(GoodType goodType, GoodQtty quantity, double unitPrice)
	{
		if (quantity <= 0) {
			// Nothing to purchase
			return false;
		}

		double totalCost = static_cast<double>(quantity) * unitPrice;

		// Check if agent can afford this purchase
		if (!CanPurchase(goodType, quantity, unitPrice)) {
			return false;
		}

		// Update inventory and money
		m_money -= totalCost;
		if (this->GetAgentType() == -1)
			static_cast<Worker*>(this)->AddToLastMonthExpenditure(totalCost);
		AddGoodQuantity(goodType, quantity);

		return true;
	}
	bool Agent::CanSell(Agent* seller, GoodQtty quantity) const
	{
		// Check if the agent has enough goods to sell
		if (seller->GetAgentType() < 0 || static_cast<size_t>(seller->GetAgentType()) >= CGoods::getNTypes()) {
			Logger::Warning("Agent::CanSell: Invalid agent type for seller " + std::to_string(seller->GetID()));
			return false;
		}
		return (static_cast<Firm*>(seller)->GetForSale() >= quantity);
	}
	bool Agent::Sell(GoodType goodType, GoodQtty quantity, double unitPrice)
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
		AddGoodQuantity(goodType, -quantity);

		return true;
	}

	// Purchase list management
	void Agent::ClearPurchaseList()
	{
		std::fill(m_purchaseList.begin(), m_purchaseList.end(), 0);
		// Alternatively, if resizing is preferred:
		// m_purchaseList.assign(CGoods::getNTypes(), 0);
	}
	void Agent::AddToPurchaseList(GoodType goodType, GoodQtty quantity)
	{
		if (goodType < 0) {
			throw std::out_of_range("Agent::AddToPurchaseList: GoodType is negative");
		}
		size_t index = static_cast<size_t>(goodType);
		if (index >= m_purchaseList.size()) {
			// EnsureVectorsSize should ideally prevent this if called correctly after CGoods is initialized.
			// However, as a safeguard, resize if necessary.
			m_purchaseList.resize(index + 1, 0);
		}
		m_purchaseList[index] += quantity;
	}
	const std::vector<GoodQtty>& Agent::GetPurchaseList() const
	{
		return m_purchaseList;
	}

	int Agent::GetLastProviderID(GoodType goodType) const
	{
		if (goodType < 0 || static_cast<size_t>(goodType) >= m_lastProviderIDs.size()) {
			return -1; // No known provider
		}
		return m_lastProviderIDs[static_cast<size_t>(goodType)];
	}
	void Agent::SetLastProviderID(GoodType goodType, int providerID)
	{
		if (goodType < 0) {
			throw std::out_of_range("Agent::SetLastProviderID: GoodType is negative");
		}
		size_t index = static_cast<size_t>(goodType);
		if (index >= m_lastProviderIDs.size()) {
			m_lastProviderIDs.resize(index + 1, -1);
		}
		m_lastProviderIDs[index] = providerID;
	}

	// Monthly activity to be performed by the agent
	void Agent::monthlyActivity()
	{
		// Derived classes should override this to implement their specific monthly behavior
		// Logger::Info("Agent::monthlyActivity: Base implementation called for agent " + std::to_string(GetID()));
	}

	void Agent::BuyGoods() {

		std::vector<GoodType> purchaseOrder;
		for (GoodType gt = 0; static_cast<size_t>(gt) < m_purchaseList.size(); ++gt) {
			if (m_purchaseList[gt] > 0) {
				purchaseOrder.push_back(gt);
			}
		}

		if (purchaseOrder.size() == 0) {
			//Logger::Info("Agent::BuyGoods: No goods to purchase for agent " + std::to_string(GetID()));  
			return;
		}

		std::shuffle(purchaseOrder.begin(), purchaseOrder.end(), DataManager::GetGlobalRandomEngine()); // Use global RNG

		for (GoodType goodType : purchaseOrder) {
			GoodQtty& quantityToBuy = m_purchaseList[goodType];

			if (quantityToBuy <= 0) continue;

			int lastProviderID = GetLastProviderID(goodType);
			if (lastProviderID != -1) {
				Firm* seller = s_region->GetFirmByID(lastProviderID);
				if (seller && seller != this) {
					AttemptPurchaseFromSeller(seller, goodType, quantityToBuy);
				}
			}

			if (quantityToBuy <= 0) continue;

			const auto& region = static_cast<Region*>(Simulator::GetRegion());
			int maxNeighbors = DataManager::GetConfigParameters().MaxNeighboringProducersPerSector
				* DataManager::GetCurrentSAM()->getnPproducerTypes();
			std::vector<int> neighborFirmIDs =
				region->getCircularNeighborhoods().GetMaxFirmNeighbors(this, maxNeighbors);

			std::shuffle(neighborFirmIDs.begin(), neighborFirmIDs.end(), DataManager::GetGlobalRandomEngine());

			// From Firm ID to Firm*
			for (int sellerID : neighborFirmIDs) {
				if (quantityToBuy <= 0) break;
				Firm* seller = s_region->GetFirmByID(sellerID);
				if (seller && seller != this) {
					AttemptPurchaseFromSeller(seller, goodType, quantityToBuy);
				}
			}
		}
	}

	void Agent::HireWorkers()
	{
		// This method is meant for Firms to hire Workers
		Firm* firm = dynamic_cast<Firm*>(this);
		if (!firm) {
			Logger::Error("Agent::HireWorkers: Only firms can hire workers");
			return;
		}

		// Check if we need workers
		if (!firm->NeedsWorkers()) {
			return;
		}

		double laborNeeded = firm->AssessLaborNeeds();
		if (laborNeeded <= 0.0) {
			return;
		}

		// Get neighbor workers using the circular neighborhood system
		const auto& region = static_cast<Region*>(Simulator::GetRegion());
		int maxNeighbors = DataManager::GetConfigParameters().MaxNeighboringWorkers; // 20; // Same as in BuyGoods
		std::vector<int> neighborWorkerIDs =
			region->getCircularNeighborhoods().GetMaxWorkerNeighbors(this, maxNeighbors);

		// Shuffle to ensure fairness
		std::shuffle(neighborWorkerIDs.begin(), neighborWorkerIDs.end(), DataManager::GetGlobalRandomEngine());

		// Try to hire unemployed workers from neighbors
		int workersHired = 0;
		double laborHired = 0.0;

		for (int workerID : neighborWorkerIDs) {
			if (laborHired >= laborNeeded) {
				break; // Hired enough
			}

			Worker* worker = s_region->GetWorkerByID(workerID);
			if (worker && !worker->IsEmployed()) {
				// Check if we can afford this worker
				double workerSalary = worker->GetSalary_mu();
				double operationalReserve = 4.0 * workerSalary; // Same factor as in checkoperationalReserve

				if (firm->GetMoney() >= operationalReserve) {
					if (firm->AddEmployee(worker)) {
						workersHired++;
						laborHired += worker->GetWorkTime();

						Logger::Info("Firm " + std::to_string(firm->GetID()) +
							" hired Worker " + std::to_string(worker->GetID()));
					}
				}
			}
		}

		if (workersHired > 0) {
			Logger::Info("Agent::HireWorkers: Firm " + std::to_string(GetID()) +
				" hired " + std::to_string(workersHired) + " workers from neighbors");
		}
	}

	bool Agent::AttemptPurchaseFromSeller(Agent* sellerAgent, GoodType goodType, GoodQtty& quantityNeeded) {
		Firm* sellerFirm = dynamic_cast<Firm*>(sellerAgent);
		if (!sellerFirm || quantityNeeded <= 0) {
			return false;
		}

		// If the good being sold is the firm's product, record the demand
		if (sellerFirm->GetAgentType() == goodType)
			sellerFirm->RecordDemand(quantityNeeded);
		else
			return false;

		GoodQtty sellerOfferQuantity = sellerFirm->GetForSale();
		double sellerOfferPrice = sellerFirm->GetPrice(goodType);

		if (sellerOfferQuantity <= 0 || sellerOfferPrice <= 0.0) {
			return false;
		}

		double buyerBidPrice = this->GetPrice(goodType);
		double priceAgreed = 0.0;

		if (buyerBidPrice >= sellerOfferPrice) {
			priceAgreed = sellerOfferPrice;
		}
		else {
			return false;
		}

		GoodQtty maxCanAfford = (priceAgreed > 0) ? static_cast<GoodQtty>(GetMoney() / priceAgreed) : 0;
		GoodQtty quantityToTrade = std::min({ quantityNeeded, sellerOfferQuantity, maxCanAfford });

		if (quantityToTrade > 0) {
			if (sellerFirm->Sell(goodType, quantityToTrade, priceAgreed)) {
				if (this->Purchase(goodType, quantityToTrade, priceAgreed)) {

					quantityNeeded -= quantityToTrade;

					SetLastProviderID(goodType, sellerFirm->GetID());

					// FIX: Accumulate GDP only for final consumption (purchases by Workers).
					// The previous logic incorrectly added intermediate consumption (purchases by Firms).
					if (this->GetAgentType() < 0) { // Buyer is a Worker (household), this is final consumption.
						// Add to GDP of the region
						// The GDP is the sum of final consumption expenditures.
						Region* region = static_cast<Region*>(Simulator::GetRegion());
						if (region) {
							double gdp = region->GetGDP();
							region->SetGDP(gdp + (static_cast<double>(quantityToTrade) * priceAgreed
								* region->GetSimulationUpscaleFactor() * 12.0)); // annualized
						}
					}

					return true;
				}
				else { // If purchase failed, revert seller's sale
					sellerFirm->AddGoodQuantity(goodType, quantityToTrade);
					sellerFirm->AddMoney(-(static_cast<double>(quantityToTrade) * priceAgreed));
				}
			}
		}
		return false;
	}

	void Agent::SetDesignatedCommercialBank(CommercialBank* bank) {
		m_designatedCommercialBank = bank;
		if (bank) {
			//Logger::Info("Agent " + std::to_string(GetID()) + " assigned CommercialBank ID: " + std::to_string(bank->GetID()));
		} else {
			Logger::Warning("Agent " + std::to_string(GetID()) + " assigned nullptr CommercialBank.");
		}
	}

	CommercialBank* Agent::GetDesignatedCommercialBank() const {
		return m_designatedCommercialBank;
	}

	bool Agent::DepositToBank(double amount) {
		if (!m_designatedCommercialBank) {
			Logger::Error("Agent " + std::to_string(GetID()) + ": No designated commercial bank to deposit to.");
			return false;
		}
		if (amount <= 0) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Deposit amount must be positive. Amount: " + std::to_string(amount));
			return false;
		}
		if (GetMoney() < amount) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Insufficient funds to deposit. Available: " + std::to_string(GetMoney()) + ", Requested: " + std::to_string(amount));
			return false;
		}

		if (m_designatedCommercialBank->MakeDeposit(this, amount)) {
			AddMoney(-amount); // Reduce agent's money
			Logger::Info("Agent " + std::to_string(GetID()) + " successfully deposited " + std::to_string(amount) + " to bank " + std::to_string(m_designatedCommercialBank->GetID()));
			return true;
		} else {
			Logger::Error("Agent " + std::to_string(GetID()) + ": Bank failed to process deposit of " + std::to_string(amount));
			return false;
		}
	}

	bool Agent::WithdrawFromBank(double amount) {
		if (!m_designatedCommercialBank) {
			Logger::Error("Agent " + std::to_string(GetID()) + ": No designated commercial bank to withdraw from.");
			return false;
		}
		if (amount <= 0) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Withdrawal amount must be positive. Amount: " + std::to_string(amount));
			return false;
		}

		if (m_designatedCommercialBank->Withdraw(this, amount)) {
			AddMoney(amount); // Increase agent's money
			//Logger::Info("Agent " + std::to_string(GetID()) + " successfully withdrew " + std::to_string(amount) + " from bank " + std::to_string(m_designatedCommercialBank->GetID()));
			return true;
		} else {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Bank failed to process withdrawal of " + std::to_string(amount) + " or insufficient funds in account.");
			return false;
		}
	}

	bool Agent::MakeLoanPayment(double amount) {
		if (amount <= 0) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Loan payment amount must be positive. Amount: " + std::to_string(amount));
			return false;
		}
		if (GetMoney() < amount) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": Insufficient funds for loan payment. Available: " + std::to_string(GetMoney()) + ", Required: " + std::to_string(amount));
			return false;
		}
		AddMoney(-amount);
		Logger::Info("Agent " + std::to_string(GetID()) + " made loan payment of " + std::to_string(amount) + ". Remaining balance: " + std::to_string(GetMoney()));
		return true;
	}

	bool Agent::RequestLoan(double amount, int termMonths) {
		if (!m_designatedCommercialBank) {
			Logger::Warning("Agent " + std::to_string(GetID()) + ": No designated commercial bank to request loan from.");
			return false;
		}
		Logger::Info("Agent " + std::to_string(GetID()) + " requesting loan of " + std::to_string(amount) + " for " + std::to_string(termMonths) + " months.");
		return m_designatedCommercialBank->RequestLoan(this, amount, termMonths);
	}

} // namespace Region