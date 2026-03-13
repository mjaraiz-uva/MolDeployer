#include "Recipe.h"
#include "ChemistryConfig.h"
#include <algorithm>

namespace Chemistry {

const std::vector<Recipe> RecipeBook::s_emptyRecipes;
const std::vector<const Recipe*> RecipeBook::s_emptyRecipePtrs;

RecipeBook& RecipeBook::GetInstance() {
	static RecipeBook instance;
	return instance;
}

RecipeBook::RecipeBook() {
	InitDefaultRecipes();
}

void RecipeBook::InitDefaultRecipes() {
	// Helper to add a recipe
	auto add = [this](const std::string& target, const std::string& a, const std::string& b, double energy) {
		Recipe r{ target, a, b, energy };
		m_recipesByTarget[target].push_back(r);
		m_allRecipes.push_back(r);
	};

	// Bond energies from ChemistryConfig (kJ/mol):
	// C-H=411, O-H=459, C-O=358, C=O=799, H-H=436, C-C=346, O=O=498

	// --- Diatomic molecules ---
	add("H2", "H", "H", 436.0);        // H + H -> H2
	add("HO", "O", "H", 459.0);        // O + H -> OH
	add("CO", "C", "O", 358.0);        // C + O -> CO (single bond)

	// --- Triatomic molecules ---
	add("H2O", "HO", "H", 459.0);      // OH + H -> H2O
	add("H2O", "O", "H2", 459.0);      // O + H2 -> H2O (alternative)

	add("CO2", "CO", "O", 799.0);      // CO + O -> CO2 (forms C=O double bond)

	// --- Methane synthesis chain ---
	add("CH", "C", "H", 411.0);        // C + H -> CH
	add("CH2", "CH", "H", 411.0);      // CH + H -> CH2
	add("CH3", "CH2", "H", 411.0);     // CH2 + H -> CH3
	add("CH4", "CH3", "H", 411.0);     // CH3 + H -> CH4

	// --- Carbon-oxygen-hydrogen ---
	add("CHO", "CO", "H", 411.0);      // CO + H -> CHO (formyl)
	add("CH2O", "CHO", "H", 411.0);    // CHO + H -> CH2O (formaldehyde)
	add("CHO2", "CO", "HO", 358.0);    // CO + OH -> COOH (formic acid)

	// --- C-C bond formation: hydrocarbon chains ---
	// C2 species
	add("C2", "C", "C", 346.0);        // C + C -> C2
	add("C2H", "C2", "H", 411.0);      // C2 + H -> C2H
	add("C2H", "C", "CH", 346.0);      // C + CH -> C2H (C-C bond)
	add("C2H2", "C2H", "H", 411.0);    // C2H + H -> C2H2 (acetylene)
	add("C2H2", "CH", "CH", 346.0);    // CH + CH -> C2H2
	add("C2H3", "C2H2", "H", 411.0);   // C2H2 + H -> C2H3 (vinyl)
	add("C2H3", "CH2", "CH", 346.0);   // CH2 + CH -> C2H3
	add("C2H4", "C2H3", "H", 411.0);   // C2H3 + H -> C2H4 (ethylene)
	add("C2H4", "CH2", "CH2", 346.0);  // CH2 + CH2 -> C2H4
	add("C2H5", "C2H4", "H", 411.0);   // C2H4 + H -> C2H5 (ethyl)
	add("C2H5", "CH2", "CH3", 346.0);  // CH2 + CH3 -> C2H5
	add("C2H6", "C2H5", "H", 411.0);   // C2H5 + H -> C2H6 (ethane)
	add("C2H6", "CH3", "CH3", 346.0);  // CH3 + CH3 -> C2H6

	// C3 species (propyl chain growth)
	add("C3H4", "C2H2", "CH2", 346.0); // C2H2 + CH2 -> C3H4 (allene/propyne)
	add("C3H5", "C3H4", "H", 411.0);   // C3H4 + H -> C3H5 (allyl)
	add("C3H5", "C2H3", "CH2", 346.0); // C2H3 + CH2 -> C3H5
	add("C3H5", "C2H2", "CH3", 346.0); // C2H2 + CH3 -> C3H5
	add("C3H6", "C3H5", "H", 411.0);   // C3H5 + H -> C3H6 (propylene)
	add("C3H6", "C2H4", "CH2", 346.0); // C2H4 + CH2 -> C3H6
	add("C3H6", "C2H3", "CH3", 346.0); // C2H3 + CH3 -> C3H6
	add("C3H7", "C3H6", "H", 411.0);   // C3H6 + H -> C3H7 (propyl)
	add("C3H7", "C2H5", "CH2", 346.0); // C2H5 + CH2 -> C3H7
	add("C3H7", "C2H4", "CH3", 346.0); // C2H4 + CH3 -> C3H7
	add("C3H8", "C3H7", "H", 411.0);   // C3H7 + H -> C3H8 (propane)
	add("C3H8", "C2H5", "CH3", 346.0); // C2H5 + CH3 -> C3H8

	// C4 species (butyl chain growth)
	add("C4H6", "C2H3", "C2H3", 346.0);  // C2H3 + C2H3 -> C4H6
	add("C4H6", "C3H4", "CH2", 346.0);   // C3H4 + CH2 -> C4H6
	add("C4H8", "C2H4", "C2H4", 346.0);  // C2H4 + C2H4 -> C4H8
	add("C4H8", "C3H6", "CH2", 346.0);   // C3H6 + CH2 -> C4H8
	add("C4H8", "C3H5", "CH3", 346.0);   // C3H5 + CH3 -> C4H8
	add("C4H8", "C2H3", "C2H5", 346.0);  // C2H3 + C2H5 -> C4H8
	add("C4H10", "C2H5", "C2H5", 346.0); // C2H5 + C2H5 -> C4H10 (butane)
	add("C4H10", "C3H7", "CH3", 346.0);  // C3H7 + CH3 -> C4H10

	// C5 species (pentyl chain growth)
	add("C5H10", "C3H6", "C2H4", 346.0); // C3H6 + C2H4 -> C5H10
	add("C5H10", "C4H8", "CH2", 346.0);  // C4H8 + CH2 -> C5H10
	add("C5H10", "C3H5", "C2H5", 346.0); // C3H5 + C2H5 -> C5H10
	add("C5H12", "C3H7", "C2H5", 346.0); // C3H7 + C2H5 -> C5H12 (pentane)

	// Thermodynamic rearrangement paths (saturated + unsaturated → longer chain)
	// These work via MakeRoomForBond: break a weak C-H to form a stronger C-C
	add("C5H12", "C4H10", "CH2", 346.0);   // butane + CH2 -> pentane (rearrangement)
	add("C5H10", "C4H8", "CH2", 346.0);    // C4H8 + CH2 -> C5H10
	add("C5H10", "C3H6", "C2H4", 346.0);   // C3H6 + C2H4 -> C5H10
	add("C5H10", "C3H5", "C2H5", 346.0);   // C3H5 + C2H5 -> C5H10

	// C6 species (hexyl — potential for cyclohexane!)
	add("C6H12", "C3H6", "C3H6", 346.0);   // C3H6 + C3H6 -> C6H12 (cyclohexane possible!)
	add("C6H12", "C4H8", "C2H4", 346.0);   // C4H8 + C2H4 -> C6H12
	add("C6H12", "C5H10", "CH2", 346.0);   // C5H10 + CH2 -> C6H12
	add("C6H14", "C3H7", "C3H7", 346.0);   // C3H7 + C3H7 -> C6H14 (hexane)
	add("C6H14", "C5H12", "CH2", 346.0);   // pentane + CH2 -> hexane (rearrangement)
	add("C6H14", "C4H10", "C2H4", 346.0);  // butane + ethylene (rearrangement)

	// C7-C8 species (longer chains)
	add("C7H14", "C4H8", "C3H6", 346.0);   // C4H8 + C3H6 -> C7H14
	add("C7H14", "C6H12", "CH2", 346.0);   // C6H12 + CH2 -> C7H14
	add("C7H16", "C4H10", "C3H6", 346.0);  // butane + propylene (rearrangement)
	add("C7H16", "C5H12", "C2H4", 346.0);  // pentane + ethylene (rearrangement)
	add("C8H16", "C4H8", "C4H8", 346.0);   // C4H8 + C4H8 -> C8H16
	add("C8H16", "C6H12", "C2H4", 346.0);  // C6H12 + C2H4 -> C8H16
	add("C8H18", "C4H10", "C4H8", 346.0);  // butane + butylene (rearrangement)
	add("C8H18", "C6H14", "C2H4", 346.0);  // hexane + ethylene (rearrangement)

	// C9-C10 species (approaching larger molecules)
	add("C9H18", "C6H12", "C3H6", 346.0);  // hexene + propylene
	add("C9H18", "C8H16", "CH2", 346.0);   // octene + CH2
	add("C10H20", "C6H12", "C4H8", 346.0); // hexene + butylene
	add("C10H20", "C5H10", "C5H10", 346.0);// pentene + pentene
	add("C10H22", "C5H12", "C5H10", 346.0);// pentane + pentene (rearrangement)
	add("C10H22", "C6H14", "C4H8", 346.0); // hexane + butylene (rearrangement)

	// C12 (dodecane range - serious hydrocarbon!)
	add("C12H24", "C6H12", "C6H12", 346.0);// hexene + hexene
	add("C12H26", "C6H14", "C6H12", 346.0);// hexane + hexene (rearrangement)

	// Build sorted target list
	m_allTargets.clear();
	for (const auto& [target, _] : m_recipesByTarget) {
		m_allTargets.push_back(target);
	}
	std::sort(m_allTargets.begin(), m_allTargets.end());

	// Build reverse index: component formula -> recipes that use it as input
	m_recipesByComponent.clear();
	for (const auto& recipe : m_allRecipes) {
		m_recipesByComponent[recipe.componentA].push_back(&recipe);
		if (recipe.componentB != recipe.componentA) {
			m_recipesByComponent[recipe.componentB].push_back(&recipe);
		}
	}
}

const std::vector<Recipe>& RecipeBook::GetRecipesFor(const std::string& targetFormula) const {
	auto it = m_recipesByTarget.find(targetFormula);
	if (it != m_recipesByTarget.end()) {
		return it->second;
	}
	return s_emptyRecipes;
}

const std::vector<const Recipe*>& RecipeBook::GetRecipesUsing(const std::string& formula) const {
	auto it = m_recipesByComponent.find(formula);
	if (it != m_recipesByComponent.end()) {
		return it->second;
	}
	return s_emptyRecipePtrs;
}

const Recipe& RecipeBook::GetRandomRecipe(std::mt19937& rng) const {
	std::uniform_int_distribution<size_t> dist(0, m_allRecipes.size() - 1);
	return m_allRecipes[dist(rng)];
}

} // namespace Chemistry
