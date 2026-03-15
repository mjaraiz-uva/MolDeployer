#pragma once

namespace DABM {

class Agent;

// A Bond connects two Agents. Owned by Environment; Agents hold non-owning pointers.
// Chemistry: covalent bond with order and energy.
// Economics: trade relationship or contract.
// Robotics: mechanical connection between assembled parts.
struct Bond {
    int id = -1;
    Agent* agentA = nullptr;
    Agent* agentB = nullptr;
    int order = 1;              // bond order (1=single, 2=double, 3=triple)
    double energy = 0.0;        // dissociation energy / value
    int formationStep = -1;
    bool markedForRemoval = false;
    Agent* daemonHolder = nullptr;  // which agent's daemon holds this bond

    Agent* GetOther(const Agent* a) const {
        return (a == agentA) ? agentB : agentA;
    }
};

} // namespace DABM
