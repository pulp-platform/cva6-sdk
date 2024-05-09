#pragma once


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
    struct platform_device *pdev;
    // Hw device memory regions
    struct shared_mem soc_ctrl_mem;
    struct shared_mem quadrant_ctrl_mem;
    struct shared_mem clint_mem;
    struct shared_mem snitch_cluster_mem;
    struct shared_mem spm_wide_mem;
    struct shared_mem l3_mem;
    // Not accessible from the host (> 4GB)
    struct shared_mem pcie_axi_bar_mem;
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
    pr_debug("Ready to map " #NAME);                                            \
    mapoffset = cardev_data->MEM_ENTRY.pbase;                                  \
    psize = cardev_data->MEM_ENTRY.size;
