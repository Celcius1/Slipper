#include "../include/Versions.hpp"
#include "../include/Logger.hpp"
#include <regex>
#include <vector>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <map>

using namespace Slipper::Versions;

// ---------------------------------------------------------
// EAPI 8: Name Validation Allow-Lists
// ---------------------------------------------------------

bool Slipper::Versions::isValidCategory(const std::string& category) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("isValidCategory", "Validating category string: " + category);
    }
    // EAPI 8: Alphanumeric, plus, underscore, dot, hyphen. Cannot start with hyphen, dot, or plus.
    const std::regex cat_regex("^[A-Za-z0-9_][A-Za-z0-9+_.-]*$");
    return std::regex_match(category, cat_regex);
}

bool Slipper::Versions::isValidPackageName(const std::string& package_name) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("isValidPackageName", "Validating package string: " + package_name);
    }
    // EAPI 8: Alphanumeric, plus, underscore, hyphen. Cannot start with hyphen or plus.
    // Must explicitly ensure it doesn't end in a hyphen followed by version syntax.
    const std::regex pkg_regex("^[A-Za-z0-9_][A-Za-z0-9+_-]*$");
    if (!std::regex_match(package_name, pkg_regex)) return false;

    // Reject if it ends in a hyphen followed by a version-like string
    const std::regex invalid_suffix_regex("-(\\d+)((\\.\\d+)*)([a-z]?)((_(pre|p|beta|alpha|rc)\\d*)*)(-r\\d+)?$");
    return !std::regex_search(package_name, invalid_suffix_regex);
}

bool Slipper::Versions::isValidUseFlag(const std::string& flag) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("isValidUseFlag", "Validating USE flag string: " + flag);
    }
    // EAPI 8: Must begin with alphanumeric
    const std::regex use_regex("^[A-Za-z0-9][A-Za-z0-9+_.-]*$");
    return std::regex_match(flag, use_regex);
}

// ---------------------------------------------------------
// EAPI 8: Version Parsing & Comparison
// ---------------------------------------------------------

const std::regex VER_REGEX("^(\\d+)((?:\\.\\d+)*)([a-z]?)((?:_(?:pre|p|beta|alpha|rc)\\d*)*)(?:-r(\\d+))?$");
const std::regex SUFFIX_REGEX("^(alpha|beta|rc|pre|p)(\\d*)$");

std::map< std::string, int > getSuffixValues() {
    // EAPI 8 specific sorting weights
    return {
        {"alpha", -4},
        {"beta", -3},
        {"pre", -2},
        {"rc", -1},
        {"p", 0}
    };
}

bool Slipper::Versions::ververify(const std::string& myver) {
    return std::regex_match(myver, VER_REGEX);
}

std::vector< std::string > splitDotted(const std::string& str) {
    std::vector< std::string > result;
    if (str.empty()) return result;
    std::stringstream ss(str.substr(1)); 
    std::string item;
    while (std::getline(ss, item, '.')) {
        result.push_back(item);
    }
    return result;
}

std::vector< std::string > splitSuffixes(const std::string& str) {
    std::vector< std::string > result;
    if (str.empty()) return result;
    std::stringstream ss(str.substr(1)); 
    std::string item;
    while (std::getline(ss, item, '_')) {
        result.push_back(item);
    }
    return result;
}

// Arbitrary precision mathematical comparison to avoid std::stoi boundaries
int compareBigInt(const std::string& a, const std::string& b) {
    if (a.length() > b.length()) return 1;
    if (a.length() < b.length()) return -1;
    return a.compare(b);
}

// Strip trailing zeros for EAPI 8 leading-zero decimal comparisons
std::string stripTrailingZeros(std::string str) {
    while (str.length() > 1 && str.back() == '0') {
        str.pop_back();
    }
    return str;
}

