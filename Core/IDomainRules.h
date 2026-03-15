#pragma once

#include <vector>
#include <string>

namespace DABM {

class Agent;
struct Bond;
class Environment;

// IDomainRules: virtual interface for domain-specific physics/rules.
// The Environment calls these methods to delegate domain-specific decisions.
//
// Chemistry: bond energies, Arrhenius kinetics, radiation, Hill formula.
// Economics: production costs, market pricing, trade rules.
// Robotics: task sequencing constraints, execution times.
class IDomainRules {
public:
    virtual ~IDomainRules() = default;

    // Can these two agents form a bond? (valence check, compatibility, etc.)
    virtual bool CanFormBond(const Agent* a, const Agent* b) const = 0;

    // Probability of forming a bond between these agents this timestep
    virtual double FormationProbability(const Agent* a, const Agent* b, double temperature) const = 0;

    // Probability of thermally breaking this bond this timestep
    virtual double BreakingProbability(const Bond* bond, double temperature) const = 0;

    // Probability of radiation/external-forcing breaking this bond
    virtual double RadiationBreakingProbability(const Bond* bond, double flux, double maxEnergy) const = 0;

    // Compute bond energy for a new bond between these agents
    virtual double ComputeBondEnergy(const Agent* a, const Agent* b, int order) const = 0;

    // Compute formula/identifier for a set of agents (Hill system, economic name, etc.)
    virtual std::string ComputeFormula(const std::vector<Agent*>& agents) const = 0;

    // Domain-specific initialization (register species, create recipes, form initial assemblies)
    virtual void Initialize(Environment& env) = 0;

    // Per-step hooks for domain-specific processing
    virtual void OnStepBegin(Environment& env, int step) = 0;
    virtual void OnStepEnd(Environment& env, int step) = 0;
};

} // namespace DABM
