#include "../../../include/dep/soname/Parser.hpp"
#include "../../../include/Logger.hpp"
#include "sstream"
#include "stdexcept"
#include "unordered_set"

using namespace Slipper::Dep::Soname;

// Using spaces inside the brackets to prevent UI stripping
std::vector< SonameAtom > Slipper::Dep::Soname::parseSonameDeps(const std::string& s) {
    Logger::logDebug("parseSonameDeps", "Parsing soname dependencies: " + s);
    
    std::vector< SonameAtom > results;
    std::unordered_set< std::string > categories;
    std::string current_category = "";
    
    bool has_category = false;
    bool previous_was_category = false;
    
    std::stringstream ss(s);
    std::string token;
    
    while (ss >> token) {
        if (!token.empty() && token.back() == ':') {
            if (has_category && previous_was_category) {
                throw std::invalid_argument("Multilib category empty: " + current_category);
            }
            
            current_category = token.substr(0, token.size() - 1);
            previous_was_category = true;
            has_category = true;
            
            if (categories.find(current_category) != categories.end()) {
                throw std::invalid_argument("Multilib category occurs more than once: " + current_category);
            }
            categories.insert(current_category);
        } else {
            if (!has_category) {
                throw std::invalid_argument("Multilib category missing: " + token);
            }
            previous_was_category = false;
            results.emplace_back(current_category, token);
            Logger::logDebug("parseSonameDeps", "Parsed SonameAtom: " + current_category + ": " + token);
        }
    }
    
    if (has_category && previous_was_category) {
        throw std::invalid_argument("Multilib category empty: " + current_category);
    }
    
    return results;
}