#include "../../include/config/KeywordsManager.hpp"
#include "../../include/Logger.hpp"

using namespace Slipper::Config;

void KeywordsManager::addKeyword(const std::string& atom_str, const std::vector< std::string >& keywords) {
    Logger::logDebug("KeywordsManager::addKeyword", "Adding accept_keywords entry for: " + atom_str);
    
    Slipper::Dep::Atom config_atom(atom_str);
    std::string cp = config_atom.getCp();
    
    pkeywords_dict[cp].push_back({config_atom, keywords});
    Logger::logDebug("KeywordsManager::addKeyword", "Stored entry under CP: " + cp + " with " + std::to_string(keywords.size()) + " keywords.");
}

std::vector< std::string > KeywordsManager::getKeywords(const Slipper::Dep::Atom& pkg) const {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("KeywordsManager::getKeywords", "Resolving accepted keywords for: " + pkg.getRawString());
    }
    
    std::vector< std::string > resolved_keywords;
    std::string cp = pkg.getCp();
    
    // 1. Apply global keywords from make.conf (*/*)
    auto global_it = pkeywords_dict.find("*/*");
    if (global_it != pkeywords_dict.end()) {
        if (Logger::isDebugEnabled()) Logger::logDebug("KeywordsManager::getKeywords", "Applying global */* keywords.");
        for (const auto& entry : global_it->second) {
            for (const auto& kw : entry.second) {
                resolved_keywords.push_back(kw);
                if (Logger::isDebugEnabled()) Logger::logDebug("KeywordsManager::getKeywords", "Added global keyword: " + kw);
            }
        }
    }

    // 2. Apply package-specific keywords
    auto it = pkeywords_dict.find(cp);
    if (it != pkeywords_dict.end()) {
        // Stack the keywords. Like UseManager, this will eventually sort by atom specificity.
    for (const auto& entry : it->second) {
        const Slipper::Dep::Atom& config_atom = entry.first;
        
        bool match = false;
        if (config_atom.getCp() == cp) {
            // If the config atom has a specific version, ensure it matches the package being evaluated
            if (config_atom.getVersion().has_value()) {
                if (config_atom.getVersion().value() == pkg.getVersion().value_or("")) {
                    match = true;
                }
            } else {
                match = true; // Unversioned applies to all
            }
        }

        if (match) {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("KeywordsManager::getKeywords", "Applying keywords from config atom: " + config_atom.getRawString());
            }
            
            for (const auto& kw : entry.second) {
                resolved_keywords.push_back(kw);
                if (Logger::isDebugEnabled()) {
                    Logger::logDebug("KeywordsManager::getKeywords", "Added keyword: " + kw);
                }
            }
        }
    }
    } else {
        if (Logger::isDebugEnabled()) Logger::logDebug("KeywordsManager::getKeywords", "No package.accept_keywords overrides found for CP: " + cp);
    }
    
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("KeywordsManager::getKeywords", "Resolution complete. Total accepted keywords: " + std::to_string(resolved_keywords.size()));
    }
    return resolved_keywords;
}