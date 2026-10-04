#include "../../include/dbapi/PortDbApi.hpp"
#include "../../include/Logger.hpp"
#include "filesystem"
#include "fstream"
#include "sstream"
#include "algorithm"

namespace fs = std::filesystem;
using namespace Slipper::Dbapi;

PortDbApi::PortDbApi(const std::vector< std::string >& porttrees) : porttrees(porttrees) {
    Logger::logDebug("PortDbApi::Constructor", "Initialized PortDbApi with " + std::to_string(porttrees.size()) + " repositories.");
}

std::vector< std::string > PortDbApi::cp_all() const {
    Logger::logDebug("PortDbApi::cp_all", "Scanning repositories for all available packages.");
    std::vector< std::string > results;

    for (const auto& repo : porttrees) {
        if (!fs::exists(repo) || !fs::is_directory(repo)) continue;

        for (const auto& cat_entry : fs::directory_iterator(repo)) {
            if (!cat_entry.is_directory()) continue;
            std::string category = cat_entry.path().filename().string();
            
            // Skip metadata, profiles, eclass, and hidden directories
            if (category[0] == '-' || category[0] == '.' || category == "metadata" || category == "profiles" || category == "eclass") continue;

            for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
                if (!pkg_entry.is_directory()) continue;
                
                std::string cp = category + "/" + pkg_entry.path().filename().string();
                if (std::find(results.begin(), results.end(), cp) == results.end()) {
                    results.push_back(cp);
                }
            }
        }
    }
    
    Logger::logDebug("PortDbApi::cp_all", "Found " + std::to_string(results.size()) + " unique packages across all repos.");
    return results;
}

std::vector< std::string > PortDbApi::cp_list(const std::string& cp) const {
    Logger::logDebug("PortDbApi::cp_list", "Looking up available versions for: " + cp);
    std::vector< std::string > results;
    
    for (const auto& repo : porttrees) {
        std::string pkg_dir = repo + "/" + cp;
        if (!fs::exists(pkg_dir) || !fs::is_directory(pkg_dir)) continue;

        for (const auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            // We only care about .ebuild files
            if (filename.length() > 7 && filename.substr(filename.length() - 7) == ".ebuild") {
                // Strip the .ebuild extension to get the CPV (e.g. portage-3.0.4.ebuild -> sys-apps/portage-3.0.4)
                std::string pf = filename.substr(0, filename.length() - 7);
                size_t slash_pos = cp.find('/');
                std::string category = cp.substr(0, slash_pos);
                results.push_back(category + "/" + pf);
            }
        }
    }
    
    // In Portage, these are sorted by version. We will rely on our Versions::vercmp later to do a stable sort here.
    Logger::logDebug("PortDbApi::cp_list", "Found " + std::to_string(results.size()) + " ebuilds.");
    return results;
}

std::vector< std::string > PortDbApi::aux_get(const std::string& cpv, const std::vector< std::string >& wants) const {
    Logger::logDebug("PortDbApi::aux_get", "Fetching repository metadata for " + cpv);
    std::vector< std::string > results(wants.size(), "");
    
    for (const auto& repo : porttrees) {
        // Portage uses metadata/md5-cache to bypass parsing raw bash ebuilds
        std::string cache_file = repo + "/metadata/md5-cache/" + cpv;
        
        if (fs::exists(cache_file) && fs::is_regular_file(cache_file)) {
            Logger::logDebug("PortDbApi::aux_get", "Cache hit at: " + cache_file);
            std::ifstream file(cache_file);
            std::string line;
            
            while (std::getline(file, line)) {
                size_t equals_pos = line.find('=');
                if (equals_pos != std::string::npos) {
                    std::string key = line.substr(0, equals_pos);
                    std::string value = line.substr(equals_pos + 1);
                    
                    for (size_t i = 0; i < wants.size(); ++i) {
                        if (wants[i] == key) {
                            results[i] = value;
                            Logger::logDebug("PortDbApi::aux_get", "Extracted key " + key + ": " + value);
                        }
                    }
                }
            }
            // Once we find a valid cache file in an overlay/repo, we stop searching
            break;
        }
    }
    
    return results;
}