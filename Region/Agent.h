#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <stdexcept>
#include "../DataManager/DataManager.h" // Added for SAM access
#include "Goods.h"

namespace Region {

// Forward declarations
class CGoods;
class Region;
class CommercialBank; // Added forward declaration

// Agent class - represents an economic agent that can have, buy and sell goods
class Agent 
{
public:
    // Static region pointer
    static Region* s_region;
    static void SetRegion(Region* region);

protected:
    // Unique identifier for this agent
    int m_id;

	GoodType m_agentType; // also type of good produced if applicable

    // Inventory of goods owned by this agent
	CGoods m_inventory; // month consumption if worker, IC if firm, etc.
    
    // Purchase price of each good type. SellPrice is another variable in firms
    std::vector<double> m_myPrice;

	// Price the worker/firm has for salary. Multiplied by initSalary to get the salary_mu in GetSalary_mu() method
    double m_myPriceOfSalary;

    // Amount of money this agent has
    GoodQtty m_money;

    // List of goods this agent plans to purchase
    std::vector<GoodQtty> m_purchaseList;

    // Stores ID of the last seller for a given good type
    std::vector<int> m_lastProviderIDs;

    // Designated commercial bank for this agent
    CommercialBank* m_designatedCommercialBank = nullptr;

    // Helper method to ensure vectors are properly sized
    void EnsureVectorsSize();
    
    // Helper method to initialize good types from SAM
    bool InitializeGoodTypesFromSAM(const DataManager::SAM* sam);

    // Helper method for BuyGoods
    bool AttemptPurchaseFromSeller(Agent* seller, GoodType goodType, GoodQtty& quantityNeeded);

    // Helper to get the region (consider making this public if needed by more classes)
    static Region* GetRegion(); // Added declaration

public:
    // Constructors and destructor
    Agent();
    explicit Agent(int id);
    explicit Agent(int id, GoodType agentType);
    virtual ~Agent();

    // Agent type management
    GoodType GetAgentType() const;
    void SetAgentType(GoodType agentType);

    // SAM initialization method
    virtual bool InitializeFromSAM(const DataManager::SAM* sam);

    // Basic getters and setters
    int GetID() const;
    double GetMoney() const;
    void SetMoney(double amount);
    void AddMoney(double amount);

    virtual double GetEquity() const;

    // Inventory management
    const CGoods& GetInventory() const;
    CGoods& GetInventoryMutable();
    GoodQtty GetGoodQuantity(GoodType goodType) const;
    void SetGoodQuantity(GoodType goodType, GoodQtty quantity);
    void AddGoodQuantity(GoodType goodType, GoodQtty quantity);

    // Price management
    double GetPrice(GoodType goodType) const;
    void SetPrice(GoodType goodType, double price);
    const std::vector<double>& GetAllPrices() const;

    // Salary management
    double GetSalaryPrice() const;
    void SetSalaryPrice(double price);
    double GetSalary_mu() const;

    // Trading operations
    bool CanPurchase(GoodType goodType, GoodQtty quantity, double unitPrice) const;
    bool Purchase(GoodType goodType, GoodQtty quantity, double unitPrice);
    bool CanSell(Agent* seller, GoodQtty quantity) const;
    virtual bool Sell(GoodType goodType, GoodQtty quantity, double unitPrice);

    // Monthly activity to be performed by the agent
    virtual void monthlyActivity();

    // Purchase list management
    void ClearPurchaseList();
    void AddToPurchaseList(GoodType goodType, GoodQtty quantity);
    const std::vector<GoodQtty>& GetPurchaseList() const;

    // Last provider ID management
    int GetLastProviderID(GoodType goodType) const;
    void SetLastProviderID(GoodType goodType, int providerID);

    // Buying goods
    void BuyGoods();

    // Labor hiring method (similar to BuyGoods)
    void HireWorkers();

    // Bank interaction methods
    void SetDesignatedCommercialBank(CommercialBank* bank);
    CommercialBank* GetDesignatedCommercialBank() const;
    bool DepositToBank(double amount);
    bool WithdrawFromBank(double amount);

    // Loan interaction methods
    virtual bool MakeLoanPayment(double amount);
    virtual bool RequestLoan(double amount, int termMonths);
};

} // namespace Region