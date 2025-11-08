#include "clock.h"

#include <time.h>

#include "err.h"

static struct timespec start_time = {.tv_sec = 0, .tv_nsec = 0};

void clock_init(void) {
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
        syserr("clock_gettime failed");
}

uint64_t get_natural_clock() {
    struct timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) < 0)
        syserr("clock_gettime failed");

    uint64_t elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000 +
                            (current_time.tv_nsec - start_time.tv_nsec) / 1000000;
    return elapsed_time;
}

uint64_t get_clock(NodeData* node_data) {
    int64_t natural_time = (int64_t)get_natural_clock();
    if (node_data->sync_level == CLOCK_UNSYNCHRONIZED) {
        return natural_time;
    } else if (natural_time >= node_data->offset_ms) {
        return natural_time - node_data->offset_ms;
    } else { // in case of unlikely overflow
        return 0;
    }
}

void set_event_in_x_seconds(struct timespec* ts, time_t x) {
    if (clock_gettime(CLOCK_MONOTONIC, ts) < 0)
        syserr("clock_gettime failed");

    ts->tv_sec += x;
}

bool now_is_after(struct timespec* ts1) {
    struct timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) < 0)
        syserr("clock_gettime failed");

    return check_order(ts1, &current_time);
}

void update_offset(NodeData* node_data) {
    node_data->offset_ms = (int64_t)node_data->T2 - (int64_t)node_data->T1 + (int64_t)node_data->T3 -
                           (int64_t)node_data->T4;
    node_data->offset_ms /= 2;
}

bool check_order(struct timespec* ts_before, struct timespec* ts_after) {
    return ts_before->tv_sec < ts_after->tv_sec ||
           (ts_before->tv_sec == ts_after->tv_sec && ts_before->tv_nsec < ts_after->tv_nsec);
}