#include <asm/io.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pulp Platform");
MODULE_DESCRIPTION("Carfield driver");

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

// General description of memory region
struct shared_mem {
    phys_addr_t pbase;
    void __iomem *vbase;
    resource_size_t size;
};

// Device private data structure
struct cardev_private_data {
    struct shared_mem soc_ctrl_mem;
    struct shared_mem l2_intl_0_mem;
    struct shared_mem l2_cont_0_mem;
    struct shared_mem l2_intl_1_mem;
    struct shared_mem l2_cont_1_mem;
    struct shared_mem safety_island_mem;
    struct shared_mem integer_cluster_mem;
    struct shared_mem spatz_cluster_mem;
    char *buffer;
    unsigned int buffer_size;
    dev_t dev_num;
    struct cdev cdev;
};

// Driver private data structure
struct cardrv_private_data {
    int total_devices;
    dev_t device_num_base;
    struct class *class_card;
    struct device *device_card;
};

struct cardrv_private_data cardrv_data;

#define RDWR 0x11
#define RDONLY 0x01
#define WRONLY 0x10

#define PDATA_SIZE 2048
#define PDATA_PERM RDWR
#define PDATA_SERIAL "1"

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

// VM_RESERVERD for mmap
#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

int card_mmap(struct file *filp, struct vm_area_struct *vma) {
    unsigned long mapoffset, vsize, psize;
    char type[20];
    int ret;
    struct cardev_private_data *cardev_data =
        (struct cardev_private_data *)filp->private_data;

    switch (vma->vm_pgoff) {
    case 0:
        strncpy(type, "soc_ctrl", sizeof(type));
        pr_info("Ready to map soc_ctrl\n");
        mapoffset = cardev_data->soc_ctrl_mem.pbase;
        psize = cardev_data->soc_ctrl_mem.size;
        break;
    case 10:
        strncpy(type, "l2_intl_0", sizeof(type));
        pr_info("Ready to map l2_intl_0\n");
        mapoffset = cardev_data->l2_intl_0_mem.pbase;
        psize = cardev_data->l2_intl_0_mem.size;
        break;
    case 11:
        strncpy(type, "l2_cont_0", sizeof(type));
        pr_info("Ready to map l2_cont_0\n");
        mapoffset = cardev_data->l2_cont_0_mem.pbase;
        psize = cardev_data->l2_cont_0_mem.size;
        break;
    case 12:
        strncpy(type, "l2_intl_1", sizeof(type));
        pr_info("Ready to map l2_intl_1\n");
        mapoffset = cardev_data->l2_intl_1_mem.pbase;
        psize = cardev_data->l2_intl_1_mem.size;
        break;
    case 13:
        strncpy(type, "l2_cont_1", sizeof(type));
        pr_info("Ready to map l2_cont_1\n");
        mapoffset = cardev_data->l2_cont_1_mem.pbase;
        psize = cardev_data->l2_cont_1_mem.size;
        break;
    case 100:
        strncpy(type, "safety_island", sizeof(type));
        pr_info("Ready to map safety_island\n");
        mapoffset = cardev_data->safety_island_mem.pbase;
        psize = cardev_data->safety_island_mem.size;
        break;
    case 200:
        strncpy(type, "integer_cluster", sizeof(type));
        pr_info("Ready to map safety_island\n");
        mapoffset = cardev_data->integer_cluster_mem.pbase;
        psize = cardev_data->integer_cluster_mem.size;
        break;
    case 300:
        strncpy(type, "spatz_cluster", sizeof(type));
        pr_info("Ready to map spatz_cluster\n");
        mapoffset = cardev_data->spatz_cluster_mem.pbase;
        psize = cardev_data->spatz_cluster_mem.size;
        break;
    default:
        pr_err("Unknown page offset\n");
        return -EINVAL;
    }

    vsize = vma->vm_end - vma->vm_start;
    if (vsize > psize) {
        pr_info("error: vsize %ld > psize %ld\n", vsize, psize);
        pr_info("  vma->vm_end %lx vma->vm_start %lx\n", vma->vm_end,
                vma->vm_start);
        return -EINVAL;
    }

    // set protection flags to avoid caching and paging
    vma->vm_flags |= VM_IO | VM_RESERVED;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    pr_info("%s mmap: phys: %#lx, virt: %#lx vsize: %#lx psize: %#lx\n", type,
            mapoffset, vma->vm_start, vsize, psize);

    ret = remap_pfn_range(vma, vma->vm_start, mapoffset >> PAGE_SHIFT, vsize,
                          vma->vm_page_prot);

    if (ret)
        pr_info("mmap error: %d\n", ret);

    return ret;
}

struct card_alloc_arg {
    size_t size;
    uint64_t result_phys_addr;
    uint64_t result_virt_addr;
};

