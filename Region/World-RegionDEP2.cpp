
//  World.cpp

//   DEPLOYERS v2

// >>>>>>  READ notes at the beginning of pch.h  <<<<<<<<<

#include "./pch.h"

//======================  CWorld  ===================================

// static
string CWorld::_SimulationName;
map<string, CSimulatedCountry*>* CWorld::_pSimulatedCountries;

CCentralBank*& pCentralBank() { return _pCentralBank; };
const CCentralBank* getpCentralBank() { return _pCentralBank; };
CCentralBank& CentralBank() { return *_pCentralBank; };
const CCentralBank& getCentralBank() { return *_pCentralBank; };

AgentType& CentralBankType() { return _CentralBankType; };
const AgentType getCentralBankType() { return _CentralBankType; };
AgentType& PrivateBankType() { return _PrivateBankType; };
const AgentType getPrivateBankType() { return _PrivateBankType; };

bool& BanksAreProducers() { return _BanksAreProducers; }
bool getBanksAreProducers() { return _BanksAreProducers; }

bool IsCentralBankType(GoodType gType) { return gType == getCentralBankType(); }
bool IsPrivateBankType(GoodType gType) { return gType == getPrivateBankType(); }
bool IsBankType(GoodType gTy) { return (gTy == getCentralBankType() || gTy == getPrivateBankType()); }
bool IsProducerBankType(GoodType gTy)
{
	return (IsBankType(gTy) && BanksAreProducers());
}
bool IsNonProducerBankType(GoodType gTy)
{
	return (IsBankType(gTy) && !BanksAreProducers());
}

CWorld::CWorld()
{
	pWorld() = this;

	_pSimulatedCountries = new map<string, CSimulatedCountry*>;
	_pSAM = nullptr;
	_pFIGARO = nullptr;

	_pGovernment = nullptr;

	pCentralBank() = nullptr;

	_pRandomListOfIndivIDs = new vector<AgentID>;
	_pInteractingAgents = new vector<CAgent*>;

	_pIndividuals = new vector<CIndividual*>;
	_pProducers = new vector<CProducer*>;

	_pFinancialMarket = nullptr;

	_pGISMap = nullptr;

	pExtSectorsToLabor() = nullptr;
}
CWorld::~CWorld()
{
	delete _pGISMap;
	delete _pIndividuals;
	delete _pProducers;
	delete _pInteractingAgents;
	delete _pRandomListOfIndivIDs;
}

ofstream& operator<<(ofstream& ofstrm, const CWorld& world)
{
	ofstrm << "//   DEPLOYERS 2.0  -----   \n\n";

	getDEPData().writeInputParameters(ofstrm);
	getSAM().writeSAMTXT(ofstrm);
	world.writeSnapshot(ofstrm);

	return ofstrm;
}
ifstream& operator>>(ifstream& ifstrm, CWorld& world)
{
	char a = 0;
	string word;
	double value = 0;

	ifstrm >> word;
	while (!ifstrm.eof())
	{
		if (word == "//")
		{
			getline(ifstrm, word); // line comment
			ifstrm >> word;
			continue;
		}
		else if (word == "/*")
		{
			while (ifstrm >> word, word != "*/"); // multiline comment
			ifstrm >> word;
			continue;
		}
		else if (word == "INPUT_PARAMETERS")
		{
			DEPData().readInputParameters();
			ifstrm >> word;

			continue;
		}
		else if (word == "SAM_table" || word == "rndstatus")
		{
			// they're read by SAM().readSAMTXT() and readrndstatus()
			const long maxchars = 30000;
			char* pField = new char[maxchars];
			while (ifstrm.peek() != '}')
				ifstrm.getline(pField, maxchars);
			ifstrm >> word;
			ifstrm >> word;
			delete[] pField;
			continue;
		}
		else if (word == "SNAPSHOT(World_status)")
		{
			ifstrm >> word;
			continue;
		}
		else if (word == "MAP")
			break;
		else
		{
			CWorld::ERRORmsg(
				"Unexpected word while reading input " + word, true);
		}
	}

	return ifstrm;
}

/////////////////////  Static objects  & functions  /////////////////////////

void CWorld::ERRORmsg(string str, bool quit)
{
#ifdef DEPLOYERS_GRAPHICS
	char* msg = (char*)str.c_str();
	GlgError(GLG_USER_ERROR, msg);

	if (quit && getInputParameter("ExitOnQuit"))
	{
		ofstream ofstrm;
		ofstrm.open("_ERROR-message.dep");
		if (ofstrm.is_open())
		{
			ofstrm << endl << str << endl;
			ofstrm.close();
		}
		else
		{
			// Handle the error of opening the file
		}
		exit(-1);
	}
#elif defined(LINUX_VERSION)
	// NoGRAPH, Linux
	cout << str << endl;
	if (quit)
		exit(-2);
#endif
};
const double CWorld::getRandom01()
{
	uniform_real_distribution<> myRand01(0, 1);

	double rnd = myRand01(myRandomEngine());

	if (DebugLevel() > 5 && currStep() >= getInputParameter("DebugFromStep"))
		LogFile() << " rnd" << rnd;

	return rnd;
}

const CNeighbors& CWorld::getNeighbors(CExtVector xy)
{
	return GISMap().getNeighbors(xy);
};

bool CWorld::getbRunning() const
{
	return _bRunning;
};
bool CWorld::getbFinished() const
{
	return _bFinished;
};
bool CWorld::getbPaused() const
{
	return _bPaused;
};
bool& CWorld::bRunning() { return _bRunning; };
bool& CWorld::bFinished() { return _bFinished; };
bool& CWorld::bPaused() { return _bPaused; };
bool& CWorld::bPlotBorders() { return _bPlotBorders; };
void CWorld::writeTimeAndDate(ofstream& ofstrm) const
{
#if defined _WINDOWS
	time_t rawtime;
	time(&rawtime); // get current calendar time

	struct tm timeinfo;
	localtime_s(&timeinfo, &rawtime);

	const long buffer_size = 256;
	char buffer[256];
	asctime_s(buffer, buffer_size, &timeinfo); //do the conversion

	ofstrm << buffer; // << endl;
#endif
}

CTypeDoubleMap& CWorld::ConsumPXFactor() { return _ConsumPXFactor; };
CTypeDoubleMap CWorld::getConsumPXFactor() const { return _ConsumPXFactor; };
CTypeDoubleMap& CWorld::RefTotalHouseholdPXConsum() { return _RefTotalHouseholdPXConsum; }
CTypeDoubleMap CWorld::getRefTotalHouseholdPXConsum() const { return _RefTotalHouseholdPXConsum; };
CTypeDoubleMap& CWorld::GovConsumPXFactor() { return _GovConsumPXFactor; };
CTypeDoubleMap CWorld::getGovConsumPXFactor() const { return _GovConsumPXFactor; };
CTypeDoubleMap& CWorld::RefGovConsumPX() { return _RefGovConsum; }
CTypeDoubleMap CWorld::getRefGovConsumPX() const { return _RefGovConsum; };

CTypeDoubleMap& CWorld::FixCapitalProductivity() { return _FixCapitalProductivity; }
CTypeDoubleMap CWorld::getFixCapitalProductivity() const { return _FixCapitalProductivity; };

CIndividual* CWorld::newIndividual()
{
	long ID;
	for (ID = 0; ID < (long)pIndividuals()->size(); ++ID)
		if (pIndividuals()->at(ID) == nullptr)
		{
			pIndividuals()->at(ID) = new CIndividual(ID);
			return pIndividuals()->at(ID);
		}

	ID = (long)pIndividuals()->size();

	CIndividual* pIndiv = new CIndividual(ID);
	pIndividuals()->push_back(pIndiv);
	return pIndiv;
};
vector<CIndividual*>*& CWorld::pIndividuals() { return _pIndividuals; };
vector<CIndividual*>& CWorld::Individuals() { return *_pIndividuals; };
const vector<CIndividual*>* CWorld::getpIndividuals() const { return _pIndividuals; };
long CWorld::getNIndividuals() const { return (long)getpIndividuals()->size(); }
void CWorld::setupExtSectorsToLaborArray() // static
{
	// ExtSectors may pay salaries to employees (see SAMEXT90agreg)

	delete pExtSectorsToLabor();
	pExtSectorsToLabor() = new vector<vector<GoodQtty>>(getExtSectors().size(),
		vector<GoodQtty>(SAM().AccountGroups().at("L").size(), 0));
	long ixSect = -1;
	for (auto& pairExtSect : getExtSectors())
	{
		++ixSect;
		auto ExtSectcol = pairExtSect.first;
		auto pExtSect = pairExtSect.second;

		long ixL = -1;
		for (auto& pLrowAcc : SAM().AccountGroups().at("L"))
		{
			++ixL;
			auto Lrow = pLrowAcc->accN();

			(*pExtSectorsToLabor())[ixSect][ixL] = (GoodQtty)(getSAM().getRowCol(Lrow, ExtSectcol)
				/ (12.0 * getSAM().getActive() * 0.01 * getSAM().getInitUnemploymentPercent()));
		}
	}
}

void CWorld::writeIndividuals(ofstream& ofstrm) const
{
	ofstrm << "\n PropToConsume " << CIndividual::getPropToConsume() << endl;
	ofstrm << "\n" << "Individuals " << getpIndividuals()->size() << " {";
	for (auto pIndiv : *getpIndividuals())
	{
		ofstrm << *pIndiv;
	}
	ofstrm << " }\n";
};
void CWorld::readIndividuals(ifstream& ifstrm)
{
	string word, bracket;
	long id = -1;

	ifstrm >> word >> CIndividual::PropToConsume();
	ifstrm >> word >> id >> bracket; // "Individuals: nn {"
	for (auto pInd : *pIndividuals())// (long n = 0; n < nIndiv; ++n)
	{
		ifstrm >> word >> word; // "Indiv {";
		ifstrm >> word >> id;
		CIndividual* pIndiv = pIndividuals()->at(id);

		ifstrm >> *pIndiv;
		ifstrm >> bracket;// " }"
	}
	ifstrm >> bracket;// " }"
};

