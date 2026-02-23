/**
 * main.cpp -- ChronoStore Interactive REPL
 *
 * Usage:  chronostore.exe [--capacity N] [--snapshot FILE] [--no-load]
 *
 * On startup : Loads snapshot if it exists.
 * On EXIT    : Auto-saves snapshot to disk.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "store.h"
#include "command_parser.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

// ---- ANSI colour helpers (Windows 10+ supports VT sequences) ----------------
namespace col {
    const char* reset  = "\033[0m";
    const char* bold   = "\033[1m";
    const char* green  = "\033[32m";
    const char* yellow = "\033[33m";
    const char* red    = "\033[31m";
    const char* cyan   = "\033[36m";
    const char* grey   = "\033[90m";
}

// ---- Enable ANSI on Windows -------------------------------------------------
static void enableAnsi() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004);
#endif
}

// ---- Helpers ----------------------------------------------------------------
static void printBanner() {
    std::cout << col::cyan << col::bold;
    std::cout << "\n";
    std::cout << "   ____  _                        ____  _\n";
    std::cout << "  / ___|| |__  _ __ ___  _ __   / ___|| |_ ___  _ __ ___\n";
    std::cout << " | |    | '_ \\| '__/ _ \\| '_ \\  \\___ \\| __/ _ \\| '__/ _ \\\n";
    std::cout << " | |___ | | | | | | (_) | | | |  ___) | || (_) | | |  __/\n";
    std::cout << "  \\____||_| |_|_|  \\___/|_| |_| |____/ \\__\\___/|_|  \\___|\n";
    std::cout << col::reset;
    std::cout << col::grey;
    std::cout << "  High-Performance In-Memory Key-Value Engine  |  C++17\n";
    std::cout << "  TTL Expiry . LRU Eviction . Snapshot Persistence\n";
    std::cout << col::reset << "\n";
    std::cout << col::yellow << "  Type HELP for available commands.\n" << col::reset << "\n";
}

static void printHelp() {
    std::cout << col::bold << "\n  Commands:\n" << col::reset;
    std::cout << "  +-----------------------------------------------+\n";
    std::cout << "  |  " << col::green << "SET" << col::reset   << "   <key> <value> [EX <seconds>]      |\n";
    std::cout << "  |  " << col::green << "GET" << col::reset   << "   <key>                             |\n";
    std::cout << "  |  " << col::green << "DEL" << col::reset   << "   <key>                             |\n";
    std::cout << "  |  " << col::green << "TTL" << col::reset   << "   <key>   (seconds remaining)       |\n";
    std::cout << "  |  " << col::green << "KEYS" << col::reset  << "  (list all live keys)               |\n";
    std::cout << "  |  " << col::green << "FLUSH" << col::reset << " (delete all keys)                   |\n";
    std::cout << "  |  " << col::green << "STATS" << col::reset << " (engine counters)                   |\n";
    std::cout << "  |  " << col::green << "SAVE" << col::reset  << "  (write snapshot to disk)           |\n";
    std::cout << "  |  " << col::green << "EXIT" << col::reset  << "  (save & quit)                      |\n";
    std::cout << "  +-----------------------------------------------+\n\n";
}

static void printStats(const Stats& s) {
    std::cout << "\n";
    std::cout << col::bold << "  +---- ChronoStore Stats ----------------------+\n" << col::reset;
    std::cout << "  |  Keys      : " << std::setw(10) << s.current_keys
              << " / " << s.capacity << "\n";
    std::cout << "  |  Hits      : " << col::green << std::setw(10) << s.hits   << col::reset << "\n";
    std::cout << "  |  Misses    : " << col::red   << std::setw(10) << s.misses << col::reset << "\n";
    std::cout << "  |  SETs      : " << std::setw(10) << s.sets        << "\n";
    std::cout << "  |  DELs      : " << std::setw(10) << s.dels        << "\n";
    std::cout << "  |  Evictions : " << std::setw(10) << s.evictions   << "\n";
    std::cout << "  |  Expirations: " << std::setw(9) << s.expirations << "\n";
    if (s.hits + s.misses > 0) {
        double ratio = 100.0 * static_cast<double>(s.hits)
                             / static_cast<double>(s.hits + s.misses);
        std::cout << "  |  Hit Ratio : " << std::setw(9) << std::fixed
                  << std::setprecision(1) << ratio << "%\n";
    }
    std::cout << col::bold << "  +--------------------------------------------+\n" << col::reset;
    std::cout << "\n";
}

// ---- Portable file-exists (no <filesystem>) ---------------------------------
static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// ---- Argument parsing -------------------------------------------------------
struct Config {
    size_t      capacity      = KVStore::DEFAULT_CAPACITY;
    std::string snapshot_file = KVStore::SNAPSHOT_FILE;
    bool        no_load       = false;
};

static Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--capacity" || arg == "-c") && i + 1 < argc)
            cfg.capacity = static_cast<size_t>(std::stoul(argv[++i]));
        else if ((arg == "--snapshot" || arg == "-s") && i + 1 < argc)
            cfg.snapshot_file = argv[++i];
        else if (arg == "--no-load")
            cfg.no_load = true;
    }
    return cfg;
}

// ---- Main REPL --------------------------------------------------------------
int main(int argc, char* argv[]) {
    enableAnsi();

    Config cfg = parseArgs(argc, argv);
    printBanner();

    KVStore store(cfg.capacity);

    // Auto-load snapshot on start
    if (!cfg.no_load && fileExists(cfg.snapshot_file)) {
        try {
            store.load(cfg.snapshot_file);
            std::cout << col::green << "  [OK] Snapshot loaded: \""
                      << cfg.snapshot_file << "\" ("
                      << store.size() << " keys)\n" << col::reset;
        } catch (const std::exception& ex) {
            std::cout << col::yellow << "  [WARN] Could not load snapshot: "
                      << ex.what() << col::reset << "\n";
        }
    }

    std::cout << col::grey
              << "  Capacity: " << cfg.capacity
              << " keys  |  Snapshot: " << cfg.snapshot_file
              << col::reset << "\n\n";

    CommandParser parser;
    std::string   line;

    while (true) {
        std::cout << col::cyan << "chronostore" << col::reset
                  << col::grey << " > " << col::reset;
        std::cout.flush();

        if (!std::getline(std::cin, line)) break;

        // Trim whitespace
        auto nf = line.find_first_not_of(" \t\r\n");
        if (nf == std::string::npos) continue;
        line = line.substr(nf, line.find_last_not_of(" \t\r\n") - nf + 1);
        if (line.empty()) continue;

        if (line == "HELP" || line == "help" || line == "?") {
            printHelp();
            continue;
        }

        Command cmd;
        try {
            cmd = parser.parse(line);
        } catch (const std::exception& ex) {
            std::cout << col::red << "  (error) " << ex.what()
                      << col::reset << "\n";
            continue;
        }

        switch (cmd.type) {
            case CommandType::SET: {
                std::string evicted = store.set(cmd.key, cmd.value, cmd.ttl);
                std::cout << col::green << "  OK" << col::reset;
                if (!evicted.empty())
                    std::cout << col::grey << "  [evicted: " << evicted << "]" << col::reset;
                if (cmd.ttl > 0)
                    std::cout << col::grey << "  [TTL: " << cmd.ttl << "s]" << col::reset;
                std::cout << "\n";
                break;
            }
            case CommandType::GET: {
                auto val = store.get(cmd.key);
                if (val)
                    std::cout << col::green << "  \"" << *val << "\"" << col::reset << "\n";
                else
                    std::cout << col::grey << "  (nil)" << col::reset << "\n";
                break;
            }
            case CommandType::DEL: {
                bool existed = store.del(cmd.key);
                if (existed)
                    std::cout << col::green << "  (deleted)" << col::reset << "\n";
                else
                    std::cout << col::grey << "  (key not found)" << col::reset << "\n";
                break;
            }
            case CommandType::TTL: {
                long long t = store.ttl(cmd.key);
                if      (t == -2) std::cout << col::grey   << "  (key does not exist)" << col::reset << "\n";
                else if (t == -1) std::cout << col::cyan   << "  -1 (no expiry)"       << col::reset << "\n";
                else              std::cout << col::yellow << "  " << t << "s remaining" << col::reset << "\n";
                break;
            }
            case CommandType::KEYS: {
                auto ks = store.keys();
                if (ks.empty()) {
                    std::cout << col::grey << "  (empty)" << col::reset << "\n";
                } else {
                    std::cout << "  " << col::bold << ks.size()
                              << col::reset << " key(s):\n";
                    for (size_t i = 0; i < ks.size(); ++i)
                        std::cout << "    " << col::cyan << (i + 1)
                                  << ") " << col::reset << ks[i] << "\n";
                }
                break;
            }
            case CommandType::FLUSH:
                store.flush();
                std::cout << col::yellow << "  (all keys flushed)" << col::reset << "\n";
                break;
            case CommandType::STATS:
                printStats(store.stats());
                break;
            case CommandType::SAVE:
                try {
                    store.save(cfg.snapshot_file);
                    std::cout << col::green << "  Snapshot saved to \""
                              << cfg.snapshot_file << "\"" << col::reset << "\n";
                } catch (const std::exception& ex) {
                    std::cout << col::red << "  (error) " << ex.what()
                              << col::reset << "\n";
                }
                break;
            case CommandType::EXIT:
                try {
                    store.save(cfg.snapshot_file);
                    std::cout << col::green << "  Snapshot saved. Goodbye!\n" << col::reset;
                } catch (...) {
                    std::cout << col::yellow << "  Could not save. Goodbye!\n" << col::reset;
                }
                return 0;
            default:
                std::cout << col::red << "  Unknown command: \"" << cmd.raw
                          << "\". Type HELP." << col::reset << "\n";
        }
    }

    // EOF / Ctrl+Z
    try { store.save(cfg.snapshot_file); } catch (...) {}
    std::cout << "\n" << col::green << "  Snapshot saved. Goodbye!\n" << col::reset;
    return 0;
}
