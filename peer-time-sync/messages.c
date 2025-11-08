#include "messages.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "err.h"
#include "networking.h"
#include "peers.h"

#ifndef NDEBUG
#define LOG(...)                      \
    do {                              \
        fprintf(stderr, "LOG ");      \
        fprintf(stderr, __VA_ARGS__); \
    } while (0);
#else
#define LOG(...)
#endif

#define HELLO 1
#define HELLO_REPLY 2
#define CONNECT 3
#define ACK_CONNECT 4
#define SYNC_START 11
#define DELAY_REQUEST 12
#define DELAY_RESPONSE 13
#define LEADER 21
#define GET_TIME 31
#define TIME 32

#define MESSAGE_SIZE 1
#define COUNT_SIZE 2
#define PEER_ADDRESS_LENGTH_SIZE 1
#define PEER_ADDRESS_SIZE 4
#define PEER_PORT_SIZE 2
#define TIMESTAMP_SIZE 8
#define SYNCHRONIZED_SIZE 1

#define LEADER_SYNCHRONIZATION_DELAY 2
#define SYNCHRONIZATION_CHECK_DELAY 20
#define DELAY_RESPONSE_TIME 5
#define SYNC_START_DELAY 5

// Custom htonll, ntohll macros for 64-bit integers endianness conversion based on __BYTE_ORDER__ macro,
// if available. If __BYTE_ORDER__ is not defined, we check endianness at runtime.
#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define htonll(x) (x)
#define ntohll(x) (x)
#else
#define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif
#else
#define htonll(x) ((1 == htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1 == ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

#define BUFFER_SIZE 65536
static uint8_t send_buffer[BUFFER_SIZE];

// Functions create_*_message create messages of specified type, placing them in send_buffer.
// The return value is length of the message.

static size_t create_simple_message(uint8_t message_type) {
    send_buffer[0] = message_type;
    return MESSAGE_SIZE;
}

static size_t create_sync_level_and_timestamp_message(uint8_t message_type, uint8_t sync_level,
                                                      uint64_t timestamp) {
    send_buffer[0] = message_type;
    send_buffer[MESSAGE_SIZE] = sync_level;

    const uint64_t network_ts = htonll(timestamp);
    memcpy(&send_buffer[MESSAGE_SIZE + SYNCHRONIZED_SIZE], &network_ts, TIMESTAMP_SIZE);

    return MESSAGE_SIZE + SYNCHRONIZED_SIZE + TIMESTAMP_SIZE;
}

static size_t create_hello_message(void) {
    return create_simple_message(HELLO);
}

// Creates a HELLO_REPLY message from given list of peers, excluding the source and destination of the
// message. Returns 0 if the message would be too large to send.
static size_t create_hello_reply_message(size_t peer_list, const struct sockaddr_in *src_addr_arr,
                                         size_t src_addr_arr_size, const struct sockaddr_in *dest_addr) {
    uint8_t *buf = send_buffer;
    *buf++ = HELLO_REPLY; // message type
    uint16_t count = peer_list_count_excl(peer_list, src_addr_arr, src_addr_arr_size, dest_addr);
    uint16_t network_count = htons(count);
    memcpy(buf, &network_count, COUNT_SIZE); // number of peers
    buf += COUNT_SIZE;

    size_t peers_list_size = count * (PEER_ADDRESS_LENGTH_SIZE + PEER_ADDRESS_SIZE + PEER_PORT_SIZE);

    if (MESSAGE_SIZE + COUNT_SIZE + peers_list_size >= BUFFER_SIZE) {
        LOG("HELLO_REPLY message too large to send using UDP\n");
        return 0;
    }

    if (!peer_list_write_to_buf_excl(peer_list, buf, peers_list_size, src_addr_arr, src_addr_arr_size,
                                     dest_addr)) {
        error("Incorrect HELLO_REPLY message created");
        return 0;
    }

    return MESSAGE_SIZE + COUNT_SIZE + peers_list_size;
}

static size_t create_connect_message(void) {
    return create_simple_message(CONNECT);
}

static size_t create_ack_connect_message(void) {
    return create_simple_message(ACK_CONNECT);
}