void CWorld::initializeIndividuals()
{
	auto initialCash = DEPData().InputParameter("nInitSalaries") * getSAM().getInitSalary(); // some years salary;
	auto GFCFtype = getSAM().GFCFtype();
	CBank* pBank0 = nullptr;

	for (auto& pIndiv : *pIndividuals())
	{
		// force call to avoid not calling Random01(), due to Release optimization, if BondAssetsFraction is zero
		double rnd = getRandom01();
		pIndiv->_BondToSharesRatio = rnd * DEPData().InputParameter("BondToSharesRatio");

		CExtVector initPos =
			GISMap().getRandomPositionOnType(pIndiv->getlocationType());

		pIndiv->insertInGISMapLocation(initPos);
		pIndiv->assignHandLgroups();

		pIndiv->myGoodsIwish().clear();

		pIndiv->InitCash() = initialCash;
		pIndiv->Cash() = initialCash;

		// ============ Open a Bank from start, for Indivs and Producers =============

		if (pIndiv->getID() == 0)
		{
			if (DEPData().InputParameter("MaxNBanks") > 1)
				pIndividuals()->at(0)->TryToFoundCommercialBank();
			pBank0 = pIndividuals()->at(0)->pOwnedBank();
		}

		pIndiv->pUsedBank() = pBank0;
	}

	CIndividual::PropToConsume() = getInputParameter("PropToConsume");
	RefTotalHouseholdPXConsum().clear();
	ConsumPXFactor().clear();
	RefGovConsumPX().clear();
	GovConsumPXFactor().clear();

	// Individuals have their own idiosyncratic preferences around average values:
	double IwishSpreadFraction = getInputParameter("IwishSpreadFraction");

	DEPData().InitialProducerTypes().clear();// ---------------   Start one Producer of each type
	for (GoodType gType = 0; gType < getSAM().getnPproducerTypes(); ++gType)
	{
		DEPData().InitialProducerTypes().push_back(gType);
		DEPData().nCurrentPProducers()[gType] = 0;
	}

	for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
	{
		RefTotalHouseholdPXConsum()[gType] = 0;
		ConsumPXFactor()[gType] = getInputParameter("InitConsumPXFactor");

		RefGovConsumPX()[gType] = 0;
		GovConsumPXFactor()[gType] = getInputParameter("InitConsumPXFactor");
	}

	// Household cols
	for (auto& pIndiv : *pIndividuals())
	{
		pIndiv->GoodsIhave()[GFCFtype] = 0; // GoodsIhave(GFCF) is flow, _myGFCF is the stock
		pIndiv->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

		for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
		{
			// The gType fraction of GFCF is included in its buyQtty below

			// ---------------   Start one PProducer of each type  -------------------
			if (gType < getSAM().getnPproducerTypes()
				&& DEPData().nCurrentPProducers().at(gType) < getInputParameter("MinCurrentPProducers"))
			{
				if (pIndiv->getID() == gType
					&& getSAM().getSAMGrossOutput_mu(gType) > 0)
					CProducer* pnewProd = newProducer(gType, pIndiv);
			}

			pIndiv->setmyPriceOf(gType, 1.0);

			// Use Hcol data for each individual
			GoodType HCol = pIndiv->getmyHgroupN();

			// total units to buy per year
			double buyQtty = (double)getSAM().getRowCol(gType, HCol);

			// Add this gType fraction of the GFCF qtty
			buyQtty += getSAM().getRowCol("GFCF", HCol) * SAM().GFCFfractionOf(gType);

			// Individuals have their own preferences around the average value
			buyQtty *= ((1.0 - 0.5 * IwishSpreadFraction) + IwishSpreadFraction * getRandom01());

			// downscale to units per month, per active (nIndividuals)
			GoodQtty buyQ = doubleToGQtty(buyQtty / (12.0 * getSAM().getActive()));

			// all sectors, no	if (buyQ > 0)
			pIndiv->myGoodsIwish()[gType] = buyQ;

			pIndiv->GoodsIhave()[gType] = 0; // no initial stock
		}
	}
};

CProducer* CWorld::newProducer(AgentType producerType, CAgent* pinitialOwner)
{
	if (producerType < 0 || producerType >= getSAM().getnPproducerTypes()
		|| getSAM().getSAMGrossOutput_mu(producerType) == 0)
		return nullptr;

	CProducer* pProducer = nullptr;
	long ID;
	for (ID = 0; ID < (long)pProducers()->size(); ++ID)
		if (pProducers()->at(ID) == nullptr) // available ID found, otherwise ID=size()
			break;

	pProducer = new CProducer(ID, producerType, pinitialOwner);
	++DEPData().FirmBirths()[currStep() % 12];
	pProducer->initialize();
	pProducer->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	int WorkDaysPerMonth = getInputParameter("WorkDaysPerMonth"); // 20 labor days
	int Nproduction_days = getInputParameter("Nproduction_days"); // 4 once a week
	int interval = WorkDaysPerMonth / Nproduction_days;
	pProducer->myFirstProductionDay() = (long)(getRandom01() * interval);

	if (ID < pProducers()->size())
		pProducers()->at(ID) = pProducer; // vector of all Producers
	else
		pProducers()->push_back(pProducer);

	CExtVector initPos(0, 0);
	if (pinitialOwner == nullptr) // ExtSector Producers
	{
		// initPos = GISMap().getRandomPosition();
		pProducer->pUsedBank() = nullptr;
	}
	else
	{
		initPos = GISMap().getRandomPositionNear(
			pinitialOwner->getLocation(), pProducer->getlocationType());

		if (initPos.Getx() >= 0)
			pProducer->insertInGISMapLocation(initPos);
		else
			ERRORmsg("Couldn't find " +
				CProducer::getProducerLabelOfType(producerType) +
				": increase MaxFindLocationIters or ProbabDistanceToOwner", true);

		if (pinitialOwner->getmyUsedBank() != nullptr)
			pProducer->pUsedBank() = pinitialOwner->getmyUsedBank(); // a private Bank
		else if (DEPData().InputParameter("MaxNBanks") == 1) // CentralBank only
			pProducer->pUsedBank() = pCentralBank();
		else if (DEPData().InputParameter("MaxNBanks") > 1)
			pProducer->pUsedBank() = pCentralBank()->getRandomBank();
	}

	return pProducer;
};
vector<CProducer*>*& CWorld::pProducers() { return _pProducers; };
vector<CProducer*>& CWorld::Producers() { return *_pProducers; };
const vector<CProducer*>* CWorld::getpProducers() const { return _pProducers; };
long CWorld::getNProducers() const { return (long)getpProducers()->size(); }

void CWorld::defineProductsAndProducersFromSAM() const
{
	// rowQtties are interpreted as qtty units
	// Each product unit is the qtty of product purchased by 1 mu at the start of the simulation
	// Therefore, all initial prices are 1.0
	// A special case is CompEmployees: its unit is 1 worker and its 
	// initial price ("salary") is InitSalary * InitSalaryCalibFactor

	// PRODUCTs DEFINITIONS  ==================================================================

	//M J X needed?  >>>>>>>>>>>> nPXproducerTypes includes groups P, X  <<<<<<<<<<<<<<<<<<

	CGoods::clearGoodsDefinitions();

	CProducer::ProductSpecs().clear();
	auto nPXproducerTypes = getSAM().getnPXproducerTypes();// Types include groups P, X
	auto nPproducerTypes = getSAM().getnPproducerTypes();// Types include groups P, X
	// Define product specs from account data

	// Intermediate comsumption square matrix  ================================================

	CProducer::ProducerTypeOfLabel().clear();
	CProducer::ProducerLabelOfType().resize(nPXproducerTypes);

	string prevAccStr = "";
	bool newGroup = true;
	// colTypes include groups P, X
	for (GoodType colType = 0; colType < nPXproducerTypes; ++colType)
	{
		string gName = getSAM().getAccNameOfN(colType);
		CGoods::defineNewGoodType(gName);

		// Define its Producer type and name
		auto& account = SAM().Account(colType);
		string producerName = account.label(); // only one GoodType per ProducerType for SAMs
		CProducer::ProducerTypeOfLabel()[producerName] = colType;
		CProducer::ProducerLabelOfType()[colType] = producerName;

		CProductSpecs& productSpecs = *new CProductSpecs(colType);
		CProducer::ProductSpecs()[colType] = &productSpecs;

		productSpecs.bIsService() = true;

		// ExtSectors column values are Exports purchased by an ExtSect
		if (Account(colType).label().at(0) == 'X')
		{
			productSpecs.bImported() = true;
			//M J needed?productSpecs.GrossOpSurplus() = 1.0; 
			//M J needed?productSpecs.GrossOutput_mu() = 1;  to avoid div. by 0

			continue;
		}
		else for (int rowType = 0; rowType < nPXproducerTypes; ++rowType) // colTypes P only
		{
			productSpecs.bImported() = false;
			// Intermediate consumptions of colType's for current rowType
			double qtty = (double)Account(rowType).rowQtties()[colType]; // to be normalized below
			productSpecs.Needed_Init(rowType) = qtty; // to be normalized below
			// productSpecs.NeededPerUnit(rowType) will be defined below in GrossOutputAndDepreciationRate
		}
	}

	// PRODUCERs DEFINITIONS  ==================================================================

	CProducer::mProducerSpecs().clear();
	ExtSectors().clear();
	for (int producerType = 0; producerType < nPXproducerTypes; ++producerType)
	{
		GoodType gType = producerType; // one ProducerType (sector) per GoodType
		CProducerSpecs& producerSpecs = *new CProducerSpecs(gType);

		CProducer::mProducerSpecs()[gType] = &producerSpecs;

		producerSpecs.locationType() = getGISMap().getCellNameToType("Natural");

		// Products of each producer: ONLY ONE, productType = producerType

		if (getSAM().IsExtSectType(producerType)) // each ExtSect type is unique Producer
		{
			CExtSect* pExtS = new CExtSect(producerType, producerType);
			addExtSect(producerType, pExtS); // build map: ExtSect(producerType)
		}
	}

	GrossOutputAndDepreciationRate();
};

