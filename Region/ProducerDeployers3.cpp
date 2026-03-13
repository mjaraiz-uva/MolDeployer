
// Producer.cpp

#include "pch.h"

// ==========================  CProducer   ==================================

CProducer::CProducer(AgentID id, const std::string& sector, const std::string& name, CRegion* ptrRegion, CSAM* sam)
	: CAgent(id, extractAgentType(sector), name, ptrRegion),
	sectorLabel(sector), samPtr(sam),
	currentProduction(0), workTimeUsed(0.0),
	demandShock(1.0), productionDay(0),
	myGrossOpSurplus(0.0), myDividendYield(1.0), purchasedFixCapital(0.0) {
	stockReference() = 100.0;
	toBeProduced() = 0.0;
	setmySalary(1.0);

	// Insert into the neighborhood
	if (getGlobalRegion().producersNeighborhood.size() == 0) {
		getGlobalRegion().producersNeighborhood.push_back(id);
	}
	else
	{
		getGlobalRegion().producersNeighborhood_randomInsert(id);
	}

	// Extract sector index
	sectorIndex = extractSectorIndex(sector);

	// Initialize banking-related members
	monthlyCashBuffer = 0.0;
	needsBankSync = true;
	lastBankSyncStep = -1; // Force initial sync

	// Get gross operating surplus coefficient from SAM
	myGrossOpSurplus = samPtr->getGrossOpSurplusCoef(sectorIndex);
}

void CProducer::initialize() {
	CAgent::initialize(); // Call base class initialize

	// Initialize dividend-related variables
	myGrossOpSurplus = samPtr->getGrossOpSurplusCoef(sectorIndex);
	myDividendYield = 1.0; // Initial yield
	purchasedFixCapital = 0.0;

	// Ensure we have an owner if required
	// Initial owner will typically be set separately
}

// Owner management methods
void CProducer::addOwner(CAgent* owner) {
	if (owner && std::find(owners.begin(), owners.end(), owner) == owners.end()) {
		owners.push_back(owner);
		// If this is a worker, update their owned producers list
		CWorker* worker = dynamic_cast<CWorker*>(owner);
		if (worker) {
			worker->addOwnedProducer(this);
		}
	}
}

void CProducer::removeOwner(CAgent* owner) {
	auto it = std::find(owners.begin(), owners.end(), owner);
	if (it != owners.end()) {
		// If this is a worker, update their owned producers list
		CWorker* worker = dynamic_cast<CWorker*>(*it);
		if (worker) {
			worker->removeOwnedProducer(this);
		}
		owners.erase(it);
	}
}

double CProducer::getmySalary_mu() const { return getmySalary() * CSimulationContext::getInstance().getSAM().getinitAvgSalary_mu(); }
void CProducer::setDemandShock(double magnitude) {
	// magnitude is a multiplier (e.g., 1.1 for +10%, 0.9 for -10%)
	demandShock = 1.0 + magnitude;
	if (demandShock < 0.1) demandShock = 0.1; // Prevent extreme negative shocks
}
double CProducer::getHistoricalEfficiency() const {
	if (productionHistory.empty() || targetHistory.empty()) {
		return 1.0; // Default if no history
	}

	double totalActual = 0.0;
	double totalTarget = 0.0;

	size_t periods = std::min(productionHistory.size(), targetHistory.size());

	for (size_t i = 0; i < periods; i++) {
		totalActual += productionHistory[i];
		totalTarget += targetHistory[i];
	}

	if (totalTarget == 0.0) {
		return 1.0;
	}

	return std::min(totalActual / totalTarget, 1.0);
}

// ==================   Worker management   ==========================

