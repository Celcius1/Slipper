#include "../../include/resolver/Resolver.hpp"
#include "../../include/Versions.hpp"
#include "../../include/Logger.hpp"
#include "sstream"
#include "cstdlib"
#include "algorithm" 

using namespace Slipper::Resolver;

Resolver::Resolver(std::shared_ptr< EnvironmentContext > env) : env_context(env) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Resolver::Constructor", "Resolver engine instantiated with immutable environment context.");
    }
}

bool Resolver::resolve(ResolutionState& state, const Slipper::Dep::Atom& root_atom) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Resolver::resolve", "Kicking off dependency resolution for: " + root_atom.getRawString());
    }
    state.queueDependency(root_atom);
    return createGraph(state);
}

// Add this directly below the existing resolve(ResolutionState&, const Slipper::Dep::Atom&) function:
bool Resolver::resolve(ResolutionState& state, const std::vector< Slipper::Dep::Atom >& root_atoms) {
    if (Logger::isDebugEnabled()) {
        Logger::logDebug("Resolver::resolve", "Kicking off dependency resolution for multiple targets (@world).");
    }
    
    // Push all requested targets onto the stack before generating the graph
    for (const auto& atom : root_atoms) {
        state.queueDependency(atom);
    }
    
    return createGraph(state);
}

bool Resolver::createGraph(ResolutionState& state) {
    while (state.hasPendingDependencies()) {
        
        // ---------------------------------------------------------
        // PRIMARY STACK PROCESSING (Mandatory Dependencies)
        // ---------------------------------------------------------
        if (!state.dep_stack.empty()) {
            Slipper::Dep::Atom current_atom = state.dep_stack.top();
            state.dep_stack.pop();

            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::createGraph", "Evaluating dependencies for: " + current_atom.getRawString());
            }

            std::string target_cpv = getBestVisible(current_atom);
            if (target_cpv.empty()) {
                Logger::logInfo("Resolver::createGraph", "Resolution Failed: Cannot find ebuild for " + current_atom.getRawString());
                return false;
            }

            if (state.tracker.contains(target_cpv)) continue;

            state.tracker.addPkg(target_cpv);

            Slipper::Dep::Atom target_atom(target_cpv);
            auto active_use = env_context->use_manager->getPUSE(target_atom);

            std::vector< std::string > wants = {"DEPEND", "RDEPEND"};
            auto metadata = env_context->portdb->aux_get(target_cpv, wants);
            std::string combined_deps = metadata[0] + " " + metadata[1];
            
            auto parsed_deps = parseDependencies(combined_deps, active_use);

            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::createGraph", "Queuing " + std::to_string(parsed_deps.mandatory.size()) + " mandatory and " + std::to_string(parsed_deps.disjunctive.size()) + " OR blocks for " + target_cpv);
            }

            // Queue mandatory dependencies
            for (const auto& dep : parsed_deps.mandatory) {
                if (dep.getBlocker() != Slipper::Dep::Blocker::None) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Resolver::createGraph", "Blocker segregated: " + dep.getRawString());
                    }
                    continue;
                }

                auto installed = env_context->vardb->cp_list(dep.getCp());
                if (!installed.empty()) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Resolver::createGraph", "Dependency satisfied by VDB (installed): " + dep.getCp());
                    }
                    continue;
                }
                state.queueDependency(dep);
            }

            // Queue disjunctive dependencies (|| blocks) to the secondary stack
            for (const auto& or_group : parsed_deps.disjunctive) {
                state.dep_disjunctive_stack.push(or_group);
            }
        } 
        // ---------------------------------------------------------
        // DISJUNCTIVE STACK PROCESSING (OR Dependencies)
        // ---------------------------------------------------------
        else if (!state.dep_disjunctive_stack.empty()) {
            auto or_group = state.dep_disjunctive_stack.top();
            state.dep_disjunctive_stack.pop();
            
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::createGraph", "Evaluating OR block with " + std::to_string(or_group.size()) + " options.");
            }

            bool satisfied = false;

            // Priority 1: Check if any option is already installed in the VDB
            for (const auto& opt : or_group) {
                if (!env_context->vardb->cp_list(opt.getCp()).empty()) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Resolver::createGraph", "OR block satisfied by installed package: " + opt.getCp());
                    }
                    satisfied = true;
                    break;
                }
            }
            if (satisfied) continue;

            // Priority 2: Check if any option is already selected in the current graph
            for (const auto& opt : or_group) {
                if (!state.tracker.match(opt).empty()) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Resolver::createGraph", "OR block satisfied by tracked package: " + opt.getCp());
                    }
                    satisfied = true;
                    break;
                }
            }
            if (satisfied) continue;

            // Priority 3: Fallback to the first available ebuild in the repositories
            for (const auto& opt : or_group) {
                std::string target_cpv = getBestVisible(opt);
                if (!target_cpv.empty()) {
                    if (Logger::isDebugEnabled()) {
                        Logger::logDebug("Resolver::createGraph", "OR block defaulting to first available option: " + opt.getRawString());
                    }
                    state.queueDependency(opt); // Queue it back to the primary stack
                    satisfied = true;
                    break;
                }
            }

            if (!satisfied) {
                Logger::logInfo("Resolver::createGraph", "Resolution Failed: Unresolvable OR block.");
                return false;
            }
        }
    }
    return true;
}

