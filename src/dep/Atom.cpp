#include "../../include/dep/Atom.hpp"
#include "../../include/Logger.hpp"

// System headers using double quotes to prevent UI stripping
#include "regex"
#include "stdexcept"
#include "sstream"

using namespace Slipper::Dep;

// ---------------------------------------------------------
// CONSTRUCTOR & MAIN PARSER ROUTINE
// ---------------------------------------------------------
Atom::Atom(const std::string& raw_atom) : raw_string(raw_atom) {
    Logger::logDebug("Atom::Constructor", "Initialising atom parsing for: " + raw_string);
    parse();
    Logger::logDebug("Atom::Constructor", "Successfully parsed atom: " + getCp());
}

void Atom::parse() {
    std::string working_str = raw_string;

    // Execute sequential parsing routines, stripping the working string down at each step
    parseBlocker(working_str);
    parseOperator(working_str);
    parseUseDeps(working_str);
    parseSlotAndRepo(working_str);

    // The remaining string should just be the Category/Package (and optionally Version)
    auto slash_pos = working_str.find('/');
    if (slash_pos == std::string::npos) {
        throw std::invalid_argument("Invalid atom (missing category): " + raw_string);
    }
    
    category = working_str.substr(0, slash_pos);
    std::string pkg_and_version = working_str.substr(slash_pos + 1);

    // Extract version suffix using regex. 
    // This finds the first dash followed by a digit as the version boundary.
    std::smatch match;
    std::regex version_regex("-([0-9].*)");
    if (std::regex_search(pkg_and_version, match, version_regex)) {
        package_name = pkg_and_version.substr(0, match.position());
        version = match[1].str();
        Logger::logDebug("Atom::parse", "Extracted Package: " + package_name + ", Version: " + version.value());
    } else {
        package_name = pkg_and_version;
        Logger::logDebug("Atom::parse", "Extracted Package: " + package_name + " (Unversioned)");
    }
}

// ---------------------------------------------------------
// BLOCKER PARSER ROUTINE
// ---------------------------------------------------------
void Atom::parseBlocker(std::string& working_str) {
    if (working_str.rfind("!!", 0) == 0) {
        blocker = Blocker::Strong;
        working_str = working_str.substr(2);
        Logger::logDebug("Atom::parseBlocker", "Detected Strong Blocker (!!).");
    } else if (working_str.rfind("!", 0) == 0) {
        blocker = Blocker::Weak;
        working_str = working_str.substr(1);
        Logger::logDebug("Atom::parseBlocker", "Detected Weak Blocker (!).");
    }
}

// ---------------------------------------------------------
// OPERATOR PARSER ROUTINE
// ---------------------------------------------------------
void Atom::parseOperator(std::string& working_str) {
    if (working_str.rfind(">=", 0) == 0) {
        op = Operator::GreaterEqual;
        working_str = working_str.substr(2);
    } else if (working_str.rfind("<=", 0) == 0) {
        op = Operator::LessEqual;
        working_str = working_str.substr(2);
    } else if (working_str.rfind("=", 0) == 0) {
        op = Operator::Equal;
        working_str = working_str.substr(1);
        // Glob check: flag the intent if we see an asterisk suffix
        if (!working_str.empty() && working_str.back() == '*') {
            op = Operator::GlobEqual;
            working_str.pop_back();
        }
    } else if (working_str.rfind(">", 0) == 0) {
        op = Operator::Greater;
        working_str = working_str.substr(1);
    } else if (working_str.rfind("<", 0) == 0) {
        op = Operator::Less;
        working_str = working_str.substr(1);
    } else if (working_str.rfind("~", 0) == 0) {
        op = Operator::Tilde;
        working_str = working_str.substr(1);
    }
    
    if (op != Operator::None) {
        Logger::logDebug("Atom::parseOperator", "Operator detected and stripped.");
    }
}

// ---------------------------------------------------------
// USE DEPENDENCIES PARSER ROUTINE
// ---------------------------------------------------------
void Atom::parseUseDeps(std::string& working_str) {
    auto bracket_pos = working_str.find('[');
    if (bracket_pos != std::string::npos) {
        // Find the matching closing bracket instead of assuming it's at the very end
        auto close_bracket_pos = working_str.find(']', bracket_pos);
        if (close_bracket_pos == std::string::npos) {
            throw std::invalid_argument("Invalid USE dependency syntax (missing closing bracket): " + raw_string);
        }
        
        // Extract just the flags inside the brackets
        std::string use_str = working_str.substr(bracket_pos + 1, close_bracket_pos - bracket_pos - 1);
        
        // Erase the entire [...] block from the working string so subsequent parsers don't trip on it
        working_str.erase(bracket_pos, close_bracket_pos - bracket_pos + 1);
        
        std::stringstream ss(use_str);
        std::string flag;
        while (std::getline(ss, flag, ',')) {
            if (!flag.empty()) {
                use_deps.push_back(flag);
            }
        }
        Logger::logDebug("Atom::parseUseDeps", "Parsed " + std::to_string(use_deps.size()) + " USE flags.");
    }
}

// ---------------------------------------------------------
// SLOT AND REPO PARSER ROUTINE
// ---------------------------------------------------------
void Atom::parseSlotAndRepo(std::string& working_str) {
    // Check for repo ::
    auto repo_pos = working_str.find("::");
    if (repo_pos != std::string::npos) {
        repo = working_str.substr(repo_pos + 2);
        working_str = working_str.substr(0, repo_pos);
        Logger::logDebug("Atom::parseSlotAndRepo", "Extracted Repo: " + repo.value());
    }

    // Check for slot :
    auto slot_pos = working_str.find(':');
    if (slot_pos != std::string::npos) {
        std::string slot_str = working_str.substr(slot_pos + 1);
        working_str = working_str.substr(0, slot_pos);

        // Check for slot operator at the end of the slot string
        if (!slot_str.empty()) {
            if (slot_str.back() == '=') {
                slot_operator = SlotOperator::Equal;
                slot_str.pop_back();
            } else if (slot_str.back() == '*') {
                slot_operator = SlotOperator::Asterisk;
                slot_str.pop_back();
            }
        }

        // Check for sub-slot /
        auto sub_slot_pos = slot_str.find('/');
        if (sub_slot_pos != std::string::npos) {
            slot = slot_str.substr(0, sub_slot_pos);
            sub_slot = slot_str.substr(sub_slot_pos + 1);
            Logger::logDebug("Atom::parseSlotAndRepo", "Extracted Slot: " + slot.value() + ", Sub-Slot: " + sub_slot.value());
        } else {
            slot = slot_str;
            Logger::logDebug("Atom::parseSlotAndRepo", "Extracted Slot: " + slot.value());
        }
    }
}