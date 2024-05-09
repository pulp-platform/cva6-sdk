#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/log2.h>

#include "occamy_driver.h"
#include "occamy.h"

ssize_t card_read(struct file *filp, char __user *buff, size_t count,
                  loff_t *f_pos) {
    struct cardev_private_data *cardev_data =
        (struct cardev_private_data *)filp->private_data;

    int max_size = cardev_data->buffer_size;

    // Adjust the count
    if ((*f_pos + count) > max_size)
        count = max_size - *f_pos;

    // Copy to user
    if (copy_to_user(buff, cardev_data->buffer + (*f_pos), count)) {
        return -EFAULT;
    }

    // Update the current file position
    *f_pos += count;

    return count;
}

int check_permission(int dev_perm, int acc_mode) {
    if (dev_perm == RDWR)
        return 0;

    // ensures readonly access
    if ((dev_perm == RDONLY) &&
        ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)))
        return 0;

    // ensures writeonly access
    if ((dev_perm == WRONLY) &&
        ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)))
        return 0;

    return -EPERM;
}

int card_open(struct inode *inode, struct file *filp) {
    int ret;
    struct cardev_private_data *cardev_data;
    cardev_data = container_of(inode->i_cdev, struct cardev_private_data, cdev);
    filp->private_data = cardev_data;
    ret = check_permission(PDATA_PERM, filp->f_mode);
    return ret;
}

int card_release(struct inode *inode, struct file *filp) {
    // pr_debug("release was successful \n");
    return 0;
}

int card_mmap(struct file *filp, struct vm_area_struct *vma) {
    struct k_list *bufs_tail;
    unsigned long mapoffset, vsize, psize;
    char type[20];
    int ret;
    struct cardev_private_data *cardev_data =
        (struct cardev_private_data *)filp->private_data;

    switch (vma->vm_pgoff) {
    case SOC_CTRL_MAPID:
        MAP_DEVICE_REGION("soc_ctrl", soc_ctrl_mem);
        break;
    case QUADRANT_CTRL_MAPID:
        MAP_DEVICE_REGION("quadrant_ctrl", quadrant_ctrl_mem);
        break;
    case CLINT_MAPID:
        MAP_DEVICE_REGION("clint", clint_mem);
        break;
    case SNITCH_CLUSTER_MAPID:
        MAP_DEVICE_REGION("snitch_cluster", snitch_cluster_mem);
        break;
    case SCRATCHPAD_WIDE_MAPID:
        MAP_DEVICE_REGION("spm_wide", spm_wide_mem);
        break;
    case L3_MMAP_ID:
        MAP_DEVICE_REGION("l3_mem", l3_mem);
        break;
    case DMA_BUFS_MMAP_ID:
        strncpy(type, "buffer", sizeof(type));
        pr_debug("Ready to map latest buffer\n");
        bufs_tail =
            list_last_entry(&cardev_data->test_head, struct k_list, list);
        mapoffset = bufs_tail->data->pbase;
        psize = bufs_tail->data->size;
        break;
    default:
        pr_err("Unknown page offset\n");
        return -EINVAL;
    }

    vsize = vma->vm_end - vma->vm_start;
    if (vsize > psize) {
        pr_err("error: vsize %ld > psize %ld\n", vsize, psize);
        pr_err("  vma->vm_end %lx vma->vm_start %lx\n", vma->vm_end,
               vma->vm_start);
        // return -EINVAL;
    }

    // set protection flags to avoid caching and paging
    vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    pr_debug("%s mmap: phys: %#lx, virt: %#lx vsize: %#lx psize: %#lx\n", type,
            mapoffset, vma->vm_start, vsize, psize);

    ret = remap_pfn_range(vma, vma->vm_start, mapoffset >> PAGE_SHIFT, vsize,
                          vma->vm_page_prot);

    if (ret)
        pr_debug("mmap error: %d\n", ret);

    return ret;
}

static long card_ioctl(struct file *file, unsigned int cmd, unsigned long arg_user_addr) {
    // Pointers to user arguments
    void __user *argp = (void __user *)arg_user_addr;
    // Get driver data
    struct cardev_private_data *cardev_data =
        (struct cardev_private_data *)file->private_data;
    // Fetch user arguments
    struct card_ioctl_arg arg;
    if (copy_from_user(&arg, argp, sizeof(struct card_ioctl_arg)))
        return -EFAULT;

    switch (cmd) {
    // Alloc physically contiguous memory
    case IOCTL_DMA_ALLOC: {
        dma_addr_t result_phys = 0;
        void *result_virt = 0;
        printk("dma_alloc_coherent %p, %llx (%llx pages)\n", &cardev_data->pdev->dev, arg.size, 1 << order_base_2(ALIGN(arg.size, PAGE_SIZE)/PAGE_SIZE));
        // Alloc memory region (note PHY address = DMA address), issue with dma_alloc_coherent on milk-v
        result_virt = __get_free_pages(GFP_KERNEL | GFP_DMA32, order_base_2(ALIGN(arg.size, PAGE_SIZE)/PAGE_SIZE));
        result_phys = virt_to_phys(result_virt);
        printk("dma_alloc_coherent returns %llx %llx\n", result_virt, result_phys);
        if (!result_virt)
            return -ENOMEM;
        arg.result_virt_addr = result_virt;
        arg.result_phys_addr = result_phys;

        // Offset if there is a PCIe endpoint in the device (then the driver should ran on the PCIe host)
        // The mask removes the host offset to the device's tree
        if (cardev_data->pcie_axi_bar_mem.pbase)
            arg.result_phys_addr += 0xffffffff & cardev_data->pcie_axi_bar_mem.pbase;

        // Add to the buffer list
        struct k_list *new = kmalloc(sizeof(struct k_list), GFP_KERNEL);
        new->data = kmalloc(sizeof(struct shared_mem), GFP_KERNEL);
        new->data->pbase = arg.result_phys_addr;
        new->data->vbase = arg.result_virt_addr;
        new->data->size = arg.size;
        list_add_tail(&new->list, &cardev_data->test_head);

        // Print the buffer list for debug
        pr_debug("Reading list :\n");
        struct list_head *p;
        struct k_list *my;
        list_for_each(p, &cardev_data->test_head) {
            my = list_entry(p, struct k_list, list);
            pr_debug("pbase = %#llx, psize = %#llx\n", my->data->pbase, my->data->size);
        }
        break;
    }
    case IOCTL_MEM_INFOS: {
        pr_debug("Lookup %i\n", arg.mmap_id);
        struct shared_mem *requested_mem;
        PTR_TO_DEVDATA_REGION(requested_mem, cardev_data, arg.mmap_id)
        // TODO differenciate errors from uninitialized memory and unknown map_id
        if( !requested_mem ){
            pr_err("Unknown mmap_id %i\n", arg.mmap_id);
            return -1;
        }
        arg.size = requested_mem->size;
        arg.result_phys_addr = requested_mem->pbase;
        break;
    }
    default:
        return -1;
    }

    // Send back result to user
    if(copy_to_user(argp, &arg, sizeof(struct card_ioctl_arg)))
        return -EFAULT;
    return 0;
}

// file operations of the driver
struct file_operations card_fops = { .open = card_open,
                                     .release = card_release,
                                     .read = card_read,
                                     .mmap = card_mmap,
                                     .unlocked_ioctl = card_ioctl,
                                     .owner = THIS_MODULE };
