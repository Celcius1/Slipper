#include "../../include/config/UseManager.hpp"
#include "../../include/Logger.hpp"

using namespace Slipper::Config;

void UseManager::addPackageUse(const std::string& atom_str, const std::vector< std::string >& flags) {
    Logger::logDebug("UseManager::addPackageUse", "Adding package.use entry for: " + atom_str);
    
    // Parse the incoming config string into a full Atom object
    Slipper::Dep::Atom config_atom(atom_str);
    std::string cp = config_atom.getCp();
    
    puse_dict[cp].push_back({config_atom, flags});
    Logger::logDebug("UseManager::addPackageUse", "Stored entry under CP: " + cp + " with " + std::to_string(flags.size()) + " flags.");
}

std::unordered_set< std::string > UseManager::getPUSE(const Slipper::Dep::Atom& pkg) const {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("UseManager::getPUSE", "Resolving final USE flags for: " + pkg.getRawString());
    }
    
    std::unordered_set< std::string > resolved_flags;
    std::string cp = pkg.getCp();
    
    // 1. Apply global USE flags from make.conf (*/*)
    auto global_it = puse_dict.find("*/*");
    if (global_it != puse_dict.end()) {
        if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Applying global */* USE flags.");
        for (const auto& entry : global_it->second) {
            for (const auto& flag : entry.second) {
                if (!flag.empty() && flag[0] == '-') {
                    std::string positive_flag = flag.substr(1);
                    resolved_flags.erase(positive_flag);
                    if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Removed global flag: " + positive_flag);
                } else {
                    resolved_flags.insert(flag);
                    if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Added global flag: " + flag);
                }
            }
        }
    }

    // 2. Apply package-specific USE flags
    auto it = puse_dict.find(cp);
    if (it != puse_dict.end()) {
        for (const auto& entry : it->second) {
            const Slipper::Dep::Atom& config_atom = entry.first;
            if (config_atom.getCp() == cp) {
                if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Applying flags from config atom: " + config_atom.getRawString());
                for (const auto& flag : entry.second) {
                    if (!flag.empty() && flag[0] == '-') {
                        std::string positive_flag = flag.substr(1);
                        resolved_flags.erase(positive_flag);
                        if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Removed flag: " + positive_flag);
                    } else {
                        resolved_flags.insert(flag);
                        if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "Added flag: " + flag);
                    }
                }
            }
        }
    } else {
        if (std::getenv("DEBUG")) Logger::logDebug("UseManager::getPUSE", "No package.use overrides found for CP: " + cp);
    }
    
    if (std::getenv("DEBUG")) {
        Logger::logDebug("UseManager::getPUSE", "Resolution complete. Total active flags: " + std::to_string(resolved_flags.size()));
    }
    return resolved_flags;
}