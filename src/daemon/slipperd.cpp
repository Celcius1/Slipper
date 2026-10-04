#include "iostream"
#include "algorithm"
#include "cctype"
#include "string"
#include "vector"
#include "memory"
#include "fstream"
#include "filesystem"
#include "sstream"
#include "sys/socket.h"
#include "sys/un.h"
#include "unistd.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "signal.h"
#include "cstring"
#include "grp.h"
#include "../../include/Logger.hpp"
#include "../../include/dep/Atom.hpp"
#include "../../include/Versions.hpp"
#include "../../include/repository/RepoConfigLoader.hpp"
#include "../../include/config/ConfigLoader.hpp"
#include "../../include/dbapi/VarDbApi.hpp"
#include "../../include/dbapi/PortDbApi.hpp"
#include "../../include/dbapi/BinDbApi.hpp"
#include "../../include/execution/Executor.hpp"
#include "../../include/resolver/EnvironmentContext.hpp"
#include "../../include/resolver/ResolutionState.hpp"
#include "../../include/resolver/Resolver.hpp"

const std::string C_RESET = "\033[0m";
const std::string C_GREEN = "\033[32m";
const std::string C_YELLOW = "\033[33m";
const std::string C_BLUE = "\033[34m";
const std::string C_CYAN = "\033[36m";
const std::string C_BOLD = "\033[1m";

void initialiseDaemon() {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("initialiseDaemon", "Entering routine: initialiseDaemon.");
        Logger::logDebug("initialiseDaemon", "Checking environment dependencies...");
        Logger::logDebug("initialiseDaemon", "Exiting routine: initialiseDaemon successfully.");
    }
}