void CWorld::GrossOutputAndDepreciationRate() const
{
	assert(DEPData().getDepreciationRate().size() == 0); // initialization at currStep = -1

	for (long gType = 0; gType < getSAM().getnPproducerTypes(); ++gType)
	{
		DEPData().DepreciationRate()[gType] = getInputParameter("DepreciationRate");
		DEPData().DepreciationFraction()[gType] = 0.;
		DEPData().prevDepreciationFraction()[gType] = 0.;

		CProductSpecs& productSpecs = *CProducer::ProductSpecs()[gType];
		productSpecs.GrossOutput_mu() = getSAM().getSAMGrossOutput_mu(gType);
	}

	// NORMALIZATION with productSpecs.GrossOutput_mu() from above ================================

	GoodType gType = UndefGoodType;
	for (int colType = 0; colType < getSAM().getnPproducerTypes(); ++colType)
	{
		auto& acc = Account(colType);
		auto& productSpecs = *CProducer::ProductSpecs()[colType];

		for (auto& pair : productSpecs.Needed_Init())
			productSpecs.NeededPerUnit(pair.first) = pair.second / productSpecs.GrossOutput_mu();

		// ============ = Possible (implemented) account rows below the Producers rows  ======================== =

		productSpecs.CompEmployees() =
			Account(getSAM().getAccNofName("CompEmployees")).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		productSpecs.GrossOpSurplus() =
			Account(getSAM().getAccNofName("GrossOpSurplus")).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (productSpecs.GrossOpSurplus() <= 0)
		{
			auto sectorName = getSAM().getAccNameOfN(colType);
			long valM = productSpecs.GrossOpSurplus() * 1.e-6;
			CWorld::ERRORmsg("WARNING: GrossOpSurplus of " + sectorName + " sector = "
				+ to_string(valM) + " millions <= 0. *** Setting it to 0 ***", false);
		}

		if (gType = getSAM().getAccNofName("TaxProduction"), gType != UndefGoodType)
			productSpecs.TaxProduction() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("TaxProducts"), gType != UndefGoodType)
			productSpecs.TaxProducts() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("IRPF"), gType != UndefGoodType)
			productSpecs.IRPF() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("TaxImportCE"), gType != UndefGoodType)
			productSpecs.TaxImportCE() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("TaxImportRW"), gType != UndefGoodType)
			productSpecs.TaxImportRW() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("Households"), gType != UndefGoodType)
			productSpecs.Households() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();

		if (gType = getSAM().getAccNofName("Government"), gType != UndefGoodType)
			productSpecs.Government() = Account(gType).rowQtties().at(colType)
			/ productSpecs.GrossOutput_mu();
	}
};

void CWorld::writeProducers(ofstream& ofstrm) const
{
	ofstrm << "\n" << "Producers " << getpProducers()->size() << " {";
	const CProducer* pProducer = nullptr;
	for (auto pProducer : *getpProducers())
	{
		if (pProducer != nullptr)
		{
			ofstrm << "\n";
			ofstrm << *pProducer << endl;
		}
	}

	ofstrm << " }\n";
};
void CWorld::readProducers(ifstream& ifstrm)
{
	string name, word, bracket;
	long id = -1;

	// set up IDs and pointers to producers and individuals

	long nProducers = 0, nIndiv;

	ifstrm >> name >> nProducers;
	ifstrm >> name >> nIndiv;

	pIndividuals()->clear();
	pProducers()->clear();

	for (long n = 0; n < nIndiv; ++n)
		CIndividual* pIndiv = newIndividual();

	long indivN = 0;
	string producerLabel;
	CIndividual* pIndiv = nullptr;
	ifstrm >> word >> nProducers >> bracket; // "Producers: nProds {"

	pProducers()->resize(nProducers, nullptr);
	while (ifstrm >> producerLabel, producerLabel != "}")
	{
		// "producerLabel { ID id"
		ifstrm >> bracket >> word >> id;

		if (getInputParameter("MaxNBanks") > 0 && getInputParameter("ProducerBanks") == 1)
		{
			if (getSAM().getAccNofLabel(producerLabel) == getCentralBankType())
				(*pProducers())[id] = ((CProducer*)pCentralBank());
			else if (getSAM().getAccNofLabel(producerLabel) == PrivateBankType())
			{
				assert(pCentralBank()->Banks().find(id) == pCentralBank()->Banks().end());
				pCentralBank()->Banks()[id] = new CBank(PrivateBankType(), getpIndividuals()->at(id));
				(*pProducers())[id] = ((CProducer*)pCentralBank()->Banks().at(id));
			}
		}
		else
			(*pProducers())[id] = new CProducer(id, getSAM().getAccNofLabel(producerLabel), nullptr);

		ifstrm >> *(*pProducers())[id];
	}
};

void CWorld::readDEPData(string& filename)
{
	ifstream& ifstrm = *new ifstream();
	ifstrm.open(filename);
	if (ifstrm.fail())
		CWorld::ERRORmsg("Couldn't read " + filename, true);

	DEPData().readPlotsInputParameters(filename);

	DEPData().initializePlotsInputParameters();

	string name;
	while (ifstrm >> name, !ifstrm.eof() && name != "DEPData");

	if (!ifstrm.eof())
	{
		ifstrm >> *pDEPData();
		ifstrm.close();
	}

	return;
}

void CWorld::writeSnapshot(ofstream& ofstrm) const
{
	ofstrm << "\nSNAPSHOT(World_status) {\n";

	ofstrm << "\n currYear " << getDEPData().getCurrYear()
		<< " currStep " << currStep()
		<< " bRunning " << getbRunning()
		<< " bFinished " << getbFinished()
		<< " bPaused 1" << endl;

	ofstrm << "\n CashadjustFactor " << getCashadjustFactor();
	ofstrm << "\n GFCFadjustFactor " << getGFCFadjustFactor();

	ofstrm << "\n InitSalaryCalibFactor " << getInitSalaryCalibFactor();

	ofstrm << "\n ConsumPXFactor ";
	ofstrm << getConsumPXFactor() << endl;
	ofstrm << "\n RefTotalHouseholdPXConsum ";
	ofstrm << getRefTotalHouseholdPXConsum() << endl;

	ofstrm << "\n GovConsumPXFactor ";
	ofstrm << getGovConsumPXFactor() << endl;
	ofstrm << "\n RefGovConsum ";
	ofstrm << getRefGovConsumPX() << endl;

	ofstrm << "\n RefGDP_VA " << getRefGDP_VA() << endl;

	ofstrm << "\n FixCapitalProductivity ";
	ofstrm << getFixCapitalProductivity();

	ofstrm << "\n\n nProducers " << getpProducers()->size();
	ofstrm << "\n nIndividuals " << getpIndividuals()->size() << endl;
	assert(getnIndividuals() == getpIndividuals()->size());

	writeProducers(ofstrm);
	writeIndividuals(ofstrm);

	ofstrm << "\n Government {";
	ofstrm << getGovernment();
	ofstrm << " }" << endl;

	ofstrm << "\n ExtSectors {";
	for (auto& gpair : _pExtSect)
	{
		ofstrm << "\n " << getSAM().getAccNameOfN(gpair.first) << " {" << endl;
		ofstrm << *gpair.second;
		ofstrm << " }" << endl;
	}
	ofstrm << " }" << endl;

	if (DEPData().InputParameter("MaxNBanks") > 0)
	{
		ofstrm << "\n CentralBank {";
		ofstrm << getCentralBank();
		ofstrm << " }" << endl;

		ofstrm << "\n Banks {";
		for (const auto& bpair : getCentralBank().getBanks())
		{
			ofstrm << "\n PrivateBank {";
			ofstrm << *bpair.second;
			ofstrm << " }" << endl;
		}
		ofstrm << " }" << endl;
	}

	ofstrm << "\n FinancialMarket {";
	ofstrm << getFinancialMarket();
	ofstrm << " }" << endl;

	ofstrm << "}\n";
};
void CWorld::readSnapshot(string fileNameExt)
{
	string word, word1, word2;
	long ID;
	long currentYear = 0, currentStep = 0;

	ifstream ifstrm(fileNameExt);
	if (ifstrm.fail())
		ERRORmsg("Failed to open " + fileNameExt, true);

	while (ifstrm >> word, word != "SNAPSHOT(World_status)")
		if (ifstrm.eof())
			return;

	ifstrm >> word; // "{"
	ifstrm
		>> word >> currentYear // getCurrYear() is calculated from currStep value
		>> word >> currentStep
		>> word >> bRunning()
		>> word >> bFinished()
		>> word >> bPaused();

	InitStep() = currentStep;
	bRunning() = true;
	bFinished() = false;
	bPaused() = true;

	_currStep = currentStep;

	ifstrm >> word >> CashadjustFactor();
	ifstrm >> word >> GFCFadjustFactor();

	ifstrm >> word >> InitSalaryCalibFactor();

	ifstrm >> word;
	ifstrm >> ConsumPXFactor();
	ifstrm >> word;
	ifstrm >> RefTotalHouseholdPXConsum();

	ifstrm >> word;
	ifstrm >> GovConsumPXFactor();
	ifstrm >> word;
	ifstrm >> RefGovConsumPX();

	ifstrm >> word >> RefGDP_VA();

	ifstrm >> word;
	ifstrm >> FixCapitalProductivity();

	readProducers(ifstrm);
	readIndividuals(ifstrm);
	nIndividuals() = (long)getpIndividuals()->size();

	ifstrm >> word >> word1; // "Government {"
	ifstrm >> *pGovernment();
	ifstrm >> word; // "}"

	ifstrm >> word >> word1; // "ExtSectors {"
	while (ifstrm >> word, word != "}")
	{
		auto aType = getSAM().getAccNofName(word);
		while (ifstrm >> word, word != "}")
		{
			ifstrm >> ExtSect(aType);
		}
	}

	if (DEPData().InputParameter("MaxNBanks") > 0)
	{
		ifstrm >> word >> word1; // "CentralBank {"
		ifstrm >> word >> word1 >> word2 >> ID; // "CentralBank { ID id"
		assert(pCentralBank()->getID() == ID);
		ifstrm >> *pCentralBank();

		ifstrm >> word >> word1; // "Banks {"
		ifstrm >> word >> word1; // "PrivateBank {"
		while (ifstrm >> word, word != "}") // PrivateBank
		{
			ifstrm >> word >> word1 >> ID; // "{ ID bankID"
			auto result = getCentralBank().getBanks().find(ID);
			assert(result != getCentralBank().getBanks().end());

			ifstrm >> *pCentralBank()->Banks()[ID];
		}
	}

	ifstrm >> word >> word1; // "FinancialMarket {"
	ifstrm >> *pFinancialMarket();
	ifstrm >> word; // "}"

	ifstrm >> word; // "}"
};

