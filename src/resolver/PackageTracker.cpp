#include "../../include/resolver/PackageTracker.hpp"
#include "../../include/Logger.hpp"
#include "algorithm"

using namespace Slipper::Resolver;

void PackageTracker::addPkg(const std::string& cpv) {
    Logger::logDebug("PackageTracker::addPkg", "Tracking package selection: " + cpv);
    Slipper::Dep::Atom atom(cpv);
    std::string cp = atom.getCp();
    
    auto& cpv_list = cp_map[cp];
    if (std::find(cpv_list.begin(), cpv_list.end(), cpv) == cpv_list.end()) {
        cpv_list.push_back(cpv);
    } else {
        Logger::logDebug("PackageTracker::addPkg", "Package already tracked: " + cpv);
    }
}

void PackageTracker::removePkg(const std::string& cpv) {
    Logger::logDebug("PackageTracker::removePkg", "Removing tracked package: " + cpv);
    Slipper::Dep::Atom atom(cpv);
    std::string cp = atom.getCp();
    
    auto it = cp_map.find(cp);
    if (it != cp_map.end()) {
        auto& cpv_list = it->second;
        cpv_list.erase(std::remove(cpv_list.begin(), cpv_list.end(), cpv), cpv_list.end());
        
        if (cpv_list.empty()) {
            cp_map.erase(it);
        }
    }
}

bool PackageTracker::contains(const std::string& cpv) const {
    Slipper::Dep::Atom atom(cpv);
    std::string cp = atom.getCp();
    
    auto it = cp_map.find(cp);
    if (it != cp_map.end()) {
        const auto& cpv_list = it->second;
        return std::find(cpv_list.begin(), cpv_list.end(), cpv) != cpv_list.end();
    }
    return false;
}

std::vector< std::string > PackageTracker::match(const Slipper::Dep::Atom& atom) const {
    Logger::logDebug("PackageTracker::match", "Looking for tracked packages matching: " + atom.getRawString());
    std::vector< std::string > results;
    std::string cp = atom.getCp();
    
    auto it = cp_map.find(cp);
    if (it != cp_map.end()) {
        for (const auto& tracked_cpv : it->second) {
            results.push_back(tracked_cpv);
            Logger::logDebug("PackageTracker::match", "Found match in tracker: " + tracked_cpv);
        }
    } else {
        Logger::logDebug("PackageTracker::match", "No tracked packages match CP: " + cp);
    }
    
    return results;
}

// NEW: Flatten the unordered map into a single vector of CPVs
std::vector< std::string > PackageTracker::getAllPackages() const {
    std::vector< std::string > all_packages;
    for (const auto& pair : cp_map) {
        for (const auto& cpv : pair.second) {
            all_packages.push_back(cpv);
        }
    }
    return all_packages;
}