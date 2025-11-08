#ifndef PEER_TIME_SYNC_ARG_PARSER_H
#define PEER_TIME_SYNC_ARG_PARSER_H

#include <stdint.h>

// Program arguments.
typedef struct {
    const char *bind_address; // listening address (or NULL -> all)
    uint16_t port;            // listening port (or 0 -> any)
    const char *peer_address; // known node's address (or NULL -> none)
    uint16_t peer_port;       // known node's port (or 0 -> none)
} Config;

// Parses command line arguments and fills the Config structure.
void parse_args(int argc, char *argv[], Config *config);

#endif // PEER_TIME_SYNC_ARG_PARSER_H
