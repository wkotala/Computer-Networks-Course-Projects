#include "arg_parser.h"

#include <unistd.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "constants.h"

// ArgParser

int ArgParser::parseAndValidateInt(const char* str, unsigned long min, unsigned long max) {
    char* endptr;
    errno = 0;
    unsigned long value = std::strtoul(str, &endptr, 10);
    if (errno != 0 || *endptr != 0 || value < min || value > max) {
        printUsage();
        fatal("%s is not a valid integer in the range [%d, %d]", str, min, max);
    }
    return (int)value;
}

uint16_t ArgParser::parseAndValidatePort(const char* str, bool allow_zero) {
    return parseAndValidateInt(str, allow_zero ? 0 : 1, UINT16_MAX);
}

[[noreturn]] void ArgParser::handle_getopt_error(char getopt_return_char, int optopt_val) {
    printUsage();
    if (getopt_return_char == ':') {
        fatal("Option -%c requires an argument", optopt_val);
    } else if (getopt_return_char == '?') {
        if (isprint(optopt_val)) {
            fatal("Unknown option -%c", optopt_val);
        } else {
            fatal("Unknown option character with ASCII code 0x%x", optopt_val);
        }
    }
    fatal("Error parsing arguments");
}

// ClientArgParser

ClientArgParser::ClientArgParser(int argc, char* argv[]) : ArgParser(argc, argv) {
    parse();
    validate();
}

void ClientArgParser::printUsage() const {
    error("Usage: %s -u player_id -s server -p port [-4] [-6] [-a]", argv[0]);
}

void ClientArgParser::logInfo() const {
    std::cout << "Starting with id '" << getPlayerId() << "' on server [" << getServerAddress()
              << "]:" << getServerPort();

    if (isIPv4Forced())
        std::cout << " forcing IPv4";
    if (isIPv6Forced())
        std::cout << " forcing IPv6";
    if (isAutoStrategy())
        std::cout << " using auto strategy";
    else
        std::cout << " reading from stdin";

    std::cout << "." << std::endl;
}

void ClientArgParser::parse() {
    int opt;

    while ((opt = getopt(argc, argv, ":u:s:p:46a")) != -1) {
        switch (opt) {
            case 'u':
                player_id = std::string(optarg);
                player_id_set = true;
                break;
            case 's':
                server_address = std::string(optarg);
                server_address_set = true;
                break;
            case 'p':
                server_port = parseAndValidatePort(optarg, false);
                server_port_set = true;
                break;
            case '4': force_ipv4 = true; break;
            case '6': force_ipv6 = true; break;
            case 'a': auto_strategy = true; break;
            default: handle_getopt_error(opt, optopt); break;
        }
    }

    if (force_ipv4 && force_ipv6) { // Cannot force both IPv4 and IPv6
        force_ipv4 = force_ipv6 = false;
    }
}

void ClientArgParser::validate() {
    if (optind < argc) {
        printUsage();
        fatal("Extra argument: %s", argv[optind]);
    }

    if (!player_id_set || player_id.empty()) {
        printUsage();
        fatal("Player ID (-u) is required");
    }
    if (!server_address_set || server_address.empty()) {
        printUsage();
        fatal("Server address (-s) is required");
    }
    if (!server_port_set) {
        printUsage();
        fatal("Server port (-p) is required");
    }

    for (const char c : player_id) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            printUsage();
            fatal("Player ID (-u) must contain only alphanumeric characters");
        }
    }
}

// ServerArgParser

void ServerArgParser::printUsage() const {
    error("Usage: %s [-p port] [-k value] [-n value] [-m value] -f file", argv[0]);
}

void ServerArgParser::logInfo() const {
    std::cout << "Starting with port=";
    if (getPort() == 0) {
        std::cout << "any";
    } else {
        std::cout << getPort();
    }

    std::cout << ", k=" << getK() << ", n=" << getN() << ", m=" << getM() << ", file='"
              << getFile() << "'." << std::endl;
}

ServerArgParser::ServerArgParser(int argc, char* argv[]) : ArgParser(argc, argv) {
    parseAndValidate();
}

void ServerArgParser::parseAndValidate() {
    int opt;

    while ((opt = getopt(argc, argv, ":p:k:n:m:f:")) != -1) {
        switch (opt) {
            case 'p': port = parseAndValidatePort(optarg, true); break;
            case 'k': k = parseAndValidateInt(optarg, 1, constants::max_k); break;
            case 'n': n = parseAndValidateInt(optarg, 1, constants::max_n); break;
            case 'm': m = parseAndValidateInt(optarg, 1, constants::max_m); break;
            case 'f':
                file = std::string(optarg);
                file_set = true;
                break;
            default: handle_getopt_error(opt, optopt); break;
        }
    }

    if (optind < argc) {
        printUsage();
        fatal("Extra argument: %s", argv[optind]);
    }

    if (!file_set || file.empty()) {
        printUsage();
        fatal("File name (-f) is required");
    }
}