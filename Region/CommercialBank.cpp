#include <stdexcept>
#include <cmath>
#include <numeric>
#include "CommercialBank.h"
#include "CentralBank.h"
#include "../Logger/Logger.h"
#include "../DataManager/DataManager.h"
#include "../Simulator/Simulator.h"

namespace Region {

    // Constructor
    CommercialBank::CommercialBank(int id, GoodType agentType)
        : Agent(id, agentType), m_nextLoanID(0) {
        m_centralBank = nullptr;
        Logger::Info("CommercialBank created with ID: " + std::to_string(id) + ", Type: " + std::to_string(agentType));
    }

    // Destructor
    CommercialBank::~CommercialBank() {
        Logger::Info("CommercialBank destroyed with ID: " + std::to_string(GetID()));
    }

    // monthlyActivity method - FIXED RESERVE MANAGEMENT
    void CommercialBank::monthlyActivity() {
        Logger::Info("CommercialBank " + std::to_string(GetID()) + " performing monthly activity.");

        // --- Phase 3: Process Loan Repayments ---
        ProcessLoanRepayments();

        // --- Phase 4: FIXED Reserve Management with Central Bank ---
        if (m_centralBank) {
            // Get reserve requirement ratio from configuration
            const double RESERVE_REQUIREMENT_RATIO = DataManager::GetConfigParameters().reserveRequirementRatio;
            const double LIQUIDITY_RATIO = DataManager::GetConfigParameters().bankLiquidityRatio;

            // Calculate required reserves based on total deposits
            double totalDeposits = GetTotalDeposits();
            double requiredReserves = totalDeposits * RESERVE_REQUIREMENT_RATIO;

            // Get current reserve balance at central bank
            double currentReserves = m_centralBank->GetReserveBalance(GetID());

            // Calculate how much additional reserves are needed (if any)
            double reserveShortfall = requiredReserves - currentReserves;

            // Only deposit if we need more reserves AND we have sufficient cash
            if (reserveShortfall > 0.01) {
                // Ensure we maintain minimum liquidity for withdrawals
                double minimumCashNeeded = totalDeposits * LIQUIDITY_RATIO;
                double availableCash = GetMoney() - minimumCashNeeded;

                if (availableCash > 0) {
                    // Deposit only what's needed and available
                    double amountToDeposit = std::min(reserveShortfall, availableCash);

                    if (amountToDeposit > 0.01) {
                        Logger::Info("CommercialBank " + std::to_string(GetID()) +
                            " depositing reserves. Required: " + std::to_string(requiredReserves) +
                            ", Current: " + std::to_string(currentReserves) +
                            ", Depositing: " + std::to_string(amountToDeposit));
                        DepositReservesToCentralBank(amountToDeposit);
                    }
                }
                else {
                    Logger::Warning("CommercialBank " + std::to_string(GetID()) +
                        " needs reserves but must maintain liquidity. Cash: " + std::to_string(GetMoney()) +
                        ", Min needed for liquidity: " + std::to_string(minimumCashNeeded));
                }
            }
        }

        // Record bank metrics for time series data
        int currentMonth = Simulator::GetCurrentMonthCalculated();
        RecordBankMetrics(currentMonth);
    }

    // --- Phase 2: Deposit Method Implementations ---
    bool CommercialBank::MakeDeposit(Agent* depositor, double amount) {
        if (!depositor) {
            Logger::Error("CommercialBank::MakeDeposit: Depositor is null.");
            return false;
        }
        if (amount <= 0) {
            Logger::Warning("CommercialBank::MakeDeposit: Deposit amount must be positive. Agent: " + std::to_string(depositor->GetID()) + ", Amount: " + std::to_string(amount));
            return false;
        }

        m_deposits[depositor->GetID()] += amount;
        AddMoney(amount); // Bank's cash increases

        //Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Agent " + std::to_string(depositor->GetID()) +
        //    " deposited " + std::to_string(amount) + ". New balance: " + std::to_string(m_deposits[depositor->GetID()]));
        return true;
    }

    bool CommercialBank::Withdraw(Agent* depositor, double amount) {
        if (!depositor) {
            Logger::Error("CommercialBank::Withdraw: Depositor is null.");
            return false;
        }
        if (amount <= 0) {
            Logger::Warning("CommercialBank::Withdraw: Withdrawal amount must be positive. Agent: " + std::to_string(depositor->GetID()) + ", Amount: " + std::to_string(amount));
            return false;
        }

        auto it = m_deposits.find(depositor->GetID());
        if (it == m_deposits.end() || it->second < amount) {
            Logger::Warning("CommercialBank " + std::to_string(GetID()) + ": Agent " + std::to_string(depositor->GetID()) +
                " has insufficient funds for withdrawal. Requested: " + std::to_string(amount) +
                ", Available: " + std::to_string(it == m_deposits.end() ? 0 : it->second));
            return false;
        }

        if (GetMoney() < amount) {
            Logger::Error("CommercialBank " + std::to_string(GetID()) + ": Bank has insufficient cash for withdrawal. Requested: " +
                std::to_string(amount) + ", Bank cash: " + std::to_string(GetMoney()));
            return false;
        }

        it->second -= amount;
        AddMoney(-amount); // Bank's cash decreases

        //Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Agent " + std::to_string(depositor->GetID()) +
        //    " withdrew " + std::to_string(amount) + ". New balance: " + std::to_string(it->second));
        return true;
    }

