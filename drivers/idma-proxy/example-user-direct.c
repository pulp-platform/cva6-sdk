// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Kevin Schaerer <schkevin@student.ethz.ch>

// This example demonstrates how to use the iDMA proxy driver to use user-space buffers directly
// WITHOUT copying to/from kernel buffers as source and destination for a memcpy transfer.
// It initializes a source buffer with a known pattern and verifies that the destination buffer is
// correctly filled with the same pattern after the transfer. The maximum buffer size is limited by
// the PAGE_SIZE, since only this size guarantees that the buffers are physically contiguous.
// This example uses the IDMA_USER_DIRECT port type, which allows direct access to user-space buffers.
// Please note that the entire source or destination buffer/range must be physically contiguous for
// the DMA transfer to work correctly.

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

void *create_pinned_buffer(size_t len);

int main(int argc, char *argv[])
{
    int ret;
	printf("iDMA proxy user-space test with IDMA_USER_DIRECT\n");

    char file_path[64] = "/dev/";
    strcat(file_path, channel_name);
    channel.fd = open(file_path, O_RDWR);
    if (channel.fd < 1) {
        printf("Unable to open iDMA proxy device file: %s\r", file_path);
        exit(EXIT_FAILURE);
    }

    unsigned int test_size = 2 * 1024; // 2 KiB

    channel.src_buffer = create_pinned_buffer(test_size);
    if (channel.src_buffer == NULL) {
        printf("Failed to allocate pinned memory for source buffer\n");
        ret = -ENOMEM;
        goto alloc_src_buffer_fail;
    }

    channel.dst_buffer = create_pinned_buffer(test_size);
    if (channel.dst_buffer == NULL) {
        printf("Failed to allocate pinned memory for destination buffer\n");
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
    transfer.src_type = IDMA_USER_DIRECT;
    transfer.dst_type = IDMA_USER_DIRECT;
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

/**
 * @brief: Creates a pinned buffer of the specified length
 *
 * This function allocates a pinned buffer of the specified length using posix_memalign
 * and locks it in memory using mlock to prevent it from being swapped out.
 * The allocated buffer is guaranteed to be aligned to the system's page size.
 *
 * @param len: Length of the buffer to allocate
 * 
 * @return: Pointer to the allocated pinned buffer
 */
void *create_pinned_buffer(size_t len)
{
    void *buffer;
    size_t page_size = sysconf(_SC_PAGESIZE);

    if (posix_memalign(&buffer, page_size, len)) {
        fprintf(stderr, "Failed to allocate aligned memory: %s\n", strerror(errno));
        exit(1);
    }

    if (mlock(buffer, len)) {
        fprintf(stderr, "Failed to lock page in memory: %s\n", strerror(errno));
        exit(1);
    }

    return buffer;
}

