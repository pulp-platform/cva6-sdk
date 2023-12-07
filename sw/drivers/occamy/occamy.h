#pragma once

#include <linux/fs.h>

// General description of memory region
struct shared_mem {
    phys_addr_t pbase;
    void __iomem *vbase;
    resource_size_t size;
};

struct k_list {
    struct list_head list;
    struct shared_mem *data;
};

// Device private data structure
struct cardev_private_data {
    // Hw device memory regions
    struct shared_mem soc_ctrl_mem;
    struct shared_mem quadrant_ctrl_mem;
    struct shared_mem clint_mem;
    struct shared_mem snitch_cluster_mem;
    struct shared_mem scratchpad_narrow_mem;
    // Buffer allocated list
    struct list_head test_head;
    // Hw device infos
    u32 n_quadrants;
    u32 n_clusters;
    u32 n_cores;
    // Chardev
    dev_t dev_num;
    struct cdev cdev;
    // String buffer (just to print out driver infos)
    char *buffer;
    unsigned int buffer_size;
};

// Driver private data structure
struct cardrv_private_data {
    int total_devices;
    dev_t device_num_base;
    struct class *class_card;
    struct device *device_card;
};

// File operations (carfield_fops.c)

extern struct file_operations card_fops;

// IOCTL arguments

struct card_alloc_arg {
    size_t size;
    uint64_t result_phys_addr;
    uint64_t result_virt_addr;
};

// File
#define RDWR 0x11
#define RDONLY 0x01
#define WRONLY 0x10

#define PDATA_SIZE 2048
#define PDATA_PERM RDWR
#define PDATA_SERIAL "1"

// Memmap macro
#define MAP_DEVICE_REGION(NAME, MEM_ENTRY)                                     \
    strncpy(type, #NAME, sizeof(type));                                        \
    pr_info("Ready to map " #NAME);                                            \
    mapoffset = cardev_data->MEM_ENTRY.pbase;                                  \
    psize = cardev_data->MEM_ENTRY.size;

// Memmap macro
#define IOCTL_GET_DEVICE_INFOS(MEM_ENTRY)                \
    arg_local.result_virt_addr = MEM_ENTRY.vbase;        \
    arg_local.result_phys_addr = MEM_ENTRY.pbase;        \
    arg_local.size             = MEM_ENTRY.size;

// Memmap offsets, used for mmap and ioctl
#define SOC_CTRL_MAPID 0
#define QUADRANT_CTRL_MAPID 2
#define BUFFER_MAPID 10000
#define CLINT_MAPID 5
#define SNITCH_CLUSTER_MAPID 100
#define SCRATCHPAD_NARROW_MAPID 10