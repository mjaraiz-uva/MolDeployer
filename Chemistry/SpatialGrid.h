#pragma once

#include <vector>
#include <utility>
#include "Vec3.h"

namespace Chemistry {

// Forward declaration
class Atom;

// 3D cell-list spatial hash for efficient neighbor detection.
// Replaces CircularNeighborhood from the economic model.
//
// The simulation box is divided into cubic cells of side length = cutoff radius.
// At each timestep, atoms are binned into cells, then neighbor pairs within
// the cutoff distance are found by checking the 27 neighboring cells (3x3x3).
class SpatialGrid {
public:
    SpatialGrid();

    // Configure the grid for a given box size and cutoff radius.
    void Configure(const Vec3& boxSize, double cutoffRadius);

    // Rebuild the grid from current atom positions. O(N).
    void Build(const std::vector<std::unique_ptr<Atom>>& atoms);

    // Get all pairs of atom indices within the cutoff radius.
    // Returns vector of (atomIndex1, atomIndex2) with index1 < index2.
    std::vector<std::pair<int, int>> GetNeighborPairs() const;

    // Get neighbors of a specific atom within cutoff.
    std::vector<int> GetNeighbors(int atomIndex) const;

    double GetCutoffRadius() const { return m_cutoffRadius; }

private:
    // Convert a 3D cell coordinate to a flat index
    int CellIndex(int cx, int cy, int cz) const;

    // Get the cell coordinates for a position
    void PositionToCell(const Vec3& pos, int& cx, int& cy, int& cz) const;

    Vec3 m_boxSize;
    double m_cutoffRadius = 3.0;
    int m_nx = 0, m_ny = 0, m_nz = 0;  // Number of cells in each dimension
    int m_totalCells = 0;

    // Each cell contains a list of atom indices (into the Reactor's atom vector)
    std::vector<std::vector<int>> m_cells;

    // Cached atom positions for neighbor distance checks
    std::vector<Vec3> m_atomPositions;
};

} // namespace Chemistry
