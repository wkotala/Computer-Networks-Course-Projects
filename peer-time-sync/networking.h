#ifndef PEER_TIME_SYNC_COMMON_H
#define PEER_TIME_SYNC_COMMON_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

// Parses a port number from a string, validating it.
uint16_t read_port(char const* string);

// Resolves host name/IP address, and combines it with the port number.
struct sockaddr_in get_address(char const* host, uint16_t port);

// Returns true if the two addresses are equal.
bool equal_addr(const struct sockaddr_in* a, const struct sockaddr_in* b);

// Binds socket to the specified address and port. Sets listen_address and changes value of port to the
// actual port if it was 0.
void bind_socket(int sock_fd, struct sockaddr_in* listen_address, const char* bind_address,
                 uint16_t* port);

// Sets a timeout for receiving data on the socket.
void set_receive_timeout(int sock_fd, time_t seconds);

// Returns true if the address is in the array.
bool address_in_array(const struct sockaddr_in* address, struct sockaddr_in* arr, size_t arr_size);

#endif // PEER_TIME_SYNC_COMMON_H
