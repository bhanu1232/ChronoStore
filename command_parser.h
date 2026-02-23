#pragma once
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * CommandType — each recognised CLI operation.
 */
enum class CommandType {
    SET,
    GET,
    DEL,
    STATS,
    SAVE,
    TTL,
    KEYS,
    FLUSH,
    EXIT,
    UNKNOWN
};

/**
 * Command — parsed representation of a user command.
 *
 * Examples:
 *   SET name Bhanu          → type=SET, key="name", value="Bhanu", ttl=-1
 *   SET name Bhanu EX 30    → type=SET, key="name", value="Bhanu", ttl=30
 *   GET name                → type=GET, key="name"
 *   DEL name                → type=DEL, key="name"
 *   STATS                   → type=STATS
 *   SAVE                    → type=SAVE
 *   TTL name                → type=TTL, key="name"
 *   KEYS                    → type=KEYS (list all keys)
 *   FLUSH                   → type=FLUSH (clear all keys)
 *   EXIT                    → type=EXIT
 */
struct Command {
    CommandType type  = CommandType::UNKNOWN;
    std::string key;
    std::string value;
    long long   ttl   = -1; // seconds; -1 means no expiry
    std::string raw;        // original input for error messages
};

/**
 * CommandParser — tokenises raw input strings into Command structs.
 *
 * Parsing is case-insensitive for the command verb only.
 * Keys and values are case-sensitive.
 */
class CommandParser {
public:
    Command parse(const std::string& input) const {
        Command cmd;
        cmd.raw = input;

        std::vector<std::string> tokens = tokenise(input);
        if (tokens.empty()) return cmd;

        std::string verb = toUpper(tokens[0]);

        if (verb == "SET") {
            if (tokens.size() < 3) {
                throw std::invalid_argument("Usage: SET <key> <value> [EX <seconds>]");
            }
            cmd.type  = CommandType::SET;
            cmd.key   = tokens[1];
            cmd.value = tokens[2];
            // Optional: EX <seconds>
            if (tokens.size() >= 5 && toUpper(tokens[3]) == "EX") {
                try {
                    cmd.ttl = std::stoll(tokens[4]);
                    if (cmd.ttl <= 0) throw std::invalid_argument("TTL must be positive");
                } catch (const std::exception&) {
                    throw std::invalid_argument("Invalid TTL value: " + tokens[4]);
                }
            }
        } else if (verb == "GET") {
            if (tokens.size() < 2) throw std::invalid_argument("Usage: GET <key>");
            cmd.type = CommandType::GET;
            cmd.key  = tokens[1];
        } else if (verb == "DEL" || verb == "DELETE") {
            if (tokens.size() < 2) throw std::invalid_argument("Usage: DEL <key>");
            cmd.type = CommandType::DEL;
            cmd.key  = tokens[1];
        } else if (verb == "TTL") {
            if (tokens.size() < 2) throw std::invalid_argument("Usage: TTL <key>");
            cmd.type = CommandType::TTL;
            cmd.key  = tokens[1];
        } else if (verb == "KEYS") {
            cmd.type = CommandType::KEYS;
        } else if (verb == "FLUSH") {
            cmd.type = CommandType::FLUSH;
        } else if (verb == "STATS") {
            cmd.type = CommandType::STATS;
        } else if (verb == "SAVE") {
            cmd.type = CommandType::SAVE;
        } else if (verb == "EXIT" || verb == "QUIT" || verb == "Q") {
            cmd.type = CommandType::EXIT;
        } else {
            cmd.type = CommandType::UNKNOWN;
        }

        return cmd;
    }

private:
    // Split on whitespace
    std::vector<std::string> tokenise(const std::string& s) const {
        std::vector<std::string> tokens;
        std::istringstream iss(s);
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        return tokens;
    }

    std::string toUpper(std::string s) const {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }
};
