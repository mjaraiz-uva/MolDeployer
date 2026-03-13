
// Agent.cpp

#include "pch.h"

//======================  CAgentFID  ===================================

std::ofstream& operator<<(std::ofstream& ofstrm, const CAgentFID& fullID)
{
    ofstrm << " " << fullID._type << " " << fullID._id;
    return ofstrm;
};
std::ifstream& operator>>(std::ifstream& ifstrm, CAgentFID& fullID)
{
    ifstrm >> fullID._type >> fullID._id;
    return ifstrm;
};

// ==========================  CAgent  ==================================

void CAgent::initialize() {};

double CAgent::mylogitProb(GoodType gType) const
{
    double sum = 0., gTypePrice = 0., logitProb = 0.;
    auto nGoodsIwish = getmyGoodsIwish().size();
	double gammaC = 4.0;
    for (const auto& gPair : getmyGoodsIwish())
    {
        double price = getmyPrice(gType);
        if (price <= 0)
            ERRORmsg("price <= 0 in mylogitProb");

        sum += exp(-gammaC * log(price));
    }

    if (sum <= 0)
        ERRORmsg("sum <= 0 in mylogitProb");

    logitProb = nGoodsIwish * exp(-gammaC * log(gTypePrice)) / sum;

    return logitProb;
}

void CAgent::BuyGoods() {
    // Get sellers: producer neighbors of this buyer agent
    std::vector<AgentID> neighborProducerIDs;
    getGlobalRegion().getNeighborProducersOf(this, neighborProducerIDs);

    // 1. Handle worker hiring by producers
    if (this->isProducer() && dynamic_cast<CProducer*>(this)->getworkTimeToBuy() > 0) {
        std::vector<AgentID> myNeighborWorkerIDs;
        getGlobalRegion().getNeighborWorkersOf(this, myNeighborWorkerIDs);

        for (auto workerID : myNeighborWorkerIDs) {
            CWorker* pNeiWorker = getGlobalRegion().getWorker(workerID);
            if (pNeiWorker) {
                dynamic_cast<CProducer*>(this)->hireWorker(pNeiWorker);

                if (dynamic_cast<CProducer*>(this)->getworkTimeToBuy() == 0)
                    break;
            }
        }
    }

	// 2. Buy goods (Households consumption or producers IC)
    for (const auto& goodToBuy : goodsToBuy) {
        for (auto producerID : neighborProducerIDs) {
            CProducer* pNeiProducer = getGlobalRegion().getProducer(producerID);
            if (pNeiProducer && pNeiProducer->getAgentType() == goodToBuy.first) {
                pNeiProducer->recentDemand() += goodToBuy.second;
                GoodQtty boughtQty = BuyFromAgent(pNeiProducer, goodToBuy.first, goodToBuy.second);

                if (boughtQty >= goodToBuy.second) {
                    break;
                }
            }
        }
    }
}

// Add to Agent.cpp

void CAgent::updateMonthlyCashBuffer() {
    // Set buffer to twice the monthly expenses
    double monthlyExpenses = calculateMonthlyExpenses();
    monthlyCashBuffer = 2.0 * monthlyExpenses;

    if (monthlyCashBuffer > 0) {
        CLogger::debug(getName() + " updated monthly cash buffer to " +
            std::to_string(monthlyCashBuffer));
    }
}

void CAgent::syncWithBank() {
    // Skip if no bank account
    if (!hasBankAccount) {
        return;
    }

    // Update the monthly cash buffer first
    updateMonthlyCashBuffer();

    // If buffer is zero or very small, nothing to sync
    if (monthlyCashBuffer <= 1.0) {
        needsBankSync = false;
        return;
    }

    // Either deposit excess or withdraw needed cash
    if (cash > monthlyCashBuffer * 1.5) {
        // If we have 50% more than buffer, deposit the excess
        double excessAmount = cash - monthlyCashBuffer;
        depositExcessCash(excessAmount);
    }
    else if (cash < monthlyCashBuffer * 0.5) {
        // If we have less than 50% of buffer, withdraw to reach buffer
        double neededAmount = monthlyCashBuffer - cash;
        withdrawNeededCash(neededAmount);
    }

    // Track the sync
    lastBankSyncStep = getpRegion()->getCurrentStep();
    needsBankSync = false;

    CLogger::debug(getName() + " completed bank sync. Cash: " + std::to_string(cash) +
        ", Buffer: " + std::to_string(monthlyCashBuffer));
}