static size_t create_sync_start_message(uint8_t sync_level, uint64_t timestamp) {
    return create_sync_level_and_timestamp_message(SYNC_START, sync_level, timestamp);
}

static size_t create_delay_request_message(void) {
    return create_simple_message(DELAY_REQUEST);
}

static size_t create_delay_response_message(uint8_t sync_level, uint64_t timestamp) {
    return create_sync_level_and_timestamp_message(DELAY_RESPONSE, sync_level, timestamp);
}

static size_t create_time_message(uint8_t sync_level, uint64_t timestamp) {
    return create_sync_level_and_timestamp_message(TIME, sync_level, timestamp);
}

// Sends a message of given length from send_buffer to the given dest_address, checking for errors.
static void send_message(int sock_fd, uint8_t message_type, size_t msg_length,
                         const struct sockaddr_in *dest_addr) {
    ssize_t sent_length = sendto(sock_fd, send_buffer, msg_length, 0, (struct sockaddr *)dest_addr,
                                 (socklen_t)sizeof(*dest_addr));

    if (sent_length < 0) {
        error("Failed to send a message of type %" PRIu8, message_type);
    } else if ((size_t)sent_length != msg_length) {
        error("Incomplete message of type %" PRIu8 " sent", message_type);
    } else {
        LOG("Message of type %" PRIu8 " sent successfully to %s:%u\n", message_type,
            inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
    }
}

// Functions handle_* check whether messages of specified types are correct and handle them.

static void handle_hello(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                         const struct sockaddr_in *sender_addr) {
    LOG("Received HELLO message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    if (address_in_array(sender_addr, node_data->my_addresses, node_data->num_my_addresses)) {
        LOG("Ignoring message from my own address and port\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    size_t msg_length = create_hello_reply_message(node_data->peer_list, node_data->my_addresses,
                                                   node_data->num_my_addresses, sender_addr);
    if (msg_length > 0 && peer_list_add(node_data->peer_list, sender_addr) >= 0) {
        send_message(node_data->sock_fd, HELLO_REPLY, msg_length, sender_addr);
    } else {
        LOG("HELLO message ignored\n")
        error_msg_hex(msg_buf, buf_len);
    }
}

// Returns whether the HELLO_REPLY message is correct.
static bool check_hello_reply(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                              const struct sockaddr_in *sender_addr) {
    // Check if we are waiting for a HELLO_REPLY from the sender.
    if (!node_data->waiting_for_hello_reply || !equal_addr(sender_addr, &node_data->known_peer)) {
        LOG("Incorrect message - wrong sender\n");
        return false;
    }

    // Check if the sender is not the same as the recipient.
    if (address_in_array(sender_addr, node_data->my_addresses, node_data->num_my_addresses)) {
        LOG("Ignoring message from my own address and port\n");
        return false;
    }

    // Check if the message is of correct length.
    if (buf_len < MESSAGE_SIZE + COUNT_SIZE) {
        LOG("Incorrect message - wrong size\n");
        return false;
    }

    uint16_t count;
    memcpy(&count, &msg_buf[MESSAGE_SIZE], COUNT_SIZE);
    count = ntohs(count);

    if (buf_len != (size_t)(MESSAGE_SIZE + COUNT_SIZE +
                            count * (PEER_ADDRESS_LENGTH_SIZE + PEER_ADDRESS_SIZE + PEER_PORT_SIZE))) {
        LOG("Incorrect message - wrong size\n");
        return false;
    }

    // Check if sent list of peers is correct.
    const uint8_t *buf = msg_buf + MESSAGE_SIZE + COUNT_SIZE;
    for (uint16_t i = 0; i < count; ++i) {
        uint8_t address_length = *buf++;
        if (address_length != PEER_ADDRESS_SIZE) {
            LOG("Incorrect message - address length is not 4\n");
            return false;
        }

        struct sockaddr_in peer_address = {0};
        peer_address.sin_family = AF_INET; // IPv4
        memcpy(&peer_address.sin_addr.s_addr, buf, PEER_ADDRESS_SIZE);
        buf += PEER_ADDRESS_SIZE;
        memcpy(&peer_address.sin_port, buf, PEER_PORT_SIZE);
        buf += PEER_PORT_SIZE;

        if (peer_address.sin_port == 0) {
            LOG("Incorrect message - wrong port\n");
            return false;
        }

        if (equal_addr(&peer_address, sender_addr) ||
            address_in_array(&peer_address, node_data->my_addresses, node_data->num_my_addresses)) {
            LOG("Incorrect message - sender and recipient should not be in the list\n");
            return false;
        }
    }

    return true;
}

static void handle_hello_reply(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                               const struct sockaddr_in *sender_addr) {
    LOG("Received HELLO_REPLY message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (!check_hello_reply(node_data, msg_buf, buf_len, sender_addr)) {
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    // The message is correct, so we're not waiting for HELLO_REPLY anymore.
    node_data->waiting_for_hello_reply = false;

    // Constructing the peer list and sending CONNECT messages.
    uint16_t count;
    memcpy(&count, &msg_buf[MESSAGE_SIZE], COUNT_SIZE);
    count = ntohs(count);
    const uint8_t *buf = msg_buf + MESSAGE_SIZE + COUNT_SIZE;

    for (uint16_t i = 0; i < count; ++i) {
        buf++; // skip address length (already checked it is 4)

        struct sockaddr_in peer_address = {0};
        peer_address.sin_family = AF_INET; // IPv4
        memcpy(&peer_address.sin_addr.s_addr, buf, PEER_ADDRESS_SIZE);
        buf += PEER_ADDRESS_SIZE;
        memcpy(&peer_address.sin_port, buf, PEER_PORT_SIZE);
        buf += PEER_PORT_SIZE;

        peer_list_add(node_data->waiting_for_ack_connect, &peer_address);
        size_t msg_length = create_connect_message();
        send_message(node_data->sock_fd, CONNECT, msg_length, &peer_address);
    }
    // Add the sender to the peer list.
    peer_list_add(node_data->peer_list, sender_addr);
}

static void handle_connect(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                           const struct sockaddr_in *sender_addr) {
    LOG("Received CONNECT message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    if (peer_list_full(node_data->peer_list) && !peer_list_contains(node_data->peer_list, sender_addr)) {
        LOG("Ignoring message which would exceed the maximum number of peers\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    if (address_in_array(sender_addr, node_data->my_addresses, node_data->num_my_addresses)) {
        LOG("Ignoring message from my own address and port\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    size_t msg_length = create_ack_connect_message();
    send_message(node_data->sock_fd, ACK_CONNECT, msg_length, sender_addr);

    peer_list_add(node_data->peer_list, sender_addr);
}

static void handle_ack_connect(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                               const struct sockaddr_in *sender_addr) {
    LOG("Received ACK_CONNECT message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    if (!peer_list_contains(node_data->waiting_for_ack_connect, sender_addr)) {
        LOG("Incorrect message - unexpected sender\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    if (peer_list_full(node_data->peer_list) && !peer_list_contains(node_data->peer_list, sender_addr)) {
        LOG("Ignoring message which would exceed the maximum number of peers\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    if (address_in_array(sender_addr, node_data->my_addresses, node_data->num_my_addresses)) {
        LOG("Ignoring message from my own address and port\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    peer_list_remove(node_data->waiting_for_ack_connect, sender_addr);
    peer_list_add(node_data->peer_list, sender_addr);
}

// Returns whether the SYNC_START message is correct.
static bool check_sync_start(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                             const struct sockaddr_in *sender_addr) {
    if (buf_len != MESSAGE_SIZE + SYNCHRONIZED_SIZE + TIMESTAMP_SIZE) {
        LOG("Incorrect message - wrong size\n");
        return false;
    }

    uint8_t sender_sync_level = msg_buf[MESSAGE_SIZE];
    bool sender_is_known = peer_list_contains(node_data->peer_list, sender_addr);

    if (sender_sync_level >= CLOCK_UNSYNCHRONIZED - 1 || !sender_is_known) {
        LOG("Incorrect message - incorrect sender\n");
        return false;
    }

    return true;
}

static void handle_sync_start(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                              const struct sockaddr_in *sender_addr) {
    uint64_t receive_time = get_natural_clock();

    LOG("Received SYNC_START message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (!check_sync_start(node_data, msg_buf, buf_len, sender_addr)) {
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    if (node_data->synchronizing) {
        return;
    }

    uint8_t sender_sync_level = msg_buf[MESSAGE_SIZE];
    uint64_t sender_timestamp;
    memcpy(&sender_timestamp, msg_buf + MESSAGE_SIZE + SYNCHRONIZED_SIZE, TIMESTAMP_SIZE);
    sender_timestamp = ntohll(sender_timestamp);

    // Basic checks passed, the continuation depends on whether we are synchronized to the sender or not.
    bool synchronized_to_sender = node_data->sync_level < CLOCK_UNSYNCHRONIZED &&
                                  equal_addr(sender_addr, &node_data->synchronized_peer);

    if (synchronized_to_sender) {
        if (sender_sync_level >= node_data->sync_level) {
            node_data->sync_level = CLOCK_UNSYNCHRONIZED;
            node_data->offset_ms = 0;
            return;
        } else {
            set_event_in_x_seconds(&node_data->next_sync_check, SYNCHRONIZATION_CHECK_DELAY);
        }
    } else {
        if (sender_sync_level + 1 >= node_data->sync_level) {
            return;
        }
    }

    // We proceed to synchronization.
    node_data->synchronizing = true;
    node_data->synchronizing_level = sender_sync_level;
    node_data->T1 = sender_timestamp;
    node_data->T2 = receive_time;
    node_data->peer_to_sync = *sender_addr;

    size_t msg_length = create_delay_request_message();
    node_data->T3 = get_natural_clock();
    send_message(node_data->sock_fd, DELAY_REQUEST, msg_length, sender_addr);
    set_event_in_x_seconds(&node_data->waiting_for_delay_response, DELAY_RESPONSE_TIME);
}

static void handle_delay_request(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                                 const struct sockaddr_in *sender_addr) {
    uint64_t synchronized_receive_time = get_clock(node_data);
    uint64_t receive_time = get_natural_clock();

    LOG("Received DELAY_REQUEST message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    if (!peer_list_contains(node_data->asked_to_synchronize, sender_addr) ||
        receive_time > node_data->last_sync_start + DELAY_RESPONSE_TIME) {
        LOG("Incorrect message - unknown or late sender\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    size_t msg_lenth = create_delay_response_message(node_data->sync_level, synchronized_receive_time);
    send_message(node_data->sock_fd, DELAY_RESPONSE, msg_lenth, sender_addr);
}

static void handle_delay_response(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                                  const struct sockaddr_in *sender_addr) {
    LOG("Received DELAY_RESPONSE message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE + SYNCHRONIZED_SIZE + TIMESTAMP_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    if (!node_data->synchronizing || !equal_addr(sender_addr, &node_data->peer_to_sync)) {
        LOG("Incorrect message - not synchronizing right now with the sender\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    uint8_t sender_sync_level = msg_buf[MESSAGE_SIZE];
    uint64_t sender_timestamp;
    memcpy(&sender_timestamp, msg_buf + MESSAGE_SIZE + SYNCHRONIZED_SIZE, TIMESTAMP_SIZE);
    sender_timestamp = ntohll(sender_timestamp);

    if (sender_sync_level != node_data->synchronizing_level || sender_timestamp < node_data->T1) {
        LOG("Inconsistent sync data from sender\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    node_data->T4 = sender_timestamp;

    // If we have just became a node, that others can synchronize to.
    if (node_data->sync_level >= CLOCK_UNSYNCHRONIZED - 1 &&
        sender_sync_level + 1 < CLOCK_UNSYNCHRONIZED - 1) {
        set_event_in_x_seconds(&node_data->next_sync_start, 0);
    }

    node_data->sync_level = sender_sync_level + 1;
    update_offset(node_data);
    node_data->synchronized_peer = *sender_addr;
    set_event_in_x_seconds(&node_data->next_sync_check, SYNCHRONIZATION_CHECK_DELAY);

    node_data->synchronizing = false;
}

static void handle_leader(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                          __attribute__((unused)) const struct sockaddr_in *sender_addr) {
    LOG("Received LEADER message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE + SYNCHRONIZED_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }
    uint8_t msg_sync = msg_buf[MESSAGE_SIZE];

    if (msg_sync == CLOCK_LEADER) {
        node_data->sync_level = CLOCK_LEADER;
        set_event_in_x_seconds(&node_data->next_sync_start, LEADER_SYNCHRONIZATION_DELAY);
    } else if (msg_sync == CLOCK_UNSYNCHRONIZED) {
        if (node_data->sync_level != CLOCK_LEADER) {
            LOG("Incorrect message - LEADER 255 sent to non-leader\n");
            error_msg_hex(msg_buf, buf_len);
            return;
        }
        node_data->sync_level = CLOCK_UNSYNCHRONIZED;
    } else {
        LOG("Incorrect message - unexpected argument\n");
        error_msg_hex(msg_buf, buf_len);
    }
}

static void handle_get_time(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                            const struct sockaddr_in *sender_addr) {
    LOG("Received GET_TIME message from %s:%u\n", inet_ntoa(sender_addr->sin_addr),
        ntohs(sender_addr->sin_port));

    if (buf_len != MESSAGE_SIZE) {
        LOG("Incorrect message - wrong size\n");
        error_msg_hex(msg_buf, buf_len);
        return;
    }

    size_t msg_length = create_time_message(node_data->sync_level, get_clock(node_data));
    send_message(node_data->sock_fd, TIME, msg_length, sender_addr);
}

// Functions defined in header:

void handle_message(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                    const struct sockaddr_in *sender_addr) {
    uint8_t message_type = msg_buf[0];

    switch (message_type) {
        case HELLO: handle_hello(node_data, msg_buf, buf_len, sender_addr); break;
        case HELLO_REPLY: handle_hello_reply(node_data, msg_buf, buf_len, sender_addr); break;
        case CONNECT: handle_connect(node_data, msg_buf, buf_len, sender_addr); break;
        case ACK_CONNECT: handle_ack_connect(node_data, msg_buf, buf_len, sender_addr); break;
        case SYNC_START: handle_sync_start(node_data, msg_buf, buf_len, sender_addr); break;
        case DELAY_REQUEST: handle_delay_request(node_data, msg_buf, buf_len, sender_addr); break;
        case DELAY_RESPONSE: handle_delay_response(node_data, msg_buf, buf_len, sender_addr); break;
        case LEADER: handle_leader(node_data, msg_buf, buf_len, sender_addr); break;
        case GET_TIME: handle_get_time(node_data, msg_buf, buf_len, sender_addr); break;
        default:
            LOG("Unknown message type\n");
            error_msg_hex(msg_buf, buf_len);
            break;
    }
}

void send_hello_message(NodeData *node_data) {
    size_t msg_length = create_hello_message();
    send_message(node_data->sock_fd, HELLO, msg_length, &node_data->known_peer);
}

void check_and_handle_timers(NodeData *node_data) {
    // If we are synchronized to a peer, check whether the peer is still sending SYNC_START messages.
    if (CLOCK_LEADER < node_data->sync_level && node_data->sync_level < CLOCK_UNSYNCHRONIZED) {
        if (now_is_after(&node_data->next_sync_check)) {
            node_data->sync_level = CLOCK_UNSYNCHRONIZED;
            node_data->offset_ms = 0;
        }
    }

    // If we want to synchronize others, check whether it is time to send SYNC_START.
    if (node_data->sync_level < CLOCK_UNSYNCHRONIZED - 1) {
        if (now_is_after(&node_data->next_sync_start)) {
            if (peer_list_iterator_begin(node_data->peer_list)) {
                do {
                    struct sockaddr_in peer_addr = peer_list_iterator_get_peer(node_data->peer_list);
                    // Send SYNC_START message
                    size_t msg_length =
                        create_sync_start_message(node_data->sync_level, get_clock(node_data));
                    send_message(node_data->sock_fd, SYNC_START, msg_length, &peer_addr);
                } while (peer_list_iterator_next(node_data->peer_list));

                peer_list_cpy(node_data->peer_list, node_data->asked_to_synchronize);
            }

            node_data->last_sync_start = get_natural_clock();
            set_event_in_x_seconds(&node_data->next_sync_start, SYNC_START_DELAY);
        }
    }
}