static long card_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg) {
    void __user *argp = (void __user *)arg;
    int __user *p = argp;

    // check correct magic
    // if (_IOC_TYPE(cmd) != SNIOC_MAGIC)
    //   return -ENOTTY;

    switch (cmd) {
    // Alloc physically contiguous memory
    case 0x1: {
        struct card_alloc_arg arg;
        phys_addr_t result_phy = 0;
        if (copy_from_user(&arg, p, sizeof(struct card_alloc_arg)))
            return -EFAULT;
        arg.result_virt_addr = kmalloc(arg.size, GFP_ATOMIC);
        if (arg.result_virt_addr < 0) {
            return arg.result_virt_addr;
        }
        arg.result_phys_addr = virt_to_phys(arg.result_virt_addr);
        copy_to_user(p, &arg, sizeof(struct card_alloc_arg));

        return arg.result_virt_addr;
    }
    }
    return 0;
}

ssize_t card_read(struct file *filp, char __user *buff, size_t count,
                  loff_t *f_pos) {
    struct cardev_private_data *cardev_data =
        (struct cardev_private_data *)filp->private_data;

    int max_size = cardev_data->buffer_size;

    // pr_info("read requested for %zu bytes \n", count);
    // pr_info("Current file position = %lld\n", *f_pos);
    // Adjust the count
    if ((*f_pos + count) > max_size)
        count = max_size - *f_pos;

    // Copy to user
    if (copy_to_user(buff, cardev_data->buffer + (*f_pos), count)) {
        return -EFAULT;
    }

    // Update the current file position
    *f_pos += count;

    // pr_info("Number of bytes successfully read= %zu\n", count);
    // pr_info("Updated file position = %lld\n", *f_pos);

    return count;
}

int card_open(struct inode *inode, struct file *filp) {
    int ret;
    struct cardev_private_data *cardev_data;
    cardev_data = container_of(inode->i_cdev, struct cardev_private_data, cdev);
    filp->private_data = cardev_data;
    ret = check_permission(PDATA_PERM, filp->f_mode);
    // (!ret) ? pr_info("open was successful\n")
    //        : pr_info("open was unsuccessful\n");
    // pr_info("Does this run lets see \n");
    return ret;
}

int card_release(struct inode *inode, struct file *filp) {
    // pr_info("release was successful \n");
    return 0;
}

// file operations of the driver
struct file_operations card_fops = { .open = card_open,
                                     .release = card_release,
                                     .read = card_read,
                                     .mmap = card_mmap,
                                     .unlocked_ioctl = card_ioctl,
                                     .owner = THIS_MODULE };

// gets called when the device is removed from the system
int card_platform_driver_remove(struct platform_device *pdev) {
    struct cardev_private_data *dev_data = dev_get_drvdata(&pdev->dev);

    // Remove a device that was created with device_create()
    device_destroy(cardrv_data.class_card, dev_data->dev_num);

    // Remove a cdev entry from the system
    cdev_del(&dev_data->cdev);

    cardrv_data.total_devices--;

    pr_info("A device is removed \n");

    return 0;
}

int probe_node(struct platform_device *pdev,
               struct cardev_private_data *dev_data, struct shared_mem *result,
               const char *name) {
    struct device_node *tmp_np;
    struct resource tmp_res;
    // Get the node in the DTS
    tmp_np = of_get_child_by_name(pdev->dev.of_node, name);
    if (tmp_np) {
        // Get addresses, remap them and save them to the struct shared_mem
        of_address_to_resource(tmp_np, 0, &tmp_res);
        result->vbase = devm_ioremap_resource(&pdev->dev, &tmp_res);
        if (IS_ERR(result->vbase)) {
            pr_err("Could not map %s io-region\n", name);
        } else {
            result->pbase = tmp_res.start;
            result->size = resource_size(&tmp_res);
            pr_debug("Allocated %s io-region\n", name);
            // Try to access the memory region
            pr_debug("Probing %s : %x\n");
            if (*((uint32_t *)result->vbase) == 0xbadcab1e) {
                pr_warn("%s not found in hardware (0xbadcab1e)!\n", name);
                *result = (struct shared_mem) { 0 };
            }
            // Save the informations in the char device (for card_read)
            dev_data->buffer_size += sprintf(
                dev_data->buffer + dev_data->buffer_size, "%s: %px size = %x\n",
                name, result->pbase, result->size);
        }
        return 0;
    } else {
        pr_err("No %s in device tree\n", name);
    }
    *result = (struct shared_mem) { 0 };
    return 1;
}