GoodQtty CProducer::MakeListToBuy() { // returns productionCost

	// Calculate how much to produce
	GoodQtty desiredProductUnits = gettoBeProduced();
	if (desiredProductUnits == 0) return 0;

	// Worktime needed to produce the goods
	workTimeToBuy() = getworkTimeRequirement(desiredProductUnits);

	// Calculate input requirements (goodsToBuy)
	goodsToBuy = getICrequirements(desiredProductUnits); // Initialize goodsToBuy with ICrequirements

	// Calculate production cost
	double productionCost = 0.0;

	for (auto& good : goodsToBuy) {
		GoodType ICtype = good.first;
		GoodQtty& qtty = good.second;
		// Calculate GFCF component based on SAM fractions
		double myFixCapToBuyOf = myFixCapToBuy * getSAM().GFCFfractionOf(ICtype);
		qtty += myFixCapToBuyOf;

		double price = getmyPrice(ICtype);

		productionCost += price * qtty;
	}

	// Add labor cost
	productionCost += workTimeToBuy() * getmySalary_mu();

	// Add markup
	productionCost *= getmyMarkupFactor();

	return productionCost;
}

void CProducer::updateStockReferenceAndToBeProduced()
{
	// Debug current state
	CLogger::debug("Producer " + getName() + " updating production: recent demand=" +
		std::to_string(getrecentDemand()) + ", current stock=" +
		std::to_string(getforSale()));

	// input params
	double priceAdaptFactor = 1.0005;
	double producersMarkup = 0.0;
	double desiredStockLevelFactor = 3.0;
	double productionChangeFraction = 0.02;
	double unsoldFraction = 0.30;

	// Get parameters using global DEPData access
	const CData& data = DEPData();
	// priceAdaptFactor = data.getpriceAdaptFactor();

	// Update desired Supply level (StockReference)

	double prevStockReference = getstockReference();
	double newStockRef = getassistedQtty() + ceil(getrecentDemand() * desiredStockLevelFactor);
	double deltaStockReference = newStockRef - prevStockReference;

	double maxdeltaStockReference = prevStockReference * productionChangeFraction;
	deltaStockReference = std::min(maxdeltaStockReference,
		std::max(-maxdeltaStockReference, deltaStockReference));

	stockReference() = getassistedQtty() // Follows smoothed demand
		+ getstockReference() + deltaStockReference;

	// Calculate goods to be produced

	toBeProduced() = std::max((GoodQtty)0, getstockReference() - getforSale());

	// INFLATION model: increase price if product was almost sold out and there is high demand

	if (getforSale() < unsoldFraction * prevStockReference
		&& deltaStockReference > 0)
	{
		if (getRandom01() < producersMarkup)
			myMarkupFactor() *= priceAdaptFactor; // Used in RunProduction
	}

	CLogger::debug("Producer " + getName() + " updated production plan: " +
		"stockReference=" + std::to_string(getstockReference()) +
		", toBeProduced=" + std::to_string(gettoBeProduced()));

	recentDemand() = 0; // Reset last demand for next iteration
}

//  ===================   Worktime requirements   ==========================