void CWorld::writeMap(ofstream& ofstrm) const
{
	ofstrm << "\nMAP {\n";

	ofstrm << getGISMap();

	ofstrm << "\n}\n";
};
bool CWorld::readMap(string fileNameExt)
{
	string word;
	ifstream ifstrm(fileNameExt);
	if (ifstrm.fail())
		return false;

	while (ifstrm >> word, word != "MAP");

	_pGISMap = new CGISMap();

	ifstrm >> GISMap();

	ifstrm.close();
	return true;
}

void CWorld::writerndstatus(ofstream& ofstrm) const
{
	ofstrm << "\nrndstatus {\n" << myRandomEngine() << "\n}" << endl;
};
bool CWorld::readrndstatus(string fileNameExt)
{
	string word;

	ifstream& ifstrm = *new ifstream(fileNameExt);
	while (ifstrm >> word, word != "rndstatus" && !ifstrm.eof());
	delete pmyRandomEngine();
	pmyRandomEngine() = new mt19937();
	if (word == "rndstatus")
	{
		ifstrm >> word; // "{"
		ifstrm >> myRandomEngine();
		ifstrm >> word; // "}"
	}
	else
		myRandomEngine().seed(5489);

	ifstrm.close();
	return true;
}
void CWorld::assignPointersToBanks(string fileNameExt)
{
	string word;
	AgentID agentID, bankID;
	AgentType bankType;
	long nProducers = 0, nIndividuals = 0, nOpen = 0;;
	ifstream ifstrm(fileNameExt);

	while (ifstrm >> word, !ifstrm.eof())
		if (word == "Producers")
			break;

	if (!ifstrm.eof())
	{
		ifstrm >> nProducers >> word; // "62 {"
		nOpen = 1;

		while (ifstrm >> word, word != "}") // " >> P_producername
		{
			ifstrm >> word >> word >> agentID; // "{ ID id"
			nOpen = 2;
			while (ifstrm >> word, word != "UsedBank")
			{
				if (word == "}")
					--nOpen;
				else if (word == "{")
					++nOpen;
			};

			ifstrm >> word >> bankType >> word >> bankID;
			if (bankType == getCentralBankType())
				pWorld()->pProducers()->at(agentID)->pUsedBank() = pCentralBank();
			else if (bankType == PrivateBankType())
			{
				pWorld()->pProducers()->at(agentID)->pUsedBank()
					= pCentralBank()->Banks().at(bankID);
			}

			while (nOpen > 1) // "}" of P_producername {
			{
				ifstrm >> word;
				if (word == "}")
					--nOpen;
				else if (word == "{")
					++nOpen;
			};
		}
	}

	while (ifstrm >> word, !ifstrm.eof() && word != "Individuals");
	if (!ifstrm.eof())
	{
		ifstrm >> nIndividuals >> word; // "200 {"
		nOpen = 1;

		while (ifstrm >> word, word != "}") // " >> Indiv
		{
			ifstrm >> word >> word >> agentID; // "{ ID id"

			// UsedBank
			nOpen = 2;
			while (ifstrm >> word, !ifstrm.eof() && word != "UsedBank")
			{
				if (word == "}")
					--nOpen;
				else if (word == "{")
					++nOpen;
			};

			assert(!ifstrm.eof());

			ifstrm >> word >> bankType >> word >> bankID;
			if (bankType == getCentralBankType())
				pWorld()->pIndividuals()->at(agentID)->pUsedBank() = pCentralBank();
			else if (bankType == PrivateBankType())
			{
				pWorld()->pIndividuals()->at(agentID)->pUsedBank()
					= pCentralBank()->Banks().at(bankID);
			}

			// OwnedBank

			while (ifstrm >> word, !ifstrm.eof() && word != "OwnedBank")
			{
				if (word == "}")
					--nOpen;
				else if (word == "{")
					++nOpen;
			};

			assert(!ifstrm.eof());

			ifstrm >> bankID; // should be a PrivateBank type
			if (bankID >= 0)
				pIndividuals()->at(agentID)->pOwnedBank() = pCentralBank()->Banks().at(bankID);

			while (nOpen > 1) // "}" of Indiv {
			{
				ifstrm >> word;
				if (word == "}")
					--nOpen;
				else if (word == "{")
					++nOpen;
			};
		}
	}
	ifstrm.close();
}

void CWorld::SetPricesToOne()
{
	for (auto& pIndiv : Individuals())
		for (auto& pair : pIndiv->myPrice())
		{
			if (pair.first == getSAM().GFCFtype()) // this is 1 + loan interestRate
				continue;

			pIndiv->setmyPriceOf(pair.first, 1.0);
		}

	for (auto& pProducer : Producers())
		if (pProducer != nullptr)
		{
			pProducer->productionPrice() = 1.0;

			for (auto& pair : pProducer->myPrice())
			{
				if (pair.first == getSAM().GFCFtype()) // this is 1 + loan interestRate
					continue;

				pProducer->setmyPriceOf(pair.first, 1.0);
			}
		}
}

