#include <random>    // For std::uniform_int_distribution (still needed for local distributions)
#include <algorithm> // For std::find, std::remove, std::min, std::distance
#include <vector>
#include <string>    // For logging agent type string
#include "CircularNeighborhood.h"
#include "Agent.h"   // Required for Agent type
#include "Worker.h"  // Required for dynamic_cast to Worker
#include "Firm.h"    // Required for dynamic_cast to Firm
#include "../Logger/Logger.h" // Assuming Logger is a custom logging utility
#include "../DataManager/DataManager.h" // Added for global RNG

namespace Region {

    CircularNeighborhood::CircularNeighborhood() {
        // Constructor no longer seeds m_rng as it's removed.
        // Global RNG is initialized and managed by DataManager.
    }

    void CircularNeighborhood::Clear() {
        m_workerRndIds.clear();
        m_firmRndIds.clear();
        Logger::Info("CircularNeighborhood::Clear: Cleared worker and firm ID lists.");
    }

    // Private helper to add an agent to a specific list
    void CircularNeighborhood::AddAgentInternal(int agentId, std::vector<int>& agentList, const std::string& agentTypeString) {
        if (ContainsAgentInternal(agentId, agentList)) {
            Logger::Warning("CircularNeighborhood::Add" + agentTypeString + ": " + agentTypeString + " ID "
                + std::to_string(agentId) + " already exists in its list.");
            return;
        }

        if (agentList.empty()) {
            agentList.push_back(agentId);
        }
        else {
            // Corrected distribution: max is inclusive for vector indexing (0 to size-1)
            // but for insert, it can be agentList.size() to append.
            std::uniform_int_distribution<size_t> dist(0, agentList.size());
            // Note: DataManager::GetGlobalRandomEngine() returns a reference.
            // If multiple operations on the RNG need to be atomic,
            // the caller (or a higher-level function) should lock DataManager::g_rng_mutex.
            size_t insert_pos = dist(DataManager::GetGlobalRandomEngine()); 
            agentList.insert(agentList.begin() + insert_pos, agentId);
        }
    }

    // Private helper to remove an agent from a specific list
    bool CircularNeighborhood::RemoveAgentInternal(int agentId, std::vector<int>& agentList) {
        auto it = std::find(agentList.begin(), agentList.end(), agentId);
        if (it != agentList.end()) {
            agentList.erase(it);
            return true;
        }
        return false;
    }

    // Private helper to check if an agent is in a specific list
    bool CircularNeighborhood::ContainsAgentInternal(int agentId, const std::vector<int>& agentList) const {
        return std::find(agentList.begin(), agentList.end(), agentId) != agentList.end();
    }

    // --- Worker Methods ---
    void CircularNeighborhood::AddWorker(int workerId) {
        AddAgentInternal(workerId, m_workerRndIds, "Worker");
    }

    bool CircularNeighborhood::RemoveWorker(int workerId) {
        return RemoveAgentInternal(workerId, m_workerRndIds);
    }

    bool CircularNeighborhood::IsWorkersEmpty() const {
        return m_workerRndIds.empty();
    }

    size_t CircularNeighborhood::GetWorkersSize() const {
        return m_workerRndIds.size();
    }

    std::vector<int> CircularNeighborhood::GetOrderedWorkers() const {
        return m_workerRndIds; // Returns a copy
    }

    bool CircularNeighborhood::ContainsWorker(int workerId) const {
        return ContainsAgentInternal(workerId, m_workerRndIds);
    }

    // --- Firm Methods ---
    void CircularNeighborhood::AddFirm(int firmId) {
        AddAgentInternal(firmId, m_firmRndIds, "Firm");
    }

    bool CircularNeighborhood::RemoveFirm(int firmId) {
        return RemoveAgentInternal(firmId, m_firmRndIds);
    }

    bool CircularNeighborhood::IsFirmsEmpty() const {
        return m_firmRndIds.empty();
    }

    size_t CircularNeighborhood::GetFirmsSize() const {
        return m_firmRndIds.size();
    }

    std::vector<int> CircularNeighborhood::GetOrderedFirms() const {
        return m_firmRndIds; // Returns a copy
    }

    bool CircularNeighborhood::ContainsFirm(int firmId) const {
        return ContainsAgentInternal(firmId, m_firmRndIds);
    }

