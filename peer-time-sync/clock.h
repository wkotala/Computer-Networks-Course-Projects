#ifndef PEER_TIME_SYNC_CLOCK_H
#define PEER_TIME_SYNC_CLOCK_H

#include <stdint.h>
#include <time.h>

#include "node_data.h"

// Initialize node's natural clock. Must be called once at the beginning of the program.
void clock_init(void);

// Returns time in ms since program start.
uint64_t get_natural_clock();

// Returns synchronized time in ms (or natural time, if node is not synchronized).
uint64_t get_clock(NodeData* node_data);

// Sets given timespec to x seconds from now.
void set_event_in_x_seconds(struct timespec* ts, time_t x);

// Returns true if current time is after the given timespec.
bool now_is_after(struct timespec* ts1);

// Updates the offset of the node based on T1, T2, T3, and T4 timestamps.
void update_offset(NodeData* node_data);

// Returns true if ts_before < ts_after.
bool check_order(struct timespec* ts_before, struct timespec* ts_after);

#endif // PEER_TIME_SYNC_CLOCK_H