void CWorld::AdjustSAMParameters()
{
	if (currStep() >= getInputParameter("AdjustIndividualsWealthFrom")
		&& currStep() <= getInputParameter("AdjustIndividualsWealthUpto"))
	{
		double IndivsWealthTarget = getInputParameter("IndivsWealthTarget");
		double currIndivsWealth = getDEPData().getIndivsWealth();
		double WtargetToCurr = IndivsWealthTarget / currIndivsWealth;

		double toTotalPopulationYear = getDEPData().getUpscaleSimulationFactor();

		double currIndivsGFCF = 0;
		for (auto& pair : this->Individuals())
		{
			auto& indiv = *pair;
			currIndivsGFCF += indiv.getmyGFCF();
		}
		currIndivsGFCF *= toTotalPopulationYear;

		double currIndivsCash = getDEPData().getTotalIndivsCash() * toTotalPopulationYear;
		double currIndivsShares = max(0, currIndivsWealth - currIndivsGFCF - currIndivsCash);

		// we want to keep currIndivsSharesand adjust Cash and GFCF to reach IndivsWealthTarget,
		// with the constraint that Cash = GFCF * IndivsCashToGFCFratio

		double desiredIndivsCashToGFCFratio = getInputParameter("IndivsCashToGFCFratio"); // 0.30
		double desiredIndivsGFCF = (IndivsWealthTarget - currIndivsShares) / (1. + getInputParameter("IndivsCashToGFCFratio"));
		double desiredIndivsCash = desiredIndivsGFCF * desiredIndivsCashToGFCFratio;

		double GFCFfactor = 1.;
		if (currIndivsGFCF > 0)
			GFCFfactor = desiredIndivsGFCF / currIndivsGFCF;
		double Cashfactor = 1.;
		if (currIndivsCash > 0)
			Cashfactor = desiredIndivsCash / currIndivsCash;

		if (WtargetToCurr > 1.) // IndivsWealth < IndivsWealthTarget
		{
			for (auto& pair : this->Individuals())
			{
				auto& indiv = *pair;

				double addMoney = indiv.getCash() * (Cashfactor - 1.);
				indiv.Cash() += addMoney;
				indiv.InitCash() += addMoney;
				DEPData().TotalInitialCash() += addMoney;

				indiv.myGFCF() = indiv.getmyGFCF() * GFCFfactor;
			}
		}
		else if (WtargetToCurr < 1.)
		{
			for (auto& pair : this->Individuals())
			{
				auto& indiv = *pair;

				double delMyMoney = indiv.getCash() * (1. - Cashfactor);
				bool done = indiv.DeleteMyMoney(delMyMoney);
				if (!done)
					ERRORmsg("DeleteMyMoney failed in AdjustSAMParameters", true);

				indiv.myGFCF() = indiv.getmyGFCF() * GFCFfactor;
			}
		}
	}

	/*
	if (currStep() >= getInputParameter("AdjustIndividualsWealthFrom")
		&& currStep() <= getInputParameter("AdjustIndividualsWealthUpto"))
	{
		double IndivsWealthTarget = getInputParameter("IndivsWealthTarget");
		double currIndivsWealth = getDEPData().getIndivsWealth();
		double WcurrToTarget = currIndivsWealth / IndivsWealthTarget;

		double toTotalPopulationYear = getDEPData().getUpscaleSimulationFactor();

		double currIndivsGFCF = 0;
		for (auto& pair : this->Individuals())
		{
			auto& indiv = *pair;
			currIndivsGFCF += indiv.getmyGFCF();
		}
		currIndivsGFCF *= toTotalPopulationYear;

		double currIndivsCash = getDEPData().getTotalIndivsCash() * toTotalPopulationYear;
		double currIndivsShares = max(0, currIndivsWealth - currIndivsGFCF - currIndivsCash);

		// we want to keep currIndivsShares and adjust Cash and GFCF to reach IndivsWealthTarget,
		// with the constraint that Cash = (GFCF+Shares) * IndivsCashToGFCFratio

		double IndivsCashToGFCFShares = getInputParameter("IndivsCashToGFCFShares"); // 0.30 for Austria 2010

		double desiredIndivsGFCF = IndivsWealthTarget / (1. + IndivsCashToGFCFShares) - currIndivsShares;
		double desiredIndivsCash = desiredIndivsGFCF * IndivsCashToGFCFShares;

		double adjustFraction = getInputParameter("ProductionChangeFraction");

		double newGFCFadjustFactor = 1.;
		if (currIndivsGFCF > 0)
			newGFCFadjustFactor = desiredIndivsGFCF / currIndivsGFCF;

		GFCFadjustFactor() = getGFCFadjustFactor() * (1.0 - adjustFraction) + newGFCFadjustFactor * adjustFraction;

		double newCashadjustFactor = 1.;
		if (currIndivsCash > 0)
			newCashadjustFactor = desiredIndivsCash / currIndivsCash;

		CashadjustFactor() = getCashadjustFactor() * (1.0 - adjustFraction) + newCashadjustFactor * adjustFraction;

		if (WcurrToTarget < 1.) // IndivsWealth < IndivsWealthTarget -> adjustFactors > 1.
		{
			for (auto& pair : this->Individuals())
			{
				auto& indiv = *pair;

				double addMoney = indiv.getCash() * (CashadjustFactor() - 1.);
				indiv.Cash() += addMoney;
				indiv.InitCash() += addMoney;
				DEPData().TotalInitialCash() += addMoney;

				indiv.myGFCF() += indiv.getmyGFCF() * (CashadjustFactor() - 1.);
			}
		}
		else
			if (WcurrToTarget > 1.) // IndivsWealth > IndivsWealthTarget -> adjustFactors < 1.
		{
			for (auto& pair : this->Individuals())
			{
				auto& indiv = *pair;

				double delMyMoney = indiv.getCash() * CashadjustFactor();
				bool done = indiv.DeleteMyMoney(delMyMoney);
				if (!done)
					ERRORmsg("DeleteMyMoney failed in AdjustSAMParameters", true);

				indiv.myGFCF() = indiv.getmyGFCF() * GFCFadjustFactor();
			}
		}
	}
	*/

	if (getInputParameter("IndivConsum") > 0
		&& currStep() <= getInputParameter("AdjustConsumUpto"))
	{
		double adjustFraction = getInputParameter("ConsumAdjustFraction");//MJ ProductionChangeFraction?
		double MaxConsumPXFactor = getInputParameter("MaxConsumPXFactor");

		for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
		{
			// Adjust ConsumPXFactor of Indivs as a function of their totWorth

			if (getRefTotalHouseholdPXConsum().at(gType) == 0)
				continue;

			double IndivsErr =
				((double)getDEPData().getTotalHouseholdPXConsum().at(gType) - getRefTotalHouseholdPXConsum().at(gType))
				/ getRefTotalHouseholdPXConsum().at(gType);

			double newVal = min(MaxConsumPXFactor, max(0., getConsumPXFactor().at(gType) * (1. - IndivsErr))); // range [0, MaxConsumPXFactor]
			if (newVal > 0.95 * MaxConsumPXFactor)
				ERRORmsg("ConsumPXFactor too high, > MaxConsumPXFactor = " + to_string(MaxConsumPXFactor), true);

			ConsumPXFactor()[gType] = getConsumPXFactor().at(gType) * (1.0 - adjustFraction) + newVal * adjustFraction;
		}
	}

	if (getInputParameter("GovConsum") > 0
		&& currStep() <= getInputParameter("AdjustConsumUpto"))
	{
		double adjustFraction = getInputParameter("ConsumAdjustFraction");
		double upscaleFactor = getDEPData().getUpscaleSimulationFactor();
		double toTotalPopulationYear = 12. * upscaleFactor;

		for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
		{
			// Adjust GovConsumPXFactor of Gov

			if (RefGovConsumPX().at(gType) == 0)
				continue;

			double GovConsum_gType = getGovernment().getGoodsIhave(gType) * toTotalPopulationYear;

			double GovErr = (GovConsum_gType - RefGovConsumPX().at(gType))
				/ RefGovConsumPX().at(gType);

			double newVal = min(1.0, max(0., GovConsumPXFactor().at(gType) * (1. - GovErr))); // range [0, 1]

			GovConsumPXFactor()[gType] = GovConsumPXFactor().at(gType) * (1.0 - adjustFraction) + newVal * adjustFraction;
		}
	}

	if (currStep() >= getInputParameter("AdjustUnemploymentFrom")
		&& currStep() <= getInputParameter("AdjustUnemploymentUpto"))
	{
		// Adust InitSalaryCalibFactor() to get the SAM's InitUnemploymentPercent ================

		double adjustFraction = getInputParameter("UnemploymentAdjustFraction");

		double Unemploymentpercent = getSAM().getInitUnemploymentPercent();
		double prevUnemp = getDEPData().getprevUnemployment();
		double currUnemp = DEPData().getUnemployment();

		double minUnemploymentpercent = 1.0;
		double smoothUnemploypercent = max(minUnemploymentpercent, 100. * currUnemp);

		if (abs(Unemploymentpercent - smoothUnemploypercent) > 1.0)
		{
			// proportional filter
			double prevval = getInitSalaryCalibFactor();
			double newval =
				getInitSalaryCalibFactor() * (Unemploymentpercent / smoothUnemploypercent);
			InitSalaryCalibFactor() = prevval + adjustFraction * (newval - prevval);
			double maxFactor = 10.0;
			InitSalaryCalibFactor() = min(maxFactor, max(1 / maxFactor, InitSalaryCalibFactor()));
		}
	}

	if (currStep() == getInputParameter("ResetPricesToOneAt"))
	{
		for (auto& pair : CAgent::PriceCalibFactor())
		{
			if (pair.first == getSAM().GFCFtype()) // this is 1 + loan interestRate
				continue;

			CAgent::PriceCalibFactor()[pair.first] = DEPData().MarketPrices()[pair.first];
			DEPData().MarketPrices()[pair.first] = 1.0;
		}

		SetPricesToOne();
	}
};

/////////////////////  running the World  ////////////////////////////

void CWorld::DismantleAndStartNewProducers()
{
	// Dismantle Unused Producers

	DEPData().FirmBirths()[currStep() % 12] = 0;
	DEPData().FirmDeaths()[currStep() % 12] = 0;

	for (auto& pProducer : *pProducers())
	{
		if (pProducer == nullptr)
			continue;

		auto prodType = pProducer->getAgentType();

		if (prodType < 0
			|| pProducer->IsExtSect()
			|| pProducer->IsCentralBank() || pProducer->IsPrivateBank())
			continue;
		else if (getDEPData().getnCurrentPProducers().at(prodType)
			<= getInputParameter("MinCurrentPProducers"))
			continue;
		else if (pProducer->getWealthPrevMonth() < 0)
		{
			DismantleProducer(pProducer);
			delete pProducer;
		}
	}

	// Start new Producers

	for (auto pIndividual : *pIndividuals())
		pIndividual->TryToStartupNewProducer();
};
long CWorld::DismantleProducer(CProducer*& pProducer)
{
	assert(getDEPData().getnCurrentPProducers().at(pProducer->getAgentType()) > 0);
	--DEPData().nCurrentPProducers().at(pProducer->getAgentType());

	DEPData().Dismantled() += pProducer->getGoodsIhave();
	DEPData().Dismantled() += pProducer->getToSell();

	while (pProducer->getEmployees().size() > 0)
		pProducer->ReleaseLastEmployee();

	CLocation location = pProducer->getLocation();
	pMap()->RemoveFromCell(pProducer);

	if ((bool)getInputParameter("WriteTransactions"))
	{
		LogFile()
			<< "\n step " << currStep()
			<< " " << pProducer->getAgentName()
			<< " dismantled. ";
		LogFile().flush();
	}

	if (getInputParameter("MaxNBanks") > 0
		|| pProducer->pUsedBank() != nullptr)
	{
		auto cash = pProducer->getCash();
		CBankEntry currStat =
			pProducer->pUsedBank()->ClientCashOperation(pProducer, cash);

		// close pProducer's account: transfer balance to Bank, not to owner

		pProducer->myBankAccountStatus() =
			pProducer->getmyUsedBank()->ClientDefaultClose(pProducer);
	}
	else if (pProducer->pOwners()->size() > 0)
	{
		auto cash = pProducer->getCash();
		pProducer->pOwners()->begin()->second->Cash() += cash;
		pProducer->Cash() -= cash;
	}

	map<CAgentFID, CAgent*> Owners = *pProducer->pOwners();
	for (auto& pair : Owners)
	{
		auto& owner = *pair.second;
		pProducer->myShares(pProducer->getFID()) += owner.myShares(pProducer->getFID());// -Prod +Owner
		owner.myShares().erase(pProducer->getFID());
		owner.pmyProducers()->erase(pProducer->getFID());
		pProducer->pOwners()->erase(owner.getFID());
	}
	pProducer->myShares().erase(pProducer->getFID());

	auto id = pProducer->getID();
	pProducers()->at(id) = nullptr;

	++DEPData().FirmDeaths()[currStep() % 12];

	return 1; // nDismantled
};

