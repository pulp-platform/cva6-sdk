#ifndef __CARFIELD_DRIVER_H
#define __CARFIELD_DRIVER_H

#include <linux/fs.h>

// Memmap offsets, used for mmap and ioctl
#define SOC_CTRL_MAPID 0
#define L3_MMAP_ID 1
#define QUADRANT_CTRL_MAPID 2
#define DMA_BUFS_MMAP_ID 10000
#define CLINT_MAPID 5
#define SNITCH_CLUSTER_MAPID 100
#define SCRATCHPAD_WIDE_MAPID 10

// TODO: Define properly with the Linux API
#define IOCTL_DMA_ALLOC 0
#define IOCTL_MEM_INFOS 1

#define PTR_TO_DEVDATA_REGION(VAR,DEVDATA,X) \
    switch(X) { \
        case(SOC_CTRL_MAPID         ): VAR = &DEVDATA->soc_ctrl_mem         ; break; \
        case(QUADRANT_CTRL_MAPID    ): VAR = &DEVDATA->quadrant_ctrl_mem    ; break; \
        case(CLINT_MAPID            ): VAR = &DEVDATA->clint_mem            ; break; \
        case(SNITCH_CLUSTER_MAPID   ): VAR = &DEVDATA->snitch_cluster_mem   ; break; \
        case(SCRATCHPAD_WIDE_MAPID  ): VAR = &DEVDATA->spm_wide_mem         ; break; \
        case(L3_MMAP_ID             ): VAR = &DEVDATA->l3_mem               ; break; \
        default                      : VAR = NULL                           ; break; \
    }

// For now keep all the ioctl args in the same struct
struct card_ioctl_arg {
    size_t size;
    uint64_t result_phys_addr;
    uint64_t result_virt_addr;
    int mmap_id; // ioctl 2
};

// Memmap macro
#define IOCTL_GET_DEVICE_INFOS(MEM_ENTRY)                \
    arg_local.result_virt_addr = MEM_ENTRY.vbase;        \
    arg_local.result_phys_addr = MEM_ENTRY.pbase;        \
    arg_local.size             = MEM_ENTRY.size;


#endif
