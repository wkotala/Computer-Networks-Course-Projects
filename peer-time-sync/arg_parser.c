#include "arg_parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>

#include "err.h"
#include "networking.h"

// może poprawić usage_msg
void parse_args(int argc, char *argv[], Config *config) {
    const char *usage_msg = "Usage: %s [-b bind_address] [-p port] [-a peer_address] [-r peer_port]";

    config->bind_address = NULL;
    config->port = 0;
    config->peer_address = NULL;
    config->peer_port = 0;

    int opt;
    bool a_provided = false;
    bool r_provided = false;

    while ((opt = getopt(argc, argv, ":b:p:a:r:")) != -1) {
        switch (opt) {
            case 'b': config->bind_address = optarg; break;
            case 'p': config->port = read_port(optarg); break;
            case 'a':
                config->peer_address = optarg;
                a_provided = true;
                break;
            case 'r':
                config->peer_port = read_port(optarg);
                r_provided = true;
                break;
            case ':':
                error(usage_msg, argv[0]);
                fatal("Option -%c requires an argument", optopt);
                break;
            case '?':
                error(usage_msg, argv[0]);
                if (isprint(optopt)) {
                    fatal("Unknown option -%c", optopt);
                } else {
                    fatal("Unknown option character with ASCII code 0x%x", optopt);
                }
                break;
            default:
                error(usage_msg, argv[0]);
                fatal("Error parsing arguments");
                break;
        }
    }

    if (a_provided != r_provided)
        fatal("Options -a and -r must be provided together");

    if (r_provided && config->peer_port == 0)
        fatal("Peer port must not be 0");

    if (optind < argc) {
        error(usage_msg, argv[0]);
        fatal("Unknown argument: %s", argv[optind]);
    }
}