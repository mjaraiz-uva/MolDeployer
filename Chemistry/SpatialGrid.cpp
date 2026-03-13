#include "SpatialGrid.h"
#include "Atom.h"
#include <cmath>
#include <algorithm>
#include <memory>

namespace Chemistry {

SpatialGrid::SpatialGrid() = default;

void SpatialGrid::Configure(const Vec3& boxSize, double cutoffRadius) {
    m_boxSize = boxSize;
    m_cutoffRadius = cutoffRadius;

    // Number of cells in each dimension (at least 1)
    m_nx = std::max(1, static_cast<int>(std::floor(boxSize.x / cutoffRadius)));
    m_ny = std::max(1, static_cast<int>(std::floor(boxSize.y / cutoffRadius)));
    m_nz = std::max(1, static_cast<int>(std::floor(boxSize.z / cutoffRadius)));
    m_totalCells = m_nx * m_ny * m_nz;

    m_cells.resize(m_totalCells);
}

void SpatialGrid::Build(const std::vector<std::unique_ptr<Atom>>& atoms) {
    // Clear all cells
    for (auto& cell : m_cells) {
        cell.clear();
    }

    // Cache positions and bin atoms into cells
    m_atomPositions.resize(atoms.size());
    for (size_t i = 0; i < atoms.size(); ++i) {
        const Vec3& pos = atoms[i]->GetPosition();
        m_atomPositions[i] = pos;

        int cx, cy, cz;
        PositionToCell(pos, cx, cy, cz);
        m_cells[CellIndex(cx, cy, cz)].push_back(static_cast<int>(i));
    }
}

std::vector<std::pair<int, int>> SpatialGrid::GetNeighborPairs() const {
    std::vector<std::pair<int, int>> pairs;
    double cutoffSq = m_cutoffRadius * m_cutoffRadius;

    for (int cx = 0; cx < m_nx; ++cx) {
        for (int cy = 0; cy < m_ny; ++cy) {
            for (int cz = 0; cz < m_nz; ++cz) {
                int cellIdx = CellIndex(cx, cy, cz);
                const auto& cellAtoms = m_cells[cellIdx];

                // Check this cell against itself and 13 forward neighbors
                // (to avoid double-counting pairs)
                // Forward neighbors: half of the 26 neighbors + self
                for (int dx = 0; dx <= 1; ++dx) {
                    for (int dy = (dx == 0 ? 0 : -1); dy <= 1; ++dy) {
                        for (int dz = (dx == 0 && dy == 0 ? 0 : -1); dz <= 1; ++dz) {
                            int nx = cx + dx;
                            int ny = cy + dy;
                            int nz = cz + dz;

                            // Skip out-of-bounds cells (no periodic boundaries)
                            if (nx < 0 || nx >= m_nx ||
                                ny < 0 || ny >= m_ny ||
                                nz < 0 || nz >= m_nz) {
                                continue;
                            }

                            int neighborIdx = CellIndex(nx, ny, nz);
                            const auto& neighborAtoms = m_cells[neighborIdx];

                            if (cellIdx == neighborIdx) {
                                // Same cell: check all unique pairs within
                                for (size_t i = 0; i < cellAtoms.size(); ++i) {
                                    for (size_t j = i + 1; j < cellAtoms.size(); ++j) {
                                        Vec3 diff = m_atomPositions[cellAtoms[i]] - m_atomPositions[cellAtoms[j]];
                                        if (diff.lengthSq() <= cutoffSq) {
                                            pairs.emplace_back(cellAtoms[i], cellAtoms[j]);
                                        }
                                    }
                                }
                            }
                            else {
                                // Different cells: check all cross-pairs
                                for (int ai : cellAtoms) {
                                    for (int bi : neighborAtoms) {
                                        Vec3 diff = m_atomPositions[ai] - m_atomPositions[bi];
                                        if (diff.lengthSq() <= cutoffSq) {
                                            int lo = std::min(ai, bi);
                                            int hi = std::max(ai, bi);
                                            pairs.emplace_back(lo, hi);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return pairs;
}

std::vector<int> SpatialGrid::GetNeighbors(int atomIndex) const {
    std::vector<int> neighbors;
    double cutoffSq = m_cutoffRadius * m_cutoffRadius;
    const Vec3& pos = m_atomPositions[atomIndex];

    int cx, cy, cz;
    // Reconstruct cell from position
    cx = std::clamp(static_cast<int>(pos.x / m_cutoffRadius), 0, m_nx - 1);
    cy = std::clamp(static_cast<int>(pos.y / m_cutoffRadius), 0, m_ny - 1);
    cz = std::clamp(static_cast<int>(pos.z / m_cutoffRadius), 0, m_nz - 1);

    // Check all 27 neighboring cells
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                int nx = cx + dx;
                int ny = cy + dy;
                int nz = cz + dz;

                if (nx < 0 || nx >= m_nx ||
                    ny < 0 || ny >= m_ny ||
                    nz < 0 || nz >= m_nz) {
                    continue;
                }

                int cellIdx = CellIndex(nx, ny, nz);
                for (int otherIdx : m_cells[cellIdx]) {
                    if (otherIdx == atomIndex) continue;
                    Vec3 diff = pos - m_atomPositions[otherIdx];
                    if (diff.lengthSq() <= cutoffSq) {
                        neighbors.push_back(otherIdx);
                    }
                }
            }
        }
    }

    return neighbors;
}

int SpatialGrid::CellIndex(int cx, int cy, int cz) const {
    return cx * m_ny * m_nz + cy * m_nz + cz;
}

void SpatialGrid::PositionToCell(const Vec3& pos, int& cx, int& cy, int& cz) const {
    cx = std::clamp(static_cast<int>(pos.x / m_cutoffRadius), 0, m_nx - 1);
    cy = std::clamp(static_cast<int>(pos.y / m_cutoffRadius), 0, m_ny - 1);
    cz = std::clamp(static_cast<int>(pos.z / m_cutoffRadius), 0, m_nz - 1);
}

} // namespace Chemistry
