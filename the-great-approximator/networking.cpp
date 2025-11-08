#include "networking.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "err.h"

namespace {

// Tries to set up ipv6 socket, returns false on error.
bool setup_listening_socket_ipv6(uint16_t port, int backlog, int& sockfd) {
    sockfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (sockfd < 0) {
        return false;
    }

    std::string error_msg = "";

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error_msg += "Enabling SO_REUSEADDR failed\n";
    }

    opt = 0;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        error_msg += "Disabling IPV6_V6ONLY failed\n";
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }

    set_socket_nonblocking(sockfd);

    if (listen(sockfd, backlog) < 0) {
        return false;
    }

    fprintf(stdout, "%s", error_msg.c_str()); // not fatal errors
    fprintf(stdout, "Listening on IPv6.\n");
    return true;
}

// Tries to set up ipv4 socket, exits on error, returns socket file descriptor.
int setup_listening_socket_ipv4(uint16_t port, int backlog) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syserr("socket");
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("Setting SO_REUSEADDR failed");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syserr("bind");
    }

    set_socket_nonblocking(sockfd);

    if (listen(sockfd, backlog) < 0) {
        syserr("Error listening on socket");
    }

    fprintf(stdout, "Listening on IPv4.\n");
    return sockfd;
}

} // namespace

int connect_to_server(const std::string& host, const std::string& port_str, bool force_ipv4,
                      bool force_ipv6, std::string& out_server_ip, int& out_server_port) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    if (force_ipv4) {
        hints.ai_family = AF_INET;
    } else if (force_ipv6) {
        hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res;
    int gai_ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (gai_ret != 0) {
        fatal("getaddrinfo for host '%s' port '%s' failed: %s", host.c_str(), port_str.c_str(),
              gai_strerror(gai_ret));
    }

    struct addrinfo* send_addr;
    int sockfd = -1;
    for (send_addr = res; send_addr; send_addr = send_addr->ai_next) {
        sockfd = socket(send_addr->ai_family, send_addr->ai_socktype, send_addr->ai_protocol);
        if (sockfd == -1) {
            continue; // Try next address
        }

        if (connect(sockfd, send_addr->ai_addr, send_addr->ai_addrlen) == 0) { // Success

            char conversion_buffer[INET6_ADDRSTRLEN]; // sufficient for IPv4 or IPv6
            const char* conversion_result = nullptr;

            if (send_addr->ai_family == AF_INET) {
                struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)send_addr->ai_addr;
                conversion_result = inet_ntop(AF_INET, &ipv4_addr->sin_addr, conversion_buffer,
                                              sizeof(conversion_buffer));
            } else if (send_addr->ai_family == AF_INET6) {
                struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*)send_addr->ai_addr;
                conversion_result = inet_ntop(AF_INET6, &ipv6_addr->sin6_addr,
                                              conversion_buffer, sizeof(conversion_buffer));
            }

            if (conversion_result) {
                out_server_ip.assign(conversion_buffer);
                out_server_port = std::stoi(port_str);
                break;
            }
        }

        // Unsuccessful connection, close the socket and try next address
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        syserr("Could not connect to '%s':'%s'", host.c_str(), port_str.c_str());
    }
    return sockfd;
}

void set_receive_timeout(int sockfd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("Failed to set receive timeout");
    }
}

int setup_listening_socket(uint16_t port, int backlog) {
    int sockfd = -1;
    if (setup_listening_socket_ipv6(port, backlog, sockfd)) {
        return sockfd;
    }
    return setup_listening_socket_ipv4(port, backlog);
}

int accept_new_connection(int listening_fd, struct sockaddr_storage* client_addr,
                          socklen_t* client_addr_len) {
    int client_fd = accept(listening_fd, (struct sockaddr*)client_addr, client_addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No pending connections
            return -1;
        }
        syserr("Error accepting connection");
    }

    set_socket_nonblocking(client_fd);

    return client_fd;
}

void set_socket_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        syserr("Error getting socket flags");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        syserr("Error setting socket to non-blocking mode");
    }
}
