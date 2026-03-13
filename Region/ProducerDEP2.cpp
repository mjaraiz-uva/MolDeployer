
//  Producer.cpp

//   DEPLOYERS v2

#include "./pch.h"

vector<vector<CProducerSpecs*>>* CProducer::_pProducerFigSpecs;
map< GoodType, CProducerSpecs* > CProducer::_ProducerSpecs;
map< GoodType, CProductSpecs* >  CProducer::_ProductSpecs;
vector<string> CProducer::_ProducerLabelOfType;
map< string, GoodType > CProducer::_ProducerTypeOfLabel;

ofstream& operator<<(ofstream& ofstrm, const CHistory& history)
{
	ofstrm << " { "
		<< history._HISTORY_LENGTH << " " << history._historyIndex << " " << history._average;

	ofstrm << " {";
	for (const auto& value : history)
		ofstrm << " " << value;
	ofstrm << " }";

	ofstrm << " }";
	return ofstrm;
};
ifstream& operator>>(ifstream& ifstrm, CHistory& history)
{
	string bracket;
	ifstrm >> bracket; // " {"
	ifstrm >> history._HISTORY_LENGTH >> history._historyIndex >> history._average;

	ifstrm >> bracket; // " {"
	history.clear();
	while (ifstrm >> bracket, bracket != "}")
	{
		double value = atof(bracket.c_str());
		history.push_back(value);
	}

	ifstrm >> bracket; // " }"
	return ifstrm;
};

double CHistory::updateAndReturnAvg() {
	double total = 0.0;
	for (double prod : *this)
		total += prod;
	_average = total / _HISTORY_LENGTH;

	return _average;
}
void CHistory::updateHistory(double newVal) {
	(*this)[_historyIndex] = newVal;
	_historyIndex = (_historyIndex + 1) % _HISTORY_LENGTH;
}

//======================  CProducer  ===================================

CProducer::CProducer(AgentID id, GoodType producerType, CAgent* powner)
	: CAgent(id, producerType)
{
	if (!IsBankType(producerType) || getInputParameter("ProducerBanks") == 1)
		locationType() = CProducer::getSpecsOfProducerType(producerType).getlocationType();

	pOwners() = new map<CAgentFID, CAgent*>;
	if (powner != nullptr)
	{
		(*pOwners())[powner->getFID()] = powner;
		powner->myProducers()[this->getFID()] = this;
		// example of use: powner->Producer(this->getFID()).getCash()
	}

	pEmployees() = new vector<CWorker*>;

	auto gType = getAgentType();
	if (gType >= 0 && gType < getSAM().getnPProducerTypes())
		//	&& !IsBankType(producerType)
		//	|| getInputParameter("MaxNBanks") > 0 && getInputParameter("ProducerBanks") == 1)
		++DEPData().nCurrentPProducers()[gType];

	initialize();
}

CProducer::~CProducer() {};

ofstream& operator<<(ofstream& ofstrm, const CProducer& producer)
{
	ofstrm << " " << CProducer::getProducerLabelOfType(producer.getAgentType()) << " {";
	ofstrm << " ID " << (long)producer.getID();

	ofstrm << (CAgent&)producer; // call base class

	ofstrm << "\n timeWorked() " << producer.gettimeWorked();

	ofstrm << endl << " EmployeesID { ";
	for (const auto& employee : producer.getEmployees())
		ofstrm << "" << employee->getID() << " ";
	ofstrm << " }";

	ofstrm << endl << "ProductionHistory "; ofstrm << producer.getProductionHistory();
	ofstrm << endl << "mySupplyHistory "; ofstrm << producer.getmySupplyHistory();
	ofstrm << endl << "LeftToSellHistory "; ofstrm << producer.getLeftToSellHistory();

	ofstrm << endl
		<< " LastUsedTimestep " << producer.getLastUsedTimestep()
		<< " myFirstProductionDay " << producer.getmyFirstProductionDay()
		<< " ToBeProduced " << producer.getToBeProduced()
		<< " currDepositInterests_mu " << producer.getcurrDepositInterests_mu();

	ofstrm << " assistedQtties "; ofstrm << producer.getassistedQtties();

	ofstrm << endl << " OwnerIDs {";
	if (producer.getpOwners() != nullptr)
		for (const auto& pair : *(producer.getpOwners()))
			ofstrm << pair.first;
	else
		ofstrm << " -1";
	ofstrm << " }";

	ofstrm << "\n ProductionPrice " << producer.getproductionPrice();
	ofstrm << "\n myMarkupFactor " << producer.getmyMarkupFactor();
	ofstrm << "\n InFinancialMarket " << producer.getInFinancialMarket();
	ofstrm << "\n NoCompEmployees " << producer.getNoCompEmployees();

	ofstrm << "\n myGrossOpSurplus " << producer.getmyGrossOpSurplus();
	ofstrm << "\n myLabourProductivity " << producer.getmyLabourProductivity();

	ofstrm << endl << " producedUnits "; ofstrm << producer.getproducedUnits();
	ofstrm << endl << " StockReference " << producer.getStockReference();
	ofstrm << " prevmyDemand " << producer.getprevmyDemand();
	ofstrm << " myDemand " << producer.getmyDemand();
	ofstrm << " prevavgmyDemand " << producer.getprevavgmyDemand();
	ofstrm << " avgmyDemand " << producer.getavgmyDemand();
	ofstrm << " productionCapacity " << producer.getproductionCapacity();
	ofstrm << " averageProduction " << producer.getaverageProduction();
	ofstrm << " mySupply " << producer.getmySupply();
	ofstrm << " LeftToSell " << producer.getLeftToSell();
	ofstrm << " prevToBeProduced " << producer.getprevToBeProduced();

	ofstrm << endl << "}";

	return ofstrm;
}
ifstream& operator>>(ifstream& ifstrm, CProducer& producer)
{
	operator>>(ifstrm, (CAgent&)producer); // call base class

	string name, word, bracket;
	long nn;

	ifstrm >> word >> producer.timeWorked();

	ifstrm >> word >> bracket; // " EmployeesID { "
	while (ifstrm >> word, word != "}")
	{
		nn = atoi(word.c_str());
		CWorker* pIndiv = getWorld().getpWorkers()->at(nn);
		producer.Employees().push_back(pIndiv);
	}

	producer.ProductionHistory().clear();
	ifstrm >> word; ifstrm >> producer.ProductionHistory();
	producer.mySupplyHistory().clear();
	ifstrm >> word; ifstrm >> producer.mySupplyHistory();
	producer.LeftToSellHistory().clear();
	ifstrm >> word; ifstrm >> producer.LeftToSellHistory();

	ifstrm
		>> name >> producer.LastUsedTimestep()
		>> name >> producer.myFirstProductionDay()
		>> name >> producer.ToBeProduced()
		>> name >> producer.currDepositInterests_mu();

	ifstrm >> name;  ifstrm >> producer.assistedQtties();

	ifstrm >> name >> bracket; // " OwnerIDs {"
	while (ifstrm >> word, word != "}")
	{
		auto ty = atoi(word.c_str());
		ifstrm >> word;
		auto id = atoi(word.c_str());
		if (ty == -1)
		{
			CWorker* pIndiv = getWorld().getpWorkers()->at(id);
			(*producer.pOwners())[CAgentFID(ty, id)] = pIndiv;
			//pIndiv->pmyProducers() = &producer;
			pIndiv->pmyProducer(producer.getFID()) = &producer;
		}
		else if (ty == GovernmentType)
		{
			(*producer.pOwners())[CAgentFID(ty, id)] = pGovernment();
			//Government().pmyProducers() = &producer;
			pGovernment()->pmyProducer(producer.getFID()) = &producer;
		}
		else if (getSAM().IsExtSectType(ty))
		{
			(*producer.pOwners())[CAgentFID(ty, id)] = pExtSect(id);
			//ExtSect(id).pmyProducers() = &producer;
			pExtSect(id)->pmyProducer(producer.getFID()) = &producer;
		}
	}

	ifstrm >> word >> producer.productionPrice();
	ifstrm >> word >> producer.myMarkupFactor();
	ifstrm >> word >> producer.InFinancialMarket();
	ifstrm >> word >> producer.NoCompEmployees();

	ifstrm >> word >> producer.myGrossOpSurplus();
	ifstrm >> word >> producer.myLabourProductivity();

	ifstrm >> word; ifstrm >> producer.producedUnits();
	ifstrm >> word >> producer.StockReference();
	ifstrm >> word >> producer.prevmyDemand();
	ifstrm >> word >> producer.myDemand();
	ifstrm >> word >> producer.prevavgmyDemand();
	ifstrm >> word >> producer.avgmyDemand();
	ifstrm >> word >> producer.productionCapacity();
	ifstrm >> word >> producer.averageProduction();
	ifstrm >> word >> producer.mySupply();
	ifstrm >> word >> producer.LeftToSell();
	ifstrm >> word >> producer.prevToBeProduced();

	ifstrm >> word; // "}" 

	//  ------------------------

	for (auto pEmployee : producer.Employees())
		pEmployee->pEmployer() = &producer;

	return ifstrm;
}