std::vector< std::string > tokenizeArgs(const std::string& str) {
    std::vector< std::string > tokens;
    std::stringstream ss(str);
    std::string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int main() {
    if (std::getenv("DEBUG")) {
        Logger::logDebug("slipperd", "Privileged Slipper Daemon initialising IPC socket.");
    }

    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        Logger::logInfo("slipperd", "Fatal: Failed to create IPC socket.");
        return 1;
    }

    std::string socket_path = "/run/slipper/slipper.sock";
    unlink(socket_path.c_str());

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        Logger::logInfo("slipperd", "Fatal: Failed to bind IPC socket.");
        return 1;
    }

    // --- NEW: Explicitly hand socket access to the wheel group ---
    struct group *grp = getgrnam("wheel");
    if (grp != nullptr) {
        chown(socket_path.c_str(), 0, grp->gr_gid);
    }
    chmod(socket_path.c_str(), 0660);
    // -------------------------------------------------------------

    listen(server_sock, 5);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        Logger::logInfo("slipperd", "Fatal: Failed to setup SIGCHLD handler.");
        return 1;
    }

    if (std::getenv("DEBUG")) {
        Logger::logDebug("slipperd", "Daemon listening on /run/slipper/slipper.sock");
    }

    while (true) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) continue;

        if (std::getenv("DEBUG")) {
            Logger::logDebug("slipperd", "Incoming connection accepted. Forking child process.");
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (std::getenv("DEBUG")) {
                Logger::logDebug("slipperd", "Client connection accepted. Resetting SIGCHLD to default in child fork.");
            }
            
            // Restore SIGCHLD so Executor's synchronous waitpid() functions correctly
            struct sigaction sa;
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGCHLD, &sa, NULL);

            close(server_sock);
            dup2(client_sock, STDOUT_FILENO);
            dup2(client_sock, STDERR_FILENO);

            char buffer[1024];
            ssize_t bytes = read(client_sock, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                std::string arg_str(buffer);
                
                auto str_args = tokenizeArgs(arg_str);
                std::vector< char* > argv_vec;
                argv_vec.push_back(const_cast< char* >("slip"));
                for (auto& a : str_args) argv_vec.push_back(const_cast< char* >(a.c_str()));
                
                int argc = argv_vec.size();
                char** argv = argv_vec.data();

                if (argc < 2) {
                    std::cout << "Usage: slip  [options] " << std::endl;
                    std::cout << "Modes:" << std::endl;
                    std::cout << "  --in           Install/Merge packages" << std::endl;
                    std::cout << "  --out          Uninstall/Unmerge packages" << std::endl;
                    std::cout << "\nOptions:" << std::endl;
                    std::cout << "  -a, --ask      Ask for confirmation before slipping" << std::endl;
                    std::cout << "  -v, --verbose  Verbose output" << std::endl;
                    std::cout << "  -u, --update   Update packages to the best version available" << std::endl;
                    std::cout << "  -D, --deep     Consider the entire dependency tree of packages" << std::endl;
                    std::cout << "  -s, --stream   Stream raw bash build output to the console" << std::endl;
                    close(client_sock);
                    exit(1);
                }
                
                std::vector< std::string > targets;
                bool ask = false, verbose = false, update = false, deep = false, stream_output = false;
                std::string mode = "--in"; // Default to install mode for legacy compatibility

                for (int i = 1; i < argc; ++i) {
                    std::string arg = argv[i];
                    if (arg == "--in" || arg == "--out") {
                        mode = arg;
                    } else if (arg.rfind("--", 0) == 0) {
                        if (arg == "--ask") ask = true;
                        else if (arg == "--verbose") verbose = true;
                        else if (arg == "--update") update = true;
                        else if (arg == "--deep") deep = true;
                        else if (arg == "--stream") stream_output = true;
                    } else if (arg[0] == '-') {
                        for (size_t j = 1; j < arg.size(); ++j) {
                            if (arg[j] == 'a') ask = true;
                            else if (arg[j] == 'v') verbose = true;
                            else if (arg[j] == 'u') update = true;
                            else if (arg[j] == 'D') deep = true;
                            else if (arg[j] == 's') stream_output = true;
                        }
                    } else {
                        targets.push_back(arg);
                    }
                }

                if (targets.empty()) {
                    std::cout << "[!] No valid target package specified." << std::endl;
                    close(client_sock);
                    exit(1);
                }

                // ------------------------------------------------------------------
                // UNMERGE MODE (--out)
                // ------------------------------------------------------------------
                if (mode == "--out") {
                    Logger::logInfo("slipperd", "Starting Slipper Unmerge Engine.");
                    std::vector< std::string > dummy_repos;
                    Slipper::Execution::Executor executor(dummy_repos);
                    
                    std::vector< std::string > unmerge_queue;

                    for (const auto& pkg : targets) {
                        std::vector< std::string > local_matches;
                        
                        // 1. Direct Category Match (e.g., app-misc/cmatrix)
                        if (pkg.find('/') != std::string::npos) {
                            try {
                                Slipper::Dep::Atom target_atom(pkg);
                                std::string eroot = "";
                                auto vardb = std::make_shared< Slipper::Dbapi::VarDbApi >(eroot);
                                auto installed_cpvs = vardb->cp_list(target_atom.getCp());
                                
                                for (const auto& cpv : installed_cpvs) {
                                    if (target_atom.getVersion().has_value()) {
                                        Slipper::Dep::Atom installed_atom(cpv);
                                        if (installed_atom.getVersion().value_or("") != target_atom.getVersion().value()) {
                                            continue; 
                                        }
                                    }
                                    local_matches.push_back(cpv);
                                }
                            } catch (const std::exception& e) {
                                std::cout << "[!] Failed to parse qualified atom '" << pkg << "': " << e.what() << std::endl;
                            }
                        } 
                        // 2. Unqualified Package Name Scan (e.g., cmatrix or cmatrix-2.0)
                        else {
                            if (std::getenv("DEBUG")) {
                                Logger::logDebug("slipperd", "Scanning VDB for unqualified package: " + pkg);
                            }
                            
                            std::string vdb_root = "/var/db/pkg";
                            if (std::filesystem::exists(vdb_root)) {
                                for (const auto& cat_entry : std::filesystem::directory_iterator(vdb_root)) {
                                    if (!cat_entry.is_directory()) continue;
                                    std::string cat = cat_entry.path().filename().string();
                                    
                                    for (const auto& pf_entry : std::filesystem::directory_iterator(cat_entry.path())) {
                                        if (!pf_entry.is_directory()) continue;
                                        std::string pf = pf_entry.path().filename().string();
                                        
                                        // Robust Match: Check if the folder name is exactly the package name, 
                                        // or if the folder name starts with the package name followed by a hyphen.
                                        if (pf == pkg || pf.find(pkg + "-") == 0) {
                                            local_matches.push_back(cat + "/" + pf);
                                        }
                                    }
                                }
                            }
                        }

                        // 3. Evaluate Match Results
                        if (local_matches.empty()) {
                            std::cout << "[!] \033[31mError:\033[0m No installed packages found matching '" << pkg << "'." << std::endl;
                        } else if (local_matches.size() > 1 && pkg.find('/') == std::string::npos) {
                            // Multiple matches found for unqualified name (requires user clarification)
                            std::cout << "\n[!] Multiple packages found matching '" << pkg << "':" << std::endl;
                            for (const auto& match : local_matches) {
                                std::cout << "    " << match << std::endl;
                            }
                            std::cout << "Please specify the exact category/package (e.g. app-misc/" << pkg << ") or specific version." << std::endl;
                            close(client_sock);
                            exit(1);
                        } else {
                            // Single exact match found (or multiple versions of a qualified atom)
                            for (const auto& match : local_matches) {
                                unmerge_queue.push_back(match);
                            }
                        }
                    }

                    if (unmerge_queue.empty()) {
                        std::cout << "\nNothing to slip out." << std::endl;
                        close(client_sock);
                        exit(0);
                    }

                    std::cout << "\n" << C_BOLD << "--- Slipper Unmerge Queue ---" << C_RESET << std::endl;
                    for (const auto& cpv : unmerge_queue) {
                        std::cout << "[" << C_YELLOW << "uninstall" << C_RESET << "] " << C_BOLD << cpv << C_RESET << std::endl;
                    }

                    if (ask) {
                        if (std::getenv("DEBUG")) {
                            Logger::logDebug("slipperd", "Prompting user for unmerge confirmation.");
                        }
                        
                        std::cout << "\nWould you like to slip out these packages? [Yes/No]: __SLIP_PROMPT__";
                        std::cout.flush();

                        char resp_buf[256];
                        ssize_t r_bytes = read(client_sock, resp_buf, sizeof(resp_buf) - 1);
                        if (r_bytes > 0) {
                            resp_buf[r_bytes] = '\0';
                            std::string response(resp_buf);
                            if (!response.empty() && response.back() == '\n') response.pop_back();

                            if (response != "Yes" && response != "yes" && response != "Y" && response != "y") {
                                Logger::logInfo("slipperd", "Slip out aborted by user.");
                                close(client_sock);
                                exit(0);
                            }
                        } else {
                            close(client_sock);
                            exit(1);
                        }
                    }

                    for (const auto& cpv : unmerge_queue) {
                        if (!executor.slipOut(cpv)) {
                            std::cout << "[!] Failed to slip out " << cpv << std::endl;
                        }
                    }
                    
                    close(client_sock);
                    exit(0);
                }
                
                // ------------------------------------------------------------------
                // MERGE MODE (--in)
                // ------------------------------------------------------------------
                std::string target_package = targets[0]; 
                Logger::logInfo("slipperd", "Starting Slipper Package Management Daemon for target: " + target_package);
                initialiseDaemon();

                try {
                    std::vector< std::string > repo_paths;
                    Slipper::Repository::RepoConfigLoader repo_loader;
                    
                    std::string repos_conf = "/etc/portage/repos.conf";
                    if (!std::filesystem::exists(repos_conf)) {
                        repos_conf = "/usr/share/portage/config/repos.conf";
                    }

                    if (std::filesystem::exists(repos_conf)) {
                        if (std::filesystem::is_directory(repos_conf)) {
                            for (const auto& entry : std::filesystem::directory_iterator(repos_conf)) {
                                if (entry.is_regular_file()) {
                                    std::ifstream file(entry.path());
                                    std::stringstream buffer;
                                    buffer << file.rdbuf();
                                    repo_loader.loadFromString(buffer.str());
                                }
                            }
                        } else {
                            std::ifstream file(repos_conf);
                            std::stringstream buffer;
                            buffer << file.rdbuf();
                            repo_loader.loadFromString(buffer.str());
                        }
                        
                        auto mapped_repos = repo_loader.getRepos();
                        for (const auto& pair : mapped_repos) {
                            std::string loc = pair.second.location.value_or("");
                            if (!loc.empty()) {
                                repo_paths.push_back(loc);
                            }
                        }
                    }

                    if (repo_paths.empty()) {
                        Logger::logInfo("slipperd", "Warning: No valid repositories found in repos.conf. Defaulting to /var/db/repos/gentoo");
                        repo_paths.push_back("/var/db/repos/gentoo");
                    }

                    // --- NEW: Unqualified package resolution for Merge Mode (--in) ---
                    if (target_package != "@world" && target_package.find('/') == std::string::npos) {
                        if (std::getenv("DEBUG")) {
                            Logger::logDebug("slipperd", "Scanning repositories for unqualified package: " + target_package);
                        }
                        std::vector< std::string > repo_matches;
                        
                        // Strip any leading modifiers (=, <, >, ~) and extract base name for folder matching
                        std::string search_name = target_package;
                        while (!search_name.empty() && (search_name[0] == '=' || search_name[0] == '<' || search_name[0] == '>' || search_name[0] == '~')) {
                            search_name.erase(0, 1);
                        }
                        // Strip version numbers to find the raw repository directory name
                        for (size_t i = 0; i < search_name.length(); ++i) {
                            if (search_name[i] == '-' && i + 1 < search_name.length() && std::isdigit(search_name[i+1])) {
                                search_name = search_name.substr(0, i);
                                break;
                            }
                        }

                        for (const auto& repo_path : repo_paths) {
                            if (!std::filesystem::exists(repo_path)) continue;
                            
                            for (const auto& cat_entry : std::filesystem::directory_iterator(repo_path)) {
                                if (!cat_entry.is_directory()) continue;
                                std::string cat = cat_entry.path().filename().string();
                                
                                // Ignore standard Portage metadata directories
                                if (cat == "metadata" || cat == "profiles" || cat == "eclass" || cat == "scripts") continue;
                                
                                std::string pkg_path = cat_entry.path().string() + "/" + search_name;
                                if (std::filesystem::exists(pkg_path) && std::filesystem::is_directory(pkg_path)) {
                                    // Construct the fully qualified name maintaining any original version modifiers
                                    std::string full_match = cat + "/" + target_package; 
                                    if (std::find(repo_matches.begin(), repo_matches.end(), full_match) == repo_matches.end()) {
                                        repo_matches.push_back(full_match);
                                    }
                                }
                            }
                        }
                        
                        if (repo_matches.empty()) {
                            std::cout << "[!] \033[31mError:\033[0m No ebuilds found in repositories matching '" << target_package << "'." << std::endl;
                            close(client_sock);
                            exit(1);
                        } else if (repo_matches.size() > 1) {
                            std::cout << "\n[!] Multiple packages found matching '" << target_package << "':" << std::endl;
                            for (const auto& match : repo_matches) {
                                std::cout << "    " << match << std::endl;
                            }
                            std::cout << "Please specify the exact category/package (e.g. " << repo_matches[0] << ")." << std::endl;
                            close(client_sock);
                            exit(1);
                        } else {
                            if (std::getenv("DEBUG")) {
                                Logger::logDebug("slipperd", "Resolved unqualified package to: " + repo_matches[0]);
                            }
                            target_package = repo_matches[0];
                        }
                    }
                    // -----------------------------------------------------------------

                    std::string eroot = ""; 
                    auto vardb = std::make_shared< Slipper::Dbapi::VarDbApi >(eroot);
                    auto portdb = std::make_shared< Slipper::Dbapi::PortDbApi >(repo_paths);
                    auto bindb = std::make_shared< Slipper::Dbapi::BinDbApi >("/var/cache/binpkgs");
                    
                    auto use_manager = std::make_shared< Slipper::Config::UseManager >();
                    auto mask_manager = std::make_shared< Slipper::Config::MaskManager >();
                    auto keywords_manager = std::make_shared< Slipper::Config::KeywordsManager >();
                    auto license_manager = std::make_shared< Slipper::Config::LicenseManager >();

                    Slipper::Config::ConfigLoader::loadSystemConfig(use_manager, mask_manager, keywords_manager, license_manager, "/etc/portage");

                    auto env_context = std::make_shared< Slipper::Resolver::EnvironmentContext >(
                        vardb, portdb, bindb, use_manager, mask_manager, keywords_manager, license_manager
                    );
                    
                    Slipper::Resolver::Resolver resolver(env_context);
                    Slipper::Resolver::ResolutionState state;

                    Logger::logInfo("slipperd", "Calculating upgrade graph against live system...");
                    if (update) Logger::logInfo("slipperd", "Update flag (-u) active: Enforcing highest available versions.");
                    if (deep) Logger::logInfo("slipperd", "Deep flag (-D) active: Traversing full dependency tree.");

                    bool resolution_success = false;

                    if (target_package == "@world") {
                        Logger::logInfo("slipperd", "Target is @world. Loading /var/lib/portage/world...");
                        std::vector< Slipper::Dep::Atom > world_atoms;
                        std::ifstream world_file("/var/lib/portage/world");
                        
                        if (world_file.is_open()) {
                            std::string line;
                            while (std::getline(world_file, line)) {
                                if (!line.empty() && line[0] != '#') {
                                    try {
                                        world_atoms.emplace_back(line);
                                    } catch (...) {}
                                }
                            }
                        } else {
                            std::cout << "[!] Could not open /var/lib/portage/world" << std::endl;
                            close(client_sock);
                            exit(1);
                        }
                        resolution_success = resolver.resolve(state, world_atoms);
                    } else {
                        Slipper::Dep::Atom root_atom(target_package);
                        resolution_success = resolver.resolve(state, root_atom);
                    }

                    if (resolution_success) {
                        std::cout << "\n" << C_BOLD << "--- Slipper Resolution Complete ---" << C_RESET << std::endl;
                        std::cout << "Packages scheduled for slipping:" << std::endl;

                        auto all_tracked = state.tracker.getAllPackages();
                        std::vector< std::string > merge_queue;

                        for (const auto& cpv : all_tracked) {
                            Slipper::Dep::Atom cpv_atom(cpv);
                            auto installed = vardb->cp_list(cpv_atom.getCp());

                            std::string status = " N ";
                            std::string color = C_GREEN;
                            bool is_reinstall = false;

                            if (!installed.empty()) {
                                bool is_upgrade = false;
                                bool is_downgrade = false;

                                for (const auto& inst : installed) {
                                    if (inst == cpv) {
                                        is_reinstall = true;
                                    } else {
                                        Slipper::Dep::Atom inst_atom(inst);
                                        auto cmp = Slipper::Versions::vercmp(inst_atom.getVersion().value_or(""), cpv_atom.getVersion().value_or(""));
                                        if (cmp.has_value()) {
                                            if (cmp.value() < 0) is_upgrade = true;
                                            if (cmp.value() > 0) is_downgrade = true;
                                        }
                                    }
                                }

                                if (is_reinstall) {
                                    status = " R ";
                                    color = C_YELLOW;
                                } else if (is_upgrade) {
                                    status = " U ";
                                    color = C_BLUE;
                                } else if (is_downgrade) {
                                    status = " UD";
                                    color = C_CYAN;
                                }
                            }

                            if (update && is_reinstall) {
                                continue; 
                            }

                            merge_queue.push_back(cpv);

                            std::string use_display = "";
                            if (verbose) {
                                auto active_use = use_manager->getPUSE(cpv_atom);
                                auto iuse_meta = portdb->aux_get(cpv, {"IUSE"});
                                
                                std::stringstream ss(iuse_meta[0]);
                                std::string flag;
                                use_display = " USE=\"";
                                
                                while (ss >> flag) {
                                    if (flag[0] == '+' || flag[0] == '-') flag = flag.substr(1);
                                    
                                    bool is_active = (active_use.find(flag) != active_use.end());
                                    if (is_active) {
                                        use_display += C_GREEN + flag + C_RESET + " ";
                                    } else {
                                        use_display += C_BLUE + "-" + flag + C_RESET + " ";
                                    }
                                }
                                if (use_display.back() == ' ') use_display.pop_back();
                                use_display += "\"";
                            }

                            std::cout << "[" << color << "ebuild" << C_RESET << "  " 
                                      << color << status << C_RESET << "] " 
                                      << color << C_BOLD << cpv << C_RESET << use_display << std::endl;
                        }
                        
                        if (ask && !merge_queue.empty()) {
                            if (std::getenv("DEBUG")) {
                                Logger::logDebug("slipperd", "Prompting user for confirmation via IPC token.");
                            }
                            
                            std::cout << "\nWould you like to slip in these packages? [Yes/No]: __SLIP_PROMPT__";
                            std::cout.flush();

                            char resp_buf[256];
                            // Wait for the client to write back to the socket
                            ssize_t r_bytes = read(client_sock, resp_buf, sizeof(resp_buf) - 1);
                            if (r_bytes > 0) {
                                resp_buf[r_bytes] = '\0';
                                std::string response(resp_buf);
                                
                                // Strip the trailing newline from the client response
                                if (!response.empty() && response.back() == '\n') {
                                    response.pop_back();
                                }

                                if (std::getenv("DEBUG")) {
                                    Logger::logDebug("slipperd", "Received IPC response: " + response);
                                }

                                if (response == "Yes" || response == "yes" || response == "Y" || response == "y") {
                                    Logger::logInfo("slipperd", "Initiating slip in process...");
                                    
                                    // Handoff to the C++ Execution Engine
                                    Slipper::Execution::Executor executor(repo_paths);
                                    
                                    // MODIFIED: Pass the stream_output boolean into the engine
                                    executor.slipQueue(merge_queue, stream_output);
                                    
                                } else {
                                    Logger::logInfo("slipperd", "Slip aborted by user.");
                                }
                            }
                        } else if (merge_queue.empty()) {
                            std::cout << "\nNothing to slip in. System is up to date." << std::endl;
                        }
                    } else {
                        std::cout << "\n" << C_BOLD << "[!] Slipper failed to resolve dependencies." << C_RESET << std::endl;
                    }

                } catch (const std::exception& e) {
                    std::cout << "[!] Fatal exception in resolution: " << e.what() << std::endl;
                }
            }
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
        }
    }

    close(server_sock);
    unlink(socket_path.c_str());
    return 0;
}