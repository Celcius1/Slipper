#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "unordered_set"
#include "PackageTracker.hpp"

namespace Slipper {
namespace Resolver {

// Represents a collision within the dependency graph
struct PackageConflict {
    std::string type;         // e.g., "slot conflict", "cpv conflict"
    std::string atom;         // The atom that caused the conflict
    std::vector< std::string > pkgs; // The specific CPVs involved
};

class SlotConflictHandler {
public:
    explicit SlotConflictHandler(const PackageTracker& tracker);

    // Identifies and catalogs all current slot conflicts in the tracker
    void evaluateConflicts();

    // Simulates the combinatorial generator to find USE changes that resolve the conflict
    bool generateSolutions();

    const std::vector< PackageConflict >& getConflicts() const { return conflicts; }

private:
    const PackageTracker& tracker;
    std::vector< PackageConflict > conflicts;
};

class CircularDependencyHandler {
public:
    CircularDependencyHandler() = default;

    // Analyzes a dependency graph (represented as adjacency lists) for cycles
    void detectCycles(const std::unordered_map< std::string, std::vector< std::string > >& graph);

    const std::vector< std::string >& getShortestCycle() const { return shortest_cycle; }

private:
    std::vector< std::string > shortest_cycle;
};

} // namespace Resolver
} // namespace Slipper