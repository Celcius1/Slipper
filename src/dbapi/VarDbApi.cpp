#include "../../include/dbapi/VarDbApi.hpp"
#include "../../include/Logger.hpp"
#include "filesystem"
#include "fstream"
#include "sstream"
#include "algorithm"

namespace fs = std::filesystem;
using namespace Slipper::Dbapi;

VarDbApi::VarDbApi(const std::string& eroot) {
    // Mimics self._dbroot = self._eroot + VDB_PATH from vartree.py
    std::string vdb_path = "/var/db/pkg";
    dbroot = eroot + vdb_path;
    Logger::logDebug("VarDbApi::Constructor", "Initialized VarDbApi with dbroot: " + dbroot);
}

std::vector< std::string > VarDbApi::cp_all() const {
    Logger::logDebug("VarDbApi::cp_all", "Scanning " + dbroot + " for all installed packages.");
    std::vector< std::string > results;

    if (!fs::exists(dbroot) || !fs::is_directory(dbroot)) {
        Logger::logDebug("VarDbApi::cp_all", "VDB root does not exist. Returning empty list.");
        return results;
    }

    // Scan /var/db/pkg//
    for (const auto& cat_entry : fs::directory_iterator(dbroot)) {
        if (!cat_entry.is_directory()) continue;
        
        std::string category = cat_entry.path().filename().string();
        // Skip metadata/hidden directories
        if (category[0] == '-' || category[0] == '.') continue;

        for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
            if (!pkg_entry.is_directory()) continue;
            
            std::string cpv = category + "/" + pkg_entry.path().filename().string();
            // In cp_all, Portage returns the CP (Category/Package), not the CPV.
            // For now, we extract the CP by stripping the version (simplified string split).
            size_t last_dash = pkg_entry.path().filename().string().find_last_of('-');
            if (last_dash != std::string::npos) {
                // Heuristic: strip the version block (naive for this initial test)
                // A full implementation will use your Atom parser here to extract the CP.
                std::string package_name = pkg_entry.path().filename().string().substr(0, last_dash);
                // Check if the dash was for a revision (e.g., -r1) and strip again if needed
                if (package_name.find("-r") != std::string::npos) {
                    size_t rev_dash = package_name.find_last_of('-');
                    package_name = package_name.substr(0, rev_dash);
                }
                
                std::string cp = category + "/" + package_name;
                if (std::find(results.begin(), results.end(), cp) == results.end()) {
                    results.push_back(cp);
                }
            }
        }
    }
    
    Logger::logDebug("VarDbApi::cp_all", "Found " + std::to_string(results.size()) + " unique packages.");
    return results;
}

std::vector< std::string > VarDbApi::cp_list(const std::string& cp) const {
    Logger::logDebug("VarDbApi::cp_list", "Looking up installed versions for: " + cp);
    std::vector< std::string > results;
    
    // We iterate through cpv_all to find matching CPs.
    // In a production system, this looks up the specific category directory.
    if (!fs::exists(dbroot)) return results;
    
    for (const auto& cat_entry : fs::directory_iterator(dbroot)) {
        if (!cat_entry.is_directory()) continue;
        std::string category = cat_entry.path().filename().string();
        
        for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
            if (!pkg_entry.is_directory()) continue;
            std::string cpv = category + "/" + pkg_entry.path().filename().string();
            
            // If the directory path starts with our target CP, it's a match
            if (cpv.find(cp + "-") == 0) {
                results.push_back(cpv);
            }
        }
    }
    
    Logger::logDebug("VarDbApi::cp_list", "Found " + std::to_string(results.size()) + " versions.");
    return results;
}

std::vector< std::string > VarDbApi::aux_get(const std::string& cpv, const std::vector< std::string >& wants) const {
    Logger::logDebug("VarDbApi::aux_get", "Fetching metadata for " + cpv);
    std::vector< std::string > results;
    
    std::string pkg_dir = dbroot + "/" + cpv;
    if (!fs::exists(pkg_dir) || !fs::is_directory(pkg_dir)) {
        Logger::logInfo("VarDbApi::aux_get", "Package directory not found: " + pkg_dir);
        // Push empty strings for missing package to match Portage's behaviour
        for (size_t i = 0; i < wants.size(); ++i) results.push_back("");
        return results;
    }

    for (const auto& key : wants) {
        std::string key_file = pkg_dir + "/" + key;
        if (fs::exists(key_file) && fs::is_regular_file(key_file)) {
            std::ifstream file(key_file);
            std::stringstream buffer;
            buffer << file.rdbuf();
            
            std::string content = buffer.str();
            // Portage sanitises trailing newlines for aux_get returns
            if (!content.empty() && content.back() == '\n') {
                content.pop_back();
            }
            results.push_back(content);
            Logger::logDebug("VarDbApi::aux_get", "Read key " + key + ": " + content);
        } else {
            Logger::logDebug("VarDbApi::aux_get", "Key file missing: " + key);
            results.push_back(""); // Empty string for missing metadata
        }
    }
    
    return results;
}