double CProducer::getworkTimeRequirement(GoodQtty productionAmount) const {
	// Get work time requirement per unit of output using index-based lookup
	double laborCoefficient = samPtr->getLaborCoef(sectorIndex); // fraction of salary per production unit
	double compensation = productionAmount * laborCoefficient;
	double workTime = compensation / getmySalary_mu();
	return workTime;
}
bool CProducer::hireWorker(CWorker* worker) {
	CLogger::debug("Producer " + getName() + " needs workTime: " + std::to_string(getworkTimeToBuy()));
	if (!worker || worker->isEmployed() || worker->getdisposableWorktime() <= 0.0)
	{
		CLogger::debug("Hire attempt for worker " + getName() + ": " + "FAILED");
		return false;
	}

	worker->setEmployed(true, this);
	worker->setmySalary(this->getmySalary());
	hiredWorkers.push_back(worker);
	workTimeToBuy() = std::max(0.0, workTimeToBuy() - worker->getdisposableWorktime());
	CLogger::info("Hire attempt for worker " + getName() + ": " + "SUCCESS");
	return true;
}
void CProducer::payDismissWorkers(const double workTimeUsed)
{
	/*
	for (auto* worker : hiredWorkers) {
		if (cash >= worker->getmySalary()) {
			worker->addCash(worker->getmySalary());
			removeCash(worker->getmySalary());
		}
		else {
			// Not enough cash to pay all workers
			fireWorker(worker);
		}
	}
	*/

	double finalProductionCost = 0;

	// Dismiss surplus employees
	double totAvailableTime = 0;
	double timeToBePayed = workTimeUsed;
	for (auto& pWorker : gethiredWorkers())
	{
		double thisWorkerTime = std::min(timeToBePayed, pWorker->getdisposableWorktime());
		timeToBePayed = std::max(0.0, timeToBePayed - thisWorkerTime);
		totAvailableTime += thisWorkerTime;

		// Dismiss surplus employees, if any
		if (totAvailableTime >= workTimeUsed)
		{
			while (true)
			{
				if (pWorker != gethiredWorkers().back())
					fireWorker(gethiredWorkers().back()); // these workers didn't work here
				else
					break;
			}
		}

		if (pWorker == gethiredWorkers().back())
			break;
	}

	// Pay CompEmployees  -------------------------------------------------------------

	double compEmployees = currentProduction * getmySalary_mu();

	// Pay salaries

	auto perYear = 12. * ((double)CSimulationContext::getInstance().getSAM().getActivePopulation()
		* (1.0 - getDEPData().getParameters().initialUnemploymentRate) / getDEPData().getinitialWorkerCount());

	double salariesPayed = 0;
	timeToBePayed = workTimeUsed;
	bool done = false;
	if (timeToBePayed > 0)
	{
		for (auto& pWorker : gethiredWorkers())
		{
			double thisWorkerTime = std::min(timeToBePayed, pWorker->getdisposableWorktime());

			double mySalary = compEmployees * thisWorkerTime / totAvailableTime;

			timeToBePayed = std::max(0.0, timeToBePayed - thisWorkerTime);

			//done = PayTo(pWorker, mySalary, txt);
			if (cash >= pWorker->getmySalary()) {
				pWorker->addCash(pWorker->getmySalary());
				removeCash(pWorker->getmySalary());
				done = true;
			}
			else {
				done = false;
				// Not enough cash to pay all workers
				//fireWorker(pWorker);
			}

			if (!done)
				ERRORmsg("not enough money to pay salary", true);
			else
			{
				finalProductionCost += mySalary;
				//currCompEmployees_mu() += mySalary;
				//DEPData().TotalSalaries() +=
				//	mySalary / (getWorld().getInitSalaryCalibFactor() * getSAM().getInitSalary());
				//DEPData().TotalWorkedTime() += thisWorkerTime;
				//pWorker->IncomeCurr() += mySalary;
				//SAM().SAMstep()[LgroupN][productType] += mySalary * perYear;
				//SAM().SAMstep()[HgroupN][LgroupN] += mySalary * perYear;
			}

			salariesPayed += mySalary;
			pWorker->disposableWorktimeRef() = std::max(0., pWorker->getdisposableWorktime() - thisWorkerTime);
			if (pWorker->getdisposableWorktime() > 0 && pWorker->getdisposableWorktime() < 1.0)
				pWorker->partTimeWorkerRef() = true;

			// Dismiss surplus employees
			if (salariesPayed >= compEmployees)
			{
				while (true)
				{
					if (pWorker == gethiredWorkers().back())
						break;
					else
						fireWorker(pWorker);
				}
			}

			if (pWorker == gethiredWorkers().back())
				break;
		}
	}

	// Dismiss part-time employees, so they can be still employed this month

	for (int n = 0; n < gethiredWorkers().size(); ++n)
	{
		auto pWorker = gethiredWorkers().at(n);
		if (pWorker->getdisposableWorktime() > 0)
			fireWorker(pWorker);
	}
}
void CProducer::fireWorker(CWorker* worker) {
	for (auto it = hiredWorkers.begin(); it != hiredWorkers.end(); ) {
		if (*it == worker) {
			(*it)->setEmployed(false, nullptr);
			(*it)->setmySalary(0);
			it = hiredWorkers.erase(it);
		}
		else {
			++it;
		}
	}
}

