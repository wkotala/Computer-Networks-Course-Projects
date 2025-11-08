#ifndef PEER_TIME_SYNC_ERR_H
#define PEER_TIME_SYNC_ERR_H

#include <stddef.h>
#include <stdint.h>

#include "node_data.h"

#ifdef __cplusplus
#define PEER_TIME_SYNC_NORETURN [[noreturn]]
extern "C" {
#else
#include <stdnoreturn.h>
#define PEER_TIME_SYNC_NORETURN noreturn
#endif

// Register the node data structure for cleanup. Should be called as soon as the data structure is
// created, before initializing with valid data.
void register_node_data(NodeData* node_data);

// Print information about a system error (along with errno), close the socket, and quit.
PEER_TIME_SYNC_NORETURN void syserr(const char* fmt, ...);

// Print information about an error (do not check errno), close the socket, and quit.
PEER_TIME_SYNC_NORETURN void fatal(const char* fmt, ...);

// Print information about an error (and errno, if != 0) and return.
void error(const char* fmt, ...);

// Print information about an invalid message (its first 10 bytes) and return.
void error_msg_hex(const uint8_t* msg_buffer, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif // PEER_TIME_SYNC_ERR_H
