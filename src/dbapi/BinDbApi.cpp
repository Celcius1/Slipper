#include "../../include/dbapi/BinDbApi.hpp"
#include "../../include/Logger.hpp"
#include "filesystem"
#include "algorithm"

namespace fs = std::filesystem;
using namespace Slipper::Dbapi;

BinDbApi::BinDbApi(const std::string& pkgdir) : pkgdir(pkgdir) {
    Logger::logDebug("BinDbApi::Constructor", "Initialized BinDbApi with PKGDIR: " + pkgdir);
}

std::vector< std::string > BinDbApi::cp_all() const {
    Logger::logDebug("BinDbApi::cp_all", "Scanning PKGDIR for binary packages.");
    std::vector< std::string > results;

    if (!fs::exists(pkgdir) || !fs::is_directory(pkgdir)) {
        return results;
    }

    for (const auto& cat_entry : fs::directory_iterator(pkgdir)) {
        if (!cat_entry.is_directory()) continue;
        std::string category = cat_entry.path().filename().string();
        if (category == "Packages" || category[0] == '.') continue;

        for (const auto& pkg_entry : fs::directory_iterator(cat_entry.path())) {
            if (!pkg_entry.is_regular_file()) continue;
            
            std::string filename = pkg_entry.path().filename().string();
            
            // Check for Gentoo binary package extensions
            if (filename.find(".tbz2") != std::string::npos || 
                filename.find(".xpak") != std::string::npos || 
                filename.find(".gpkg.tar") != std::string::npos) {
                
                // Heuristic: Strip the extension and version to extract CP
                size_t cp_dash = filename.find_last_of('-');
                if (cp_dash != std::string::npos) {
                    std::string package_name = filename.substr(0, cp_dash);
                    // Handle revision numbers
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
    }
    
    Logger::logDebug("BinDbApi::cp_all", "Found " + std::to_string(results.size()) + " unique binary packages.");
    return results;
}

std::vector< std::string > BinDbApi::cp_list(const std::string& cp) const {
    Logger::logDebug("BinDbApi::cp_list", "Looking up binary versions for: " + cp);
    std::vector< std::string > results;
    
    size_t slash_pos = cp.find('/');
    if (slash_pos == std::string::npos) return results;
    
    std::string category = cp.substr(0, slash_pos);
    std::string cat_dir = pkgdir + "/" + category;
    
    if (fs::exists(cat_dir) && fs::is_directory(cat_dir)) {
        for (const auto& pkg_entry : fs::directory_iterator(cat_dir)) {
            if (!pkg_entry.is_regular_file()) continue;
            
            std::string filename = pkg_entry.path().filename().string();
            // A full implementation will strip extensions exactly.
            if (filename.find(cp.substr(slash_pos + 1)) == 0) {
                results.push_back(category + "/" + filename);
            }
        }
    }
    
    return results;
}

std::vector< std::string > BinDbApi::aux_get(const std::string& cpv, const std::vector< std::string >& wants) const {
    Logger::logDebug("BinDbApi::aux_get", "Fetching binary metadata for " + cpv);
    // TODO: Integrate libarchive to extract XPAK/GPKG metadata block
    Logger::logDebug("BinDbApi::aux_get", "Archive extraction stubbed. Returning empty metadata.");
    return std::vector< std::string >(wants.size(), "");
}