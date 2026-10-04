#include "../../include/dbapi/VarDbApi.hpp"
#include "../../include/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;
using namespace Slipper::Dbapi;

VarDbApi::VarDbApi(const std::string& eroot) {
    std::string vdb_path = "/var/db/pkg";
    dbroot = eroot + vdb_path;
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::Constructor", "Initialized VarDbApi with dbroot: " + dbroot);
    }
}

std::vector< std::string > VarDbApi::cp_all() const {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::cp_all", "Scanning " + dbroot + " for all installed packages.");
    }
    std::vector< std::string > results;

    if (!fs::exists(dbroot) || !fs::is_directory(dbroot)) {
        if (Logger::isDebugEnabled()) {
            Logger::logDebug("VarDbApi::cp_all", "VDB root does not exist. Returning empty list.");
        }
        return results;
    }

    for (const auto& cat_entry : fs::directory_iterator(dbroot)) {
        if (!cat_entry.is_directory()) continue;
        
        std::string category = cat_entry.path().filename().string();
        if (category[0] == '-' || category[0] == '.') continue;

        for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
            if (!pkg_entry.is_directory()) continue;
            
            size_t last_dash = pkg_entry.path().filename().string().find_last_of('-');
            if (last_dash != std::string::npos) {
                std::string package_name = pkg_entry.path().filename().string().substr(0, last_dash);
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
    
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::cp_all", "Found " + std::to_string(results.size()) + " unique packages.");
    }
    return results;
}

std::vector< std::string > VarDbApi::cp_list(const std::string& cp) const {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::cp_list", "Looking up installed versions for: " + cp);
    }
    std::vector< std::string > results;
    
    if (!fs::exists(dbroot)) return results;
    
    for (const auto& cat_entry : fs::directory_iterator(dbroot)) {
        if (!cat_entry.is_directory()) continue;
        std::string category = cat_entry.path().filename().string();
        
        for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
            if (!pkg_entry.is_directory()) continue;
            std::string cpv = category + "/" + pkg_entry.path().filename().string();
            
            if (cpv.find(cp + "-") == 0) {
                results.push_back(cpv);
            }
        }
    }
    
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::cp_list", "Found " + std::to_string(results.size()) + " versions.");
    }
    return results;
}

std::vector< std::string > VarDbApi::aux_get(const std::string& cpv, const std::vector< std::string >& wants) const {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("VarDbApi::aux_get", "Fetching metadata for " + cpv);
    }
    std::vector< std::string > results;
    
    std::string pkg_dir = dbroot + "/" + cpv;
    if (!fs::exists(pkg_dir) || !fs::is_directory(pkg_dir)) {
        Logger::logInfo("VarDbApi::aux_get", "Package directory not found: " + pkg_dir);
        for (size_t i = 0; i < wants.size(); ++i) results.push_back("");
        return results;
    }

    for (const auto& key : wants) {
        std::string key_file = pkg_dir + "/" + key;
        if (fs::exists(key_file) && fs::is_regular_file(key_file)) {
            std::ifstream file(key_file);
            
            // MODERN C++: Read directly into the string using stream iterators, skipping the stringstream double-buffer
            std::string content((std::istreambuf_iterator< char >(file)), std::istreambuf_iterator< char >());
            
            if (!content.empty() && content.back() == '\n') {
                content.pop_back();
            }
            results.push_back(content);
            
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("VarDbApi::aux_get", "Read key " + key + ": " + content);
            }
        } else {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("VarDbApi::aux_get", "Key file missing: " + key);
            }
            results.push_back(""); 
        }
    }
    
    return results;
}