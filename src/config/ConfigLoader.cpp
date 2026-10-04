#include "../../include/config/ConfigLoader.hpp"
#include "../../include/Logger.hpp"
#include "fstream"
#include "sstream"
#include "filesystem"
#include "cstdlib"
#include "algorithm"

using namespace Slipper::Config;

std::vector< std::string > ConfigLoader::tokenize(const std::string& str) {
    std::vector< std::string > tokens;
    std::stringstream ss(str);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void ConfigLoader::processPath(const std::string& path_str, std::function< void(const std::string&) > line_processor) {
    if (!std::filesystem::exists(path_str)) {
        if (std::getenv("DEBUG")) {
            Logger::logDebug("ConfigLoader", "Path does not exist, skipping: " + path_str);
        }
        return;
    }

    auto process_file = [&line_processor](const std::filesystem::path& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            // Strip leading whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;
            
            line_processor(line);
        }
    };

    if (std::filesystem::is_directory(path_str)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path_str)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    } else {
        process_file(path_str);
    }
}

void ConfigLoader::loadSystemConfig(
    std::shared_ptr< UseManager > use_mgr,
    std::shared_ptr< MaskManager > mask_mgr,
    std::shared_ptr< KeywordsManager > kw_mgr,
    std::shared_ptr< LicenseManager > lic_mgr,
    const std::string& config_root
) {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("ConfigLoader", "Parsing system configuration from: " + config_root);
    }

    // 0. Pre-load Gentoo Repository License Groups
    std::string license_groups_path = "/var/db/repos/gentoo/profiles/license_groups";
    processPath(license_groups_path, [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (tokens.size() > 1) {
            std::string group_name = "@" + tokens[0];
            tokens.erase(tokens.begin());
            lic_mgr->addLicenseGroup(group_name, tokens);
        }
    });

    if (std::getenv("DEBUG")) {
        Logger::logDebug("ConfigLoader", "Pre-loaded license groups from repository profiles.");
    }

    // 1. Parse make.conf (Basic variable extraction for USE, ACCEPT_KEYWORDS, ACCEPT_LICENSE)
    std::string make_conf_path = config_root + "/make.conf";
    processPath(make_conf_path, [&](const std::string& line) {
        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);
            
            // Strip quotes from value
            val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
            val.erase(std::remove(val.begin(), val.end(), '\''), val.end());
            
            auto tokens = tokenize(val);
            if (key == "USE" || key == "USE_EXPAND") {
                // Apply global USE flags to a generic "*" atom fallback
                use_mgr->addPackageUse("*/*", tokens);
            } else if (key == "ACCEPT_KEYWORDS") {
                kw_mgr->addKeyword("*/*", tokens);
            } else if (key == "ACCEPT_LICENSE") {
                lic_mgr->addPackageLicense("*/*", tokens);
            }
        }
    });

    // 2. Parse package.use
    processPath(config_root + "/package.use", [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (tokens.size() > 1) {
            std::string atom = tokens[0];
            tokens.erase(tokens.begin());
            use_mgr->addPackageUse(atom, tokens);
        }
    });

    // 3. Parse package.mask & package.unmask
    processPath(config_root + "/package.mask", [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (!tokens.empty()) mask_mgr->addMask(tokens[0]);
    });
    
    processPath(config_root + "/package.unmask", [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (!tokens.empty()) mask_mgr->addUnmask(tokens[0]);
    });

    // 4. Parse package.accept_keywords
    processPath(config_root + "/package.accept_keywords", [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (!tokens.empty()) {
            std::string atom = tokens[0];
            tokens.erase(tokens.begin());
            // If no keyword is explicitly defined, Portage defaults to ~arch
            if (tokens.empty()) tokens.push_back("~*"); 
            kw_mgr->addKeyword(atom, tokens);
        }
    });

    // 5. Parse package.license
    processPath(config_root + "/package.license", [&](const std::string& line) {
        auto tokens = tokenize(line);
        if (tokens.size() > 1) {
            std::string atom = tokens[0];
            tokens.erase(tokens.begin());
            lic_mgr->addPackageLicense(atom, tokens);
        }
    });

    if (std::getenv("DEBUG")) {
        Logger::logDebug("ConfigLoader", "System configuration parsing complete.");
    }
}