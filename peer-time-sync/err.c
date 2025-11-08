#include "err.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include "node_data.h"

static NodeData* node_data_to_cleanup; // global pointer to node's data needed for cleanup

void register_node_data(NodeData* node_data) {
    node_data_to_cleanup = node_data;
    node_data->sock_fd = -1;        // invalid socket
    node_data->my_addresses = NULL; // invalid addresses
}

static void cleanup() {
    if (node_data_to_cleanup) {
        if (node_data_to_cleanup->sock_fd >= 0) {
            close(node_data_to_cleanup->sock_fd);
        }
        if (node_data_to_cleanup->my_addresses != NULL) {
            free(node_data_to_cleanup->my_addresses);
        }
    }
}

noreturn void syserr(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    fprintf(stderr, "ERROR ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, " (%d; %s)\n", org_errno, strerror(org_errno));
    cleanup();
    exit(1);
}

noreturn void fatal(const char* fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, "\n");
    cleanup();
    exit(1);
}

void error(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    fprintf(stderr, "ERROR ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (org_errno != 0) {
        fprintf(stderr, " (%d; %s)", org_errno, strerror(org_errno));
    }
    fprintf(stderr, "\n");
}

void error_msg_hex(const uint8_t* msg_buffer, size_t msg_len) {
    if (msg_buffer == NULL) {
        fprintf(stderr, "ERROR MSG \n");
        return;
    }

    fprintf(stderr, "ERROR MSG ");
    size_t bytes_to_print = msg_len > 10 ? 10 : msg_len;

    for (size_t i = 0; i < bytes_to_print; ++i) {
        fprintf(stderr, "%02x", msg_buffer[i]);
    }
    fprintf(stderr, "\n");
}