    // Private Helper Method for GetMaxNeighbors logic across different lists
    std::vector<int> CircularNeighborhood::GetMaxNeighborsAcrossLists(
        int actingAgentId,
        const std::vector<int>& sourceActingAgentList,
        int maxNeighborsParam,
        const std::vector<int>& targetNeighborList
    ) const {
        std::vector<int> neighbors;
        size_t Ns = sourceActingAgentList.size(); // Size of the source list
        size_t Nt = targetNeighborList.size();   // Size of the target list

        if (Ns == 0 || Nt == 0 || maxNeighborsParam <= 0) {
            return neighbors;
        }

        auto it_acting = std::find(sourceActingAgentList.begin(), sourceActingAgentList.end(), actingAgentId);
        if (it_acting == sourceActingAgentList.end()) {
            Logger::Warning("CircularNeighborhood::GetMaxNeighborsAcrossLists: actingAgentId " +
                std::to_string(actingAgentId) + " not found in its source list.");
            return neighbors;
        }
        size_t acting_agent_idx_in_source = std::distance(sourceActingAgentList.begin(), it_acting);

        // Calculate the proportional "center" index in the target list
        // Ensure Ns > 0 before division to prevent division by zero.
        double proportion = (Ns > 0) ? (static_cast<double>(acting_agent_idx_in_source) / static_cast<double>(Ns)) : 0.0;
        size_t target_center_idx = static_cast<size_t>(proportion * static_cast<double>(Nt));

        // Ensure target_center_idx is within bounds [0, Nt-1] for targetNeighborList
        // This also handles Nt=1 case where target_center_idx will be 0.
        // Check Nt > 0 before modulo to prevent division by zero if Nt is 0 (though caught by initial check).
        if (Nt > 0) {
            target_center_idx = target_center_idx % Nt;
        }
        else { // Should be caught by Nt == 0 earlier, but as a safeguard
            return neighbors;
        }

        size_t num_to_fetch = std::min(static_cast<size_t>(maxNeighborsParam), Nt);

        if (num_to_fetch == 0) {
            return neighbors;
        }

        neighbors.reserve(num_to_fetch);

        for (size_t d = 0; neighbors.size() < num_to_fetch; ++d) {
            if (d == 0) { // Element at the target_center_idx (distance 0)
                if (std::find(neighbors.begin(), neighbors.end(), targetNeighborList[target_center_idx]) == neighbors.end()) {
                    neighbors.push_back(targetNeighborList[target_center_idx]);
                    if (neighbors.size() == num_to_fetch) break;
                }
                if (Nt == 1) break; // If target list has only one element, we've got it.
                continue; // Proceed to d=1 for other neighbors
            }

            // Candidate 1: Left neighbor at distance d
            size_t left_idx = (target_center_idx - d + Nt * d) % Nt; // Nt*d ensures positive result before modulo
            if (std::find(neighbors.begin(), neighbors.end(), targetNeighborList[left_idx]) == neighbors.end()) {
                neighbors.push_back(targetNeighborList[left_idx]);
                if (neighbors.size() == num_to_fetch) break;
            }

            // Candidate 2: Right neighbor at distance d
            // This is only added if it's different from the left_idx (can be same if d = Nt/2 for even Nt)
            size_t right_idx = (target_center_idx + d) % Nt;
            if (right_idx != left_idx) { // Avoid double-adding the same element
                if (std::find(neighbors.begin(), neighbors.end(), targetNeighborList[right_idx]) == neighbors.end()) {
                    neighbors.push_back(targetNeighborList[right_idx]);
                    if (neighbors.size() == num_to_fetch) break;
                }
            }

            // Optimization/Safety break:
            if (Nt > 1 && d >= (Nt / 2)) { // Check d only if Nt > 1
                // If d has iterated roughly half the circle, all unique neighbors should have been considered.
                if (neighbors.size() >= Nt) break; // All agents from target list have been added.
            }
        }
        return neighbors;
    }

    // MODIFIED Public Method: Gets Worker neighbors, centered based on callingFirm's position in the firm list
    std::vector<int> CircularNeighborhood::GetMaxWorkerNeighbors(const Agent* callingFirmAgent, int maxNeighborsParam) const {
        if (!callingFirmAgent) {
            Logger::Error("CircularNeighborhood::GetMaxWorkerNeighbors: callingFirmAgent is null.");
            return {}; // Return empty vector
        }

        // This method is for Firms seeking Workers.
        // Ensure callingFirmAgent is actually a Firm for type safety and logical correctness.
        const Firm* firm = dynamic_cast<const Firm*>(callingFirmAgent);
        if (!firm) {
            Logger::Warning("CircularNeighborhood::GetMaxWorkerNeighbors: callingFirmAgent (ID: " +
                std::to_string(callingFirmAgent->GetID()) + ") is not a Firm type. Cannot find worker neighbors based on its position.");
            return {}; // Return empty vector
        }

        return GetMaxNeighborsAcrossLists(firm->GetID(), m_firmRndIds, maxNeighborsParam, m_workerRndIds);
    }

    // MODIFIED Public Method: Gets Firm neighbors, centered based on callingAgent's position (Worker or Firm)
    std::vector<int> CircularNeighborhood::GetMaxFirmNeighbors(const Agent* callingAgent, int maxNeighborsParam) const {
        if (!callingAgent) {
            Logger::Error("CircularNeighborhood::GetMaxFirmNeighbors: callingAgent is null.");
            return {}; // Return empty vector
        }

        int agentId = callingAgent->GetID();
        const std::vector<int>* sourceList = nullptr;

        // Determine the source list based on the dynamic type of callingAgent
        if (dynamic_cast<const Worker*>(callingAgent)) {
            sourceList = &m_workerRndIds;
        }
        else if (dynamic_cast<const Firm*>(callingAgent)) {
            sourceList = &m_firmRndIds;
        }
        else {
            // This case should ideally not be reached if Agent is an abstract base class
            // or if only Workers and Firms are expected here.
            Logger::Error("CircularNeighborhood::GetMaxFirmNeighbors: callingAgent (ID: " +
                std::to_string(agentId) + ") is of an unknown or unexpected concrete type.");
            return {}; // Return empty vector
        }

        // The sourceList pointer should be valid if one of the dynamic_casts succeeded.
        // No explicit null check for sourceList needed here due to logic flow.

        return GetMaxNeighborsAcrossLists(agentId, *sourceList, maxNeighborsParam, m_firmRndIds);
    }

} // namespace Region
