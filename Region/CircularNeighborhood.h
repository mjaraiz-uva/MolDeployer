#pragma once

#include <vector>
#include <string>

// Forward declaration (if Agent.h includes CircularNeighborhood.h to avoid circular dependency)
// namespace Region { class Agent; }
// For simplicity, assuming direct include is fine or managed by build system.
// If Agent.h is lightweight and doesn't include CircularNeighborhood.h, direct include is fine.
#include "Agent.h" // Include Agent definition for const Agent*

namespace Region {

	class CircularNeighborhood {
	private:
		std::vector<int> m_workerRndIds;
		std::vector<int> m_firmRndIds;

		void AddAgentInternal(int agentId, std::vector<int>& agentList, const std::string& agentTypeString);
		bool RemoveAgentInternal(int agentId, std::vector<int>& agentList);
		bool ContainsAgentInternal(int agentId, const std::vector<int>& agentList) const;

		std::vector<int> GetMaxNeighborsAcrossLists(
			int actingAgentId,
			const std::vector<int>& sourceActingAgentList,
			int maxNeighborsParam,
			const std::vector<int>& targetNeighborList
		) const;

	public:
		CircularNeighborhood();

		void Clear(); // Added

		void AddWorker(int agentId);
		bool RemoveWorker(int agentId);
		bool IsWorkersEmpty() const;
		size_t GetWorkersSize() const;
		std::vector<int> GetOrderedWorkers() const;
		bool ContainsWorker(int agentId) const;

		// MODIFIED: Now takes const Agent* (expected to be a Firm)
		std::vector<int> GetMaxWorkerNeighbors(const Agent* callingFirm, int maxNeighborsParam) const;

		void AddFirm(int agentId);
		bool RemoveFirm(int agentId);
		bool IsFirmsEmpty() const;
		size_t GetFirmsSize() const;
		std::vector<int> GetOrderedFirms() const;
		bool ContainsFirm(int agentId) const;

		// MODIFIED: Now takes const Agent* (can be Worker or Firm)
		std::vector<int> GetMaxFirmNeighbors(const Agent* callingAgent, int maxNeighborsParam) const;
	};

} // namespace Region