bool CWorld::LoadWorld()
{
	initializeWorld();

	string fileNameExt = getDEPData().getInputFileName() + ".dep";
	LogFile() << "\n // Input file name " << fileNameExt << ",  ";
	writeTimeAndDate(LogFile());

	// 1. Copy input file to output log file

	ifstream ifstrm(fileNameExt);
	string line;
	while (getline(ifstrm, line))
		LogFile() << line << endl;
	LogFile().flush();
	ifstrm.close();

	// -------------------------------------

	readSnapshot(fileNameExt);

	assignPointersToBanks(fileNameExt);

	GISMap().initializeLinksFrom(fileNameExt);

	setupExtSectorsToLaborArray();

	readDEPData(fileNameExt);

	// -------------------------------------------------------------------

	LogFile() << "\n Log begins:\n\n";
	LogFile().flush();

	return true;
};
bool CWorld::SaveWorld() const
{
	if (currStep() < 0)
		return true;

	// save step (config) file  ------------------------------------------------------

	string SimulationName = CWorld::getSimulationName();
	string outputFileName = SimulationName;
	if (getpFigaro() != nullptr)
		outputFileName += "_" + getSAM().CountryCode();
	outputFileName += "_" + to_string(currStep());

	ofstream outfile(outputFileName + ".dep");
	if (outfile.fail())
		return false;

	outfile.precision(17); // double: up to all 17 significant digits
	outfile << "\n // " << outputFileName << "  ";
	writeTimeAndDate(outfile);

	DEPData().writeInputParameters(outfile);

	getDEPData().writePlotsInputParameters(outfile);

	writeMap(outfile);

	getSAM().writeSAMTXT(outfile);

	outfile.flush();

	writeSnapshot(outfile);

	// Write DEPData

	outfile << getDEPData();

	writerndstatus(outfile);

	outfile.close();

	// save Log file  ------------------------------------------------------

	string LogFileName = SimulationName;
	if (getpFigaro() != nullptr)
		LogFileName += "_" + getSAM().CountryCode();
	LogFileName += "_Log.dep";

	string newLogFileName = outputFileName + "_Log.dep"; // includes step n

	string comand =
#ifdef _WINDOWS
		" copy "
#else
		" cp "
#endif // _WINDOWS
		+ LogFileName + " " + newLogFileName;
	int a = system(comand.c_str());

	return true;
};

void CWorld::initializeWorld()
{
	std::remove("_ERROR-message.dep");

	DEPData().initialize_constructor(); // again

	delete _pSimulatedCountries;
	_pSimulatedCountries = new map<string, CSimulatedCountry*>;
	_pSAM = nullptr;
	_pFIGARO = nullptr;

	_pGovernment = nullptr;

	pCentralBank() = nullptr;

	delete _pRandomListOfIndivIDs;
	_pRandomListOfIndivIDs = new vector<AgentID>;
	delete _pInteractingAgents;
	_pInteractingAgents = new vector<CAgent*>;

	delete _pIndividuals;
	_pIndividuals = new vector<CIndividual*>;
	delete _pProducers;
	_pProducers = new vector<CProducer*>;

	_pFinancialMarket = nullptr;

	_pGISMap = nullptr;

#ifdef DEPLOYERS_GRAPHICS
	if (ColorDefinitions().size() == 0)
		defineColors();
#endif

	pExtSectorsToLabor() = nullptr;

	// -----------------------------------------------------------

	pLogFile() = new ofstream();
	_pExternalSectorsIO = new ofstream();

	_currStep = -1;
	InitStep() = 0;

	string InputFileExt = getDEPData().getInputFileName() + ".dep";

	readrndstatus(InputFileExt);

	CashadjustFactor() = 1.0;
	GFCFadjustFactor() = 1.0;

	InitSalaryCalibFactor() = 1.0;

	delete _pSAM;
	_pSAM = new CSAM();
	_pFIGARO = nullptr;

	bRunning() = false;
	bFinished() = false;
	bPaused() = false;

	SAM().readSAMTXT(InputFileExt); // keep a text copy of SAM, to write it to output file

	SAM().readSAM(InputFileExt); // builds the GoodType and ProducerType lists

	World().nIndividuals() = (long)getInputParameter("nIndivsPerMillionActiveAndSector")
		* getSAM().getActive() * 1.e-6 * getSAM().getnPproducerTypes();

	readMap(InputFileExt);

	defineProductsAndProducersFromSAM(); // defines each ProductSpecs and ProducerSpecs

	DEPData().NYears() = (long)DEPData().InputParameter("NYears");
	DebugLevel() = (long)DEPData().InputParameter("DebugLevel");

	//  -------  CentralBank  -----------------

	delete pCentralBank();
	pCentralBank() = nullptr;
	if (DEPData().InputParameter("MaxNBanks") > 0)
		pCentralBank() = new CCentralBank(getCentralBankType(), nullptr);

	//  -------  Individuals  -----------------

	for (auto pIndiv : *pIndividuals())
		delete (CIndividual*)pIndiv;

	pIndividuals()->clear();

	// Add new nIndividuals only to initial, empty snapshot:
	long nIndiv = getWorld().getnIndividuals();
	for (long n = 0; n < nIndiv; ++n)
		CIndividual* pIndiv = newIndividual();

	//  -------  Producers  -----------------

	for (auto pProd : *pProducers())
		if (pProd != nullptr && !pProd->IsCentralBank() && !pProd->IsPrivateBank())
			delete (CProducer*)pProd;

	pProducers()->clear();

	//  -------  Government  -----------------
	delete pGovernment();
	pGovernment() = new CGovernment(GovernmentType, GovernmentType);

	// --------------------------------------------------------------

	delete pFinancialMarket();
	pFinancialMarket() = new CFinancialMarket();

	DEPData().initialize();
	// start Log output file

	string logFname = CWorld::getSimulationName();
	if (getpFigaro() != nullptr)
		logFname += "_" + getFigaro()._thisCountryCode;
	logFname += "_Log.dep";
	LogFile().open(logFname);
}

void CWorld::InitializeSimulation()
{
	if (pGovernment() != nullptr)
		pGovernment()->initialize();

	for (auto& gpair : getExtSectors())
		gpair.second->initialize();

	initializeIndividuals();

	CAgent::PriceAdaptFactor() = getInputParameter("PriceAdaptFactor");// 1.01;

	FixCapitalProductivity().clear();

	DEPData().initialize();

	writeExternalSectorsIO();
	_currStep = 0;

	//  ----------------------------------------------------------------------

	for (auto& pAgent : *pProducers())
		if (pAgent != nullptr)
			pAgent->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	if (pGovernment() != nullptr)
		pGovernment()->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	if (pCentralBank() != nullptr)
		pCentralBank()->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	if (pCentralBank() != nullptr && pCentralBank()->Banks().size() > 0)
		for (auto& pair : pCentralBank()->Banks())
			pair.second->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	for (auto& pair : ExtSectors())
		pair.second->MonthlyActivityStep() = (long)(getRandom01() * getInputParameter("WorkDaysPerMonth"));

	RefTotalHouseholdPXConsum().clear();
	for (const auto pAcc : getAccountGroups().at("H"))
	{
		auto Hcol = pAcc->accN();

		for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
			RefTotalHouseholdPXConsum()[gType] += (double)getSAM().getRowCol(gType, Hcol);

		RefTotalHouseholdPXConsum()[getSAM().GFCFtype()] += (double)getSAM().getRowCol("GFCF", Hcol);
	}

	RefGovConsumPX().clear();
	for (long gType = 0; gType < getSAM().getnPXproducerTypes(); ++gType)
		RefGovConsumPX()[gType] += (double)getSAM().getRowCol(gType, "Government");

	RefGovConsumPX()[getSAM().GFCFtype()] += (double)getSAM().getRowCol("GFCF", "Government");

	RefGDP_VA() = 0;
	if (getpFigaro() != nullptr)
	{
		for (long gType = 0; gType < getSAM().getnPproducerTypes(); ++gType)
		{
			RefGDP_VA() += (double)getSAM().getRowCol("CompEmployees", gType)
				+ (double)getSAM().getRowCol("GrossOpSurplus", gType)
				+ (double)getSAM().getRowCol("TaxProduction", gType)
				+ (double)getSAM().getRowCol("TaxProducts", gType);
		}
	}
	else
		ERRORmsg("No Figaro object in InitializeSimulation", true);
};
bool CWorld::RunSimulation()
{
	// return false if not finished

	if (getDEPData().getNYears() <= 0
		|| (currStep() >= getInitStep() + getDEPData().getNSteps()))// - 1))
	{
		bFinished() = true;
		bRunning() = false;
		bPaused() = false;
		bPlotBorders() = true;

		return true;
	}

	_currStep++;
	runOneMonth(); // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

	if (currStep() + 1 < getInitStep() + getDEPData().getStepsPerYear()
		* getDEPData().getNYears())
	{
		for (const auto& stepN : getDEPData().getSaveSteps())
		{
			if (stepN == currStep())
			{
				SaveWorld();
				break;
			}
		}

		return false; // not finished NYears
	}
	else
	{
		bRunning() = false;
		bFinished() = true;

		SaveWorld();

		return true; // finished NYears
	}
};
void CWorld::initializeMonth()
{
	// Remove unused Producers, startup new ones

	DismantleAndStartNewProducers();

	GISMap().clearlastGoodsDemanded();
	GISMap().updateAllNeighbors(); // with the new Producers sites

	// -----------------------------------------------------------------------------------

	AdjustSAMParameters();

	// -----------------------------------------------------------------------------------

	DEPData().TotSupply().clear();
	DEPData().TotProduced().clear();
	DEPData().TotalProduction() = 0;
	DEPData().TotDemand().clear();

	// make a randomized copy of Individuals and Producers

	static vector<CAgent*> rndAgents;
	rndAgents.clear();
	rndAgents.insert(rndAgents.end(), pIndividuals()->begin(), pIndividuals()->end());
	rndAgents.insert(rndAgents.end(), pProducers()->begin(), pProducers()->end());
	std::shuffle(rndAgents.begin(), rndAgents.end(), myRandomEngine());

	for (auto& pAgent : rndAgents) // Individuals & Producers
		if (pAgent != nullptr)
			pAgent->stepInitialize();

	pGovernment()->stepInitialize();

	for (auto& gPair : ExtSectors())
		pExtSect(gPair.first)->stepInitialize();

	DEPData().stepInitialize();
}

