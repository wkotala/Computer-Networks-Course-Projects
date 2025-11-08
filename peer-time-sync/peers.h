#ifndef PEER_TIME_SYNC_PEERS_H
#define PEER_TIME_SYNC_PEERS_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
namespace cxx {
extern "C" {
#endif

// Creates a new peer list, returning its id.
size_t peer_list_create(void);

// Attempts to add a peer to the list, checking for duplicates and maximum capacity (65535).
// Returns: 1 if the peer was successfully added,
//          0 if the peer was already in the list,
//         -1 if the list is full.
int peer_list_add(size_t list_id, const struct sockaddr_in *peer);

// Returns true if the peer is in the list, false otherwise.
bool peer_list_contains(size_t list_id, const struct sockaddr_in *peer);

// Removes a peer from the list.
void peer_list_remove(size_t list_id, const struct sockaddr_in *peer);

// Returns the number of peers in the list.
uint16_t peer_list_count(size_t list_id);

// Returns the number of peers in the list, excluding the source and destination addresses.
uint16_t peer_list_count_excl(size_t list_id, const struct sockaddr_in *src_addr_arr,
                              size_t src_addr_arr_size, const struct sockaddr_in *dest_addr);

// Writes the peer list to a buffer, excluding the source and destination addresses.
// Exactly size bytes should be written to the buffer (returns false otherwise).
// Format of the written data is (peer_address_length, peer_address, peer_port) for each peer.
bool peer_list_write_to_buf_excl(size_t list_id, uint8_t *buf, size_t size,
                                 const struct sockaddr_in *src_addr_arr, size_t src_addr_arr_size,
                                 const struct sockaddr_in *dest_addr);

// Copies the contents of one peer list to another. Clears the destination list first.
void peer_list_cpy(size_t src_list, size_t dest_list);

// Initializes iterator for the given peer list.
// If the list is empty, returns false and the iterator is invalid.
bool peer_list_iterator_begin(size_t list_idk);

// Proceeds to the next peer in the list.
// If the end of the list is reached, returns false and invalidates the iterator.
bool peer_list_iterator_next(size_t list_id);

// Returns the current peer from the iterator.
struct sockaddr_in peer_list_iterator_get_peer(size_t list_id);

// Returns true if the peer list has reached maximum capacity.
bool peer_list_full(size_t list_id);

#ifdef __cplusplus
} // extern "C"
} // namespace cxx
#endif

#endif // PEER_TIME_SYNC_PEERS_H
