#pragma once

#include <string>
#include <vector> // Added for std::vector
#include "Agent.h"

namespace Region {

	// Forward declaration
	class Agent;
	class Firm; // Forward declaration for Firm

	// Worker class - represents a labor agent that can be employed
	class Worker : public Agent
	{
	private:

		// Household consumption adjustment factor
		static double s_HouseholdConsumFactor;

		// Amount of work time available per month (1.0 = full time)
		double m_availableWorkTime;

		// Pointer to employer agent (nullptr if unemployed)
		Agent* m_employer;

		// List of producers owned by this worker
		std::vector<Firm*> m_ownedProducers; // Changed to store raw pointers to firms owned by the worker

		// Quantity coefficient calculated from SAM for each good type, normalized to sum to 1
		std::vector<double> m_myGoodsPurchaseProportions;

		// Member to store last month's total expenditure
		GoodQtty m_lastMonthExpenditure;

		// Helper method to create a purchase list based on wealth and coefficients
		double makePurchaseList(); // Changed return type from void to double

		// Helper methods for business startup
		bool canOwnMoreProducers() const;

	public:
		// Constructors and destructor
		Worker();
		explicit Worker(int id);
		virtual ~Worker();

		// Employment management
		bool IsEmployed() const;
		Agent* GetEmployer() const;
		void SetEmployer(Agent* employer);

		// Work time management
		double GetWorkTime() const;
		void SetWorkTime(double workTime);
		void ResetWorkTimeForMonth();

		// Static getters and setters for the household consumption factor
		static double GetHouseholdConsumFactor();
		static void SetHouseholdConsumFactor(double factor);

		// Quantity coefficient management
		double GetGoodsPurchaseProportions(GoodType goodType) const;
		void SetGoodsPurchaseProportions(GoodType goodType, double coef);
		const std::vector<double>& GetAllGoodsPurchaseProportions() const;

		// Last month's expenditure
		GoodQtty GetLastMonthExpenditure() const;
		void AddToLastMonthExpenditure(GoodQtty amount);
		void ResetLastMonthExpenditure();
		bool checkestimatedHouseholdPurchaseCost(double estimatedHouseholdPurchaseCost);
		virtual double GetEquity() const;

		// Monthly activity implementation - override from Agent
		void monthlyActivity() override;

		// Business startup
		bool tryStartNewBusiness();
		void addOwnedProducer(Firm* producer);
		void RemoveOwnedProducer(Firm* producer); // Added method
	};

} // namespace Region