void CProducer::initialize()
{
	if (getProductSpecs().size() == 0)
		return; // not ready yet

	GoodType producerType = getAgentType();

	ProductionHistory()._HISTORY_LENGTH = getInputParameter("HISTORY_LENGTH");
	ProductionHistory().resize(ProductionHistory()._HISTORY_LENGTH, 0.0);
	mySupplyHistory()._HISTORY_LENGTH = getInputParameter("HISTORY_LENGTH");
	mySupplyHistory().resize(mySupplyHistory()._HISTORY_LENGTH, 0.0);
	LeftToSellHistory()._HISTORY_LENGTH = getInputParameter("HISTORY_LENGTH");
	LeftToSellHistory().resize(LeftToSellHistory()._HISTORY_LENGTH, 0.0);

	NoCompEmployees() = false;
	if (!IsNonProducerBankType(producerType))
		if (getProductSpecs(getAgentType()).getCompEmployees() == 0)
			NoCompEmployees() = true;

	CAgent::initialize(); // base class

	// Initialize producer variables

	_initK = 0;
	_initL = 0;
	_initVA = 0;
	_initGrossOutput = 0;
	_initIC = 0;
	_alpha = 0;
	_beta = 0;

	myGrossOpSurplus() = 0;
	myMarkupFactor() = 1.0;
	myLabourProductivity() = 0;

	auto& productSpecs = *CProducer::ProductSpecs()[getAgentType()];
	if (!IsExtSect()
		&& !IsNonProducerBankType(producerType))
	{
		if (getInputParameter("CobbDouglas"))
		{
			for (GoodType gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
				_initIC += getSAM().getRowCol(gType, getAgentType());

			_initK = getSAM().getRowCol("GrossOpSurplus", getAgentType());
			_initL = getSAM().getRowCol("CompEmployees", getAgentType());

			_initVA = _initK + _initL;
			for (const auto& pair : getSAM().getAccountGroups().at("T"))
				_initVA += getSAM().getRowCol(pair->accN(), getAgentType());

			_initGrossOutput = _initIC + _initVA;

			_alpha = _initK / _initVA; //
			_beta = _initVA / (pow(_initK, _alpha) * pow(_initL, 1. - _alpha));
		}

		if (getSAM().getRowCol("CompEmployees", getAgentType()) > 0)
			myLabourProductivity() = (double)productSpecs.getGrossOutput_mu()
			/ getSAM().getRowCol("CompEmployees", getAgentType());

		if (getSAM().getRowCol("GrossOpSurplus", getAgentType()) > 0)
		{
			myGrossOpSurplus() = getSAM().getRowCol("GrossOpSurplus", getAgentType())
				/ (double)productSpecs.getGrossOutput_mu();
			myMarkupFactor() = 1.0;
		}
	}

	Employees().clear();

	currIC_mu() = 0;
	currCompEmployees_mu() = 0;
	currNetTaxes_mu() = 0;
	currDepositInterests_mu() = 0;
	currLoanPaymentsAndInterests_mu() = 0;
	LastUsedTimestep() = currStep();
	myFirstProductionDay() = -1;
	timeWorked() = 0;

	LeftToSell() = 0;
	ToBeProduced() = 0;
	productionCapacity() = 10.0 * getmySalary_mu();
	averageProduction() = 0;
	mySupply() = 0;
	prevToBeProduced() = 0;
	producedUnits().clear();
	assistedQtties().clear();

	StockReference() = max(0.5 * getavgmyDemand(), 10.0 * getmySalary_mu());
	ProductionHistory()._average = 0;
	mySupplyHistory()._average = 0;
	LeftToSellHistory()._average = 0;
	prevmyDemand() = 0;
	myDemand() = 0;
	prevavgmyDemand() = 0;
	avgmyDemand() = 0;

	NewSharesIssued() = 0; // Only one initial owner
	if (pOwners()->size() > 1) // keep first owner only
	{
		CAgent* pOwner = (*pOwners()).begin()->second;
		assert(pOwners()->size() > 0);
		pOwners()->clear();
		(*pOwners())[pOwner->getFID()] = pOwner;
		pOwner->pmyProducer(this->getFID()) = this;
	}

	WorkTimeToBuy() = 0;

	if (!IsNonProducerBankType(producerType))
	{
		const auto& NeededPerUnit = productSpecs.getNeededPerUnit();
		for (const auto& inPair : NeededPerUnit)
		{
			auto gType = inPair.first;
			GoodsIhave()[gType] = 0;
			GoodsToBuy()[gType] = 0;
			myPrice()[gType] = 1.0;
		}
	}

	productionPrice() = 1.0;
	if (producerType >= 0)
	{
		myPrice()[producerType] = productionPrice();
		ToSell(producerType) = 0;// get ready to offer it to clients
	}

	InFinancialMarket() = false;
}

bool CProducer::getHasWorkedCurrentMonth() const { return gettimeWorked() > 0.; };

map<CAgentFID, CAgent*>*& CProducer::pOwners() { return _pOwners; };
map<CAgentFID, CAgent*>& CProducer::Owners() { return *_pOwners; };
vector<CWorker*>*& CProducer::pEmployees() { return _pEmployees; };
vector<CWorker*>* CProducer::getpEmployees() const { return _pEmployees; };
vector<CWorker*>& CProducer::Employees() { return *_pEmployees; };
GoodQtty& CProducer::ToBeProduced() { return _ToBeProduced; };
long& CProducer::LastUsedTimestep() { return _LastUsedTimestep; };
const map<CAgentFID, CAgent*>* CProducer::getpOwners() const { return _pOwners; };
const map<CAgentFID, CAgent*>& CProducer::getOwners() const { return *_pOwners; };
const vector<CWorker*>& CProducer::getEmployees() const { return *_pEmployees; };
GoodQtty CProducer::getToBeProduced() const { return _ToBeProduced; };
const long CProducer::getLastUsedTimestep() const { return _LastUsedTimestep; };
void CProducer::releaseEmployees()
{
	for (auto pair : Employees())
		pair->pEmployer() = nullptr;
}
CGoods CProducer::getInventory()
{
	return CAgent::getInventory();
};

//--------------------  Producer Specs  ----------------------------

const string CProducer::getProducerName() const { return getAgentName(); }
map<GoodType, CProducerSpecs*>& CProducer::mProducerSpecs() { return CProducer::_ProducerSpecs; };
const map<GoodType, CProducerSpecs*>& CProducer::getProducerSpecs() { return CProducer::_ProducerSpecs; };
const CProducerSpecs& CProducer::getSpecsOfProducerType(AgentType producerType) { return *getProducerSpecs().at(producerType); };
const CProducerSpecs& CProducer::Specs() { return CProducer::getSpecsOfProducerType(this->getAgentType()); };
const CProducerSpecs& CProducer::getSpecs() const { return CProducer::getSpecsOfProducerType(this->getAgentType()); };

//--------------------  This Producer Status & Specs  ----------------------------

const vector<string>& CProducer::getProducerLabelOfType() { return _ProducerLabelOfType; };
const string CProducer::getProducerLabelOfType(GoodType gType)
{
	if (gType == getCentralBankType())
		return "CentralBank";
	else if (gType == getPrivateBankType())
		return "PrivateBank";
	else
		return getAccount(gType).label();
};
const AgentType CProducer::getProducerTypeOfLabel(string label) { return getSAM().getAccNofLabel(label); };
vector<string>& CProducer::ProducerLabelOfType() { return _ProducerLabelOfType; };
map<string, AgentType>& CProducer::ProducerTypeOfLabel() { return _ProducerTypeOfLabel; };
const map<string, AgentType>& CProducer::getProducerTypeOfLabel() { return _ProducerTypeOfLabel; };
const CProductSpecs& CProducer::getSpecsOfProductType(GoodType productType)
{
	return CProducer::getProductSpecs(productType);
};
void CProducer::readProducerSpecs(ifstream& ifstrm) // static
{
	string producerName, bracket;

	ProducerTypeOfLabel().clear();

	ifstrm >> bracket; // " {"
	mProducerSpecs().clear();
	AgentType producerType = UndefAgentType;
	while (ifstrm >> producerName, producerName != "}")
	{
		producerType = getSAM().getAccNofName(producerName);

		CProducerSpecs& producerSpecs = *new CProducerSpecs(producerType);

		ifstrm >> producerSpecs;

		mProducerSpecs()[producerSpecs.getproducerType()]
			= &producerSpecs;
	}
};
void CProducer::writeProducerSpecs(ofstream& ofstrm) // static
{
	ofstrm
		<< "PRODUCER_DEFINITIONS(Specs) {" << endl;
	for (const auto& pair : getProducerSpecs())
	{
		ofstrm << *pair.second;
	}
	ofstrm << "}" << endl << endl;
};
void CProducer::readProductSpecs(ifstream& ifstrm) // static
{
	string productName, bracket;

	ifstrm >> bracket; // " {"
	CProducer::ProductSpecs().clear();
	while (ifstrm >> productName, productName != "}")
	{
		CProductSpecs& productDef =
			*new CProductSpecs(CGoods::GoodNameToType(productName));

		ifstrm >> productDef;

		CProducer::ProductSpecs()[productDef.getproductType()]
			= &productDef;
	}
};
void CProducer::writeProductSpecs(ofstream& ofstrm) // static
{
	ofstrm << "PRODUCT_DEFINITIONS(Specs) {" << endl;
	for (const auto& pair : CProducer::ProductSpecs())
	{
		ofstrm << *pair.second;
	}
	ofstrm << "}" << endl << endl;

	ofstrm << endl;
};
map<GoodType, CProductSpecs*>& CProducer::ProductSpecs() { return CProducer::_ProductSpecs; };
const map<GoodType, CProductSpecs*>& CProducer::getProductSpecs() { return CProducer::_ProductSpecs; };
GoodQtty& CProducer::StockReference() { return _StockReference; };
const GoodQtty& CProducer::getStockReference() const { return _StockReference; }
GoodQtty CProducer::getNewSharesIssued() const { return _NewSharesIssued; };
double CProducer::getmyCurrShareVal() const
{
	double currShareVal = 0;
	GoodQtty myShs = getmySharesOf(getFID());
	if (myShs != 0)
		currShareVal = (double)getWealth() / (-myShs); // -: n shares sold

	return currShareVal;
};
double CProducer::getmyTotSharesValue() const
{
	return (double)getWealth(); // -: n shares sold
}
const CProductSpecs& CProducer::getProductSpecs(GoodType productType)
{
	return *CProducer::getProductSpecs().at(productType);
};
const GoodQtty CProducer::MinimumProductUnits(const GoodType productType) const
{
	const auto& Specs = CProducer::getProductSpecs(productType);
	if (Specs.getImportPrice() > 0)
		return 1;

	GoodQtty minQtty = 1;
	double minEfficiency = 1.0; // 0.1 avoid too expensive production

	return 1;
};
GoodQtty CProducer::releaseCash()
{
	GoodQtty cash = Cash();
	Cash() = 0;
	return cash;
};
CGoods CProducer::transferGoodsIhave()
{
	CGoods Ihave = GoodsIhave();
	GoodsIhave() -= GoodsIhave(); // reset to 0
	return Ihave;
};
CGoods CProducer::transferToSell()
{
	CGoods toSell = ToSell();
	ToSell() -= ToSell(); // reset to 0
	return toSell;
};
const CWorker* CProducer::ReleaseLastEmployee()
{
	CWorker* pEmployee = Employees().back();
	assert(pEmployee->getpEmployer() == this);

	pEmployee->pEmployer() = nullptr;

	Employees().pop_back();

	return pEmployee;
}
const bool CProducer::ReleaseThisEmployee(CWorker& employee)
{
	assert(employee.getpEmployer() == this);

	bool ok = false;
	for (long nEmployee = 0; nEmployee < getEmployees().size(); ++nEmployee)
	{
		if (Employees()[nEmployee]->getID() == employee.getID())
		{
			swap(Employees().at(nEmployee), Employees().back());
			ReleaseLastEmployee();
			ok = true;
			break;
		}
	}
	return ok;
}

double CProducer::alpha() const
{
	const CProductSpecs& specs = CProducer::getProductSpecs(getAgentType());
	assert(specs.getCompEmployees() > 0);
	double initKLratio = specs.getGrossOpSurplus() / specs.getCompEmployees();
	double alpha = 0.6741 - 0.0594 / initKLratio; // 'general' empirical fit from MCAESP08 SAM data
	return alpha;
}
double CProducer::beta() const
{
	return 2.0; // aprox common value
}
double CProducer::calcVA(double K, double L) const
{
	if (K == 0 || L == 0)
		return 0;

	double VA = beta() * pow(K, alpha()) * pow(L, 1.0 - alpha());
	return VA;
};
double CProducer::calcK(double VA, double L) const
{
	double K = pow(VA / (beta() * pow(L, 1.0 - alpha())), 1.0 / alpha());
	return K;
};
double CProducer::calcL(double VA, double K) const
{
	double L = pow(VA / (beta() * pow(K, alpha())), 1.0 / (1.0 - alpha()));
	return L;
};
GoodQtty CProducer::OutputProductUnits(GoodQtty VA) const
{
	GoodQtty productUnits = floor(VA * (double)_initGrossOutput / _initVA);

	return productUnits;
};
GoodQtty CProducer::DesiredVA(GoodQtty desiredProductUnits) const
{
	GoodQtty desiredVA = ceil(desiredProductUnits * (double)_initVA / _initGrossOutput);
	return desiredVA;
};
GoodQtty CProducer::currProductionSize_mu() const
{
	double currentK = getmyFixCapital() / getInputParameter("KtoFixCapitalFactor");
	double currentWorkers = getEmployees().size();
	double currentL = currentWorkers * getmySalary_mu();

	if (currentK == 0 || currentL == 0)
		return 0;

	double VA = calcVA(currentK, currentL); // mu
	GoodQtty grossOutput = OutputProductUnits(VA);

	return grossOutput;
};
GoodQtty CProducer::minProductionSize() const // approx. 1 employee size
{
	const CProductSpecs& specs = CProducer::getProductSpecs(getAgentType());
	if (specs.getCompEmployees() == 0)
		return 10. * getSAM().getInitSalary();

	double minL = 1.0 * getmySalary_mu();
	GoodQtty minGrossOutput = ceil(minL / specs.getCompEmployees()); // approx.

	return minGrossOutput;
};
GoodQtty CProducer::getWealth() const
{
	GoodQtty Wealth = getCash() + getmyBankBalance() + getmyFixCapital();

	return Wealth;
}
void CProducer::PayDividends(GoodQtty dividends)
{
	if (dividends == 0)
		return;

	double Wealth = getWealth();
	if (Wealth <= 0)
		return;

	if (getOwners().size() == 0)
	{
		CWorld::ERRORmsg(getAgentName() + ": Owners().size() == 0 in CProducer::PayDividends", true);
		return;
	}

	if (dividends < 0)
	{
		CWorld::ERRORmsg(getAgentName() + ": dividends < 0", true);
		return;
	}

	GoodType productType = getAgentType();
	if (getOwners().begin()->second->IsGovernment())
	{
		bool ok = PayTo(pGovernment(), dividends);
		if (!ok)
			CWorld::ERRORmsg(getAgentName() + ": not enough money to pay dividends", true);

		auto perHolderYear = dividends * 12. * getSAM().getActive() / getWorld().getNWorkers();
		SAM().SAMstepAt("GrossOpSurplus", productType) += perHolderYear;
		DEPData().GDPVAnominal() += perHolderYear;
	}
	else
	{
		auto NShares = -getmySharesOf(getFID()); // total shares issued

		//current step profit
		double prevDividendYield = getmyDividendYield(); // used in the FinancialMarket
		double dividendYield = 1.0 + (double)dividends / Wealth;
		double newDividendYield = max(0., prevDividendYield + (dividendYield - prevDividendYield) * 0.2);
		myDividendYield() = newDividendYield;

		auto perHolderYear = 12. * getSAM().getActive() / getWorld().getNWorkers();
		for (auto& pair : Owners())
		{
			auto& SHolder = *pair.second;
			GoodQtty nshares = SHolder.getmySharesOf(this->getFID());
			GoodQtty rent = floor(((double)nshares * dividends) / NShares);
			if (rent < 0)
				CWorld::ERRORmsg(getAgentName() + ": SHolder rent < 0", true);

			bool ok = PayTo(&SHolder, rent);
			if (!ok)
				CWorld::ERRORmsg(getAgentName() + ": not enough money to pay dividends", true);

			SAM().SAMstepAt("GrossOpSurplus", productType) += rent * perHolderYear;
			DEPData().GDPVAnominal() += rent * perHolderYear;

			if (getWorld().getpFigaro() == nullptr)
			{
				if (SHolder.IsWorker())
				{
					auto pEmployee = ((CWorker*)(&SHolder));
					auto HgroupN = pEmployee->getmyHgroupN();
					SAM().SAMstepAt(HgroupN, "GrossOpSurplus") += rent * perHolderYear;
				}
				else
					CWorld::ERRORmsg(getAgentName() + " is not a Household receiving GrossOpSurplus as Owner", true);
			}
		}
	}
};
double CProducer::getavailableWorkTime() const
{
	double availableWorkTime = 0;
	for (int n = 0; n < getEmployees().size(); ++n)
		availableWorkTime += getEmployees().at(n)->getmyAvailableTime();

	return availableWorkTime;
}

bool CProducer::TryToHireNeighbor(CWorker& neighborIndiv)
{
	CProducer* thisEmployer = this;
	double workTimeToBuy = thisEmployer->getWorkTimeToBuy();
	if (thisEmployer->getWorkTimeToBuy() <= 0 // if no workTime to buy
		|| neighborIndiv.getpEmployer() != nullptr  // or already employed
		|| neighborIndiv.getmyAvailableTime() <= 0) // or no more workTime available
		return false;

	// Since hiring should only be initiated by employer (buyer):
	double PriceAdaptFactor = getInputParameter("PriceAdaptFactor");

	double myprice = 0.0, neiprice = 0.0;

	double AdaptFraction = PriceAdaptFactor - 1.0;
	double rndAdaptFactor = 0.0;
	double priceAgreed = 0.0;
	// employer offers more than asked by worker
	if (thisEmployer->getmyPriceOfSalary() >= neighborIndiv.getmyPriceOfSalary())
		priceAgreed = thisEmployer->getmyPriceOfSalary();

	if (priceAgreed > 0)
	{
		thisEmployer->WorkTimeToBuy() = max<double>(0,
			getWorkTimeToBuy() - neighborIndiv.getmyAvailableTime());

		neighborIndiv.pEmployer() = thisEmployer;

		thisEmployer->Employees().push_back(&neighborIndiv);

		// Both traders learn from this SUCCESSFUL Market interaction

		if (currStep() > getInputParameter("ReleasePricesAt"))
		{
			// (salaries will be paid not now but after each production step)
			// force call to tRandom01()rndAdaptFactor = (1. + AdaptFraction * World().getRandom01()); 
			rndAdaptFactor = PriceAdaptFactor;
			neiprice = neighborIndiv.getmyPriceOfSalary();
			neighborIndiv.myPriceOfSalary() = neiprice * rndAdaptFactor;

			// force call to tRandom01()rndAdaptFactor = (1. + AdaptFraction * World().getRandom01()); 
			rndAdaptFactor = PriceAdaptFactor;
			myprice = thisEmployer->getmyPriceOfSalary();
			thisEmployer->myPriceOfSalary() = myprice / rndAdaptFactor;
		}

		if (DebugLevel() > 1 && currStep() >= getInputParameter("DebugFromStep"))
			LogFile() << "\n Indiv_" << neighborIndiv.getID() << " hired ";

		return true;
	}
	else
	{
		// Both traders learn from this FAILED Market interaction

		if (currStep() > getInputParameter("ReleasePricesAt"))
		{
			// Seller (employee): can ask for a lower salary down to minSalary = 1+CPI%?
			// force call to tRandom01()rndAdaptFactor = (1. + AdaptFraction * World().getRandom01()); 
			rndAdaptFactor = PriceAdaptFactor;
			neiprice = neighborIndiv.getmyPriceOfSalary();

			double CPI = 0.;
			if (getDEPData().getCPItracker().getCPI().size() > 0)
				CPI = getDEPData().getCPItracker().getlastCPI();

			double minSalary = 1. * CPI / 100.;
			neighborIndiv.myPriceOfSalary() = max(minSalary, neiprice / rndAdaptFactor);

			// Buyer (employer) increase the salary
			// force call to tRandom01()rndAdaptFactor = (1. + AdaptFraction * World().getRandom01()); 
			rndAdaptFactor = PriceAdaptFactor;
			myprice = thisEmployer->getmyPriceOfSalary();
			thisEmployer->myPriceOfSalary() = myprice * rndAdaptFactor;
		}

		if (DebugLevel() > 1 && currStep() >= getInputParameter("DebugFromStep"))
			LogFile() << "\n Indiv_" << neighborIndiv.getID() << " not hired ";

		return false;
	}
};

bool CProducer::isProductionDay(int Ntoday, int daysInMonth,
	int productionDays, int myFirstProductionDay) const
{
	if (productionDays <= 0 || daysInMonth <= 0 || Ntoday <= 0 || Ntoday > daysInMonth
		|| myFirstProductionDay <= 0 || myFirstProductionDay > daysInMonth) {
		return false;
	}

	int interval = daysInMonth / productionDays;
	int remainder = daysInMonth % productionDays;

	for (int i = 0; i < productionDays; ++i) {
		int day = myFirstProductionDay + i * interval + min(i, remainder);
		if (day > daysInMonth) {
			break;
		}
		if (day == Ntoday) {
			return true;
		}
	}

	return false;
}

bool CProducer::isProductionDay(int workDayN) const
{
	int WorkDaysPerMonth = getInputParameter("WorkDaysPerMonth"); // 20 labor days
	int Nproduction_days = getInputParameter("Nproduction_days"); // 4 once a week
	int myFirstProductionDay = getmyFirstProductionDay();

	if (Nproduction_days <= 0 || WorkDaysPerMonth <= 0
		|| myFirstProductionDay < 0 || myFirstProductionDay >= WorkDaysPerMonth)
		return false;

	int Ntoday = workDayN % WorkDaysPerMonth;
	int interval = WorkDaysPerMonth / Nproduction_days;
	int remainder = WorkDaysPerMonth % Nproduction_days;

	for (int i = 0; i < Nproduction_days; ++i) {
		int day = myFirstProductionDay + i * interval + min(i, remainder);
		if (day > WorkDaysPerMonth) {
			break;
		}
		if (day == Ntoday) {
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////

void CProducer::stepInitialize()
{
	if (getSAM().getSAMGrossOutput_mu(getAgentType()) == 0)
		return;

	CAgent::stepInitialize();

	IncomePrevMonth() = IncomeCurr();
	IncomeCurr() = 0;
	GoodType productType = getAgentType();

	NetTaxesPrevMonth() = NetTaxesCurr();
	NetTaxesCurr() = 0;

	ConsumptionBudget() = 0;
	timeWorked() = 0;
	SharesToTrade() = CShare(getFID(), 0);
};
void CProducer::stepActivity()
{
	if (getSAM().getSAMGrossOutput_mu(getAgentType()) == 0)
		return;

	if (DebugLevel() > 2 && currStep() >= getInputParameter("DebugFromStep"))
	{
		assert(DEPData().CheckAccountingBalance());
		LogFile()
			<< "\n step " << currStep()
			<< " " << getAgentName()
			<< " Cash " << getCash();
		LogFile().flush();
	}

	GoodType productType = getAgentType();

	// Producer initCash is taken from owner
	if (getCash() == 0 && getmyBankAccountStatus()._pAgent == nullptr)
	{
		GoodQtty initCash = max<GoodQtty>(getmySalary_mu(), getWorld().getFinancialMarket().getInitShareValue());
		auto& owner = *Owners().begin()->second;
		if (owner.Cash() > 2. * initCash)
			owner.Cash() -= initCash;
		else
		{
			bool done = GetCashFromBank(initCash);
			if (!done)
				CWorld::ERRORmsg(getAgentName() + " cannot get initCash from Bank", true);
		}

		Cash() += initCash;
		InitCash() = 0; // because initCash is taken from the owner or Bank

		myFixCapital() = initCash;

		GoodQtty nShares = ceil(initCash / getWorld().getFinancialMarket().getInitShareValue());
		myShares(getFID()) += -nShares;
		SharesToTrade() = CShare(getFID(), 0);

		owner.myShares(getFID()) += nShares;
	}

	double deprecRatePerMonth = getDEPData().getDepreciationRate().at(productType) / 12.0;
	if (currStep() <= getInputParameter("AssistedProductionUpto"))
		deprecRatePerMonth *= 4.;

	myFixCapital() = myFixCapital() - myFixCapital() * deprecRatePerMonth;
	KinvestmentOwnerdebt() = 0;

	GoodQtty buyGoodsBudget = MakeListOfGoodsToBuy();
	ConsumptionBudget() = 3 * buyGoodsBudget;

	// ========================  Production ===================================

	BuyGoods();
	BuyGoods(); //M J  two buy rounds because of ExtSect and Gov? price adapt at each round

	GoodType GFCFtype = getSAM().GFCFtype();
	vector<double> GFCFfractionOf = getSAM().getGFCFfractions(); // make a local copy that can be modified
	double toTotalPopulationYear = getDEPData().toTotalPopulationYear();

	double FixCapIncrease = getmyFixCapToBuy();

	if (getmyFixCapToBuy() > 0)
	{
		// Assemble myFixCapToBuy components from GoodsIhave (purchased)

		CTypeDoubleMap maxFixCapfromGoodsIhave;

		// Find the limiting gType from GoodsIhave to preserve the GFCF structure (SAM column)

		for (GoodType gType = 0; gType < getSAM().getnPProducerTypes(); ++gType)
		{
			if (getSAM().GFCFfractionOf(gType) > 0)
			{
				maxFixCapfromGoodsIhave[gType] = getGoodsIhave(gType) / getSAM().GFCFfractionOf(gType);

				if (maxFixCapfromGoodsIhave[gType] < FixCapIncrease)
				{
					if (maxFixCapfromGoodsIhave[gType] > 0)
						FixCapIncrease = maxFixCapfromGoodsIhave[gType];
					else
					{
						if (getRandom01() < getSAM().GFCFfractionOf(gType))
							FixCapIncrease = 0;
						else
							GFCFfractionOf[gType] = 0; // using a local copy that can be modified
					}
				}
			}
		}

		// Assemble this FixCapIncrease as gType components

		if (FixCapIncrease > 0)
		{
			for (GoodType gType = 0; gType < getSAM().getnPProducerTypes(); ++gType)
			{
				double gFixCapcomponent = FixCapIncrease * GFCFfractionOf[gType];
				GoodsIhave()[gType] = max<double>(0, getGoodsIhave(gType) - gFixCapcomponent);
				myFixCapToBuy() = max<double>((GoodQtty)0, myFixCapToBuy() - gFixCapcomponent);

				myFixCapital() += gFixCapcomponent;
				purchasedFixCapital() += gFixCapcomponent; // to be deducted from PayDividends, see RunProducer

				// fix printSAMstep?: split gType row value = IC + GFCF(%denominator not in the SAM)

				if (currStep() > getInputParameter("AssistedProductionUpto"))
					SAM().SAMstepAt(gType, getAgentType()) -= gFixCapcomponent * toTotalPopulationYear; // producer=> deltamyFixCapital
				//M J	SAM().SAMstepAt(GFCFtype, getAgentType()) += gFixCapcomponent * toTotalPopulationYear;
			}
		}
	}

	GoodQtty producedUnits = RunProducer();

	if (producedUnits > 0) // report Producer active this month
		LastUsedTimestep() = currStep();

	mySupply() = getToSell(productType); // includes assisted production

	DEPData().TotSupply()[productType] += getToSell(getAgentType());

	// ========================================================================

	// Return loans to Bank after having paid production dividends

	if (!this->IsCentralBank() && !this->IsPrivateBank())
	{
		auto balance = getmyBankBalance();

		// Return loans 
		if (balance < 0)
			pUsedBank()->ClientCashOperation(this, min(-balance, Cash()));

		double productionExpenses = producedUnits * getmyPriceOf(productType);
		double surplusDividends = max(0, getCash() - 2. * productionExpenses);
		PayDividends(surplusDividends);
	}

	// ----------------------------------------------------------------

	IncomePrevMonth() = IncomeCurr();
	IncomeCurr() = 0;

	NetTaxesPrevMonth() = NetTaxesCurr();
	NetTaxesCurr() = 0;

	currIC_mu() = 0;
	currCompEmployees_mu() = 0;
	currNetTaxes_mu() = 0;
	currDepositInterests_mu() = 0;
	currLoanPaymentsAndInterests_mu() = 0;

	if (DebugLevel() > 1 && currStep() >= getInputParameter("DebugFromStep"))
		assert(DEPData().CheckAccountingBalance());
};

void CProducer::updateStockReferenceAndToBeProduced()
{
	GoodType productType = getAgentType();

	// 1. Update average values

	avgmyDemand() = getavgmyDemand()
		+ getInputParameter("ProductionChangeFraction") * (getmyDemand() - getavgmyDemand());

	LeftToSellHistory().updateHistory(getToSell(productType));
	double avgLeftToSell = updateAndReturnAvgLeftToSell();

	mySupplyHistory().updateHistory(mySupply()); // mySupply is updated in stepActivity
	double avgmySupply = updateAndReturnAvgmySupply();

	double prevProducedUnits = producedUnitsOf(productType); // producedUnitsOf set in RunProducer
	ProductionHistory().updateHistory(prevProducedUnits);
	double avgProduction = updateAndReturnAvgProduction();

	// 2. Decide desired Supply level (StockReference)

	double prevStockReference = StockReference();
	double newStockRef = assistedQttiesOf(productType) // Follows demand
		+ ceil(getmyDemand() * getInputParameter("DesiredStockLevelFactor"));
	double deltaStockReference = newStockRef - prevStockReference;

	double maxdeltaStockReference = StockReference() * getInputParameter("ProductionChangeFraction");
	deltaStockReference = min(maxdeltaStockReference,
		max(-maxdeltaStockReference, deltaStockReference));

	StockReference() = assistedQttiesOf(productType) // Follows smoothed demand
		+ StockReference() + deltaStockReference;

	// Calculate goods to be produced

	ToBeProduced() = max(0.0, StockReference() - getToSell(productType));

	// INFLATION model: increase price if product was almost sold out and there is high demand

	if (getToSell(productType) < getInputParameter("UnsoldFraction") * prevStockReference
		&& deltaStockReference > 0)
	{
		if (getRandom01() < getInputParameter("ProducersMarkup"))
			myMarkupFactor() *= getInputParameter("PriceAdaptFactor"); // Used in RunProduction
	}

	// Keep a copy of these values for next iteration
	prevToBeProduced() = ToBeProduced();
	prevavgmyDemand() = getavgmyDemand();
	prevmyDemand() = getmyDemand();
	myDemand() = 0;
}

void CProducer::adjustProductionCapacityAndStockRef()
{
	double adaptationRate = getInputParameter("ProductionChangeFraction");

	double avgProduction = getAvgProduction();
	double currentUtilization = avgProduction / _productionCapacity;

	if (currentUtilization > 0.9)  // High utilization
		_productionCapacity *= (1 + adaptationRate);
	else if (currentUtilization < 0.6) // Low utilization
		_productionCapacity *= (1 - adaptationRate);

	// Try to ensure capacity doesn't fall below average demand...
	_productionCapacity = max(_productionCapacity, getavgmyDemand());

	// ... but prevent possible unconstrained increase generated by production loops feedback
	if (avgProduction > 0)
	{
		double maxproductionCapacity = avgProduction / getInputParameter("MinUtilizationFactor");
		if (_productionCapacity > maxproductionCapacity)
			_productionCapacity = _productionCapacity
			+ getInputParameter("ProductionChangeFraction") * (maxproductionCapacity - _productionCapacity);

		if (_productionCapacity >= maxproductionCapacity
			&& currStep() > 0)
		{
			GoodType productType = getAgentType();
			double currentStock = getToSell(productType);

			StockReference() *= (1. - getInputParameter("ProductionChangeFraction"));
		}
	}
}

double CProducer::updateAndReturnAvgProduction() {
	return ProductionHistory().updateAndReturnAvg(); // see RunProducer for last data
}

double CProducer::updateAndReturnAvgmySupply() {
	return mySupplyHistory().updateAndReturnAvg();
}
double CProducer::updateAndReturnAvgLeftToSell() {
	return LeftToSellHistory().updateAndReturnAvg();
}

GoodQtty CProducer::MakeListOfGoodsToBuy() // returns productionCost
{
	// -------------- update stockReference, make list to buy and estimate the purchaseBudget -------------

	WorkTimeToBuy() = 0;
	myFixCapToBuy() = 0;
	GoodsToBuy() -= GoodsToBuy(); // set to 0's

	updateStockReferenceAndToBeProduced();

	GoodQtty desiredProductUnits = getToBeProduced();
	if (desiredProductUnits == 0)
		return 0;

	// Query returns extraNeededInputGoods to prepare the _GoodsToBuy list

	GoodType productType = getAgentType();
	const CProductSpecs& productSpecs = CProducer::getProductSpecs(productType);
	GoodQtty producedUnits = 0;
	CTypeDoubleMap deltaInputGoods;
	CTypeDoubleMap extraNeededInputGoods;
	double extraNeededWorkTime = 0;
	double extraNeededK = 0;

	double productionCost = -1; // as a flag from MakeListOfGoodsToBuy instead of from RunProducer
	//  ----------------------------------------------
	double timeworked = Query(desiredProductUnits, producedUnits, productionCost,
		deltaInputGoods, extraNeededInputGoods, extraNeededWorkTime, extraNeededK);

	//  ----------------------------------------------

	// extra needed WorkTime

	if (extraNeededWorkTime < 0)
	{
		// dismiss (-)extraQtty employees
		int tobedismissed = floor(-extraNeededWorkTime);
		for (int n = 0; n < tobedismissed; ++n)
			ReleaseLastEmployee();
	}
	else
		WorkTimeToBuy() = extraNeededWorkTime;

	// extra needed FixCap

	myFixCapToBuy() = extraNeededK * getInputParameter("KtoFixCapitalFactor");

	productionCost += extraNeededK * (getInputParameter("KtoFixCapitalFactor") - 1.);

	//  ------------   make list of IC goods to buy   ----------------------------------

	for (auto& gPair : extraNeededInputGoods)
	{
		auto ICtype = gPair.first;

		double myICtoBuyOf = extraNeededInputGoods.at(ICtype);// best
		double myFixCapToBuyOf = myFixCapToBuy() * getSAM().GFCFfractionOf(ICtype);

		double toBuyOf = myICtoBuyOf + myFixCapToBuyOf;

		setGoodsToBuyOf(ICtype, toBuyOf);
	}

	return productionCost;
};

double CProducer::Query(GoodQtty& desiredProductUnits, GoodQtty& producedUnits,
	double& productionCost, CTypeDoubleMap& deltaInputGoods, CTypeDoubleMap& extraNeededInputGoods,
	double& extraNeededWorkTime, double& extraNeededK) const
{
	// Reports usedWorkTime, current productQtty and
	//  extraNeededInputGoods for desiredProductUnits

	if (desiredProductUnits == 0)
		return 0;

	const CGoods availableInputGoods = getGoodsIhave();
	GoodType productType = getAgentType();
	const CProductSpecs& productSpecs = CProducer::getProductSpecs(productType);
	CTypeDoubleMap NeededPerUnit = productSpecs.getNeededPerUnit(); // make a local copy that can be modified
	double NeededWorkTimePerUnit = productSpecs.getCompEmployees();
	double neededWorkers = 0;
	CTypeDoubleMap MaxFromGoodType;
	deltaInputGoods.clear();
	extraNeededInputGoods.clear();

	double MaxFromWorkTime = 0;
	double usedWorkTime = 0;
	extraNeededWorkTime = 0;

	double MaxFromFixCap = 0;
	extraNeededK = 0;

	double availableWorkTime = getavailableWorkTime();
	double currentK = getmyFixCapital() / getInputParameter("KtoFixCapitalFactor");

	// ------------------------------------------------------------------------------------

	// 1. First calculate MaxFromGoodType and extraNeededInputGoods for workers and owners (K)

	if (getInputParameter("CobbDouglas") == 1)
	{
		if (currentK <= 0) // initialisation
			CWorld::ERRORmsg("currentK<=0", true);

		// Set the next quantities based on desiredProductUnits
		extraNeededWorkTime = 0;
		extraNeededK = 0;
		GoodQtty currentMaxOutput = currProductionSize_mu(); // using full current K and L
		MaxFromWorkTime = currentMaxOutput;
		MaxFromFixCap = currentMaxOutput;

		// 1. FIRM SIZE for desiredProductUnits: Labor and Capital factors

		double desiredVA = DesiredVA(desiredProductUnits);

		double desiredL = desiredVA * _initL / _initVA;
		usedWorkTime = desiredL / getmySalary_mu(); // allow part time workers
		extraNeededWorkTime = usedWorkTime - availableWorkTime; // hire/dismiss if positive/negative

		double desiredK = desiredVA * _initK / _initVA;
		currentK = getmyFixCapital() / getInputParameter("KtoFixCapitalFactor");
		extraNeededK = max(0., desiredK - currentK);
		// K can only decay with time (Depreciation rate)

		if (currStep() > getInputParameter("AssistedProductionUpto"))
			extraNeededK = min(extraNeededK, currentK * getInputParameter("ProductionChangeFraction"));
	}
	// Leontief
	else
	{
		double availableWorkTime = 0.0;
		if (productSpecs.getCompEmployees() > 0)
			availableWorkTime = getavailableWorkTime();

		// Check this desiredProductUnits for constraints from labor requirements

		if (productSpecs.getCompEmployees() > 0)
		{
			// WorkTime: current MaxFromGoodType from availableWorkTime, and extraNeededWorkTime

			usedWorkTime = (desiredProductUnits * productSpecs.getCompEmployees()) / getmySalary_mu();
			MaxFromWorkTime =
				floor(availableWorkTime * getmySalary_mu() / productSpecs.getCompEmployees());
			extraNeededWorkTime = usedWorkTime - availableWorkTime; // negative: dismiss
		}
		else // "CompEmployees == 0": no workers constraint
		{
			MaxFromWorkTime = desiredProductUnits;
			extraNeededWorkTime = 0;
		}

		// FixCapital: current MaxFromGoodType, and extraNeeded for desiredProductUnits

		if (getmyGrossOpSurplus() > 0)
		{
			// Includes start up producer: currentK = myFixCapital() == 0
			MaxFromFixCap = floor(currentK / getmyGrossOpSurplus());
			double newK = desiredProductUnits * getmyGrossOpSurplus();

			extraNeededK = max<double>(0, newK - currentK);
			if (currentK == 0)
				extraNeededK = min(extraNeededK, getmySalary_mu() * getInputParameter("ProductionChangeFraction"));
			else
			{
				double mincurrentK = max(currentK, 10. * getmySalary_mu());
				extraNeededK = min(extraNeededK, mincurrentK * getInputParameter("ProductionChangeFraction"));
				//	extraNeededK = min(extraNeededK, currentK * getInputParameter("ProductionChangeFraction"));
			}
		}
		else // "K == 0": no FixCapital constraint
		{
			MaxFromFixCap = desiredProductUnits;
			extraNeededK = 0;
		}
	}

	// ------------------------------------------------------------------------------------

	// 2. Find current MaxFromGoodType and extraNeededInputGoods for desiredProductUnits

	for (const auto& ICpair : availableInputGoods)
	{
		GoodType ICtype = ICpair.first;

		GoodQtty neededForGFCF = ceil(extraNeededK * getSAM().GFCFfractionOf(ICtype));

		GoodQtty availableOfICtype = max<GoodQtty>(0, availableInputGoods(ICtype) - neededForGFCF); // secure GFCFqtty

		GoodQtty maxFromThisICtype = desiredProductUnits;
		double neededPerUnit = NeededPerUnit.at(ICtype);
		if (neededPerUnit > 0)
			maxFromThisICtype = floor(availableOfICtype / neededPerUnit);
		/*
		else if (neededForGFCF > 0)
			//M J ?	maxFromThisICtype = desiredProductUnits * (availableInputGoods(ICtype) / neededForGFCF);
		*/

		// buy extraNeededInputGoods for desiredProductUnits

		if (maxFromThisICtype < desiredProductUnits) // buy ICproductType is included here
			extraNeededInputGoods[ICtype] =
			max<double>(0., ceil(desiredProductUnits * neededPerUnit - availableOfICtype));

		if (neededPerUnit > 0) //no seed products, force purchase from other producers of this same sector
			// && ICtype != productType) // don't limit production from own product lack of stock
			MaxFromGoodType[ICtype] = maxFromThisICtype;// it will be withdrawn from final output
	}

	// 3. Find the max productUnits that can be produced with the availableInputGoods

	producedUnits = desiredProductUnits;

	if (MaxFromWorkTime < producedUnits)
		producedUnits = MaxFromWorkTime;

	if (MaxFromFixCap < producedUnits)
	{
		if (MaxFromFixCap > 0)
			producedUnits = MaxFromFixCap;
		else
		{
			double NeededKPerUnit = 1. * getmyGrossOpSurplus();
			if (getRandom01() < NeededKPerUnit)
				producedUnits = 0;
		}
	}

	for (const auto& ICpair : MaxFromGoodType)
	{
		GoodType ICtype = ICpair.first;

		if (MaxFromGoodType[ICtype] < producedUnits)
		{
			if (MaxFromGoodType[ICtype] > 0)
				producedUnits = MaxFromGoodType[ICtype];
			else
			{
				if (getRandom01() < NeededPerUnit.at(ICtype))
					producedUnits = 0;
				else
					NeededPerUnit.at(ICtype) = 0; // using a local copy that can be modified
			}
		}
	}

	// producedUnits (may be 0) can be produced with the availableInputGoods

	//  ===================================================================================

	// 4. Report productionCost and deltaInputGoods

	GoodQtty costUnits = max(producedUnits, desiredProductUnits);

	if (productionCost == -1) // call from MakeListOfGoodsToBuy instead of from RunProducer
		NeededPerUnit = productSpecs.getNeededPerUnit(); // use real NeededPerUnit values

	productionCost = 0;
	productionCost += costUnits * productSpecs.getCompEmployees() * getmyPriceOfSalary();
	productionCost += costUnits * getmyGrossOpSurplus();

	//  ------------------------------------------------------------------------------------

	// Intermediate consumption:

	double neededPerUnit = 0;
	for (const auto& ICpair : NeededPerUnit)
	{
		const GoodType ICtype = ICpair.first;

		neededPerUnit = NeededPerUnit.at(ICtype);

		if (neededPerUnit == 0)
			continue;

		productionCost += costUnits * neededPerUnit * getmyPriceOf(ICtype);
		double neededOfType = producedUnits * neededPerUnit;

		deltaInputGoods[ICtype] = -neededOfType;  // product consumed as IC is included here 
	}

	// Taxes
	if (getSAM().getAccountGroups().find("T") != getSAM().getAccountGroups().end())
		for (const auto& Tpair : getSAM().getAccountGroups().at("T"))
		{
			auto TRow = Tpair->accN();
			double taxPerUnit = ((double)getSAM().getRowCol(TRow, productType)
				/ CProducer::getProductSpecs(getAgentType()).getGrossOutput_mu());

			// --------------------------------------------------------------------
			if (currStep() >= getInputParameter("TaxChangeFrom")
				&& currStep() <= getInputParameter("TaxChangeUpto")
				&& productType == getInputParameter("ProducerAccN")
				&& TRow == getInputParameter("TaxAccN"))
				taxPerUnit *= getInputParameter("TaxChangeFactor");
			// --------------------------------------------------------------------

			productionCost += costUnits * taxPerUnit;
		}

	assert(productionCost >= 0);

	usedWorkTime = (producedUnits * productSpecs.getCompEmployees()) / getmySalary_mu();

	return usedWorkTime;
};

GoodQtty CProducer::RunProducer()
{
	GoodQtty producedUnits = 0;
	GoodQtty desiredProductUnits = getToBeProduced();
	if (desiredProductUnits == 0)
		return producedUnits;

	GoodType productType = getAgentType();
	GoodType GFCFtype = getSAM().GFCFtype();

	if (getSAM().IsExtSectType(productType))
	{
		ToSell(productType) += desiredProductUnits;
		ToBeProduced() = 0;
		return desiredProductUnits;
	}

	LeftToSell() = getToSell(productType);

	double productionCost = 0;
	CTypeDoubleMap deltaInputGoods;
	CTypeDoubleMap extraNeededInputGoods; // return desirable extra input quantities
	double extraNeededWorkTime = 0;
	double extraNeededK = 0;
	const CProductSpecs& productSpecs = CProducer::getProductSpecs(productType);

	//  *******************  RUN PRODUCER  *********************

	timeWorked() = Query(desiredProductUnits, producedUnits, productionCost,
		deltaInputGoods, extraNeededInputGoods, extraNeededWorkTime, extraNeededK);

	producedUnitsOf(productType) = producedUnits;

	DEPData().TotProduced()[productType] += producedUnits * 12. * getDEPData().getUpscaleSimulationFactor();
	DEPData().TotalProduction() += producedUnits * 12. * getDEPData().getUpscaleSimulationFactor();

	// If timeWorked>0 then also producedUnits>0, carry out the production changes.

	if (producedUnits == 0)
		return 0;

	GoodQtty neededFromBank = max(0., 2 * productionCost - getCash());
	bool granted = true;
	if (neededFromBank > 0)
		granted = GetCashFromBank(neededFromBank);

	if (granted == false)
		return 0;

	double finalProductionCost = 0;

	// Dismiss surplus employees
	double totAvailableTime = 0;
	double timeToBePayed = gettimeWorked();
	//	GoodQtty minWorkers = doubleToGQtty(getEmployees().size() * (1. - getInputParameter("ProductionChangeFraction")));
	for (auto& pWorker : Employees())
	{
		double thisWorkerTime = min(timeToBePayed, pWorker->getmyAvailableTime());
		timeToBePayed -= thisWorkerTime;
		totAvailableTime += thisWorkerTime;

		// Dismiss surplus employees, if any
		if (totAvailableTime >= gettimeWorked())
		{
			while (true)
			{
				if (pWorker != Employees().back())
					//		&& (GoodQtty)Employees().size() > minWorkers
					ReleaseLastEmployee(); // these workers didn't work here
				else
					break;
			}
		}

		if (pWorker == Employees().back())
			break;
	}

	// Pay CompEmployees  -------------------------------------------------------------

	double compEmployees = producedUnits * productSpecs.getCompEmployees();//M J * mypriceOfSalary?

	// Pay salaries

	string txt = "";
	auto firstLrow = getSAM().getAccountGroups().at("L").at(0)->accN();
	auto perYear = 12. * ((double)getSAM().getActive()
		* (1.0 - getDEPData().getUnemployment()) / getWorld().getNWorkers());

	double salariesPayed = 0;
	timeToBePayed = gettimeWorked();
	bool done = false;
	if (timeToBePayed > 0)
	{
		for (auto& pWorker : Employees())
		{
			double thisWorkerTime = min(timeToBePayed, pWorker->getmyAvailableTime());

			double mySalary = compEmployees * thisWorkerTime / totAvailableTime;

			timeToBePayed -= thisWorkerTime;
			auto LgroupN = pWorker->getmyLgroupN();
			auto HgroupN = pWorker->getmyHgroupN();

			done = PayTo(pWorker, mySalary, txt);
			DEPData().GDPVAnominal() += mySalary * perYear;

			if (!done)
				CWorld::ERRORmsg("not enough money to pay salary", true);
			else
			{
				finalProductionCost += mySalary;
				currCompEmployees_mu() += mySalary;
				DEPData().TotalSalaries() +=
					mySalary / (getWorld().getInitSalaryCalibFactor() * getSAM().getInitSalary());
				DEPData().TotalWorkedTime() += thisWorkerTime;
				pWorker->IncomeCurr() += mySalary;
				SAM().SAMstep()[LgroupN][productType] += mySalary * perYear;
				SAM().SAMstep()[HgroupN][LgroupN] += mySalary * perYear;
			}

			// ExtSectors may pay salaries to employees (e.g. SAMEXT90agreg)

			auto myLix = pWorker->getmyLgroupN() - firstLrow;
			long ixSect = -1;
			for (auto& pairExtSect : getExtSectors())
			{
				++ixSect;
				auto qtty = getWorld().getExtSectorsToLabor().at(ixSect).at(myLix);
				if (qtty > 0)
				{
					CWorld::ERRORmsg("Is this correct?", true);
					string ExtSectconcept = "";

					done = pairExtSect.second->PayTo(pWorker, qtty, ExtSectconcept);
					if (!done)
						CWorld::ERRORmsg("not enough money to pay ExtSectSalary", true);

					pWorker->IncomeCurr() += qtty;
				}
			}

			salariesPayed += mySalary;
			pWorker->myAvailableTime() = max(0., pWorker->getmyAvailableTime() - thisWorkerTime);
			if (pWorker->myAvailableTime() > 0 && pWorker->myAvailableTime() < 1.0)
				pWorker->bPartTimeWorker() = true;

			// Dismiss surplus employees
			if (salariesPayed >= compEmployees)
			{
				while (true)
				{
					if (pWorker == Employees().back())
						break;
					else
						ReleaseLastEmployee();
				}
			}

			if (pWorker == Employees().back())
				break;
		}
	}

	// Dismiss part-time employees, so they can be still employed this month

	for (int n = 0; n < Employees().size(); ++n)
	{
		auto pWorker = Employees().at(n);
		if (pWorker->getmyAvailableTime() > 0)
			ReleaseThisEmployee(*pWorker);
	}

	// deltaInputGoods  ---------------------------

	for (const auto& pair : deltaInputGoods)
	{
		auto ICtype = pair.first;

		auto ICqtty = pair.second;

		GoodsIhave()[ICtype] += deltaInputGoods.at(ICtype);
		assert(getGoodsIhave(ICtype) >= 0);

		finalProductionCost += -ICqtty * getmyPriceOf(ICtype);
		currIC_mu() += -ICqtty * getmyPriceOf(ICtype);
	}

	auto finalProducedUnits = producedUnits;

	// pay Taxes  ----------------------------------------------------------------------

	double TotalNetTax = 0;
	double toTotalPopulationYear = getDEPData().toTotalPopulationYear();
	if (getSAM().getAccountGroups().find("T") != getSAM().getAccountGroups().end())
	{
		for (const auto& Tpair : getSAM().getAccountGroups().at("T"))
		{
			auto TRow = Tpair->accN();
			double taxPerUnit = ((double)getSAM().getRowCol(TRow, productType)
				/ CProducer::getProductSpecs(getAgentType()).getGrossOutput_mu());

			// --------------------------------------------------------------------
			if (currStep() >= getInputParameter("TaxChangeFrom")
				&& currStep() <= getInputParameter("TaxChangeUpto")
				&& productType == getInputParameter("ProducerAccN")
				&& TRow == getInputParameter("TaxAccN"))
				taxPerUnit *= getInputParameter("TaxChangeFactor");
			// --------------------------------------------------------------------

			SAM().SAMstep()[TRow][productType]
				+= taxPerUnit * finalProducedUnits * toTotalPopulationYear;
				SAM().SAMstep()[getSAM().getAccNofName("Government")][TRow]
					+= taxPerUnit * finalProducedUnits * toTotalPopulationYear;

					TotalNetTax += taxPerUnit
						* finalProducedUnits // * (1.0 + getproductSpecs.getNeededPerUnitOf(productType)))
						* getmyPriceOf(productType);
		}
	}

	if (TotalNetTax != 0)
	{
		txt = "";
		bool done = PayTo(pGovernment(), TotalNetTax, txt);
		if (!done)
			CWorld::ERRORmsg("not enough money to pay TotalNetTax", true);

		NetTaxesCurr() += TotalNetTax;
		currNetTaxes_mu() += TotalNetTax;
	}
	finalProductionCost += TotalNetTax;
	DEPData().GDPVAnominal() += TotalNetTax * perYear;

	// Pay dividends to owners, net of purchasedFixCapital (see stepActivity FixCap assemblage): ---------------------------------------

	GoodQtty dividends = producedUnits * getmyGrossOpSurplus();

	if (purchasedFixCapital() <= dividends)
	{
		dividends = dividends - purchasedFixCapital();
		purchasedFixCapital() = 0;
	}
	else
	{
		purchasedFixCapital() -= dividends;
		dividends = 0;
	}

	PayDividends(dividends); // see PayDividends(surplusDividends) in stepActivity()

	// the IC that was consumed for assembling the purchasedFixCapital are not whitin these producedUnits
	finalProductionCost += producedUnits * getmyGrossOpSurplus();

	// -------  PRODUCTION PRICE  ------------------------

	if (currStep() > getInputParameter("AdjustUnemploymentFrom") && finalProducedUnits > 0)
	{
		productionPrice() = finalProductionCost / finalProducedUnits;

		if (productionPrice() > 4)
			CWorld::ERRORmsg("productionPrice() > 4", true);
	}

	// INFLATION model

	if (currStep() > getInputParameter("AssistedProductionUpto") && finalProducedUnits > 0)
		setmyPriceOf(productType, max(productionPrice() * getmyMarkupFactor(), getmyPriceOf(productType)));

	// ASSISTED PRODUCTION stage: Help to start by adding non-produced qtty  -----------

	assistedQtties().clear();
	double assistedUptoStep = getInputParameter("AssistedProductionUpto");
	double assistedRange = assistedUptoStep; // Assisted always from 0
	GoodQtty delta = 0;
	if (productType >= getInputParameter("freeTypeFrom") // for debugging purposes
		&& productType <= getInputParameter("freeTypeTo"))
	{
		delta = max<GoodQtty>(0, ToBeProduced() - producedUnits);
		if (delta > 0)
		{
			ToSell(productType) += delta;
			assistedQttiesOf(productType) += delta;
		}
	}
	else if (currStep() <= assistedUptoStep)
	{
		// Note: disable fade if using selfadjustment of AssistedProductionUpto
		delta = max<GoodQtty>(0, ToBeProduced() - producedUnits)
			* max<double>(0, assistedUptoStep - currStep()) / assistedRange; // fade out
		if (delta > 0)
		{
			finalProducedUnits += delta;
			ToSell(productType) += delta; // twice, see below. Sould be removable, but it's ok because <= assistedUptoStep
			assistedQttiesOf(productType) += delta;

			// DeleteMyMoney(delta); does not work here
		}
	}

	ToSell(productType) += finalProducedUnits;

	return finalProducedUnits;
};

///////////////////////////////////////////////////////////////////////////////////////

//======================  CProducerSpecs  ===================================

CProducerSpecs::CProducerSpecs(GoodType producerTy)
	: _producerType(producerTy)
{
	// Producer definition parameters, they are not modified during the simulation

	locationType() = -1;
};
CProducerSpecs::~CProducerSpecs() {};

ofstream& operator<<(ofstream& ofstrm, const CProducerSpecs& producerSpecs)
{
	string tab1 = "\t", tab2 = "\t\t", tab3 = "\t\t\t";

	ofstrm << tab1 << CProducer::ProducerLabelOfType()[producerSpecs.getproducerType()]
		<< " {" << endl;

	if (producerSpecs.getlocationType() >= 0)
		ofstrm << tab2 << "locationTypeName " << producerSpecs.getlocationTypeName() << endl;

	ofstrm << tab1 << "}" << endl;

	return ofstrm;
};
ifstream& operator>>(std::ifstream& infile, CProducerSpecs& producerSpecs)
{
	std::string productName, word, name, bracket;
	double value = 0;

	infile >> bracket;// bracket"{"

	while (infile >> word, word != "}")
	{
		if (word == "locationTypeName")
		{
			infile >> word;
			producerSpecs.locationType() = getWorld().getGISMap().getCellNameToType(word);
			continue;
		}

		CWorld::ERRORmsg(
			"Unexpected word reading PRODUCER_DEFINITIONS " + word, true);
	}

	return infile;
}

GoodType& CProducerSpecs::producerType() { return _producerType; };
CellType& CProducerSpecs::locationType() { return _locationType; };
const CellType& CProducerSpecs::getlocationType() const { return _locationType; };
const CellName& CProducerSpecs::getlocationTypeName() const
{
	return getWorld().getGISMap().getCellTypeToName(_locationType);
};
const GoodType& CProducerSpecs::getproducerType() const { return _producerType; };
const CProductSpecs& CProducerSpecs::getSpecsOfProductType(GoodType productType)
{
	return CProducer::getProductSpecs(productType);
};