//  ===================  IC input requirements   ==========================

CGoods CProducer::getICrequirements(GoodQtty productionAmount) const {
	CGoods requirements;

	// Get all production sector indices
	std::vector<size_t> sectorIndices = samPtr->getProductionSectorIndices();

	// Calculate input requirements from each sector
	for (size_t supplierIdx : sectorIndices) {
		// Get IC requirement per unit of output using index-based lookup
		double coefficient = samPtr->getInputCoef(sectorIndex, supplierIdx);
		if (coefficient > 0.0) {
			// Create good with type = supplierIdx
			GoodType gType = static_cast<GoodType>(supplierIdx);
			GoodQtty qtty = static_cast<GoodQtty>(ceil(coefficient * productionAmount));
			requirements[gType] += qtty;
		}
	}

	return requirements;
}

double CProducer::assembleFixedCapital() {
	double FixCapIncrease = this->getMyFixCapToBuy();
	GoodType GFCFtype = getSAM().getGFCFIndex();
	double toTotalPopulationYear = getDEPData().toTotalPopulationYear();

	if (FixCapIncrease <= 0) {
		return 0.0;
	}

	// Get GFCF fractions for all goods (make a modifiable local copy)
	std::vector<double> GFCFfractionOf = getSAM().getGFCFfractions();

	// Find the limiting gType from GoodsIhave to preserve the GFCF structure (SAM column)
	std::map<GoodType, double> maxFixCapfromGoodsIhave;

	for (GoodType gType = 0; gType < getSAM().getNumPXproducers(); ++gType) {
		if (GFCFfractionOf[gType] > 0) {
			maxFixCapfromGoodsIhave[gType] = getGoodsIhave(gType) / GFCFfractionOf[gType];

			if (maxFixCapfromGoodsIhave[gType] < FixCapIncrease) {
				if (maxFixCapfromGoodsIhave[gType] > 0) {
					FixCapIncrease = maxFixCapfromGoodsIhave[gType];
				}
				else {
					// If we don't have this type, either cancel or modify fractions
					if (getRandom01() < GFCFfractionOf[gType]) {
						FixCapIncrease = 0;  // Can't assemble without this component
					}
					else {
						GFCFfractionOf[gType] = 0;  // Modify local copy
					}
				}
			}
		}
	}

	// Assemble this FixCapIncrease as gType components
	if (FixCapIncrease > 0) {
		for (GoodType gType = 0; gType < getSAM().getNumPproducers(); ++gType) {
			double gFixCapcomponent = FixCapIncrease * GFCFfractionOf[gType];

			// Remove from GoodsIhave (consumption goods)
			GoodsIhave()[gType] = std::max<double>(0, getGoodsIhave(gType) - gFixCapcomponent);

			// Reduce pending myFixCapToBuy
			setMyFixCapToBuy(std::max<double>((GoodQtty)0, getMyFixCapToBuy() - gFixCapcomponent));

			// Add to fixed capital
			setMyFixCapital(getMyFixCapital() + gFixCapcomponent);

			// Track for dividend adjustment
			purchasedFixCapital += gFixCapcomponent;

			// Update SAM accounting
			if (Region().getCurrentStep() > getDEPData().getParameters().AssistedProductionUpto) {
//				getSAM().SAMstepAt(gType, getAgentType()) -= gFixCapcomponent * toTotalPopulationYear;
				// Optional: Also update GFCF column in SAM if needed
				// SAM().SAMstepAt(GFCFtype, getAgentType()) += gFixCapcomponent * toTotalPopulationYear;
			}
		}
	}

	return FixCapIncrease;
}

