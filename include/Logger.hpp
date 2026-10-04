#pragma once
#include "iostream"
#include "cstdlib"
#include "string"
#include "fstream"
#include "chrono"
#include "ctime"

class Logger {
private:
    static inline std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
        return std::string(buf);
    }

    static inline void writeToFile(const std::string& level, const std::string& component, const std::string& message) {
        // Atomic append mode ensures forked child processes don't overwrite each other
        std::ofstream log_file("/var/log/slipper/slipper.log", std::ios_base::app);
        if (log_file.is_open()) {
            log_file << "[" << getCurrentTime() << "] [" << level << "] [" << component << "] " << message << "\n";
        }
    }

public:
    // Evaluates if the DEBUG environment variable is set to 1 or true.
    // This perfectly hooks into Docker's environment variable settings.
    static inline bool isDebugEnabled() {
        if (const char* env_p = std::getenv("DEBUG")) {
            std::string debugFlag(env_p);
            return debugFlag == "1" || debugFlag == "true" || debugFlag == "TRUE";
        }
        return false;
    }

    // Use this for step-by-step routine tracking.
    // It will only output to the console and file if the DEBUG flag is active.
    static inline void logDebug(const std::string& routine, const std::string& message) {
        if (isDebugEnabled()) {
            std::cout << "[DEBUG] [" << routine << "] " << message << std::endl;
            writeToFile("DEBUG", routine, message);
        }
    }

    // Standard output for major daemon lifecycle events (always visible).
    static inline void logInfo(const std::string& routine, const std::string& message) {
        std::cout << "[INFO] [" << routine << "] " << message << std::endl;
        writeToFile("INFO", routine, message);
    }

    static inline void logError(const std::string& routine, const std::string& message) {
        std::cerr << "\033[1;31m[ERROR] [" << routine << "] " << message << "\033[0m" << std::endl;
        writeToFile("ERROR", routine, message);
    }
};