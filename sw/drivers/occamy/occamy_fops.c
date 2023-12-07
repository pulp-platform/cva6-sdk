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
    // pr_info("release was successful \n");
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
    case SCRATCHPAD_NARROW_MAPID:
        MAP_DEVICE_REGION("scratchpad_narrow", scratchpad_narrow_mem);
        break;
    case BUFFER_MAPID:
        strncpy(type, "buffer", sizeof(type));
        pr_info("Ready to map latest buffer\n");
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

    pr_info("%s mmap: phys: %#lx, virt: %#lx vsize: %#lx psize: %#lx\n", type,
            mapoffset, vma->vm_start, vsize, psize);

    ret = remap_pfn_range(vma, vma->vm_start, mapoffset >> PAGE_SHIFT, vsize,
                          vma->vm_page_prot);

    if (ret)
        pr_info("mmap error: %d\n", ret);

    return ret;
}

long card_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct card_alloc_arg __user *arg_ptr = (void __user *)arg;
    struct card_alloc_arg arg_local;
    struct cardev_private_data *cardev_data;

    // Get arg from the user
    if (copy_from_user(&arg_local, arg_ptr, sizeof(struct card_alloc_arg)))
        return -EFAULT;

    // Fetch driver data
    cardev_data = (struct cardev_private_data *)file->private_data;

    // Execute operation
    switch (cmd) {
    // Alloc physically contiguous memory
    case BUFFER_MAPID:
        arg_local.result_virt_addr = kmalloc(arg_local.size, GFP_ATOMIC | GFP_DMA);
        if (arg_local.result_virt_addr < 0) {
            return arg_local.result_virt_addr;
        }
        arg_local.result_phys_addr = virt_to_phys(arg_local.result_virt_addr);

        // Add to the buffer list
        struct k_list *new = kmalloc(sizeof(struct k_list), GFP_KERNEL);
        new->data = kmalloc(sizeof(struct shared_mem), GFP_KERNEL);
        new->data->pbase = arg_local.result_phys_addr;
        new->data->vbase = arg_local.result_virt_addr;
        new->data->size = arg_local.size;
        list_add_tail(&new->list, &cardev_data->test_head);
        break;
    // Return infos of the device regions
    case SOC_CTRL_MAPID:
        IOCTL_GET_DEVICE_INFOS(cardev_data->soc_ctrl_mem);
        break;
    case QUADRANT_CTRL_MAPID:
        IOCTL_GET_DEVICE_INFOS(cardev_data->soc_ctrl_mem);
        break;
    case CLINT_MAPID:
        IOCTL_GET_DEVICE_INFOS(cardev_data->soc_ctrl_mem);
        break;
    case SNITCH_CLUSTER_MAPID:
        IOCTL_GET_DEVICE_INFOS(cardev_data->soc_ctrl_mem);
        break;
    case SCRATCHPAD_NARROW_MAPID:
        IOCTL_GET_DEVICE_INFOS(cardev_data->soc_ctrl_mem);
        break;
    // Unknown operation
    default:
        pr_err("Unknown IOCTL control\n");
        return -EINVAL;
    }
    // Return arg to the user
    copy_to_user(arg_ptr, &arg_local, sizeof(struct card_alloc_arg));
    return 0;
}

// file operations of the driver
struct file_operations card_fops = { .open = card_open,
                                     .release = card_release,
                                     .read = card_read,
                                     .mmap = card_mmap,
                                     .unlocked_ioctl = card_ioctl,
                                     .owner = THIS_MODULE };
