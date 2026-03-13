#include <sstream>
#include <algorithm>
#include <cstdlib> // Add for std::rand
#include <vector>  // Added for std::vector
#include <random>  // Added for std::random_device, std::mt19937, std::shuffle
#include <cmath>   // Added for std::ceil (though integer arithmetic for ranges might be used)
#include <numeric> // For std::accumulate if needed
#include "Region.h"
#include "AgentFactory.h"
#include "../Logger/Logger.h"
#include "Worker.h" // Include for m_workers
#include "Firm.h"   // Include for m_firms
#include "CentralBank.h" // Added
#include "CommercialBank.h" // Added
#include "Agent.h"  // Include for Agent* return type and casting
#include "../DataManager/DataManager.h" // For SAM and GetGlobalRandomEngine
#include "../Simulator/Simulator.h" // For getCurrentMonth

namespace Region {

	// Constructor
	Region::Region(const std::string& name)
		: m_name(name), m_initialized(false), m_currentGDP(0.0), m_unemploymentRate(0.0), m_currentTotalAnnualizedHouseholdExpenditure(0.0)
	{
		Logger::Info("Region::Region: Created region '" + m_name + "'");
	}

	// Destructor
	Region::~Region()
	{
		Cleanup();
		Logger::Info("Region::~Region: Destroyed region '" + m_name + "'");
	}

	// Initialize the region with workers, firms, and banks
	bool Region::Initialize()
	{
		if (m_initialized) {
			Logger::Warning("Region::Initialize: Region '" + m_name + "' already initialized");
			// Even if already initialized, we might be re-initializing (e.g. simulation restart)
			// Clear existing agents from neighborhood lists to prevent warnings/errors.
			m_circularNeighborhoods.Clear();
		}
		else {
			// If not initialized before, clear is not strictly necessary but harmless.
			// Or, if Clear() is lightweight, call it unconditionally.
			// For now, let's assume Clear() is fine to call even if lists are already empty.
			m_circularNeighborhoods.Clear();
		}

		// Initialize the AgentFactory
		if (!AgentFactory::Initialize()) {
			Logger::Error("Region::Initialize: Failed to initialize AgentFactory");
			return false;
		}
		Agent::SetRegion(this); // Set the region for all agents

		// Create workers (20 as specified)
		if (!CreateWorkers(static_cast<size_t>(std::max(0.0f, std::round(DataManager::GetConfigParameters().Nworkers))))) {
			Logger::Error("Region::Initialize: Failed to create workers");
			AgentFactory::Cleanup();
			return false;
		}

		// Create a central bank
		if (!CreateCentralBank()) {
			Logger::Error("Region::Initialize: Failed to create central bank");
			AgentFactory::Cleanup();
			return false;
		}

		// Create commercial banks
		if (!CreateCommercialBanks(DataManager::GetConfigParameters().Ncommercialbanks)) { // Assuming Ncommercialbanks in config
			Logger::Error("Region::Initialize: Failed to create commercial banks");
			AgentFactory::Cleanup();
			return false;
		}

		// Assign the commercial bank to all agents
		if (!m_commercialBanks.empty()) {
			CommercialBank* primaryCommercialBank = m_commercialBanks[0].get(); // Assuming one for now
			if (primaryCommercialBank) {
				for (auto& worker : m_workers) {
					worker->SetDesignatedCommercialBank(primaryCommercialBank);
				}
				for (auto& firm : m_firms) {
					firm->SetDesignatedCommercialBank(primaryCommercialBank);
				}
				//Logger::Info("Region::Initialize: Assigned Commercial Bank ID " + std::to_string(primaryCommercialBank->GetID()) + " to all workers and firms.");
			}
			else {
				Logger::Error("Region::Initialize: Primary commercial bank is null, cannot assign to agents.");
			}
		}
		else {
			Logger::Warning("Region::Initialize: No commercial banks created, cannot assign to agents.");
		}

		// Assign Central Bank to Commercial Banks
		if (m_centralBank && !m_commercialBanks.empty()) {
			CentralBank* cb = m_centralBank.get();
			if (cb) {
				for (auto& commercialBank : m_commercialBanks) {
					if (commercialBank) {
						commercialBank->SetCentralBank(cb);
						Logger::Info("Region::Initialize: Assigned Central Bank ID " + std::to_string(cb->GetID()) +
							" to Commercial Bank ID " + std::to_string(commercialBank->GetID()));
					}
				}
			}
			else {
				Logger::Error("Region::Initialize: Central bank instance is null, cannot assign to commercial banks.");
			}
		}
		else {
			if (!m_centralBank) {
				Logger::Warning("Region::Initialize: No central bank created, cannot assign to commercial banks.");
			}
			if (m_commercialBanks.empty()) {
				Logger::Warning("Region::Initialize: No commercial banks created, cannot be assigned a central bank.");
			}
		}

		m_initialized = true;
		Logger::Info("Region::Initialize: Region '" + m_name + "' initialized with " +
			std::to_string(m_workers.size()) + " workers, " +
			std::to_string(m_firms.size()) + " firms, " +
			(m_centralBank ? "1" : "0") + " central bank(s), and " +
			std::to_string(m_commercialBanks.size()) + " commercial bank(s).");

		return true;
	}