void CWorld::writeExternalSectorsIO()
{
	if (_pFIGARO == nullptr || getExtSectors().size() <= 1 // size==1: RW
		|| currStep() < getInputParameter("ReadIOfilesFromStep"))
		return;

	if (currStep() > 1 + getInputParameter("ReadIOfilesFromStep")
		&& currStep() > InitStep() + 1) // InitStep>0: LoadStep
		waitMyExternalCountriesToReadMyIO();

	string thisCountryCode = Figaro()._thisCountryCode;
	string filename = CWorld::getSimulationName() + "_" + thisCountryCode + "_IO.dep";
	ofstream outf(filename);

#ifdef LINUX_VERSION
	cout << "thisCountryCode " << Figaro()._thisCountryCode << " , currStep " << currStep() << endl;
#endif

	outf.precision(17); // double: up to all 17 significant digits
#ifdef WINDOWS_VERSION
	writeTimeAndDate(outf);
#endif
	string SAMname = SAM()._SAMname;
	outf << "thisCountryCode " << Figaro()._thisCountryCode << " currStep " << currStep() << endl;

	//  --------------------------  IMPORTS  --------------

	outf << "IMPORTS {" << endl;

	// -------  Disaggregated extCountries  -------------------------------

	for (const auto& extCountryCode : *Figaro()._pDisaggExtSectCountries)
	{
		outf << "PurchasedFromCountryCode " << extCountryCode << " { " << endl;
		for (const auto& eS : ExtSectors())
		{
			auto& eSectRowN = eS.first;
			auto& eSect = *eS.second;
			string eSectCountryCode = eSect.name().substr(0, 2);
			if (eSectCountryCode != extCountryCode)
				continue;

			outf << " from_ExtSector " << eSectRowN << " " << eSect.name()
				<< " SectorCode " << eSect.name().substr(3) << " { to_mySectors ";
			for (long producerCol = 0; producerCol < getSAM().getnPproducerTypes(); ++producerCol)
				outf << " " << producerCol << " " << eSect.getImports()[producerCol];

			outf << " }" << endl; // SectorCode
		}

		outf << " }" << endl; // FromDisaggCountryCode
	}

	outf << endl;

	// -------  Aggregated extCountries  -------------------------------

	for (const auto& extCountryCode : *Figaro()._pAggExtSectCountries)
	{
		outf << "PurchasedFromCountryCode " << extCountryCode << " { " << endl;
		string eSectName = "X" + extCountryCode;
		GoodType eSectRowN = getSAM().getAccNofName(eSectName);

		outf << " from_aggExtSector " << eSectRowN << " " << eSectName << " { to_mySectors ";
		for (long producerCol = 0; producerCol < getSAM().getnPproducerTypes(); ++producerCol)
			outf << " " << producerCol << " " << getExtSect(eSectRowN).getImports()[producerCol];

		outf << " }" << endl;

		outf << " }" << endl;
	}

	outf << "}" << endl;

	//  --------------------------  EXPORTS  --------------

	outf << "EXPORTS {" << endl;

	// -------  Disaggregated to my DisaggExtSectCountries  -------------------------------

	for (const auto& extCountryCode : *Figaro()._pDisaggExtSectCountries)
	{
		GoodType baseExtSectN = getSAM().getAccNofName("X" + extCountryCode) - 1;
		outf << "SoldToCountryCode " << extCountryCode << " { " << endl;
		for (long producerRowN = 0; producerRowN < getSAM().getnPproducerTypes(); ++producerRowN)
		{
			outf << " to_extSector " << producerRowN << " " << getSAM().getAccNameOfN(producerRowN)
				<< " { from_mySectors ";

			for (const auto& es : ExtSectors())
			{
				auto& eSectColN = es.first;
				auto& eSect = *es.second;
				if (eSect.name().substr(0, 2) != extCountryCode)
					continue;

				outf << " " << eSectColN << " " << eSect.getExports().at(producerRowN);
			}

			outf << " }" << endl;
		}
		outf << " }" << endl;
	}

	outf << endl;

	// -------  Aggregated to my AggExtSectCountries  -------------------------------

	for (const auto& extCountryCode : *Figaro()._pAggExtSectCountries)
	{
		GoodType ExtSectorN = getSAM().getAccNofName("X" + extCountryCode);

		outf << "SoldToCountryCode " << extCountryCode << " { " << endl;
		outf << " to_aggExtSector " << ExtSectorN << " " << "X" + extCountryCode << " { from_mySectors ";

		for (long productRow = 0; productRow < getSAM().getnPproducerTypes(); ++productRow)
			outf << " " << productRow << " " << getExtSectors().at(ExtSectorN)->getExports().at(productRow);

		outf << " }" << endl;

		outf << " }" << endl;
	}

	outf << "}" << endl;

	outf.flush();
	outf.close();

	// update written step flag file

	filename = CWorld::getSimulationName() + "_" + thisCountryCode
		+ "_IO_written.dep";
	ofstream flagFile(filename);
	flagFile << " " << currStep();
	flagFile.flush();
	flagFile.close();
}
void CWorld::waitMyExternalCountriesToReadMyIO() const
{
	string thisCountryCode = getFigaro()._thisCountryCode;

	set<string> MyExtCountriesCodes;
	for (const auto& code : *getFigaro()._pDisaggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	for (const auto& code : *getFigaro()._pAggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	MyExtCountriesCodes.erase("RW");
	set<string> extCountriesCodes = MyExtCountriesCodes;

	ifstream flagFile;
	string filename;
	long filestep = 0;
	for (const auto& extCountryCode : MyExtCountriesCodes)
	{
		if (extCountryCode == "RW")
			ERRORmsg("extCountryCode == RW", true);

		filename = CWorld::getSimulationName() + "_" + thisCountryCode + "_IO_readBy_" + extCountryCode + ".dep";

		long m = 0;
		bool fileRead = false;
		while (!fileRead) {
			flagFile.open(filename);
			if (flagFile.fail()) {
				flagFile.close();
				crossPlatformSleep(getInputParameter("sleep_seconds"));
				if (m++ > getInputParameter("NsleepForIO"))
				{
					ERRORmsg(thisCountryCode + ": couldn't open flagFile " + filename, true);
					exit(-3); // give up
				}
				continue; // keep waiting
			}
			else // flagFile open, read it
			{
				flagFile >> filestep;
				flagFile.close();
				if (filestep < currStep() - 1) // not read yet
				{
					crossPlatformSleep(getInputParameter("sleep_seconds"));
					if (m++ > getInputParameter("NsleepForIO"))
					{
						ERRORmsg(thisCountryCode + ": couldn't open flagFile " + filename, true);
						exit(-3); // give up
					}
					continue; // keep waiting
				}
				if (filestep == currStep() - 1) // OK, extCountry has read my previous IO file
				{
					fileRead = true;
					break;
				}
				else
					if (filestep >= currStep())
					{
						ERRORmsg(thisCountryCode + ": flagFile " + filename + " read ahead my currStep", true);
						exit(-3);
						return;
					}
			}
		}

		extCountriesCodes.erase(extCountryCode); // don't change the for range
		if (extCountriesCodes.size() == 0)
			break;
	}
}

void CWorld::readMyExternalSectorsIO() const
{
	if (_pFIGARO == nullptr || getExtSectors().size() <= 1 // size==1: RW
		|| currStep() < getInputParameter("ReadIOfilesFromStep"))
		return;

	waitMyExternalCountriesToWriteTheirIO();

	long abscurrStep = 0;
	string fname, countryCode, SectorCode, SectName, ExtSectName, word, word1, word2;
	GoodQtty SectorN = -1, qtty = 0;
	double d0 = 0., d1 = 0.;
	GoodType gType = undefinedGoodType;
	char line[1000] = { " " };

	ifstream ExtSectIO;

	string thisCountryCode = getFigaro()._thisCountryCode;

	set<string> MyExtCountriesCodes;
	for (const auto& code : *getFigaro()._pDisaggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	for (const auto& code : *getFigaro()._pAggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	MyExtCountriesCodes.erase("RW");
	set<string> extCountriesCodes = MyExtCountriesCodes;

	ifstream flagFile;
	string filename;
	long currstep = 0;
	for (auto& pair : ExtSectors())
	{
		auto& ExtSect = *pair.second;
		ExtSect.Exports().clear();
		ExtSect.Exports().resize(getSAM().getnPXproducerTypes(), 0);
		ExtSect.Exports_Init().clear();
		ExtSect.Exports_Init().resize(getSAM().getnPXproducerTypes(), 0);
		ExtSect.Imports().clear();
		ExtSect.Imports().resize(getSAM().getnPXproducerTypes(), 0);
	}

	for (const auto& extCountryCode : MyExtCountriesCodes)
	{
		bool IsDisaggExtCountry = false;
		if (getFigaro()._pDisaggExtSectCountries->find(extCountryCode)
			!= getFigaro()._pDisaggExtSectCountries->end())
			IsDisaggExtCountry = true;

		string filename = CWorld::getSimulationName() + "_"
			+ extCountryCode + "_IO.dep";

		ExtSectIO.open(filename);
		if (!ExtSectIO.is_open())
			ERRORmsg("Couldn't open " + filename, true);

#ifdef WINDOWS_VERSION
		ExtSectIO.getline(line, 999, '\n'); // time date
#endif

		ExtSectIO >> word >> countryCode >> word;
		assert(countryCode == extCountryCode);

		ExtSectIO >> abscurrStep;

		// ==========   EXPORTS    ============================

		// This country ES reading _FR_IO.dep, go to:
		// "IMPORTS
		//     PurchasedFromCountryCode ES
		//		from_aggExtSector [nn] XES { to_mySectors [mm...]"

		ExtSectIO >> word >> word1; // IMPORTS {

		while (true) {
			while (word != "PurchasedFromCountryCode")
			{
				ExtSectIO >> word;
				if (word != "PurchasedFromCountryCode")
					ExtSectIO.getline(line, 999, '\n');
			}
			ExtSectIO >> countryCode; // PurchasedFromCountryCode extCountryCode
			if (countryCode == thisCountryCode)
			{
				ExtSectIO >> word; // {
				break;
			}
			else
				word = "";
		}

		GoodQtty extSectorN = -1;
		while (ExtSectIO >> word, word != "}") // from_(agg)ExtSector
		{
			ExtSectIO >> extSectorN >> ExtSectName;

			if (ExtSectName.substr(0, 1) == "X")
			{
				//IsDisaggExtCountry = false;
				ExtSectIO >> word >> word1; // { to_mySectors:
			}
			else
			{
				//IsDisaggExtCountry = true;
				ExtSectIO >> word >> word >> word >> word1; // { to_mySectors:
			}

			GoodType myExtSectorN = getSAM().getAccNofName("X" + extCountryCode);

			while (ExtSectIO >> word, word != "}") // read this row of sectors values
			{
				SectorN = atoi(word.c_str());
				ExtSectIO >> qtty;
				ExtSectors().at(myExtSectorN)->Exports()[SectorN] = qtty;
				ExtSectors().at(myExtSectorN)->Exports_Init()[SectorN] = qtty;
				// Exports_Init is used as ExtSector.GoodsIwish and for normalization in allocateBuyersQttyAndMoney
			}

			if (!IsDisaggExtCountry)	//M J ?? not verified for Aggregated
				break;
		}

		// ==========   IMPORTS    ============================

		// find again thisCountryCode (now in EXPORTS of extCountryCode)

		while (true)
		{
			while (word != "SoldToCountryCode")
			{
				ExtSectIO >> word;
				if (word != "SoldToCountryCode")
					ExtSectIO.getline(line, 999, '\n');
			}
			ExtSectIO >> countryCode; // PurchasedFromCountryCode extCountryCode
			if (countryCode == thisCountryCode)
			{
				ExtSectIO >> word; // {
				break;
			}
			else
				word = "";
		}

		while (ExtSectIO >> word, word != "}") // to_(agg)extSector
		{
			ExtSectIO >> extSectorN >> ExtSectName;

			ExtSectIO >> word >> word1; // { from_mySectors:

			GoodType myExtSectorN = getSAM().getAccNofName("X" + extCountryCode);

			//---------------ExtSectIO >> SectorN >> SectName >> word >> word1; // { from_mySectors:

			while (ExtSectIO >> word, word != "}")
			{
				SectorN = atoi(word.c_str());
				ExtSectIO >> qtty;
				ExtSectors().at(myExtSectorN)->Imports()[SectorN] = qtty; // used as ToSell here
			}

			if (!IsDisaggExtCountry)
				break;
		}

		ExtSectIO.close();

		// update read step flag file

		filename = CWorld::getSimulationName() + "_"
			+ extCountryCode + "_IO_readBy_"
			+ thisCountryCode + ".dep";
		ofstream flagFile(filename);
		flagFile << " " << abscurrStep;
		flagFile.flush();
		flagFile.close();
	}
};
void CWorld::waitMyExternalCountriesToWriteTheirIO() const
{
	string thisCountryCode = getFigaro()._thisCountryCode;

	set<string> MyExtCountriesCodes;
	for (const auto& code : *getFigaro()._pDisaggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	for (const auto& code : *getFigaro()._pAggExtSectCountries)
		MyExtCountriesCodes.insert(code);
	MyExtCountriesCodes.erase("RW");

	ifstream flagFile;
	string filename;
	long currstep = 0;
	set<string> extCountriesCodes = MyExtCountriesCodes; // make an erasable, countdown copy
	for (const auto& extCountryCode : MyExtCountriesCodes)
	{
		if (extCountryCode == "RW")
			ERRORmsg("ERROR: extCountryCode == RW", true);

		filename = CWorld::getSimulationName() + "_" + extCountryCode + "_IO_written.dep";

		while (true)
		{
			long m = 0;
			bool fileOpen = false;
			while (!fileOpen)
			{
				flagFile.open(filename);
				if (flagFile.fail())
				{
					flagFile.close();
					crossPlatformSleep(getInputParameter("sleep_seconds"));
					if (m++ > getInputParameter("NsleepForIO"))
					{
						ERRORmsg(thisCountryCode + ": couldn't open flagFile " + filename, true);
						return;
					}
				}
				else
					fileOpen = true;
			}

			flagFile >> currstep;
			flagFile.close();
			crossPlatformSleep(getInputParameter("sleep_seconds"));

			if (currstep == currStep() - 1)
				break;
			else
				if (currstep >= currStep())
				{
					ERRORmsg(thisCountryCode + ": flagFile " + filename + " past my currStep", true);
					return;
				}
		}

		extCountriesCodes.erase(extCountryCode);
		if (extCountriesCodes.size() == 0)
			break;
	}
}

void CWorld::runOneMonth()
{
	if (currStep() == 0)
	{
		InitializeSimulation();
		getSAM().writeInputSAM();
		SAM().saveInputSAM("SAM.csv");
	}

	assert(DEPData().CheckAccountingBalance());

	pDEPData()->AnalizeLastMonth();// review

	if (currStep() > InitStep() + 1) // skip if it is a continued simulation
		// read this country's Imports from (Exports to) its ExtSect countries
		readMyExternalSectorsIO();

	initializeMonth();

	// 1. Make a randomized copy of individuals  ----------------------------------------------

	static vector<CAgent*> rndIndivs;
	rndIndivs.clear();
	rndIndivs.insert(rndIndivs.end(), pIndividuals()->begin(), pIndividuals()->end()); // sorted by increasing ID
	std::shuffle(rndIndivs.begin(), rndIndivs.end(), myRandomEngine());

	// 2. Randomize Producers and sort them by their MonthlyActivityStep *only*  ----------------------------------------

	static vector<AgentSortedByFirst> sortedProducersByWorkDayOnly; // sort only by .first

	sortedProducersByWorkDayOnly.clear();
	for (auto pProd : (*pProducers())) // sorted by increasing ID
		if (pProd != nullptr)
			sortedProducersByWorkDayOnly.push_back(AgentSortedByFirst(pProd->getMonthlyActivityStep(), pProd));

	// shuffle the ID-sorted vector
	std::shuffle(sortedProducersByWorkDayOnly.begin(), sortedProducersByWorkDayOnly.end(), myRandomEngine());

	// sort by .first only
	// warning: don't use pair<x, y>, it sorts by pair's first and then by second
	// AgentSortedByFirst sorts by pair's first only


	std::sort(sortedProducersByWorkDayOnly.begin(),
		sortedProducersByWorkDayOnly.end(), myless<AgentSortedByFirst>()); // by increasing date

	// don't shuffle Producers again, preserve activity sequence to have one month activity period

// =============  Monthly activity of Indivs, Producers, Gov and ExtSects  ===================

	GoodQtty nIndividuals = pIndividuals()->size();
	// Each activity group is like a workDay of month (RunProducer)
	GoodQtty WorkDaysPerMonth = min(nIndividuals, (GoodQtty)DEPData().InputParameter("WorkDaysPerMonth"));
	long nIndivsPerWorkDay = (GoodQtty)(ceil((double)rndIndivs.size() / WorkDaysPerMonth));
	GoodQtty nProducers = sortedProducersByWorkDayOnly.size();

	//  >>>>>>>>>>>>>>>>>>>>>>>  MAIN LOOP <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

	Government().GoodsToBuy() = Government().getmyGoodsIwish();

	for (auto& pair : ExtSectors())
	{
		auto& ExtSector = *pair.second;
		ExtSector.GoodsToBuy() = ExtSector.getmyGoodsIwish();
	}

	long indivN = 0;
	long producerN = 0;
	for (long workDayN = 0; workDayN < WorkDaysPerMonth; ++workDayN)
	{
		// Final consumers buy from producers, by groups of nIndivsPerWorkDay, randomized list every month
		do
		{
			CIndividual* pIndiv = (CIndividual*)rndIndivs.at(indivN);

			if (DEPData().InputParameter("IndivConsum"))
				pIndiv->stepActivity();

			if (DEPData().InputParameter("GovConsum"))
			{
				Government().Location() = pIndiv->Location();
				Government().myPrice() = pIndiv->myPrice();

				Government().BuyGoods();
			}

			if (DEPData().InputParameter("ExtSectConsum"))
			{
				for (auto& pair : ExtSectors())
				{
					auto& ExtSector = *pair.second;

					ExtSector.Location() = pIndiv->Location();

					ExtSector.BuyGoods();
				}
			}

		} while (++indivN % nIndivsPerWorkDay != 0 && indivN < nIndividuals);

		if (currStep() <= getInputParameter("ProductionDayFrom"))
		{
			while (producerN < nProducers)
			{
				auto& sortedAgent = sortedProducersByWorkDayOnly.at(producerN);
				CProducer* pProd = (CProducer*)(sortedAgent.second);
				if (sortedAgent.first != workDayN)
					break;

				pProd->stepActivity();
				++producerN;
			}
		}
		else
			for (auto& pair : sortedProducersByWorkDayOnly)
			{
				CProducer* pProd = (CProducer*)(pair.second);

				if (pProd->isProductionDay(workDayN) == true)
					pProd->stepActivity(); // full month activity if Nproduction_days=1
			}
		/*
		else
			for (auto pProd : (*pProducers())) // sorted by increasing ID
			{
				if (pProd->isProductionDay(workDayN) == true)
					pProd->stepActivity(); // full month activity if Nproduction_days=1
			}
		*/
	}

	//  ==============================================================================================

	// Remaining tasks at the end of month: FinancialMarket, Taxes, Subsidies, Interests...

	Government().monthlyActivity();

	for (auto& pair : ExtSectors())
		pair.second->stepActivity();

	FinancialMarket().stepActivity();

	getSAM().writeSAMstep();

	writeExternalSectorsIO();

	assert(DEPData().CheckAccountingBalance());
};

///////////////////////////////////////////////////////////////////////////////////////