std::string Resolver::getBestVisible(const Slipper::Dep::Atom& atom) const {
    std::vector< std::string > available = env_context->portdb->cp_list(atom.getCp());
    if (available.empty()) return "";

    std::vector< std::string > unmasked_available;
    
    bool explicit_live_request = false;
    if (atom.getVersion().has_value()) {
        std::string req_ver = atom.getVersion().value();
        if (req_ver == "9999" || req_ver == "99999999") {
            explicit_live_request = true;
        }
    }

    // Baseline system architecture (e.g. amd64)
    // In a full implementation, this is pulled from profile/make.defaults, but for now we define it locally.
    std::string system_arch = "amd64"; 

    // 1. Filter out masked, unstable, and unlicensed packages
    for (const auto& cpv : available) {
        Slipper::Dep::Atom cpv_atom(cpv);
        
        // A. Live Ebuild Filter
        if (!explicit_live_request) {
            std::string ver = cpv_atom.getVersion().value_or("");
            if (ver == "9999" || ver == "99999999") {
                if (Logger::isDebugEnabled()) {
                    Logger::logDebug("Resolver::getBestVisible", "Skipped live ebuild during standard resolution: " + cpv);
                }
                continue;
            }
        }
        
        // B. package.mask Filter
        auto mask_result = env_context->mask_manager->getMaskAtom(cpv_atom);
        if (mask_result.has_value()) {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::getBestVisible", "Dropped masked package: " + cpv + " (Masked by: " + mask_result.value().getRawString() + ")");
            }
            continue;
        }

        // Fetch repository metadata for Keyword and License evaluation
        std::vector< std::string > wants = {"KEYWORDS", "LICENSE"};
        auto metadata = env_context->portdb->aux_get(cpv, wants);
        std::string ebuild_keywords = metadata[0];
        std::string ebuild_license = metadata[1];

        // C. KEYWORDS Filter
        // We must ensure the ebuild's keywords match the user's ACCEPT_KEYWORDS.
        bool keyword_accepted = false;
        
        // Get accepted keywords for this specific package
        auto accepted_keywords = env_context->keywords_manager->getKeywords(cpv_atom);
        
        // NEW: Fetch global keywords from make.conf (mapped to */*)
        Slipper::Dep::Atom global_atom("*/*");
        auto global_keywords = env_context->keywords_manager->getKeywords(global_atom);
        accepted_keywords.insert(accepted_keywords.end(), global_keywords.begin(), global_keywords.end());
        
        // Default Portage fallback: if nothing is configured, only accept stable system arch
        if (accepted_keywords.empty()) {
            accepted_keywords.push_back(system_arch);
        }

        std::stringstream kw_stream(ebuild_keywords);
        std::string kw;
        while (kw_stream >> kw) {
            if (kw == "-*") {
                // Ebuild explicitly drops all archs. We only proceed if a specific override exists.
                // (Handled by checking if our accepted_keywords explicitly permits this arch).
                continue; 
            }
            
            // If the ebuild is marked stable for our arch (e.g. "amd64") 
            // AND we accept stable arch (which is default).
            if (kw == system_arch && std::find(accepted_keywords.begin(), accepted_keywords.end(), system_arch) != accepted_keywords.end()) {
                keyword_accepted = true;
                break;
            }
            
            // If the ebuild is testing for our arch (e.g. "~amd64") 
            // AND we explicitly accept testing for this package.
            std::string testing_arch = "~" + system_arch;
            if (kw == testing_arch && (
                std::find(accepted_keywords.begin(), accepted_keywords.end(), testing_arch) != accepted_keywords.end() ||
                std::find(accepted_keywords.begin(), accepted_keywords.end(), "**") != accepted_keywords.end() ||
                std::find(accepted_keywords.begin(), accepted_keywords.end(), "~*") != accepted_keywords.end()
            )) {
                keyword_accepted = true;
                break;
            }
            
            // If the user accepts ALL keywords (e.g. "**")
            if (std::find(accepted_keywords.begin(), accepted_keywords.end(), "**") != accepted_keywords.end()) {
                keyword_accepted = true;
                break;
            }
        }

        if (!keyword_accepted) {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::getBestVisible", "Dropped architecture-masked package: " + cpv + " (Ebuild KEYWORDS: " + ebuild_keywords + ")");
            }
            continue;
        }

        // D. LICENSE Filter
        // Note: Full license parsing requires evaluating || () and () logic blocks.
        // For this baseline, we verify the user hasn't explicitly masked it, or if they have a global blanket.
        bool license_accepted = true;
        auto accepted_licenses = env_context->license_manager->getAcceptedLicenses(cpv_atom);
        
        // If they accept all licenses globally (ACCEPT_LICENSE="*"), it is permitted.
        if (accepted_licenses.find("*") == accepted_licenses.end()) {
            // Simplified check: Ensure at least one token from the ebuild is in the accepted set.
            std::stringstream lic_stream(ebuild_license);
            std::string lic_token;
            bool found_valid = false;
            while (lic_stream >> lic_token) {
                if (lic_token == "||" || lic_token == "(" || lic_token == ")") continue;
                if (accepted_licenses.find(lic_token) != accepted_licenses.end()) {
                    found_valid = true;
                    break;
                }
            }
            if (!found_valid && !ebuild_license.empty()) {
                // If it requires a license and we didn't explicitly accept it, it's blocked.
                // (Note: To mirror Gentoo exactly, we would need to check against the @FREE group profile default).
                license_accepted = false; 
            }
        }

        if (!license_accepted) {
            if (Logger::isDebugEnabled()) {
                Logger::logDebug("Resolver::getBestVisible", "Dropped license-masked package: " + cpv + " (Ebuild LICENSE: " + ebuild_license + ")");
            }
            continue;
        }

        // Ebuild passed all masks, keywords, and license checks!
        unmasked_available.push_back(cpv);
    }

    if (unmasked_available.empty()) {
        Logger::logInfo("Resolver::getBestVisible", "All available ebuilds for " + atom.getCp() + " are masked by package.mask, KEYWORDS, or LICENSE.");
        return "";
    }

    // 2. Mathematical version sorting to guarantee the highest unmasked version is selected
    std::string best_match = unmasked_available[0];
    for (size_t i = 1; i < unmasked_available.size(); ++i) {
        Slipper::Dep::Atom a1(best_match);
        Slipper::Dep::Atom a2(unmasked_available[i]);
        
        auto v1 = a1.getVersion().value_or("");
        auto v2 = a2.getVersion().value_or("");

        if (!v1.empty() && !v2.empty()) {
            auto cmp = Slipper::Versions::vercmp(v1, v2);
            if (cmp.has_value() && cmp.value() < 0) {
                best_match = unmasked_available[i];
            }
        }
    }
    return best_match;
}

