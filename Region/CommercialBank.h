#pragma once

#include "Agent.h"
#include <vector> // Required for std::vector (e.g., for loans, accounts)
#include <string> // Required for std::string (e.g., for logging)
#include <map>    // Required for std::map (e.g., for deposits, loans)

namespace Region {

    class CentralBank; // Forward declaration

    // Basic structure for loan details (Phase 3)
    struct LoanDetails {
        int loanID; // Unique ID for the loan
        Agent* borrower;
        double principal;
        double interestRate; // Annual rate
        int termMonths;
        double outstandingBalance;
        int monthsRemaining;
        double monthlyPayment; // Calculated monthly payment
    };

    class CommercialBank : public Agent {
    public:
        // Constructor
        CommercialBank(int id, GoodType agentType = -3); // Default agentType for CommercialBank

        // Destructor
        ~CommercialBank() override;

        // Override Agent's monthly activity method
        void monthlyActivity() override;

        // --- Phase 2: Deposit Methods ---
        bool MakeDeposit(Agent* depositor, double amount);
        bool Withdraw(Agent* depositor, double amount);
        double GetDepositBalance(int agentID) const;

        // --- Phase 3: Loan Methods ---
        bool RequestLoan(Agent* borrower, double amount, int termMonths);
        void ProcessLoanRepayments(); // Called in monthlyActivity

        // --- Phase 4: Interaction with Central Bank ---
        void SetCentralBank(CentralBank* cb);
        bool DepositReservesToCentralBank(double amount);

        // Method to get total outstanding loan balance for a specific agent
        double GetTotalOutstandingLoanBalanceForAgent(const Agent* agent) const;

        // New method to clear records for a defunct agent
        void ClearAgentRecords(int agentId);

        // Methods for time series data
        double GetTotalDeposits() const;
        double GetTotalLoans() const;
        void RecordBankMetrics(int currentMonth);

    private:
        // --- Phase 2: Data for Deposits ---
        std::map<int, double> m_deposits; // agentID -> balance

        // --- Phase 3: Data for Loans ---
        std::map<int, LoanDetails> m_loans; // loanID -> LoanDetails
        int m_nextLoanID;

        // --- Phase 4: Pointer to Central Bank ---
        CentralBank* m_centralBank;

        // Example: Internal logic for loan approval (Phase 3)
        bool assessLoanApplication(Agent* borrower, double amount, int termMonths);
    };

} // namespace Region