	// Cleanup resources
	void Region::Cleanup()
	{
		// Clear workers and firms
		m_workers.clear();
		m_firms.clear();
		m_centralBank.reset();
		m_commercialBanks.clear(); // Added

		// Cleanup agent factory resources
		AgentFactory::Cleanup();

		m_initialized = false;
		Logger::Info("Region::Cleanup: Region '" + m_name + "' resources released");
	}

	// Static method to get the current simulation month
	int Region::getCurrentMonth()
	{
		return 1 + Simulator::GetCurrentMonthCalculated();
	}

	const CircularNeighborhood& Region::getCircularNeighborhoods() { return m_circularNeighborhoods; }

	double Region::GetSimulationUpscaleFactor() const
	{
		GoodQtty activePopulation = DataManager::GetCurrentSAM()->getHeaderData().active_population;
		GoodQtty nWorkersConfig = DataManager::GetConfigParameters().Nworkers;
		if (nWorkersConfig <= 0) {
			Logger::Error("Region::GetSimulationUpscaleFactor: Nworkers is zero or negative, cannot calculate upscale factor. Returning 1.0.");
			return 1.0;
		}
		double upscaleFactor = static_cast<double>(activePopulation) / static_cast<double>(nWorkersConfig);
		return upscaleFactor;
	}

	void Region::SetGDP(double gdp)
	{
		try {
			if (gdp < 0) {
				throw std::invalid_argument("GDP cannot be negative");
			}
			m_currentGDP = gdp;
		}
		catch (const std::exception& e) {
			Logger::Error("Region::SetGDP: " + std::string(e.what()));
		}
	}

	// Getters for region statistics
	double Region::GetGDP() const
	{
		return m_currentGDP;
	}

	double Region::GetUnemploymentRate() const
	{
		return m_unemploymentRate;
	}

	double Region::GetTotalAnnualizedHouseholdExpenditure() const
	{
		return m_currentTotalAnnualizedHouseholdExpenditure;
	}

	// In Region.cpp, add these implementations:

	double Region::getTotalCommercialBankDeposits() const {
		double total = 0.0;
		for (const auto& bank : m_commercialBanks) {
			if (bank) {
				total += bank->GetTotalDeposits();
			}
		}
		return total;
	}

	double Region::getTotalCommercialBankLoans() const {
		double total = 0.0;
		for (const auto& bank : m_commercialBanks) {
			if (bank) {
				total += bank->GetTotalLoans();
			}
		}
		return total;
	}

	double Region::getTotalCommercialBankCash() const {
		double total = 0.0;
		for (const auto& bank : m_commercialBanks) {
			if (bank) {
				total += bank->GetMoney();
			}
		}
		return total;
	}

	double Region::getTotalCommercialBankReserves() const {
		if (m_centralBank) {
			return m_centralBank->GetTotalReserves();
		}
		return 0.0;
	}

	double Region::getCentralBankTotalReserves() const {
		if (m_centralBank) {
			return m_centralBank->GetTotalReserves();
		}
		return 0.0;
	}

	double Region::getCentralBankTotalMoney() const {
		if (m_centralBank) {
			return m_centralBank->GetMoney();
		}
		return 0.0;
	}

