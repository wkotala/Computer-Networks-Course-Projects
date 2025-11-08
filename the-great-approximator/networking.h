#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <string>

// Connects to the server based on arguments and returns socket file descriptor.
int connect_to_server(const std::string& host, const std::string& port_str, bool force_ipv4,
                      bool force_ipv6, std::string& out_server_ip, int& out_server_port);

// Sets the receive timeout for a socket.
void set_receive_timeout(int sockfd, int timeout_ms);

// Accepts a new connection from the listening socket.
// Returns the client socket file descriptor.
// If there are no pending connections, returns -1.
int accept_new_connection(int listening_fd, struct sockaddr_storage* client_addr,
                          socklen_t* client_addr_len);

// Sets the socket to non-blocking mode.
void set_socket_nonblocking(int sockfd);

// Creates a socket that can handle both IPv4 and IPv6 connections and binds to all interfaces.
// If IPv6 is not available, the socket will be created as an IPv4 socket.
// Sets the socket to non-blocking mode and starts listening for connections.
// Returns the listening socket file descriptor.
// Backlog is the maximum number of pending connections.
int setup_listening_socket(uint16_t port, int backlog);

#endif // NETWORKING_H