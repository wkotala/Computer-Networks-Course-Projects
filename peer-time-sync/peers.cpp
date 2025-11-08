#include "peers.h"

#include <netinet/in.h>

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "err.h"

#define MAX_CAPACITY 65535

namespace {

#ifndef NDEBUG
#define LOG(...)                      \
    do {                              \
        fprintf(stderr, "LOG ");      \
        fprintf(stderr, __VA_ARGS__); \
    } while (0);
#define LOG_CONTINUE(...) fprintf(stderr, __VA_ARGS__);
#else
#define LOG(...)
#define LOG_CONTINUE(...)
#endif

struct SockaddrInLess {
    bool operator()(const struct sockaddr_in &a, const struct sockaddr_in &b) const {
        if (a.sin_addr.s_addr != b.sin_addr.s_addr) {
            return a.sin_addr.s_addr < b.sin_addr.s_addr;
        }
        return a.sin_port < b.sin_port;
    }
};

using PeerListImpl = std::set<struct sockaddr_in, SockaddrInLess>;

std::vector<PeerListImpl> peer_lists;
std::vector<PeerListImpl::iterator> peer_list_iterators;

// Returns true, if the address is in the array.
bool is_src_addr(const struct sockaddr_in *addr, const struct sockaddr_in *src_addr_arr,
                 size_t src_addr_arr_size) {
    for (size_t i = 0; i < src_addr_arr_size; ++i) {
        if (addr->sin_addr.s_addr == src_addr_arr[i].sin_addr.s_addr &&
            addr->sin_port == src_addr_arr[i].sin_port) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace cxx {
size_t peer_list_create(void) {
    static size_t next_id = 0;
    assert(next_id < SIZE_MAX && next_id == peer_lists.size());

    peer_lists.emplace_back();
    peer_list_iterators.push_back(peer_lists.back().begin());
    return next_id++;
}

uint16_t peer_list_count(size_t list_id) {
    assert(list_id < peer_lists.size());
    return peer_lists[list_id].size();
}

uint16_t peer_list_count_excl(size_t list_id, const struct sockaddr_in *src_addr_arr,
                              size_t src_addr_arr_size, const struct sockaddr_in *dest_addr) {
    assert(list_id < peer_lists.size());
    uint16_t count = 0;
    for (const auto &peer_addr : peer_lists[list_id]) {
        if (is_src_addr(&peer_addr, src_addr_arr, src_addr_arr_size)) {
            continue;
        }

        if (peer_addr.sin_addr.s_addr == dest_addr->sin_addr.s_addr &&
            peer_addr.sin_port == dest_addr->sin_port) {
            continue;
        }
        count++;
    }

    return count;
}

bool peer_list_contains(const size_t list_id, const struct sockaddr_in *peer) {
    assert(list_id < peer_lists.size());
    return peer_lists[list_id].find(*peer) != peer_lists[list_id].end();
}

int peer_list_add(size_t list_id, const struct sockaddr_in *peer) {
    LOG("Trying to add a new peer to peer list %zu:", list_id);
    assert(list_id < peer_lists.size());

    if (peer_list_contains(list_id, peer)) {
        LOG_CONTINUE(" already in the list\n")
        return 0;
    } else if (peer_lists[list_id].size() < MAX_CAPACITY) {
        peer_lists[list_id].insert(*peer);
        LOG_CONTINUE(" successful\n")
        return 1;
    } else {
        LOG_CONTINUE(" list is full\n")
        return -1;
    }
}

void peer_list_remove(size_t list_id, const struct sockaddr_in *peer) {
    LOG("Trying to remove a peer from list %zu:", list_id);
    assert(list_id < peer_lists.size());
    if (peer_lists[list_id].erase(*peer)) {
        LOG_CONTINUE(" successful\n")
    } else {
        LOG_CONTINUE(" not found\n")
    }
}

bool peer_list_write_to_buf_excl(size_t list_id, uint8_t *buf, size_t size,
                                 const struct sockaddr_in *src_addr_arr, size_t src_addr_arr_size,
                                 const struct sockaddr_in *dest_addr) {
    assert(list_id < peer_lists.size());
    constexpr uint8_t addr_len = 4;   // = PEER_ADDRESS_SIZE = IPv4 address length
    constexpr uint8_t port_size = 2;  // = PEER_PORT_SIZE
    constexpr size_t record_size = 7; // PEER_ADDRESS_LENGTH_SIZE + PEER_ADDRESS_SIZE + PEER_PORT_SIZE

    for (const auto &peer_addr : peer_lists[list_id]) {
        if (is_src_addr(&peer_addr, src_addr_arr, src_addr_arr_size)) {
            continue;
        }

        if (peer_addr.sin_addr.s_addr == dest_addr->sin_addr.s_addr &&
            peer_addr.sin_port == dest_addr->sin_port) {
            continue;
        }

        if (size < record_size) {
            return false; // not enough space in the buffer
        }
        size -= record_size;

        *buf++ = addr_len;                                 // length of the IPv4 address
        memcpy(buf, &peer_addr.sin_addr.s_addr, addr_len); // IPv4 address
        buf += addr_len;
        memcpy(buf, &peer_addr.sin_port, port_size); // port
        buf += port_size;
    }

    if (size > 0) {
        return false; // written less than expected
    }

    return true;
}

void peer_list_cpy(size_t src_list, size_t dest_list) {
    assert(src_list < peer_lists.size());
    assert(dest_list < peer_lists.size());

    peer_lists[dest_list] = peer_lists[src_list];
}

bool peer_list_iterator_begin(size_t list_id) {
    assert(list_id < peer_lists.size());
    peer_list_iterators[list_id] = peer_lists[list_id].begin();
    return peer_list_iterators[list_id] != peer_lists[list_id].end();
}

bool peer_list_iterator_next(size_t list_id) {
    assert(list_id < peer_lists.size());
    assert(peer_list_iterators[list_id] != peer_lists[list_id].end());
    ++peer_list_iterators[list_id];
    return peer_list_iterators[list_id] != peer_lists[list_id].end();
}

struct sockaddr_in peer_list_iterator_get_peer(size_t list_id) {
    assert(list_id < peer_lists.size());
    assert(peer_list_iterators[list_id] != peer_lists[list_id].end());
    return *peer_list_iterators[list_id];
}

bool peer_list_full(size_t list_id) {
    assert(list_id < peer_lists.size());
    return peer_lists[list_id].size() >= MAX_CAPACITY;
}

} // namespace cxx
