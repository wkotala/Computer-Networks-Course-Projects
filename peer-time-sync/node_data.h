#ifndef PEER_TIME_SYNC_NODE_DATA_H
#define PEER_TIME_SYNC_NODE_DATA_H

#include <netinet/in.h>
#include <stdbool.h>

#define CLOCK_UNSYNCHRONIZED 255
#define CLOCK_LEADER 0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Networking data.
    struct sockaddr_in *my_addresses; // array of addresses of the current node
    size_t num_my_addresses;          // size of my_addresses array
    struct sockaddr_in known_peer;    // address of the known peer from the program arguments
    int sock_fd;

    // Peers data.
    size_t peer_list;               // list of known peers
    bool waiting_for_hello_reply;   // true if we are still waiting for a HELLO_REPLY
    size_t waiting_for_ack_connect; // list of peers from whom we are expecting ACK_CONNECT

    // Clock synchronization data.
    uint8_t sync_level;                   // synchronization level
    int64_t offset_ms;                    // offset to be applied to the local clock
    struct sockaddr_in synchronized_peer; // address of the peer we are synchronized with (valid if
                                          // sync_level < CLOCK_UNSYNCHRONIZED)
    struct timespec next_sync_start;      // time after which next synchronization should start (valid if
                                          // sync_level < CLOCK_UNSYNCHRONIZED - 1)
    struct timespec next_sync_check;      // time after which next synchronization check should be done
                                          // (valid if CLOCK_LEADER < sync_level < CLOCK_UNSYNCHRONIZED)
    uint64_t last_sync_start;             // natural clock time when the last SYNC_START message sent
                                          // (or 0 if none has been sent yet)
    size_t asked_to_synchronize;          // list of peers we asked to synchronize with us (and await
                                          // DELAY_REQUEST from)

    // Data for synchronization process.
    bool synchronizing;                         // true if we are in the process of synchronizing
    uint8_t synchronizing_level;                // sync level of the peer we are synchronizing with
    struct sockaddr_in peer_to_sync;            // address of the peer we are synchronizing with
    struct timespec waiting_for_delay_response; // deadline for waiting for a DELAY_RESPONSE
    uint64_t T1, T2, T3, T4;                    // timestamps for the offset calculation

} NodeData;

#ifdef __cplusplus
}
#endif

#endif // PEER_TIME_SYNC_NODE_DATA_H
