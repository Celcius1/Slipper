#include "../../include/config/MaskManager.hpp"
#include "../../include/Logger.hpp"

using namespace Slipper::Config;

void MaskManager::addMask(const std::string& atom_str) {
    Logger::logDebug("MaskManager::addMask", "Adding mask entry for: " + atom_str);
    Slipper::Dep::Atom mask_atom(atom_str);
    pmask_dict[mask_atom.getCp()].push_back(mask_atom);
}

void MaskManager::addUnmask(const std::string& atom_str) {
    Logger::logDebug("MaskManager::addUnmask", "Adding unmask entry for: " + atom_str);
    Slipper::Dep::Atom unmask_atom(atom_str);
    punmask_dict[unmask_atom.getCp()].push_back(unmask_atom);
}

std::optional< Slipper::Dep::Atom > MaskManager::getMaskAtom(const Slipper::Dep::Atom& pkg) const {
    Logger::logDebug("MaskManager::getMaskAtom", "Checking mask status for: " + pkg.getRawString());
    std::string cp = pkg.getCp();

    // 1. Check if the CP exists in the mask dictionary
    auto mask_it = pmask_dict.find(cp);
    if (mask_it == pmask_dict.end()) {
        Logger::logDebug("MaskManager::getMaskAtom", "No mask entries found for CP: " + cp);
        return std::nullopt; 
    }

    // Evaluate against specific mask atoms (basic CP matching for now until full resolver integration)
    std::optional< Slipper::Dep::Atom > matched_mask = std::nullopt;
    for (const auto& mask_atom : mask_it->second) {
        if (mask_atom.getCp() == cp) {
            matched_mask = mask_atom;
            break;
        }
    }

    if (!matched_mask.has_value()) {
        return std::nullopt;
    }

    // 2. Check if the mask is explicitly overridden by package.unmask
    auto unmask_it = punmask_dict.find(cp);
    if (unmask_it != punmask_dict.end()) {
        for (const auto& unmask_atom : unmask_it->second) {
            if (unmask_atom.getCp() == cp) {
                Logger::logDebug("MaskManager::getMaskAtom", "Package mask overridden by unmask atom: " + unmask_atom.getRawString());
                return std::nullopt;
            }
        }
    }

    Logger::logDebug("MaskManager::getMaskAtom", "Package is actively MASKED by: " + matched_mask.value().getRawString());
    return matched_mask;
}