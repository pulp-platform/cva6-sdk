// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Kevin Schaerer <schkevin@student.ethz.ch>

// This example demonstrates how to use the iDMA proxy driver to use a user-space buffer
// as source and destination for a memcpy transfer. It initializes a source buffer with a known pattern
// and verifies that the destination buffer is correctly filled with the same pattern after the transfer.
// The maximum buffer size is NOT limited by the page size, since the buffers are copied to/from the kernel.
// The allocated kernel buffers can be larger than a single page and still be physically contiguous.
// Moreover, the user buffers do not have to be locked/pinned in memory.
// However, all of this comes with the overhead of copying the data to/from the kernel.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>
#include <string.h>

#include "idma-proxy.h"

const char *channel_name = "idma_chan0";

struct channel {
	int fd;
	void *src_buffer;
    void *dst_buffer;
};

struct channel channel;

int main(int argc, char *argv[])
{
    int ret;
	printf("iDMA proxy user-space test with IDMA_COPY_BUFFER\n");

    char file_path[64] = "/dev/";
    strcat(file_path, channel_name);
    channel.fd = open(file_path, O_RDWR);
    if (channel.fd < 1) {
        printf("Unable to open iDMA proxy device file: %s\r", file_path);
        exit(EXIT_FAILURE);
    }

    unsigned int test_size = 1 * 1024 * 1024; // 1 MiB

    channel.src_buffer = malloc(test_size);
    if (channel.src_buffer == NULL) {
        printf("Failed to allocate memory for source buffer\n");
        ret = -ENOMEM;
        goto alloc_src_buffer_fail;
    }

    channel.dst_buffer = malloc(test_size);
    if (channel.dst_buffer == NULL) {
        printf("Failed to allocate memory for destination buffer\n");
        ret = -ENOMEM;
        goto alloc_dst_buffer_fail;
    }

    // Write a known pattern to the source buffer
    unsigned int *src_buffer = (unsigned int *)channel.src_buffer;
    for (unsigned int i = 0; i < test_size / sizeof(unsigned int); i++) {
        src_buffer[i] = i;
    }

    unsigned int *dst_buffer = (unsigned int *)channel.dst_buffer;
    for (unsigned int i = 0; i < test_size / sizeof(unsigned int); i++) {
        dst_buffer[i] = 0x0badc0de;
    }
    printf("Source buffer initialized with test pattern, destination buffer initialized with 0x0badc0de\n");

    idma_memcpy_transfer_t transfer;
    transfer.src = (uintptr_t)channel.src_buffer;
    transfer.dst = (uintptr_t)channel.dst_buffer;
    transfer.src_type = IDMA_COPY_BUFFER;
    transfer.dst_type = IDMA_COPY_BUFFER;
    transfer.length = test_size;
    transfer.conf = 0; // No special configuration bits
    printf("Prepared transfer with src: %p, dst: %p, length: %zu\n", (void *)transfer.src, (void *)transfer.dst, transfer.length);

    ioctl(channel.fd, IOCTL_ISSUE_MEMCPY_TRANSFER, &transfer);

    printf("iDMA transfer successfully finished\n");

    // Verify the destination buffer
    dst_buffer = (unsigned int *)channel.dst_buffer;
    for (unsigned int i = 0; i < test_size / sizeof(unsigned int); i++) {
        if (dst_buffer[i] != i) {
            printf("Data mismatch at index %u: expected %u, got %u\n", i, i, dst_buffer[i]);
            exit(EXIT_FAILURE);
        }
    }
    printf("Data verification successful, all values match expected pattern\n");

    // Clean up
    free(channel.dst_buffer);
    free(channel.src_buffer);
    if (close(channel.fd) < 0) {
        printf("Failed to close tx channel file descriptor\n");
        exit(EXIT_FAILURE);
    }
    printf("Cleaned up resources, exiting test\n");

    return 0;
alloc_dst_buffer_fail:
    free(channel.src_buffer);
alloc_src_buffer_fail:
    close(channel.fd);
    return ret;
}