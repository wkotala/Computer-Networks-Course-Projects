#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "err.h"

class ArgParser {
 public:
    virtual void printUsage() const = 0;
    virtual void logInfo() const = 0;

 protected:
    int argc;
    char** argv;

    ArgParser(int argc, char* argv[]) : argc(argc), argv(argv) {}
    virtual ~ArgParser() = default;

    [[noreturn]] void handle_getopt_error(char getopt_return_char, int optopt_val);

    uint16_t parseAndValidatePort(const char* str, bool allow_zero);
    int parseAndValidateInt(const char* str, unsigned long min, unsigned long max);
};

class ClientArgParser : public ArgParser {
 public:
    // Constructor parses arguments.
    // On failure, exits with code 1 and prints an error message to stderr.
    ClientArgParser(int argc, char* argv[]);

    void printUsage() const override;
    void logInfo() const override;

    // Getters
    const std::string& getPlayerId() const { return player_id; }
    const std::string& getServerAddress() const { return server_address; }
    uint16_t getServerPort() const { return server_port; }
    bool isIPv4Forced() const { return force_ipv4; }
    bool isIPv6Forced() const { return force_ipv6; }
    bool isAutoStrategy() const { return auto_strategy; }

 private:
    void parse();
    void validate();

    std::string player_id;
    bool player_id_set = false;
    std::string server_address;
    bool server_address_set = false;
    uint16_t server_port;
    bool server_port_set = false;
    bool force_ipv4 = false;
    bool force_ipv6 = false;
    bool auto_strategy = false;
};

class ServerArgParser : public ArgParser {
 public:
    // Constructor parses arguments.
    // On failure, exits with code 1 and prints an error message to stderr.
    ServerArgParser(int argc, char* argv[]);

    void printUsage() const override;
    void logInfo() const override;

    // Getters
    uint16_t getPort() const { return port; }
    int getK() const { return k; }
    int getN() const { return n; }
    int getM() const { return m; }
    const std::string& getFile() const { return file; }

 private:
    void parseAndValidate();

    uint16_t port = 0;
    int k = 100;
    int n = 4;
    int m = 131;
    std::string file;
    bool file_set = false;
};

#endif // ARG_PARSER_H
