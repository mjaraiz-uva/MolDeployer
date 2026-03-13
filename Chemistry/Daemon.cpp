#include "Daemon.h"
#include "Atom.h"
#include "Bond.h"
#include "Molecule.h"

namespace Chemistry {

nlohmann::json DaemonState::Save() const {
	nlohmann::json j;
	j["targetFormula"] = recipe ? recipe->targetFormula : "";
	j["componentA"] = recipe ? recipe->componentA : "";
	j["componentB"] = recipe ? recipe->componentB : "";
	j["assignedStep"] = assignedStep;
	j["timeoutSteps"] = timeoutSteps;
	j["successCount"] = successCount;
	j["heldBondId"] = heldBond ? heldBond->id : -1;
	return j;
}

std::string GetEntityFormula(const Atom* atom) {
	if (!atom) return "";
	if (atom->IsFree()) {
		return ElementSymbol(atom->GetElement());
	}
	Molecule* mol = atom->GetMolecule();
	if (mol) {
		return mol->GetMolecularFormula();
	}
	return ElementSymbol(atom->GetElement());
}

Atom* FindBondableAtomInEntity(Atom* anchor) {
	if (!anchor) return nullptr;
	if (anchor->CanBond()) return anchor;

	Molecule* mol = anchor->GetMolecule();
	if (mol) {
		for (Atom* a : mol->GetAtoms()) {
			if (a->CanBond()) return a;
		}
	}
	return nullptr;
}

} // namespace Chemistry