// gets called when matched platform device
int card_platform_driver_probe(struct platform_device *pdev) {
    int ret;
    struct cardev_private_data *dev_data;

    pr_info("A device is detected \n");

    dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data) {
        pr_info("Cannot allocate memory \n");
        return -ENOMEM;
    }

    // Save the device private data pointer in platform device structure
    dev_set_drvdata(&pdev->dev, dev_data);

    pr_info("Device size = %d\n", PDATA_SIZE);
    pr_info("Device permission = %d\n", PDATA_PERM);
    pr_info("Device serial number = %s\n", PDATA_SERIAL);
    pr_info("platform_device(%p) : name=%s ; id=%i ; id_auto=%i ; dev=%p(%s) ; "
            "num_ressources=%i ; id_entry=%p\n",
            pdev, pdev->name, pdev->id, pdev->id_auto, pdev->dev,
            pdev->dev.init_name, pdev->num_resources, pdev->id_entry);

    /* Dynamically allocate memory for the device buffer using size information
     * from the platform data */
    dev_data->buffer = devm_kzalloc(&pdev->dev, PDATA_SIZE, GFP_KERNEL);
    if (!dev_data->buffer) {
        pr_info("Cannot allocate memory \n");
        return -ENOMEM;
    }
    dev_data->buffer_size = 0;

    // Get the device number
    dev_data->dev_num = cardrv_data.device_num_base + pdev->id;

    // cdev init and cdev add
    cdev_init(&dev_data->cdev, &card_fops);

    dev_data->cdev.owner = THIS_MODULE;

    // Probe soc_ctrl
    probe_node(pdev, dev_data, &dev_data->soc_ctrl_mem, "soc-ctrl");

    // Probe safety_island
    probe_node(pdev, dev_data, &dev_data->safety_island_mem, "safety-island");

    // Probe integer_cluster
    probe_node(pdev, dev_data, &dev_data->integer_cluster_mem,
               "integer-cluster");

    // Probe spatz_cluster
    probe_node(pdev, dev_data, &dev_data->spatz_cluster_mem, "spatz-cluster");

    // Probe L2
    probe_node(pdev, dev_data, &dev_data->l2_intl_0_mem, "l2-intl-0");
    probe_node(pdev, dev_data, &dev_data->l2_cont_0_mem, "l2-cont-0");
    probe_node(pdev, dev_data, &dev_data->l2_intl_1_mem, "l2-intl-1");
    probe_node(pdev, dev_data, &dev_data->l2_cont_1_mem, "l2-cont-1");

    ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
    if (ret < 0) {
        pr_err("Cdev add failed \n");
        return ret;
    }

    // Create device file for the detected platform device
    cardrv_data.device_card =
        device_create(cardrv_data.class_card, NULL, dev_data->dev_num, NULL,
                      "cardev-%d", pdev->id);
    if (IS_ERR(cardrv_data.device_card)) {
        pr_err("Device create failed \n");
        ret = PTR_ERR(cardrv_data.device_card);
        cdev_del(&dev_data->cdev);
        return ret;
    }

    cardrv_data.total_devices++;

    pr_info("Probe was successful \n");

    return 0;
}

static const struct of_device_id carfield_of_match[] = {
    { .compatible = "eth,carfield-soc", }, {},
};
MODULE_DEVICE_TABLE(of, carfield_of_match);

struct platform_driver card_platform_driver = {
    .probe = card_platform_driver_probe,
    .remove = card_platform_driver_remove,
    .driver = { .name = "eth-carfield", .of_match_table = carfield_of_match, }
};

#define MAX_DEVICES 10

static int __init card_platform_driver_init(void) {
    int ret;

    // Dynamically allocate a device number for MAX_DEVICES
    ret = alloc_chrdev_region(&cardrv_data.device_num_base, 0, MAX_DEVICES,
                              "cardevs");
    if (ret < 0) {
        pr_err("Alloc chrdev failed \n");
        return ret;
    }

    // Create a device class under /sys/class
    cardrv_data.class_card = class_create(THIS_MODULE, "card_class");
    if (IS_ERR(cardrv_data.class_card)) {
        pr_err("Class creation failed \n");
        ret = PTR_ERR(cardrv_data.class_card);
        unregister_chrdev_region(cardrv_data.device_num_base, MAX_DEVICES);
        return ret;
    }

    platform_driver_register(&card_platform_driver);

    pr_info("card platform driver loaded \n");

    return 0;
}

static void __exit card_platform_driver_cleanup(void) {
    // Unregister the platform driver
    platform_driver_unregister(&card_platform_driver);

    // Class destroy
    class_destroy(cardrv_data.class_card);

    // Unregister device numbers for MAX_DEVICES
    unregister_chrdev_region(cardrv_data.device_num_base, MAX_DEVICES);

    pr_info("card platform driver unloaded \n");
}

module_init(card_platform_driver_init);
module_exit(card_platform_driver_cleanup);