	double Region::getTotalProduction() const {
		double total = 0.0;
		for (const auto& firm : m_firms) {
			if (firm) {
				total += firm->GetLastMonthProduction();
			}
		}
		return total * GetSimulationUpscaleFactor();
	}

	double Region::getTotalAssistedProduction() const {
		// You'll need to implement this based on your Firm class
		// This might track production that received some form of assistance
		double total = 0.0;
		for (const auto& firm : m_firms) {
			if (firm) {
				// Assuming there's a method to get assisted production
				// total += firm->GetAssistedProduction();
			}
		}
		return total * GetSimulationUpscaleFactor();
	}

	double Region::getTotalActualProduction() const {
		// Similar to getTotalProduction but might exclude certain types
		double total = 0.0;
		for (const auto& firm : m_firms) {
			if (firm) {
				// Assuming there's a method to get actual production
				// total += firm->GetActualProduction();
			}
		}
		return total * GetSimulationUpscaleFactor();
	}

	// Agent management
	size_t Region::GetWorkerCount() const
	{
		return m_workers.size();
	}

	size_t Region::GetFirmCount() const
	{
		return m_firms.size();
	}

	size_t Region::GetCentralBankCount() const // Added
	{
		return m_centralBank ? 1 : 0;
	}

	size_t Region::GetCommercialBankCount() const // Added
	{
		return m_commercialBanks.size();
	}

	const Worker* Region::GetWorkerByIndex(size_t index) const
	{
		if (index >= m_workers.size()) {
			return nullptr;
		}
		return m_workers[index].get();
	}

	const Firm* Region::GetFirmByIndex(size_t index) const
	{
		if (index >= m_firms.size()) {
			Logger::Warning("Region::GetFirmByIndex: Index out of bounds: " + std::to_string(index));
			return nullptr;
		}
		return m_firms[index].get();
	}

	const CentralBank* Region::GetCentralBank() const // Changed from GetCentralBankByIndex
	{
		return m_centralBank.get();
	}

	const CommercialBank* Region::GetCommercialBankByIndex(size_t index) const // Added
	{
		if (index >= m_commercialBanks.size()) {
			Logger::Warning("Region::GetCommercialBankByIndex: Index out of bounds: " + std::to_string(index));
			return nullptr;
		}
		return m_commercialBanks[index].get();
	}

	Worker* Region::GetWorkerByID(int id)
	{
		if (id >= 0 && static_cast<size_t>(id) < m_workers.size()) {
			return m_workers[id].get();
		}
		return nullptr;
	}

	Firm* Region::GetFirmByID(int agentID) const
	{
		if (agentID < 0 || static_cast<size_t>(agentID) >= m_firms.size()) {
			Logger::Warning("Region::GetFirmByID: Firm ID " + std::to_string(agentID) + " is out of bounds. Max index: " + (m_firms.empty() ? "N/A" : std::to_string(m_firms.size() - 1)));
			return nullptr;
		}

		Firm* firm = m_firms[static_cast<size_t>(agentID)].get();

		if (!firm) {
			//Logger::Info("Region::GetFirmByID: No firm found at index " +
			// std::to_string(agentID) + " (firm may have been removed or not initialized).");
			return nullptr;
		}

		if (firm->GetID() != agentID) {
			Logger::Error("Region::GetFirmByID: ID mismatch. Firm at index " + std::to_string(agentID) + " has ID " + std::to_string(firm->GetID()) + ". This indicates an issue with ID assignment or firm list management.");
			return nullptr;
		}

		return firm;
	}

	// Helper method to create workers
	bool Region::CreateWorkers(size_t count)
	{
		m_workers.reserve(m_workers.size() + count);

		const auto* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			Logger::Error("Region::CreateWorkers: Failed to get SAM data.");
			return false;
		}
		int nPproducerTypes = sam->getnPproducerTypes();

