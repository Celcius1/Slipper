#include "../../include/repository/RepoConfigLoader.hpp"
#include "../../include/Logger.hpp"
#include "sstream"
#include "algorithm"
#include "cctype"

using namespace Slipper::Repository;

std::string RepoConfigLoader::trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](int c) { return std::isspace(c); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](int c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : "";
}

void RepoConfigLoader::loadFromString(const std::string& config_content) {
    Logger::logDebug("RepoConfigLoader::loadFromString", "Parsing repos.conf content...");

    std::stringstream ss(config_content);
    std::string line;
    std::string current_section = "";

    while (std::getline(ss, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Check for section headers e.g., [gentoo]
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            if (repos.find(current_section) == repos.end()) {
                repos[current_section] = RepoConfig(current_section);
                Logger::logDebug("RepoConfigLoader", "Discovered repository section: [" + current_section + "]");
            }
            continue;
        }

        // Check for key-value pairs e.g., location = /var/db/repos/gentoo
        auto delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos && !current_section.empty()) {
            std::string key = trim(line.substr(0, delimiter_pos));
            std::string value = trim(line.substr(delimiter_pos + 1));
            
            RepoConfig& repo = repos[current_section];
            
            if (key == "location") {
                repo.location = value;
            } else if (key == "sync-type") {
                repo.sync_type = value;
            } else if (key == "sync-uri") {
                repo.sync_uri = value;
            } else if (key == "priority") {
                try { repo.priority = std::stoi(value); } 
                catch (...) { Logger::logDebug("RepoConfigLoader", "Invalid priority value: " + value); }
            } else if (key == "auto-sync") {
                repo.auto_sync = (value != "no" && value != "false");
            }
            
            Logger::logDebug("RepoConfigLoader", "Mapped key [" + key + "] = [" + value + "] to repo: " + current_section);
        }
    }
}