void CProducer::consumeInputs(double productionAmount) {
	// Get input requirements by index
	auto requirements = getICrequirements(productionAmount);

	// Consume each input type
	for (const auto& pair : requirements) {
		// Remove from inventory (inputs are consumed)
		auto q = GoodsIhave()[pair.first];
		auto qtty = std::max<GoodQtty>((GoodQtty)0, q - pair.second);
		goodsIhave[pair.first] = qtty;
	}
}

//  ==================   Production management   ==========================

double CProducer::calculateMaxPossibleProduction() {
	double maxProduction = std::numeric_limits<double>::max();

	double availableWorktime = 0.0;
	// Get the total work time available for production
	for (const auto& worker : hiredWorkers)
		availableWorktime += worker->getdisposableWorktime();

	// Get work time requirement per unit of output using index-based lookup
	double laborCoefficient = samPtr->getLaborCoef(sectorIndex);
	if (laborCoefficient > 0.0) {
		// Calculate max production possible with available work time
		double maxFromWorkTime = (availableWorktime * getmySalary_mu()) / laborCoefficient;
		maxProduction = std::min(maxProduction, maxFromWorkTime);
	}

	// Check each input to find the limiting factor
	const CGoods& availableInputs = getGoodsIhave();
	for (const auto& gpair : availableInputs) {
		size_t gType = gpair.first;
		auto available = gpair.second;

		// Get IC requirement per unit of output using index-based lookup
		double requiredPerUnit = samPtr->getInputCoef(sectorIndex, gType);

		if (requiredPerUnit > 0.0) {
			// Calculate max production possible with this input
			double maxFromInput = available / requiredPerUnit;

			// Update max production if this is more limiting
			if (maxFromInput < maxProduction)
				maxProduction = maxFromInput;
		}
	}

	return maxProduction;
}
double CProducer::produce() {
	// Calculate maximum possible production given available inputs
	GoodQtty maxProduction = calculateMaxPossibleProduction();

	// Actual production is limited by target and availability
	GoodQtty actualProduction = std::min(toBeProduced(), maxProduction);

	// After successful production:
	if (actualProduction > 0) {
		// Calculate production dividends
		double productionDividends = calculateProductionDividends(actualProduction);

		// Distribute production dividends
		if (productionDividends > 0) {
			PayDividends(productionDividends);
		}
	}

	// ASSISTED PRODUCTION ------------------------------

	assistedQtties().clear();
	double assistedUptoStep = getDEPData().getParameters().AssistedProductionUpto;
	double assistedRange = assistedUptoStep; // Assisted always from 0
	GoodQtty delta = 0;
	auto currentStep = getGlobalRegion().getCurrentStep();
	if (currentStep <= assistedUptoStep)
	{
		// Note: disable fade if using selfadjustment of AssistedProductionUpto
		delta = std::max<GoodQtty>(0, toBeProduced() - actualProduction)
			* std::max<double>(0, assistedUptoStep - currentStep) / assistedRange; // fade out
		if (delta > 0)
		{
			actualProduction += delta;
			forSale() += delta; //MJ twice¿?, see below. Sould be removable, but it's ok because <= assistedUptoStep
			assistedQttiesOf(getAgentType()) += delta;

			// DeleteMyMoney(delta); does not work here
		}
	}

	if (actualProduction <= 0) {
		currentProduction = 0;
		workTimeUsed = 0.0;

		// Update production history
		if (productionHistory.size() >= 5) {
			productionHistory.pop_front();
		}
		productionHistory.push_back(0.0);

		return 0;
	}

	// Update production and employment values
	currentProduction = actualProduction;
	workTimeUsed = getworkTimeRequirement(actualProduction);

	// Consume inputs for production
	consumeInputs(actualProduction);

	// Update production history
	if (productionHistory.size() >= 5) {
		productionHistory.pop_front();
	}
	productionHistory.push_back(actualProduction);

	return workTimeUsed;
}

// ======================  stepActivity  =============================