		for (size_t i = 0; i < count; ++i) {
			int newWorkerId = static_cast<int>(m_workers.size());

			auto worker = AgentFactory::CreateWorker(newWorkerId);
			if (!worker) {
				Logger::Error("Region::CreateWorkers: Failed to create worker with numeric ID "
					+ std::to_string(newWorkerId));
				return false;
			}

			double initialWorkerMoney = DataManager::GetConfigParameters().nInitSalaries
				* DataManager::GetCurrentSAM()->getInitSalary_mu();
			worker->SetMoney(initialWorkerMoney);

			m_workers.push_back(std::move(worker));
			m_circularNeighborhoods.AddWorker(newWorkerId);
		}
		return true;
	}

	// Helper method to create a central bank
	bool Region::CreateCentralBank()
	{
		if (m_centralBank) {
			Logger::Warning("Region::CreateCentralBank: Central bank already exists.");
			return true;
		}

		int centralBankId = AgentFactory::GetNextAgentID();
		GoodType centralBankType = -2;

		auto centralBank = AgentFactory::CreateCentralBank(centralBankId, centralBankType);
		if (!centralBank) {
			Logger::Error("Region::CreateCentralBank: Failed to create central bank with ID " 
				+ std::to_string(centralBankId));
			return false;
		}

		const DataManager::SAM* sam = DataManager::GetCurrentSAM();
		double initialMoney = DataManager::GetConfigParameters().MonetaryBasePerActiveSalaries
			* sam->getInitSalary_mu() * DataManager::GetNworkers();
		centralBank->SetMoney(initialMoney);

		m_centralBank = std::move(centralBank);

		Logger::Info("Region::CreateCentralBank: Created central bank with ID " +
			std::to_string(m_centralBank->GetID()) + " and initial money " + std::to_string(initialMoney));

		return true;
	}

	// Helper method to create commercial banks
	bool Region::CreateCommercialBanks(size_t count)
	{
		if (count == 0) {
			Logger::Info("Region::CreateCommercialBanks: Requested to create 0 commercial banks.");
			return true;
		}

		m_commercialBanks.reserve(m_commercialBanks.size() + count);

		for (size_t i = 0; i < count; ++i) {
			int newBankId = AgentFactory::GetNextAgentID();
			GoodType bankType = -3;

			auto commercialBank = AgentFactory::CreateCommercialBank(newBankId, bankType);
			if (!commercialBank) {
				Logger::Error("Region::CreateCommercialBanks: Failed to create commercial bank with ID "
					+ std::to_string(newBankId));
				continue;
			}

			const DataManager::SAM* sam = DataManager::GetCurrentSAM();
			double initialMoney = DataManager::GetConfigParameters().MonetaryBasePerActiveSalaries
				* sam->getInitSalary_mu() * DataManager::GetNworkers();
			commercialBank->SetMoney(initialMoney);

			Logger::Info("Region::CreateCommercialBanks: Created commercial bank with ID " +
				std::to_string(commercialBank->GetID()) + " and initial money "
				+ std::to_string(initialMoney));

			m_commercialBanks.push_back(std::move(commercialBank));
		}
		return true;
	}

	// Method to dynamically create a new producer (Firm)
	Firm* Region::createFirm(const std::string& sectorLabel, Worker* firstOwner)
	{
		const DataManager::SAM* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			Logger::Error("Region::createFirm: Failed to get SAM data.");
			return nullptr;
		}

		int producerTypeIndex = sam->indexOfLabel(sectorLabel);
		if (producerTypeIndex < 0 || static_cast<size_t>(producerTypeIndex) >= static_cast<size_t>(sam->getnPproducerTypes())) {
			Logger::Error("Region::createFirm: Sector label '" + sectorLabel +
				"' does not correspond to a valid producer type index (" + std::to_string(producerTypeIndex) + "). Max type index: " + std::to_string(sam->getnPproducerTypes() - 1));
			return nullptr;
		}

		int newFirmId = static_cast<int>(m_firms.size());
		GoodType firmType = static_cast<GoodType>(producerTypeIndex);

		auto firm = std::make_unique<Firm>(newFirmId, firmType);
		if (!firm) {
			Logger::Error("Region::createFirm: AgentFactory failed to create firm for sector: " + sectorLabel +
				" with ID " + std::to_string(newFirmId));
			return nullptr;
		}

		// Initialize it with SAM data
		if (sam && !firm->InitializeFromSAM(sam)) {
			Logger::Error("AgentFactory::CreateFirm: Failed to initialize firm from SAM data for ID: " + std::to_string(newFirmId));
			return nullptr;
		}

		// Set up default values for the agent and firm-specific values
		AgentFactory::SetupAgentDefaults(*firm);
		AgentFactory::SetupFirmDefaults(*firm);

		firm->SetOwner(firstOwner); // Set the first owner of the firm
		firstOwner->addOwnedProducer(firm.get()); // Add to worker's list of owned producers

		double initialFirmMoney = 10.0 * DataManager::GetConfigParameters().nInitSalaries
			* DataManager::GetCurrentSAM()->getInitSalary_mu();
		firm->SetMoney(initialFirmMoney);

		if (!m_commercialBanks.empty() && m_commercialBanks[0]) {
			firm->SetDesignatedCommercialBank(m_commercialBanks[0].get());
			//Logger::Info("Region::createFirm: Assigned Commercial Bank ID " + std::to_string(m_commercialBanks[0]->GetID()) + " to new firm ID " + std::to_string(newFirmId));
		}
		else {
			Logger::Error("Region::createFirm: No commercial bank available to assign to new firm ID " + std::to_string(newFirmId));
		}

		//Logger::Info("Region::createFirm: Created new firm (ID: " + std::to_string(newFirmId) +
		//	") of type " + std::to_string(firmType) + " (Sector: " + sectorLabel + ")" +
		//	" with initial money " + std::to_string(initialMoney));

		Firm* newFirm = firm.get();
		m_firms.push_back(std::move(firm));
		m_circularNeighborhoods.AddFirm(newFirmId);

		return newFirm;
	}

	// Implementation of RemoveFirmByID method
	void Region::RemoveFirmByID(int firmId) {
		auto it = std::find_if(m_firms.begin(), m_firms.end(), [firmId](const std::unique_ptr<Firm>& firm_ptr) {
			return firm_ptr && firm_ptr->GetID() == firmId;
			});

		if (it != m_firms.end()) {
			Firm* firm_to_remove = it->get(); // Get raw pointer before moving/erasing

			Logger::Info("Region::RemoveFirmByID: Preparing to remove firm with ID " + std::to_string(firmId));

			// Handle Employees
			if (firm_to_remove) {
				std::vector<Worker*> employees_copy = firm_to_remove->GetEmployees(); // Create a copy for safe iteration
				Logger::Info("Region::RemoveFirmByID: Firm ID " + std::to_string(firmId) + " has " + std::to_string(employees_copy.size()) + " employees to remove.");
				for (Worker* worker : employees_copy) {
					if (worker) {
						Logger::Info("Region::RemoveFirmByID: Removing employee ID " + std::to_string(worker->GetID()) + " from firm ID " + std::to_string(firmId));
						firm_to_remove->RemoveEmployee(worker); // This should set worker's employer to nullptr
					}
				}

				// Handle Ownership
				Worker* owner = firm_to_remove->GetOwner();
				if (owner) {
					Logger::Info("Region::RemoveFirmByID: Firm ID " + std::to_string(firmId) + " is owned by worker ID " + std::to_string(owner->GetID()) + ". Notifying owner.");
					// Assuming Worker class has a method like RemoveOwnedProducer or similar.
					// If not, this part of the logic would need to be added to the Worker class.
					// For now, we'll assume such a method exists or the owner relationship is implicitly handled
					// by the firm's destruction or other mechanisms.
					owner->RemoveOwnedProducer(firm_to_remove);
				}

				// Handle Banking Relationships
				CommercialBank* bank = firm_to_remove->GetDesignatedCommercialBank();
				if (bank) {
					Logger::Info("Region::RemoveFirmByID: Clearing bank records for firm ID " + std::to_string(firmId) + " at bank ID " + std::to_string(bank->GetID()));
					bank->ClearAgentRecords(firmId);
				}
				else {
					Logger::Info("Region::RemoveFirmByID: Firm ID " + std::to_string(firmId) + " has no designated commercial bank.");
				}
			}

			Logger::Info("Region::RemoveFirmByID: Removing firm with ID " + std::to_string(firmId) + " from region collections.");
			m_circularNeighborhoods.RemoveFirm(firmId); // Remove from neighborhoods
			//m_firms.erase(it); // Erase the firm
			it->reset(); // Reset the unique_ptr to release the firm

			Logger::Info("Region::RemoveFirmByID: Firm with ID " + std::to_string(firmId) + " successfully removed.");
		}
		else {
			Logger::Warning("Region::RemoveFirmByID: Firm with ID " + std::to_string(firmId) + " not found.");
		}
	}

	// Run monthly activities for all agents  ---------------------------------------------------

	void Region::initializeMonthlyActivity()
	{
		if (!m_initialized) {
			Logger::Error("Region::initializeMonthlyActivity: Region not initialized");
			return;
		}

		// Reset GDP and other statistics
		SetGDP(0.0);
		m_currentTotalAnnualizedHouseholdExpenditure = 0.0;

		// Dismantle and start new firms if needed   ------------------------------------

		// Check for and remove firms with negative equity before processing monthly activities
		std::vector<int> firmsToRemove;
		double initialFirmMoney = 10.0 * DataManager::GetConfigParameters().nInitSalaries
			* DataManager::GetCurrentSAM()->getInitSalary_mu();;
		for (const auto& firm : m_firms) {
			if (getCurrentMonth() > 24 && firm && firm->GetEquity() < 0.5 * initialFirmMoney) {
				firmsToRemove.push_back(firm->GetID());
			}
		}

		// Remove firms with negative equity
		if (!firmsToRemove.empty()) {
			Logger::Info("Region::RunMonthlyActivity: Found " + std::to_string(firmsToRemove.size()) +
				" firms with negative equity to remove");
			for (int firmId : firmsToRemove) {
				RemoveFirmByID(firmId);
			}
		}

		// Then let workers try to start new firms
		for (auto& worker : m_workers) {
			worker->tryStartNewBusiness();
		}
	}

	void Region::RunMonthlyActivity(int month)
	{
		initializeMonthlyActivity();

		Logger::Info("Region::RunMonthlyActivity: Starting month " + std::to_string(month) +
			" activities for region '" + m_name + "'");

		// Prepare active agent lists
		std::vector<Worker*> activeWorkers;
		activeWorkers.reserve(m_workers.size());
		for (const auto& worker_ptr : m_workers) {
			if (worker_ptr) {
				activeWorkers.push_back(worker_ptr.get());
			}
		}

		std::vector<Firm*> activeFirms;
		activeFirms.reserve(m_firms.size());
		for (const auto& firm_ptr : m_firms) {
			if (firm_ptr) {
				activeFirms.push_back(firm_ptr.get());
			}
		}

		// Shuffle agents using DataManager's global RNG for deterministic behavior
		std::shuffle(activeWorkers.begin(), activeWorkers.end(), DataManager::GetGlobalRandomEngine());
		std::shuffle(activeFirms.begin(), activeFirms.end(), DataManager::GetGlobalRandomEngine());

		const int workDaysPerMonth = 20; // Or from config: DataManager::GetConfigParameters().WorkDaysPerMonth;

		size_t numActiveWorkers = activeWorkers.size();
		size_t numActiveFirms = activeFirms.size();

		Logger::Info("Region::RunMonthlyActivity: Processing " + std::to_string(numActiveWorkers) + " workers and " +
			std::to_string(numActiveFirms) + " firms over " + std::to_string(workDaysPerMonth) + " workdays.");

		for (int workDayN = 0; workDayN < workDaysPerMonth; ++workDayN) {
			// Process a subset of workers for this workday
			// Integer division ensures that all workers are processed over the month
			size_t worker_start_index = (numActiveWorkers * workDayN) / workDaysPerMonth;
			size_t worker_end_index = (numActiveWorkers * (workDayN + 1)) / workDaysPerMonth;
			for (size_t i = worker_start_index; i < worker_end_index; ++i) {
				if (activeWorkers[i]) {
					activeWorkers[i]->monthlyActivity();
				}
			}

			// Process firms whose m_WorkDay matches the current workDayN
			for (Firm* firm : activeFirms) {
				if (firm && firm->GetWorkDay() == workDayN) {
					firm->monthlyActivity();
				}
			}
		}

		// Central bank performs its monthly activity (after all worker/firm daily activities)
		if (m_centralBank) {
			m_centralBank->monthlyActivity();
		}

		// Commercial banks perform their monthly activity
		for (auto& commercialBank : m_commercialBanks) {
			if (commercialBank) { // Ensure commercialBank is not null
				commercialBank->monthlyActivity();
			}
		}

		// Calculate economic statistics after all monthly activities
		CalculateStats();

		// Adjust parameters for calibration
		AdjustParameters();

		Logger::Info("Region::RunMonthlyActivity: Completed month " + std::to_string(month) +
			" activities for region '" + m_name + "', GDP: " + std::to_string(m_currentGDP));
	}

	void Region::AdjustParameters()
	{
		// Only adjust during calibration period
		int currentMonth = getCurrentMonth();
		const auto& config = DataManager::GetConfigParameters();
		if (currentMonth > config.FinishCalibrationAt) {
			return;
		}

		// Get actual and target consumption values
		std::vector<double> actualConsumption = CalculateTotalHouseholdConsumptionBySector();
		std::vector<double> targetConsumption = GetTargetHouseholdConsumptionFromSAM();

		const auto* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			Logger::Error("Region::AdjustParameters: No SAM data available");
			return;
		}

		int numSectors = sam->getnPproducerTypes();

		// Calculate aggregate error across all sectors
		double totalError = 0.0;
		double totalWeight = 0.0;
		int validSectors = 0;

		for (int sector = 0; sector < numSectors; ++sector) {
			if (targetConsumption[sector] > 0.0) {
				double sectorError = (actualConsumption[sector] - targetConsumption[sector]) / targetConsumption[sector];
				totalError += sectorError * targetConsumption[sector]; // Weight by target value
				totalWeight += targetConsumption[sector];
				validSectors++;

				// Log significant deviations
				if (std::abs(sectorError) > 0.1) { // More than 10% error
					Logger::Info("Region::AdjustParameters: Sector " + sam->labelOfIndex(sector) +
						" consumption error: " + std::to_string(sectorError * 100.0) + "%");
				}
			}
		}

		if (totalWeight <= 0.0 || validSectors == 0) {
			Logger::Warning("Region::AdjustParameters: No valid sectors for calibration");
			return;
		}

		// Calculate weighted average error
		double avgError = totalError / totalWeight;

		// Get current factor and adjustment parameters
		double currentFactor = Worker::GetHouseholdConsumFactor();
		double adjustFraction = DataManager::GetConfigParameters().ConsumAdjustFraction; // Default 0.1
		double maxFactor = DataManager::GetConfigParameters().MaxConsumPXFactor; // Default 2.0

		// Calculate new factor (inverse relationship: if consuming too much, reduce factor)
		double newFactor = currentFactor * (1.0 - avgError);

		// Apply bounds
		newFactor = std::max(0.1, std::min(maxFactor, newFactor)); // Minimum 0.1 to avoid zero consumption

		// Check if approaching bounds
		if (newFactor > 0.95 * maxFactor) {
			Logger::Warning("Region::AdjustParameters: HouseholdConsumFactor approaching maximum: " +
				std::to_string(newFactor) + " (max: " + std::to_string(maxFactor) + ")");
		}

		// Apply gradual adjustment
		double adjustedFactor = currentFactor * (1.0 - adjustFraction) + newFactor * adjustFraction;

		// Set the new factor
		Worker::SetHouseholdConsumFactor(adjustedFactor);

		// Log adjustment
		Logger::Info("Region::AdjustParameters: Month " + std::to_string(currentMonth) +
			" - Adjusted HouseholdConsumFactor from " + std::to_string(currentFactor) +
			" to " + std::to_string(adjustedFactor) +
			" (avg error: " + std::to_string(avgError * 100.0) + "%)");

		// Ajustar InitSalaryCalibFactor para obtener el InitUnemploymentPercent del SAM
		if (currentMonth >= config.AdjustUnemploymentFrom &&
			currentMonth <= config.FinishCalibrationAt)
		{
			double adjustFraction = config.UnemploymentAdjustFraction;
			double targetUnemploymentPercent = DataManager::GetCurrentSAM()->getInitUnemploymentPercent();
			double currUnemploymentPercent = m_unemploymentRate * 100.0; // Convertir a porcentaje
			double minUnemploymentPercent = 1.0;
			double smoothUnemploymentPercent = std::max(minUnemploymentPercent, currUnemploymentPercent);

			if (std::abs(targetUnemploymentPercent - smoothUnemploymentPercent) > 1.0)
			{
				// Filtro proporcional
				double prevval = DataManager::GetInitSalaryCalibFactor();
				double newval = prevval * (targetUnemploymentPercent / smoothUnemploymentPercent);
				double adjustedValue = prevval + adjustFraction * (newval - prevval);

				// Aplicar limites
				double maxFactor = 10.0;
				adjustedValue = std::min(maxFactor, std::max(1.0 / maxFactor, adjustedValue));

				DataManager::SetInitSalaryCalibFactor(adjustedValue);

				Logger::Info("Region::AdjustParameters: Adjusted InitSalaryCalibFactor from " +
					std::to_string(prevval) + " to " + std::to_string(adjustedValue) +
					" (Target unemployment: " + std::to_string(targetUnemploymentPercent) +
					"%, Current: " + std::to_string(smoothUnemploymentPercent) + "%)");
			}
		}

		m_prevUnemploymentRate = m_unemploymentRate;
	}

	std::vector<double> Region::CalculateTotalHouseholdConsumptionBySector() const
	{
		const auto* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			return std::vector<double>();
		}

		int numSectors = sam->getnPproducerTypes();
		std::vector<double> sectorConsumption(numSectors, 0.0);

		// Sum up consumption from all workers' last month expenditure
		for (const auto& worker : m_workers) {
			if (worker) {
				double workerExpenditure = worker->GetLastMonthExpenditure();
				const auto& proportions = worker->GetAllGoodsPurchaseProportions();

				for (int sector = 0; sector < numSectors && sector < proportions.size(); ++sector) {
					sectorConsumption[sector] += workerExpenditure * proportions[sector];
				}
			}
		}

		// Apply upscale factor to match population scale
		double upscaleFactor = GetSimulationUpscaleFactor();
		for (auto& consumption : sectorConsumption) {
			consumption *= upscaleFactor * 12.0; // Annualize the monthly consumption
		}

		return sectorConsumption; // annualized values
	}

	std::vector<double> Region::GetTargetHouseholdConsumptionFromSAM() const
	{
		const auto* sam = DataManager::GetCurrentSAM();
		if (!sam) {
			return std::vector<double>();
		}

		int numSectors = sam->getnPproducerTypes();
		std::vector<double> targetConsumption(numSectors, 0.0);

		// Get household consumption from SAM
		int householdIndex = sam->getHouseholdsIndex();
		if (householdIndex < 0) {
			Logger::Error("Region::GetTargetHouseholdConsumptionFromSAM: Invalid household index");
			return targetConsumption;
		}

		// Get producer indices
		std::vector<int> producerIndices = sam->getProducerSAMIndices();

		for (int sector = 0; sector < numSectors && sector < producerIndices.size(); ++sector) {
			int producerSAMIndex = producerIndices[sector];
			// Household consumption is the value from household row to producer column
			double samValue = sam->getValue(producerSAMIndex, householdIndex);
			targetConsumption[sector] = samValue; // annualized
		}

		return targetConsumption;
	}


	// Calculate economic statistics
	void Region::CalculateStats()
	{
		// Calculate total annualized household expenditure
		m_currentTotalAnnualizedHouseholdExpenditure = 0.0;
		double monthlyTotalExpenditure = 0.0;
		for (const auto& worker : m_workers) {
			monthlyTotalExpenditure += worker->GetLastMonthExpenditure();
		}

		m_currentTotalAnnualizedHouseholdExpenditure =
			monthlyTotalExpenditure * GetSimulationUpscaleFactor() * 12.0; // Annualize

		// Calculate unemployment rate
		size_t employed = 0;
		for (const auto& worker : m_workers) {
			if (worker->IsEmployed()) {
				employed++;
			}
		}

		if (!m_workers.empty()) {
			m_unemploymentRate = 1.0 - (static_cast<double>(employed) / m_workers.size());
		}
		else {
			m_unemploymentRate = 0.0;
		}

		Logger::Info("Region::CalculateStats: GDP=" + std::to_string(m_currentGDP) +
			", Unemployment=" + std::to_string(m_unemploymentRate * 100.0) + "%" +
			", Annualized Household Expenditure=" + std::to_string(m_currentTotalAnnualizedHouseholdExpenditure));
	}

} // namespace Region