bool CAgent::depositExcessCash(double amount) {
    if (!hasBankAccount || amount <= 0 || amount > cash) {
        return false;
    }

    CCommercialBank* bank = getpRegion()->getBank(bankID);
    if (!bank) return false;

    // Try to deposit
    if (bank->processDeposit(id, amount)) {
        // If successful, reduce agent's cash
        removeCash(amount);
        CLogger::debug(getName() + " deposited " + std::to_string(amount) + " to bank");
        return true;
    }

    CLogger::warning(getName() + " failed to deposit " + std::to_string(amount));
    return false;
}

bool CAgent::withdrawNeededCash(double amount) {
    if (!hasBankAccount || amount <= 0) {
        return false;
    }

    CCommercialBank* bank = getpRegion()->getBank(bankID);
    if (!bank) return false;

    // Check account balance before attempting withdrawal
    double accountBalance = bank->getAccountBalance(id);
    if (accountBalance < amount) {
        // If not enough funds, withdraw whatever is available
        amount = std::max(0.0, accountBalance);
        if (amount <= 0) {
            CLogger::warning(getName() + " insufficient funds to withdraw");
            return false;
        }
    }

    // Try to withdraw
    if (bank->processWithdrawal(id, amount)) {
        // If successful, increase agent's cash
        addCash(amount);
        CLogger::debug(getName() + " withdrew " + std::to_string(amount) + " from bank");
        return true;
    }

    CLogger::warning(getName() + " failed to withdraw " + std::to_string(amount));
    return false;
}

bool CAgent::shouldSyncWithBank(int currentStep, int syncFrequency) const {
    // No bank account, no sync needed
    if (!hasBankAccount) {
        return false;
    }

    // Check if it's time for periodic sync (default is monthly)
    bool timeForPeriodicSync = (currentStep - lastBankSyncStep) >= syncFrequency;

    // Check if cash level is very high or very low compared to buffer
    bool cashLevelCritical = (cash > monthlyCashBuffer * 2.0) || (cash < monthlyCashBuffer * 0.25);

    return timeForPeriodicSync || cashLevelCritical || needsBankSync;
}

bool CAgent::depositToBankAccount(double amount) {
    if (!hasBankAccount || amount <= 0 || amount > cash) {
        return false;
    }

    CCommercialBank* bank = getGlobalRegion().getBank(bankID);
    if (!bank) return false;

    // Try to deposit
    if (bank->processDeposit(id, amount)) {
        // If successful, reduce agent's cash
        removeCash(amount);
        return true;
    }
    return false;
}

bool CAgent::withdrawFromBankAccount(double amount) {
    if (!hasBankAccount || amount <= 0) {
        return false;
    }

    CCommercialBank* bank = getGlobalRegion().getBank(bankID);
    if (!bank) return false;

    // Try to withdraw
    if (bank->processWithdrawal(id, amount)) {
        // If successful, increase agent's cash
        addCash(amount);
        return true;
    }
    return false;
}

bool CAgent::transferToAgent(AgentID recipient, double amount) {
    if (!hasBankAccount || amount <= 0) {
        return false;
    }

    CCommercialBank* bank = getGlobalRegion().getBank(bankID);
    if (!bank) return false;

    // Get recipient agent
    CAgent* recipientAgent = nullptr;

    // First try to find a worker
    recipientAgent = getGlobalRegion().getWorker(recipient);

    // If not a worker, try a producer
    if (!recipientAgent) {
        recipientAgent = getGlobalRegion().getProducer(recipient);
    }

    if (!recipientAgent || !recipientAgent->hasBank()) {
        return false;
    }

    // Simple implementation: withdraw and deposit
    if (bank->processWithdrawal(id, amount)) {
        CCommercialBank* recipientBank = getGlobalRegion().getBank(recipientAgent->getBankID());
        if (recipientBank) {
            if (recipientBank->processDeposit(recipient, amount)) {
                return true;
            }
            // If deposit fails, revert the withdrawal
            bank->processDeposit(id, amount);
        }
    }
    return false;
}

bool CAgent::requestLoan(double amount, int termMonths) {
    if (!hasBankAccount || amount <= 0 || termMonths <= 0) {
        return false;
    }

    CCommercialBank* bank = getGlobalRegion().getBank(bankID);
    if (!bank) return false;

    return bank->issueLoan(id, amount, termMonths);
}

bool CAgent::repayLoan(double amount) {
    if (!hasBankAccount || amount <= 0 || amount > cash) {
        return false;
    }

    CCommercialBank* bank = getGlobalRegion().getBank(bankID);
    if (!bank) return false;

    if (bank->processLoanPayment(id, amount)) {
        removeCash(amount);
        return true;
    }
    return false;
}
//  ===========================================================