std::optional< int > Slipper::Versions::vercmp(const std::string& ver1, const std::string& ver2) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("vercmp", "Comparing versions: " + ver1 + " against " + ver2);
    }

    if (ver1 == ver2) {
        if (Logger::isDebugEnabled()) Logger::logDebug("vercmp", "Versions are identical strings. Returning 0.");
        return 0;
    }

    std::smatch match1, match2;
    if (!std::regex_match(ver1, match1, VER_REGEX)) {
        Logger::logError("vercmp", "Syntax error in version 1: " + ver1);
        return std::nullopt;
    }
    if (!std::regex_match(ver2, match2, VER_REGEX)) {
        Logger::logError("vercmp", "Syntax error in version 2: " + ver2);
        return std::nullopt;
    }

    // 1. Compare the primary numeric parts (no leading zero rules apply here in PMS)
    int cmp = compareBigInt(match1[1].str(), match2[1].str());
    if (cmp != 0) return cmp;

    // 2. Compare dotted parts component-by-component
    auto vlist1 = splitDotted(match1[2].str());
    auto vlist2 = splitDotted(match2[2].str());
    size_t max_parts = std::max(vlist1.size(), vlist2.size());
    
    for (size_t i = 0; i < max_parts; ++i) {
        std::string p1 = (i < vlist1.size()) ? vlist1[i] : "";
        std::string p2 = (i < vlist2.size()) ? vlist2[i] : "";

        if (p1.empty()) return -1; // v1 has fewer parts, so it's older
        if (p2.empty()) return 1;  // v2 has fewer parts, so it's older

        bool p1_leading_zero = (p1[0] == '0');
        bool p2_leading_zero = (p2[0] == '0');

        if (p1_leading_zero && !p2_leading_zero) return -1;
        if (!p1_leading_zero && p2_leading_zero) return 1;

        if (p1_leading_zero && p2_leading_zero) {
            // EAPI 8: ASCII stringwise with trailing zeros removed
            std::string s1 = stripTrailingZeros(p1);
            std::string s2 = stripTrailingZeros(p2);
            int lex_cmp = s1.compare(s2);
            if (lex_cmp != 0) return (lex_cmp > 0) ? 1 : -1;
        } else {
            // EAPI 8: Standard integer comparison without fixed upper boundaries
            int big_cmp = compareBigInt(p1, p2);
            if (big_cmp != 0) return big_cmp;
        }
    }

    // 3. Compare ASCII single letters
    std::string l1 = match1[3].str();
    std::string l2 = match2[3].str();
    if (!l1.empty() && l2.empty()) return 1;
    if (l1.empty() && !l2.empty()) return -1;
    if (!l1.empty() && !l2.empty() && l1[0] != l2[0]) {
        return (l1[0] > l2[0]) ? 1 : -1;
    }

    // 4. Compare underscore suffixes (alpha, beta, rc, pre, p)
    auto suff1 = splitSuffixes(match1[4].str());
    auto suff2 = splitSuffixes(match2[4].str());
    auto suffix_values = getSuffixValues();
    size_t max_suff = std::max(suff1.size(), suff2.size());
    
    for (size_t i = 0; i < max_suff; ++i) {
        if (i >= suff1.size()) return 1;  // v1 has no more suffixes (v1 > v2)
        if (i >= suff2.size()) return -1; // v2 has no more suffixes (v1 < v2)

        std::smatch s1, s2;
        std::regex_match(suff1[i], s1, SUFFIX_REGEX);
        std::regex_match(suff2[i], s2, SUFFIX_REGEX);

        std::string s1_name = s1[1].str();
        std::string s2_name = s2[1].str();
        
        if (s1_name != s2_name) {
            int a = suffix_values[s1_name];
            int b = suffix_values[s2_name];
            return (a > b) ? 1 : -1;
        }

        std::string s1_val = s1[2].str().empty() ? "0" : s1[2].str();
        std::string s2_val = s2[2].str().empty() ? "0" : s2[2].str();
        int val_cmp = compareBigInt(s1_val, s2_val);
        if (val_cmp != 0) return val_cmp;
    }

    // 5. Compare revisions (-rX)
    std::string r1 = match1[5].matched ? match1[5].str() : "0";
    std::string r2 = match2[5].matched ? match2[5].str() : "0";
    
    int final_result = compareBigInt(r1, r2);
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("vercmp", "Final revision comparison result: " + std::to_string(final_result));
    }
    return final_result;
}