    double CommercialBank::GetDepositBalance(int agentID) const {
        auto it = m_deposits.find(agentID);
        if (it != m_deposits.end()) {
            return it->second;
        }
        return 0.0;
    }

    // --- Phase 3: Loan Method Implementations ---
    bool CommercialBank::RequestLoan(Agent* borrower, double amount, int termMonths) {
        if (!borrower) {
            Logger::Error("CommercialBank::RequestLoan: Borrower is null.");
            return false;
        }
        if (amount <= 0 || termMonths <= 0) {
            Logger::Warning("CommercialBank::RequestLoan: Invalid loan parameters. Amount: " + std::to_string(amount) + ", Term: " + std::to_string(termMonths));
            return false;
        }

        if (!assessLoanApplication(borrower, amount, termMonths)) {
            Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan application DENIED for Agent " + std::to_string(borrower->GetID()));
            return false;
        }

        double annualRate = 0.05;
        double monthlyRate = annualRate / 12.0;
        double monthlyPayment = amount * (monthlyRate * std::pow(1 + monthlyRate, termMonths)) /
            (std::pow(1 + monthlyRate, termMonths) - 1);

        LoanDetails loan;
        loan.loanID = m_nextLoanID++;
        loan.borrower = borrower;
        loan.principal = amount;
        loan.interestRate = annualRate;
        loan.termMonths = termMonths;
        loan.outstandingBalance = amount;
        loan.monthsRemaining = termMonths;
        loan.monthlyPayment = monthlyPayment;

        m_loans[loan.loanID] = loan;

        AddMoney(-amount);
        borrower->AddMoney(amount);

        Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan APPROVED. ID: " + std::to_string(loan.loanID) +
            ", Agent: " + std::to_string(borrower->GetID()) + ", Amount: " + std::to_string(amount) +
            ", Term: " + std::to_string(termMonths) + " months, Monthly Payment: " + std::to_string(monthlyPayment));
        return true;
    }

    bool CommercialBank::assessLoanApplication(Agent* borrower, double amount, int termMonths) {
        double totalOutstanding = GetTotalOutstandingLoanBalanceForAgent(borrower);
        double maxLoanAmount = 50000.0;
        if (totalOutstanding + amount > maxLoanAmount) {
            Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan exceeds max limit for Agent " +
                std::to_string(borrower->GetID()) + ". Total would be: " + std::to_string(totalOutstanding + amount));
            return false;
        }

        if (GetMoney() < amount) {
            Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Insufficient funds to grant loan. Available: " +
                std::to_string(GetMoney()) + ", Requested: " + std::to_string(amount));
            return false;
        }

        return true;
    }

    void CommercialBank::ProcessLoanRepayments() {
        auto it = m_loans.begin();
        while (it != m_loans.end()) {
            LoanDetails& loan = it->second;
            Agent* borrower = loan.borrower;

            if (!borrower) {
                Logger::Error("CommercialBank " + std::to_string(GetID()) + ": Loan ID " + std::to_string(loan.loanID) + " has null borrower.");
                it = m_loans.erase(it);
                continue;
            }

            double paymentDue = loan.monthlyPayment;

            Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan ID " + std::to_string(loan.loanID) +
                " - Attempting to collect payment: " + std::to_string(paymentDue) +
                ", Outstanding: " + std::to_string(loan.outstandingBalance));

            if (borrower->MakeLoanPayment(paymentDue)) {
                AddMoney(paymentDue);

                double interestPortion = loan.outstandingBalance * (loan.interestRate / 12.0);
                interestPortion = std::min(interestPortion, paymentDue);
                double principalPortion = paymentDue - interestPortion;

                loan.outstandingBalance -= principalPortion;
                loan.monthsRemaining--;

                Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan ID " + std::to_string(loan.loanID) +
                    " - Payment successful. Amount: " + std::to_string(paymentDue) +
                    ". Interest: " + std::to_string(interestPortion) + ", Principal: " + std::to_string(principalPortion) +
                    ". New outstanding balance: " + std::to_string(loan.outstandingBalance) +
                    ", Months remaining: " + std::to_string(loan.monthsRemaining));

                if (loan.outstandingBalance <= 0.01 || loan.monthsRemaining <= 0) {
                    Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Loan ID " + std::to_string(loan.loanID) + " fully repaid by Agent " + std::to_string(borrower->GetID()));
                    it = m_loans.erase(it);
                }
                else {
                    ++it;
                }
            }
            else {
                Logger::Warning("CommercialBank " + std::to_string(GetID()) + ": Loan ID " + std::to_string(loan.loanID) +
                    " - Payment FAILED by Agent " + std::to_string(borrower->GetID()) + ". Amount due: " + std::to_string(paymentDue));
                ++it;
            }
        }
    }

    // --- Phase 4: Interaction with Central Bank ---
    void CommercialBank::SetCentralBank(CentralBank* cb) {
        m_centralBank = cb;
        if (cb) {
            Logger::Info("CommercialBank " + std::to_string(GetID()) + " set CentralBank to ID: " + std::to_string(cb->GetID()));
        }
        else {
            Logger::Warning("CommercialBank " + std::to_string(GetID()) + " set CentralBank to nullptr.");
        }
    }

    bool CommercialBank::DepositReservesToCentralBank(double amount) {
        if (!m_centralBank) {
            Logger::Error("CommercialBank " + std::to_string(GetID()) + ": No CentralBank assigned to deposit reserves.");
            return false;
        }
        if (amount <= 0) {
            Logger::Warning("CommercialBank " + std::to_string(GetID()) + ": Reserve deposit amount must be positive. Amount: " + std::to_string(amount));
            return false;
        }
        if (GetMoney() < amount) {
            Logger::Warning("CommercialBank " + std::to_string(GetID()) + ": Insufficient funds for reserve deposit. Available: " +
                std::to_string(GetMoney()) + ", Requested: " + std::to_string(amount));
            return false;
        }

        AddMoney(-amount);
        m_centralBank->AcceptReserveDeposit(this, amount);

        Logger::Info("CommercialBank " + std::to_string(GetID()) + " deposited " + std::to_string(amount) +
            " as reserves to CentralBank " + std::to_string(m_centralBank->GetID()) +
            ". Remaining cash: " + std::to_string(GetMoney()));
        return true;
    }

    double CommercialBank::GetTotalOutstandingLoanBalanceForAgent(const Agent* agent) const {
        if (!agent) {
            Logger::Error("CommercialBank::GetTotalOutstandingLoanBalanceForAgent: Agent pointer is null.");
            return 0.0;
        }

        double totalBalance = 0.0;
        for (const auto& pair : m_loans) {
            const LoanDetails& loan = pair.second;
            if (loan.borrower && loan.borrower->GetID() == agent->GetID()) {
                totalBalance += loan.outstandingBalance;
            }
        }
        return totalBalance;
    }

    void CommercialBank::ClearAgentRecords(int agentId) {
        Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Clearing records for agent ID " + std::to_string(agentId));

        auto deposit_it = m_deposits.find(agentId);
        if (deposit_it != m_deposits.end()) {
            double depositAmount = deposit_it->second;
            m_deposits.erase(deposit_it);
            Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Cleared deposit of " +
                std::to_string(depositAmount) + " for agent ID " + std::to_string(agentId));
        }

        auto loan_it = m_loans.begin();
        while (loan_it != m_loans.end()) {
            if (loan_it->second.borrower && loan_it->second.borrower->GetID() == agentId) {
                double outstandingBalance = loan_it->second.outstandingBalance;
                Logger::Info("CommercialBank " + std::to_string(GetID()) + ": Writing off loan ID " +
                    std::to_string(loan_it->second.loanID) + " with outstanding balance " +
                    std::to_string(outstandingBalance) + " for agent ID " + std::to_string(agentId));
                loan_it = m_loans.erase(loan_it);
            }
            else {
                ++loan_it;
            }
        }
    }

    // New methods for time series data
    double CommercialBank::GetTotalDeposits() const {
        double total = 0.0;
        for (const auto& deposit : m_deposits) {
            total += deposit.second;
        }
        return total;
    }

    double CommercialBank::GetTotalLoans() const {
        double total = 0.0;
        for (const auto& loan : m_loans) {
            total += loan.second.outstandingBalance;
        }
        return total;
    }

    void CommercialBank::RecordBankMetrics(int currentMonth) {
        if (currentMonth < 0) return;

        double totalDeposits = GetTotalDeposits();
        double totalLoans = GetTotalLoans();
        double cash = GetMoney();
        double reserves = 0.0;

        if (m_centralBank) {
            reserves = m_centralBank->GetReserveBalance(GetID());
        }

        DataManager::UpdateCommercialBankDepositValue(currentMonth, static_cast<float>(totalDeposits));
        DataManager::UpdateCommercialBankLoanValue(currentMonth, static_cast<float>(totalLoans));
        DataManager::UpdateCommercialBankCashValue(currentMonth, static_cast<float>(cash));
        DataManager::UpdateCommercialBankReserveValue(currentMonth, static_cast<float>(reserves));

        Logger::Info("CommercialBank " + std::to_string(GetID()) +
            ": Month " + std::to_string(currentMonth) +
            ", Total Deposits: " + std::to_string(totalDeposits) +
            ", Total Loans: " + std::to_string(totalLoans) +
            ", Cash: " + std::to_string(cash) +
            ", Reserve Deposits: " + std::to_string(reserves));
    }

} // namespace Region