ParsedDeps Resolver::parseDependencies(const std::string& dep_string, const std::unordered_set< std::string >& active_use) const {
    ParsedDeps result;
    std::stringstream ss(dep_string);
    std::string token;

    // Context Stack tracks the active parsing state
    // 0 = Mandatory, 1 = OR block (||), 2 = Skipped (failed USE condition)
    std::vector< int > context_stack;
    context_stack.push_back(0); 

    bool next_is_or = false;
    bool skip_next_block = false;
    std::vector< Slipper::Dep::Atom > current_or_group;

    while (ss >> token) {
        // 1. Detect OR operator
        if (token == "||") {
            next_is_or = true;
            continue;
        }

        // 2. Evaluate USE conditions (e.g., flag? or !flag?)
        if (token.back() == '?') {
            std::string flag = token.substr(0, token.size() - 1);
            bool is_negative = (!flag.empty() && flag[0] == '!');
            if (is_negative) flag = flag.substr(1);

            bool condition_met = is_negative ? (active_use.find(flag) == active_use.end()) 
                                             : (active_use.find(flag) != active_use.end());

            // If the condition fails, we flag the next block to be skipped
            if (!condition_met) skip_next_block = true;
            continue;
        }

        // 3. Handle Block Openings
        if (token == "(") {
            if (context_stack.back() == 2 || skip_next_block) {
                context_stack.push_back(2); // Inherit skipped state
                skip_next_block = false;
                next_is_or = false;
            } else if (next_is_or) {
                context_stack.push_back(1); // Enter active OR block
                next_is_or = false;
            } else {
                context_stack.push_back(0); // Enter standard nested block
            }
            continue;
        }

        // 4. Handle Block Closings
        if (token == ")") {
            if (context_stack.size() > 1) {
                int ending_context = context_stack.back();
                context_stack.pop_back();

                // If we just successfully closed an active OR block, commit the group
                if (ending_context == 1 && !current_or_group.empty()) {
                    result.disjunctive.push_back(current_or_group);
                    current_or_group.clear();
                }
            }
            continue;
        }

        // 5. Normal Token Processing
        // Only evaluate the atom if we are not inside a skipped context
        if (context_stack.back() != 2) {
            
            // Strip inline USE dependencies (e.g., [ssl, -static]) to prevent Atom parsing failures
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                token = token.substr(0, bracket_pos);
            }

            try {
                // Check if we are inside an active OR block ANYWHERE in the current stack
                bool in_active_or = false;
                for (auto it = context_stack.rbegin(); it != context_stack.rend(); ++it) {
                    if (*it == 1) { in_active_or = true; break; }
                }

                if (in_active_or) {
                    current_or_group.emplace_back(token);
                } else {
                    result.mandatory.emplace_back(token);
                }
            } catch (...) {
                if (Logger::isDebugEnabled()) {
                    Logger::logDebug("Resolver::parseDependencies", "Ignored malformed atom token: " + token);
                }
            }
        }
    }
    return result;
}