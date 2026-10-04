#include "../../include/config/LicenseManager.hpp"
#include "../../include/Logger.hpp"
#include "functional"

using namespace Slipper::Config;

void LicenseManager::addLicenseGroup(const std::string& group_name, const std::vector< std::string >& licenses) {
    Logger::logDebug("LicenseManager::addLicenseGroup", "Registering license group: " + group_name);
    license_groups[group_name] = licenses;
}

void LicenseManager::addPackageLicense(const std::string& atom_str, const std::vector< std::string >& licenses) {
    Logger::logDebug("LicenseManager::addPackageLicense", "Adding package.license entry for: " + atom_str);
    
    Slipper::Dep::Atom config_atom(atom_str);
    std::string cp = config_atom.getCp();
    
    plicense_dict[cp].push_back({config_atom, licenses});
}

std::vector< std::string > LicenseManager::expandLicenseToken(const std::string& token) const {
    std::vector< std::string > expanded;
    std::unordered_set< std::string > seen_groups; 

    // Safely spaced template arguments to prevent UI stripping
    std::function< void(const std::string&) > expand_recursive = [&](const std::string& current_token) {
        if (!current_token.empty() && current_token[0] == '@') {
            if (seen_groups.find(current_token) != seen_groups.end()) return;
            seen_groups.insert(current_token);

            if (Logger::isDebugEnabled()) {
                Logger::logDebug("LicenseManager::expandLicenseToken", "Expanding group token: " + current_token);
            }
            
            auto it = license_groups.find(current_token);
            if (it != license_groups.end()) {
                for (const auto& lic : it->second) {
                    expand_recursive(lic);
                }
            } else {
                if (Logger::isDebugEnabled()) {
                    Logger::logDebug("LicenseManager::expandLicenseToken", "Undefined license group: " + current_token);
                }
                expanded.push_back(current_token); 
            }
        } else {
            expanded.push_back(current_token);
        }
    };

    expand_recursive(token);
    return expanded;
}

std::unordered_set< std::string > LicenseManager::getAcceptedLicenses(const Slipper::Dep::Atom& pkg) const {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("LicenseManager::getAcceptedLicenses", "Resolving accepted licenses for: " + pkg.getRawString());
    }
    
    std::unordered_set< std::string > resolved_licenses;
    std::string cp = pkg.getCp();
    
    // 1. Apply global licenses from make.conf (*/*)
    auto global_it = plicense_dict.find("*/*");
    if (global_it != plicense_dict.end()) {
        if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Applying global */* licenses.");
        for (const auto& entry : global_it->second) {
            for (const auto& lic_token : entry.second) {
                auto expanded = expandLicenseToken(lic_token);
                for (const auto& ex_lic : expanded) {
                    if (!ex_lic.empty() && ex_lic[0] == '-') {
                        std::string positive_lic = ex_lic.substr(1);
                        resolved_licenses.erase(positive_lic);
                        if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Removed global license: " + positive_lic);
                    } else {
                        resolved_licenses.insert(ex_lic);
                        if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Added global license: " + ex_lic);
                    }
                }
            }
        }
    }

    // 2. Apply package-specific licenses
    auto it = plicense_dict.find(cp);
    if (it != plicense_dict.end()) {
        for (const auto& entry : it->second) {
            const Slipper::Dep::Atom& config_atom = entry.first;
            if (config_atom.getCp() == cp) {
                if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Applying licenses from config atom: " + config_atom.getRawString());
                for (const auto& lic_token : entry.second) {
                    auto expanded = expandLicenseToken(lic_token);
                    for (const auto& ex_lic : expanded) {
                        if (!ex_lic.empty() && ex_lic[0] == '-') {
                            std::string positive_lic = ex_lic.substr(1);
                            resolved_licenses.erase(positive_lic);
                            if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Removed license: " + positive_lic);
                        } else {
                            resolved_licenses.insert(ex_lic);
                            if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "Added license: " + ex_lic);
                        }
                    }
                }
            }
        }
    } else {
        if (Logger::isDebugEnabled()) Logger::logDebug("LicenseManager::getAcceptedLicenses", "No package.license overrides found for CP: " + cp);
    }
    
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("LicenseManager::getAcceptedLicenses", "Resolution complete. Total accepted licenses: " + std::to_string(resolved_licenses.size()));
    }
    return resolved_licenses;
}