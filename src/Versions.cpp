#include "../include/Versions.hpp"
#include "../include/Logger.hpp"
#include "regex"
#include "vector"
#include "sstream"
#include "cmath"
#include "algorithm"
#include "map"

using namespace Slipper::Versions;

// The core Gentoo version regular expression broken down:
// 1: Primary number (\d+)
// 2: Dotted minors ((\.\d+)*)
// 3: Single letter suffix ([a-z]?)
// 4: Underscore suffixes ((_(pre|p|beta|alpha|rc)\d*)*)
// 5: Revision (-r(\d+))?
const std::regex VER_REGEX(
    "^(\\d+)((?:\\.\\d+)*)([a-z]?)((?:_(?:pre|p|beta|alpha|rc)\\d*)*)(?:-r(\\d+))?$"
);

const std::regex SUFFIX_REGEX("^(alpha|beta|rc|pre|p)(\\d*)$");

// Mapping suffix strings to integer weights for comparison
std::map< std::string, int > getSuffixValues() {
    return {
        {"pre", -2},
        {"p", 0},
        {"alpha", -4},
        {"beta", -3},
        {"rc", -1}
    };
}

bool Slipper::Versions::ververify(const std::string& myver) {
    return std::regex_match(myver, VER_REGEX);
}

// Helper to split dotted version strings (e.g., ".1.2" -> {"1", "2"})
std::vector< std::string > splitDotted(const std::string& str) {
    std::vector< std::string > result;
    if (str.empty()) return result;
    
    std::stringstream ss(str.substr(1)); // skip the leading dot
    std::string item;
    while (std::getline(ss, item, '.')) {
        result.push_back(item);
    }
    return result;
}

// Helper to split underscore suffixes (e.g., "_pre1_p2" -> {"pre1", "p2"})
std::vector< std::string > splitSuffixes(const std::string& str) {
    std::vector< std::string > result;
    if (str.empty()) return result;
    
    std::stringstream ss(str.substr(1)); // skip the leading underscore
    std::string item;
    while (std::getline(ss, item, '_')) {
        result.push_back(item);
    }
    return result;
}

std::optional< int > Slipper::Versions::vercmp(const std::string& ver1, const std::string& ver2) {
    Logger::logDebug("vercmp", "Comparing versions: " + ver1 + " against " + ver2);

    if (ver1 == ver2) {
        Logger::logDebug("vercmp", "Versions are identical strings. Returning 0.");
        return 0;
    }

    std::smatch match1, match2;
    if (!std::regex_match(ver1, match1, VER_REGEX)) {
        Logger::logDebug("vercmp", "Syntax error in version 1: " + ver1);
        return std::nullopt;
    }
    if (!std::regex_match(ver2, match2, VER_REGEX)) {
        Logger::logDebug("vercmp", "Syntax error in version 2: " + ver2);
        return std::nullopt;
    }

    // 1. Compare the numeric parts
    std::vector< int > list1 = { std::stoi(match1[1].str()) };
    std::vector< int > list2 = { std::stoi(match2[1].str()) };

    auto vlist1 = splitDotted(match1[2].str());
    auto vlist2 = splitDotted(match2[2].str());

    size_t max_parts = std::max(vlist1.size(), vlist2.size());
    for (size_t i = 0; i < max_parts; ++i) {
        if (i >= vlist1.size() || vlist1[i].empty()) {
            list1.push_back(-1);
            list2.push_back(std::stoi(vlist2[i]));
        } else if (i >= vlist2.size() || vlist2[i].empty()) {
            list1.push_back(std::stoi(vlist1[i]));
            list2.push_back(-1);
        } else if (vlist1[i][0] != '0' && vlist2[i][0] != '0') {
            list1.push_back(std::stoi(vlist1[i]));
            list2.push_back(std::stoi(vlist2[i]));
        } else {
            // Padding logic for numbers starting with zeros (e.g. 1.02 vs 1.1)
            size_t max_len = std::max(vlist1[i].length(), vlist2[i].length());
            std::string padded1 = vlist1[i];
            std::string padded2 = vlist2[i];
            padded1.append(max_len - padded1.length(), '0');
            padded2.append(max_len - padded2.length(), '0');
            list1.push_back(std::stoi(padded1));
            list2.push_back(std::stoi(padded2));
        }
    }

    // Append ASCII values of single letters
    if (match1[3].length() > 0) list1.push_back(match1[3].str()[0]);
    if (match2[3].length() > 0) list2.push_back(match2[3].str()[0]);

    size_t max_list = std::max(list1.size(), list2.size());
    for (size_t i = 0; i < max_list; ++i) {
        if (i >= list1.size()) return -1;
        if (i >= list2.size()) return 1;
        if (list1[i] != list2[i]) {
            int result = (list1[i] > list2[i]) - (list1[i] < list2[i]);
            Logger::logDebug("vercmp", "Numeric/Letter mismatch found. Returning " + std::to_string(result));
            return result;
        }
    }

    // 2. Compare underscore suffixes (alpha, beta, rc, pre, p)
    auto suff1 = splitSuffixes(match1[4].str());
    auto suff2 = splitSuffixes(match2[4].str());
    auto suffix_values = getSuffixValues();

    size_t max_suff = std::max(suff1.size(), suff2.size());
    for (size_t i = 0; i < max_suff; ++i) {
        std::smatch s1, s2;
        std::string s1_name = "p", s1_val = "-1";
        std::string s2_name = "p", s2_val = "-1";

        if (i < suff1.size() && std::regex_match(suff1[i], s1, SUFFIX_REGEX)) {
            s1_name = s1[1];
            s1_val = s1[2].str().empty() ? "0" : s1[2].str();
        }
        if (i < suff2.size() && std::regex_match(suff2[i], s2, SUFFIX_REGEX)) {
            s2_name = s2[1];
            s2_val = s2[2].str().empty() ? "0" : s2[2].str();
        }

        if (s1_name != s2_name) {
            int a = suffix_values[s1_name];
            int b = suffix_values[s2_name];
            int result = (a > b) - (a < b);
            Logger::logDebug("vercmp", "Suffix type mismatch (" + s1_name + " vs " + s2_name + "). Returning " + std::to_string(result));
            return result;
        }
        if (s1_val != s2_val) {
            int r1 = std::stoi(s1_val);
            int r2 = std::stoi(s2_val);
            int result = (r1 > r2) - (r1 < r2);
            if (result != 0) {
                Logger::logDebug("vercmp", "Suffix value mismatch. Returning " + std::to_string(result));
                return result;
            }
        }
    }

    // 3. Compare revisions (-rX)
    int r1 = match1[5].matched ? std::stoi(match1[5].str()) : 0;
    int r2 = match2[5].matched ? std::stoi(match2[5].str()) : 0;
    
    int final_result = (r1 > r2) - (r1 < r2);
    Logger::logDebug("vercmp", "Falling back to revision comparison. Returning " + std::to_string(final_result));
    return final_result;
}