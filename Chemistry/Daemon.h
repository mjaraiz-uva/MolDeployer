#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "Recipe.h"

namespace Chemistry {

// Forward declarations
class Atom;
struct Bond;

// DaemonState: lightweight agent attached to every Atom.
// Replaces the old free-flying Daemon class. Daemons now "ride" atoms.
//
// SEARCHING (heldBond == nullptr): atom looks for complement to form a bond.
// HOLDING   (heldBond != nullptr): daemon holds a bond alive. On timeout, bond breaks.
struct DaemonState {
	const Recipe* recipe = nullptr;
	int assignedStep = 0;
	int timeoutSteps = 500;
	int successCount = 0;
	Bond* heldBond = nullptr;       // null = SEARCHING, non-null = HOLDING

	bool IsSearching() const { return heldBond == nullptr; }
	bool IsHolding() const { return heldBond != nullptr; }
	bool HasTimedOut(int currentStep) const {
		return (currentStep - assignedStep) >= timeoutSteps;
	}

	// Snapshot persistence
	nlohmann::json Save() const;
};

// Free helper functions used by Reactor during daemon processing
std::string GetEntityFormula(const Atom* atom);
Atom* FindBondableAtomInEntity(Atom* anchor);

} // namespace Chemistry