double CProducer::calculateMonthlyExpenses() const {
	// Production inputs based on typical production
	double inputCosts = 0.0;

	// Use stockReference as a proxy for monthly production
	GoodQtty typicalProduction = getstockReference();
	if (typicalProduction > 0) {
		// Get input requirements for this production level
		CGoods inputs = getICrequirements(typicalProduction);

		// Calculate total cost of inputs
		for (const auto& input : inputs) {
			GoodType type = input.first;
			GoodQtty qty = input.second;
			double price = getmyPrice(type);

			inputCosts += qty * price;
		}
	}

	// Add labor costs
	double laborCosts = getworkTimeRequirement(typicalProduction) * getmySalary_mu();

	// Add a safety margin for unexpected expenses (10%)
	double totalExpenses = (inputCosts + laborCosts) * 1.1;

	return totalExpenses;
}

double CProducer::calculateProductionDividends(double producedAmount) {
	// Calculate production-based dividends
	double dividends = producedAmount * myGrossOpSurplus;

	// Adjust for capital investments
	if (purchasedFixCapital <= dividends) {
		dividends -= purchasedFixCapital;
		purchasedFixCapital = 0.0;
	}
	else {
		purchasedFixCapital -= dividends;
		dividends = 0.0;
	}

	return dividends;
}

double CProducer::calculateSurplusDividends() {
	// Calculate surplus-based dividends
	// Keep twice production expenses as reserve
	double productionExpenses = currentProduction * getmyPrice(getAgentType());
	double surplusDividends = std::max(0.0, getCash() - 2.0 * productionExpenses);

	return surplusDividends;
}

void CProducer::PayDividends(double dividendAmount) {
	if (dividendAmount <= 0 || owners.empty()) {
		return;
	}

	// Log dividend payment
	CLogger::debug(getName() + " paying dividends: " + std::to_string(dividendAmount));

	// Get wealth for dividend yield calculation
	double wealth = getCash() + getMyFixCapital(); // Simplified wealth calculation

	// Calculate and update dividend yield for financial markets
	double prevDividendYield = myDividendYield;
	double currentDividendYield = 1.0 + dividendAmount / std::max(1.0, wealth);
	double newDividendYield = std::max(0.0, prevDividendYield +
		(currentDividendYield - prevDividendYield) * 0.2); // Smooth change
	myDividendYield = newDividendYield;

	// Get sector index for accounting
	GoodType productType = getAgentType();

	// Get SAM upscaling factor for accounting
	double perHolderYear = 12.0 * samPtr->getActivePopulation() /
		CSimulationContext::getInstance().getData().getinitialWorkerCount();

	// Check if government is an owner (simplified)
	bool governmentOwned = false;
	for (auto owner : owners) {
		if (owner->getAgentType() == GovernmentType) {
			governmentOwned = true;
			break;
		}
	}

	if (governmentOwned) {
		// Handle government ownership - all dividends to government
		// Find government agent
		CAgent* government = nullptr;
		for (auto owner : owners) {
			if (owner->getAgentType() == GovernmentType) {
				government = owner;
				break;
			}
		}

		if (government && removeCash(dividendAmount)) {
			// Pay government
			government->addCash(dividendAmount);

			// Update accounting. THIS IS FOR SIMULATED SAM values
			/*if (samPtr) {
			//	samPtr->updateValue("GrossOpSurplus", productType, dividendAmount * perHolderYear);
				// Update GDP if needed through SAM or other mechanism
			//}*/
		}
	}
	else {
		// Handle private ownership - proportional to ownership shares
		// For DEP3, we'll simplify by assuming equal ownership for now
		// Later, implement share-based ownership

		double dividendPerOwner = dividendAmount / owners.size();

		for (auto owner : owners) {
			if (owner && removeCash(dividendPerOwner)) {
				// Pay this owner
				owner->addCash(dividendPerOwner);

				// Update accounting. THIS IS FOR SIMULATED SAM values
				/*if (samPtr) {
				//	samPtr->updateValue("GrossOpSurplus", productType, dividendPerOwner * perHolderYear);

					// If owner is a worker, update household accounting
					CWorker* worker = dynamic_cast<CWorker*>(owner);
					if (worker) {
						// Update household account in SAM if needed
						// This would depend on how you've implemented household groups
					}
				}*/
			}
		}
	}
}
void CProducer::stepInitialize() {
	// Reset for the new step
}
void CProducer::stepActivity() {
	// Log initial state
	CLogger::debug("Producer " + getName() + " activity: Cash=" + std::to_string(getCash()) +
		", ForSale=" + std::to_string(getforSale()));

	// Update production targets based on market demand
	updateStockReferenceAndToBeProduced();

	// Create list of inputs needed for production
	auto productionCost = MakeListToBuy();

	// Check if bank sync is needed
	int currentStep = getpRegion()->getCurrentStep();
	int syncFrequency = DEPData().getStepsPerYear(); // Once per year in simulation time

	if (shouldSyncWithBank(currentStep, syncFrequency)) {
		syncWithBank();
	}

	// Log production plan
	CLogger::debug("Producer " + getName() + " production plan: ToBeProduce=" +
		std::to_string(gettoBeProduced()) + ", Cost=" + std::to_string(productionCost));

	// First, attempt to buy necessary inputs
	BuyGoods();

	// Then try to produce with available inputs
	double workTimeUsed = produce();

	// Add newly produced goods to inventory
	if (workTimeUsed > 0) {
		forSale() += currentProduction;
		CLogger::info("Producer " + getName() + " produced: " + std::to_string(currentProduction) +
			" units using " + std::to_string(workTimeUsed) + " work time");
	}
	else {
		CLogger::info("Producer " + getName() + " no production this step");
	}

	// Pay workers and dismiss if necessary
	payDismissWorkers(workTimeUsed);

	// Calculate and pay surplus dividends
	double surplusDividends = calculateSurplusDividends();
	if (surplusDividends > 0) {
		PayDividends(surplusDividends);
	}

	// Log employment status
	CLogger::debug("Producer " + getName() + " retain " + std::to_string(hiredWorkers.size()) +
		" workers");

	// Log final state
	CLogger::info("Producer " + getName() + " end activity: Cash=" + std::to_string(getCash()) +
		", ForSale=" + std::to_string(getforSale()));

	// After all transactions, check if we need bank sync next step
	if (cash < monthlyCashBuffer * 0.25) {
		needsBankSync = true;
		CLogger::debug(getName() + " cash low, flagged for bank sync next step");
	}
}

