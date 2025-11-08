#include "networking.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "err.h"
#include "node_data.h"

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t)port;
}

struct sockaddr_in get_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =     // IP address
        ((struct sockaddr_in *)(address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

bool equal_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

void bind_socket(int sock_fd, struct sockaddr_in *listen_address, const char *bind_address,
                 uint16_t *port) {
    listen_address->sin_family = AF_INET; // IPv4
    listen_address->sin_port = htons(*port);

    if (bind_address == NULL) {
        // By default, bind to all interfaces.
        listen_address->sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, bind_address, &listen_address->sin_addr) <= 0) {
            fatal("Invalid bind address provided: %s. Must be a valid IPv4 address.", bind_address);
        }
    }

    if (bind(sock_fd, (struct sockaddr *)listen_address, (socklen_t)sizeof(*listen_address)) < 0) {
        syserr("cannot bind to address");
    }

    // Save the actual port if it was not specified.
    if (*port == 0) {
        struct sockaddr_in addr = {0};
        socklen_t addr_len = sizeof(addr);
        if (getsockname(sock_fd, (struct sockaddr *)&addr, &addr_len) < 0) {
            syserr("cannot get socket name");
        }
        *port = ntohs(addr.sin_port);
    }
}

void set_receive_timeout(int sock_fd, time_t seconds) {
    struct timeval recv_timeout;
    recv_timeout.tv_sec = seconds;
    recv_timeout.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recv_timeout, sizeof(recv_timeout)) <
        0) {
        syserr("setsockopt(SO_RCVTIMEO) failed");
    }
}

bool address_in_array(const struct sockaddr_in *address, struct sockaddr_in *arr, size_t arr_size) {
    for (size_t i = 0; i < arr_size; ++i) {
        if (equal_addr(address, arr + i)) {
            return true;
        }
    }
    return false;
}