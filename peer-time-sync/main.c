#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "arg_parser.h"
#include "clock.h"
#include "err.h"
#include "messages.h"
#include "networking.h"
#include "node_data.h"
#include "peers.h"

#define BUFFER_SIZE 65536

static uint8_t recv_buffer[BUFFER_SIZE];

static void initialize_node_data(NodeData *node_data, int sock_fd, struct sockaddr_in listen_address,
                                 Config *config) {
    node_data->my_addresses = NULL; // initialized in case of error
    node_data->sock_fd = sock_fd;
    node_data->peer_list = peer_list_create();
    node_data->waiting_for_hello_reply = false;
    node_data->waiting_for_ack_connect = peer_list_create();
    node_data->sync_level = CLOCK_UNSYNCHRONIZED;
    node_data->offset_ms = 0;
    node_data->last_sync_start = 0;
    node_data->synchronizing = false;
    node_data->asked_to_synchronize = peer_list_create();

    // Initializing my_addresses array properly.
    if (config->bind_address == NULL) {
        struct ifaddrs *ifaddr;
        if (getifaddrs(&ifaddr) == -1) {
            syserr("getifaddrs");
        }

        node_data->num_my_addresses = 0;
        for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                node_data->num_my_addresses++;
            }
        }

        if (node_data->num_my_addresses == 0) {
            freeifaddrs(ifaddr);
            return;
        }

        node_data->my_addresses =
            (struct sockaddr_in *)malloc(node_data->num_my_addresses * sizeof(struct sockaddr_in));
        if (node_data->my_addresses == NULL) {
            freeifaddrs(ifaddr);
            syserr("malloc");
        }

        size_t idx = 0;
        for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                memcpy(node_data->my_addresses + idx, ifa->ifa_addr, sizeof(struct sockaddr_in));
                node_data->my_addresses[idx].sin_port = htons(config->port);
                idx++;
            }
        }

        freeifaddrs(ifaddr);
    } else {
        node_data->my_addresses = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
        node_data->my_addresses[0] = listen_address;
        node_data->my_addresses[0].sin_port = htons(config->port);
        node_data->num_my_addresses = 1;
    }
}

int main(int argc, char *argv[]) {
    // Initialize natural clock.
    clock_init();

    // Parse arguments and create structure for node's data.
    Config config;
    parse_args(argc, argv, &config);
    NodeData node_data;
    register_node_data(&node_data);

    // Create a socket, bind it to the specified address and port, and set a timeout for receiving.
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        syserr("cannot create a socket");
    }

    struct sockaddr_in listen_address = {0};
    bind_socket(sock_fd, &listen_address, config.bind_address, &config.port);

    set_receive_timeout(sock_fd, 1); // 1 second timeout for periodic actions

    // Initialize node data.
    initialize_node_data(&node_data, sock_fd, listen_address, &config);

    // Send first HELLO
    if (config.peer_address != NULL) {
        node_data.waiting_for_hello_reply = true;

        struct sockaddr_in peer_address = {0};
        peer_address = get_address(config.peer_address, config.peer_port);
        node_data.known_peer = peer_address;

        send_hello_message(&node_data);
    }

    // Main loop: receive messages and handle them.
    while (true) {
        check_and_handle_timers(&node_data);

        struct sockaddr_in sender_address;
        socklen_t sender_address_len = sizeof(sender_address);
        ssize_t recv_length = recvfrom(sock_fd, recv_buffer, BUFFER_SIZE, 0,
                                       (struct sockaddr *)&sender_address, &sender_address_len);

        if (recv_length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred, continue the loop.
                errno = 0;
                continue;
            } else {
                error("recvfrom failed");
            }
        } else if (recv_length == 0) {
            error_msg_hex(NULL, 0);
            continue;
        } else {
            handle_message(&node_data, recv_buffer, (size_t)recv_length, &sender_address);
        }
    }

    return 0;
}