#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace DABM {

// A Species describes a type of agent in the simulation.
// Chemistry: Element (C, H, O) with valence and mass.
// Economics: Worker type with skills.
// Robotics: Actuator type with capabilities.
struct SpeciesInfo {
    int id = -1;
    std::string name;           // "C", "H", "O", "worker", "actuator_arm"
    int maxValence = 0;         // bonding capacity (4 for C, 1 for H)
    double mass = 1.0;          // for Brownian motion / kinetics
    std::unordered_map<std::string, double> properties;  // domain-specific extensible
};

// Registry of all known species in the simulation.
class SpeciesRegistry {
public:
    int RegisterSpecies(const std::string& name, int maxValence, double mass = 1.0) {
        int id = static_cast<int>(m_species.size());
        SpeciesInfo info;
        info.id = id;
        info.name = name;
        info.maxValence = maxValence;
        info.mass = mass;
        m_species.push_back(info);
        m_nameToId[name] = id;
        return id;
    }

    const SpeciesInfo& GetSpecies(int id) const { return m_species.at(id); }

    int GetId(const std::string& name) const {
        auto it = m_nameToId.find(name);
        if (it == m_nameToId.end()) return -1;
        return it->second;
    }

    size_t Count() const { return m_species.size(); }
    const std::vector<SpeciesInfo>& GetAll() const { return m_species; }

private:
    std::vector<SpeciesInfo> m_species;
    std::unordered_map<std::string, int> m_nameToId;
};

} // namespace DABM