// ==========================================================================

GoodQtty CProducer::BuyFromAgent(CAgent* seller, const GoodType goodType, GoodQtty requestedQty) {
	// Ensure the seller is a valid producer
	CProducer* producer = dynamic_cast<CProducer*>(seller);
	if (!producer || producer->getSectorIndex() != static_cast<size_t>(goodType))
		return 0; // Seller is not a valid producer or doesn't produce the good we want

	// Find maximum quantity that can be sold
	GoodQtty availableQty = producer->getforSale();
	GoodQtty transferQty = std::min(availableQty, requestedQty);
	if (transferQty <= 0)
		return 0; // Nothing to transfer

	// Get the unit price and ensure it's ok
	double unitPrice = producer->getmyPrice(goodType);
	if (unitPrice > getmyPrice(goodType))
		return 0; // Price is too high
	else
		setmyPrice(goodType, unitPrice); // Update price if lower

	// Calculate cost
	GoodQtty cost = static_cast<GoodQtty>(transferQty * unitPrice);

	// Check if we can afford it
	if (getCash() < cost)
		return 0; // Not enough cash

	// Execute transaction
	if (!removeCash(cost)) {
		ERRORmsg("Not enough cash to buy goods");
		return 0;
	}

	// Update seller's inventory and cash
	producer->forSale() -= transferQty;
	producer->addCash(cost);

	// Add purchased goods to our inventory
	goodsIhave[goodType] += transferQty;

	return transferQty; // Return the quantity successfully purchased
}

// ==========================================================================
