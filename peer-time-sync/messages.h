#ifndef PEER_TIME_SYNC_MESSAGES_H
#define PEER_TIME_SYNC_MESSAGES_H

#include <netinet/in.h>
#include <stdint.h>

#include "node_data.h"

// Send a HELLO message to a peer.
void send_hello_message(NodeData *node_data);

// Handle incoming messages.
void handle_message(NodeData *node_data, const uint8_t *msg_buf, size_t buf_len,
                    const struct sockaddr_in *sender_addr);

// Check for periodic timer events and handle them.
void check_and_handle_timers(NodeData *node_data);

#endif // PEER_TIME_SYNC_MESSAGES_H
