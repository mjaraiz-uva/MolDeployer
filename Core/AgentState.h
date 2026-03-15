#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace DABM {

struct Recipe;
struct Bond;

// AgentState: the daemon/goal state riding on every Agent.
// Replaces Chemistry::DaemonState with domain-agnostic naming.
//
// SEARCHING (heldBond == nullptr): agent seeks complement to produce.
// HOLDING   (heldBond != nullptr): agent maintains a production bond. On timeout, bond breaks.
//
// This is the RAA (Rational Activity Algorithm) state machine from Jaraiz 2020.
struct AgentState {
    const Recipe* recipe = nullptr;
    int assignedStep = 0;
    int timeoutSteps = 500;
    int successCount = 0;
    Bond* heldBond = nullptr;   // null = SEARCHING, non-null = HOLDING

    bool IsSearching() const { return heldBond == nullptr; }
    bool IsHolding() const { return heldBond != nullptr; }
    bool HasTimedOut(int currentStep) const {
        return (currentStep - assignedStep) >= timeoutSteps;
    }

    nlohmann::json Save() const;
};

} // namespace DABM
