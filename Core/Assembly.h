#pragma once

#include <vector>
#include <string>

namespace DABM {

class Agent;
struct Bond;

// An Assembly is a connected component of bonded Agents.
// Chemistry: Molecule (set of bonded atoms).
// Economics: Firm (set of cooperating workers/producers).
// Robotics: Assembled structure (completed sub-assembly).
class Assembly {
public:
    Assembly();
    explicit Assembly(int id);
    ~Assembly();

    int GetID() const { return m_id; }

    // Agent membership
    const std::vector<Agent*>& GetAgents() const { return m_agents; }
    void AddAgent(Agent* agent);
    void RemoveAgent(Agent* agent);
    size_t GetAgentCount() const { return m_agents.size(); }

    // Bond membership
    const std::vector<Bond*>& GetBonds() const { return m_bonds; }
    void AddBond(Bond* bond);
    void RemoveBond(Bond* bond);

    // Computed properties (domain-specific formula computed via IDomainRules)
    const std::string& GetFormula() const { return m_formula; }
    void SetFormula(const std::string& formula) { m_formula = formula; }
    double GetTotalEnergy() const { return m_totalEnergy; }
    void SetTotalEnergy(double e) { m_totalEnergy = e; }

    // Lifecycle
    int GetFormationStep() const { return m_formationStep; }
    void SetFormationStep(int step) { m_formationStep = step; }
    int GetDestructionStep() const { return m_destructionStep; }
    void SetDestructionStep(int step) { m_destructionStep = step; }
    bool IsAlive() const { return m_destructionStep < 0; }

private:
    int m_id = -1;
    std::vector<Agent*> m_agents;
    std::vector<Bond*> m_bonds;
    std::string m_formula;
    double m_totalEnergy = 0.0;
    int m_formationStep = -1;
    int m_destructionStep = -1;
};

} // namespace DABM
