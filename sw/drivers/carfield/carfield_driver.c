#include <asm/io.h>

// Todo clean includes
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
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/string.h>

#include "carfield_driver.h"

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

struct k_list {
    struct list_head list;
    struct shared_mem *data;
};

// Device private data structure
struct cardev_private_data {
    struct platform_device *pdev;
    struct shared_mem idma_mem;
    struct shared_mem soc_ctrl_mem;
    struct shared_mem ctrl_regs_mem;
    struct shared_mem gpio_mem;
    struct shared_mem l2_intl_0_mem;
    struct shared_mem l2_cont_0_mem;
    struct shared_mem l2_intl_1_mem;
    struct shared_mem l2_cont_1_mem;
    struct shared_mem safety_island_mem;
    struct shared_mem integer_cluster_mem;
    struct shared_mem spatz_cluster_mem;
    struct shared_mem l3_mem;
    // Not accessible from the host (> 4GB)
    uintptr_t pcie_axi_bar_mem;
    // DMA buffer list
    struct list_head test_head;
    // Char device
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

#define ISOLATE_BEGIN_OFFSET 15 * 4
#define ISOLATE_END_OFFSET 20 * 4
#define CARFIELD_GPIO_N_IRQS 4
#define CARFIELD_GPIO_FIRST_IRQ 19

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
    case SOC_CTRL_MMAP_ID:
        strncpy(type, "soc_ctrl", sizeof(type));
        pr_info("Ready to map soc_ctrl\n");
        mapoffset = cardev_data->soc_ctrl_mem.pbase;
        psize = cardev_data->soc_ctrl_mem.size;
        break;
    case DMA_BUFS_MMAP_ID:
        strncpy(type, "buffer", sizeof(type));
        pr_info("Ready to map latest buffer\n");
        struct k_list* tail = list_last_entry(&cardev_data->test_head, struct k_list, list);
        pr_info("dma_addr: %llx\n", tail->data->pbase);
        mapoffset = tail->data->pbase;
        psize = tail->data->size;
        break;
    case L3_MMAP_ID:
        strncpy(type, "l3_mem", sizeof(type));
        pr_info("Ready to map l3_mem\n");
        mapoffset = cardev_data->l3_mem.pbase;
        psize = cardev_data->l3_mem.size;
        break;
    case CTRL_REGS_MMAP_ID:
        strncpy(type, "ctrl_regs", sizeof(type));
        pr_info("Ready to map ctrl_regs\n");
        mapoffset = cardev_data->ctrl_regs_mem.pbase;
        psize = cardev_data->ctrl_regs_mem.size;
        break;
    case L2_INTL_0_MMAP_ID:
        strncpy(type, "l2_intl_0", sizeof(type));
        pr_info("Ready to map l2_intl_0\n");
        mapoffset = cardev_data->l2_intl_0_mem.pbase;
        psize = cardev_data->l2_intl_0_mem.size;
        break;
    case L2_CONT_0_MMAP_ID:
        strncpy(type, "l2_cont_0", sizeof(type));
        pr_info("Ready to map l2_cont_0\n");
        mapoffset = cardev_data->l2_cont_0_mem.pbase;
        psize = cardev_data->l2_cont_0_mem.size;
        break;
    case L2_INTL_1_MMAP_ID:
        strncpy(type, "l2_intl_1", sizeof(type));
        pr_info("Ready to map l2_intl_1\n");
        mapoffset = cardev_data->l2_intl_1_mem.pbase;
        psize = cardev_data->l2_intl_1_mem.size;
        break;
    case L2_CONT_1_MMAP_ID:
        strncpy(type, "l2_cont_1", sizeof(type));
        pr_info("Ready to map l2_cont_1\n");
        mapoffset = cardev_data->l2_cont_1_mem.pbase;
        psize = cardev_data->l2_cont_1_mem.size;
        break;
    case IDMA_MMAP_ID:
        strncpy(type, "idma", sizeof(type));
        pr_info("Ready to map idma\n");
        mapoffset = cardev_data->idma_mem.pbase;
        psize = cardev_data->idma_mem.size;
        break;
    case SAFETY_ISLAND_MMAP_ID:
        strncpy(type, "safety_island", sizeof(type));
        pr_info("Ready to map safety_island\n");
        mapoffset = cardev_data->safety_island_mem.pbase;
        psize = cardev_data->safety_island_mem.size;
        break;
    case INTEGER_CLUSTER_MMAP_ID:
        strncpy(type, "integer_cluster", sizeof(type));
        pr_info("Ready to map safety_island\n");
        mapoffset = cardev_data->integer_cluster_mem.pbase;
        psize = cardev_data->integer_cluster_mem.size;
        break;
    case SPATZ_CLUSTER_MMAP_ID:
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
        pr_err("error: %s vsize %lx > psize %lx\n", type, vsize, psize);
        pr_err("  vma->vm_end %lx vma->vm_start %lx\n", vma->vm_end,
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
        void *result_virt = 0;
        // Alloc memory region (note PHY address = DMA address)
        arg.result_virt_addr = dma_alloc_coherent(&cardev_data->pdev->dev, arg.size, &arg.result_phys_addr, GFP_KERNEL);
        if (!arg.result_virt_addr)
            return -ENOMEM;

        // Offset if there is a PCIe endpoint in the device (then the driver should ran on the PCIe host)
        if (cardev_data->pcie_axi_bar_mem)
            arg.result_phys_addr += cardev_data->pcie_axi_bar_mem;

        // Add to the buffer list
        struct k_list *new = kmalloc(sizeof(struct k_list), GFP_KERNEL);
        new->data = kmalloc(sizeof(struct shared_mem), GFP_KERNEL);
        new->data->pbase = arg.result_phys_addr;
        new->data->vbase = arg.result_virt_addr;
        new->data->size = arg.size;
        list_add_tail(&new->list, &cardev_data->test_head);

        // Print the buffer list for debug
        pr_info("Reading list :\n");
        struct list_head *p;
        struct k_list *my;
        list_for_each(p, &cardev_data->test_head) {
            my = list_entry(p, struct k_list, list);
            pr_info("pbase = %#llx, psize = %#llx\n", my->data->pbase, my->data->size);
        }
        break;
    }
    case IOCTL_MEM_INFOS: {
        pr_info("Lookup %i\n", arg.mmap_id);
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

// Handle GPIO interrupts to clear the GPIO register
// (Note: other drivers, as ethernet, might handle the same irq)
int already_entered[64];
static irqreturn_t carfield_handle_irq(int irq, void *_pdev) {
    struct platform_device *pdev = _pdev;
    unsigned int pending;
    struct cardev_private_data *dev_data = dev_get_drvdata(&pdev->dev);
    struct irq_desc *desc = irq_data_to_desc(irq_get_irq_data(irq));
    uint32_t old;
    if (!already_entered[irq]) {
        already_entered[irq] = 1;
        return IRQ_NONE;
    }
    if (!desc) {
        pr_err("Could not get hw irq for %i\n", irq);
    }
    int hw_irq = desc->irq_data.hwirq;
    old = ioread32(dev_data->gpio_mem.vbase + 0x00);
    iowrite32(BIT(hw_irq - CARFIELD_GPIO_FIRST_IRQ),
              dev_data->gpio_mem.vbase + 0x00);

    already_entered[irq] = 0;
    return IRQ_HANDLED;
}

int probe_node(struct platform_device *pdev,
               struct cardev_private_data *dev_data, struct shared_mem *result,
               const char *name) {
    struct device_node *tmp_np, *tmp_mem_np;
    struct reserved_mem *tmp_mem;
    struct resource tmp_res, tmp_mem_res;

    // Get the node in the DTS
    tmp_np = of_get_child_by_name(pdev->dev.of_node, name);
    if (tmp_np) {

        // Check for reserved L3 memory
        tmp_mem_np = of_parse_phandle(tmp_np, "memory-region", 0);
        if(tmp_mem_np) {
            if(of_address_to_resource(tmp_mem_np, 0, &tmp_mem_res)) {
                pr_err("of_address_to_resource error\n");
                goto probe_node_error;
            }
            if(dev_data->l3_mem.pbase && dev_data->l3_mem.pbase != tmp_mem_res.start) {
                pr_err("I do not support multiple L3 memory regions\n");
            }
            if(!dev_data->l3_mem.pbase) {
                dev_data->l3_mem.vbase = devm_ioremap_resource(&pdev->dev, &tmp_mem_res);
                if (!dev_data->l3_mem.vbase)
                    goto probe_node_error;
                dev_data->l3_mem.pbase = tmp_mem_res.start;
                dev_data->l3_mem.size = resource_size(&tmp_mem_res);
                // Save the informations in the char device (for card_read)
                dev_data->buffer_size += sprintf(
                    dev_data->buffer + dev_data->buffer_size, "%s: %px size = %x\n",
                    tmp_mem_np->name, dev_data->l3_mem.pbase, dev_data->l3_mem.size);
            }
            pr_info("Found reserved mem\n");
        }

        // Get addresses, remap them and save them to the struct shared_mem
        of_address_to_resource(tmp_np, 0, &tmp_res);
        result->vbase = devm_ioremap_resource(&pdev->dev, &tmp_res);

        if (IS_ERR(result->vbase)) {
            pr_err("Could not map %s io-region\n", name);
        } else {
            result->pbase = tmp_res.start;
            result->size = resource_size(&tmp_res);
            pr_info("Allocated %s io-region\n", name);
            // Try to access the first address
            pr_info("Probing %s : %x\n", name, *((uint32_t *)result->vbase));
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
probe_node_error:
    *result = (struct shared_mem) { 0 };
    return -1;
}

// gets called when matched platform device
int card_platform_driver_probe(struct platform_device *pdev) {
    int ret, irq, i;
    struct cardev_private_data *dev_data;
    struct device_node *tmp_np;
    const __be32 *pcie_axi_bar_addr_field;

    pr_info("A device is detected \n");

    dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data) {
        pr_info("Cannot allocate memory \n");
        return -ENOMEM;
    }

    dev_data->pdev = pdev;

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

    // Probe idma
    probe_node(pdev, dev_data, &dev_data->idma_mem, "idma");

    // Probe AXI Bar (note that we don't want to map it but just get the phy_addr)
    tmp_np = of_get_child_by_name(pdev->dev.of_node, "pcie-axi-bar");
    if (tmp_np) {
        pcie_axi_bar_addr_field = of_get_address(tmp_np, 0, NULL, NULL);
        dev_data->pcie_axi_bar_mem =  ((uint64_t)be32_to_cpu(pcie_axi_bar_addr_field[0])) << 32;
        dev_data->pcie_axi_bar_mem += ((uint64_t)be32_to_cpu(pcie_axi_bar_addr_field[1]));
        pr_info("Found pcie-axi-bar at device address %llx\n", dev_data->pcie_axi_bar_mem);
    } else {
        dev_data->pcie_axi_bar_mem = 0;
        pr_info("No pcie-axi-bar in device tree\n");
    }

    // Probe scratch registers
    probe_node(pdev, dev_data, &dev_data->ctrl_regs_mem, "ctrl-regs");

    // Probe gpio and activate rising edge interrupts
    probe_node(pdev, dev_data, &dev_data->gpio_mem, "gpio");
    *((uint32_t *)(dev_data->gpio_mem.vbase + 0x04)) = (uint32_t)0xffffffff;
    *((uint32_t *)(dev_data->gpio_mem.vbase + 0x34)) = (uint32_t)0xffffffff;

    // Request gpio irqs
    // TODO: Do not do that if running on PCIe host
    for (i = 0; i < CARFIELD_GPIO_N_IRQS; i++) {
        irq = of_irq_get(of_get_child_by_name(pdev->dev.of_node, "gpio"), i);
        if (irq < 0)
            pr_err("Gpio irq %i not found in device tree\n", i);
        ret = request_irq(irq, carfield_handle_irq, IRQF_SHARED, pdev->name,
                          pdev);
        if (ret)
            pr_err("Request gpio irq %i failed with: %i\n", i, ret);
    }

    // Deisolate all islands
    for (i = ISOLATE_BEGIN_OFFSET; i < ISOLATE_END_OFFSET; i += 4)
        *((uint32_t *)(dev_data->soc_ctrl_mem.vbase + i)) = (uint32_t)0x0;

    // Probe safety_island
    probe_node(pdev, dev_data, &dev_data->safety_island_mem, "safety-island");

    // Probe integer_cluster
    probe_node(pdev, dev_data, &dev_data->integer_cluster_mem, "integer-cluster");

    // Probe spatz_cluster
    probe_node(pdev, dev_data, &dev_data->spatz_cluster_mem, "spatz-cluster");

    // Probe L2
    probe_node(pdev, dev_data, &dev_data->l2_intl_0_mem, "l2-intl-0");
    probe_node(pdev, dev_data, &dev_data->l2_cont_0_mem, "l2-cont-0");
    probe_node(pdev, dev_data, &dev_data->l2_intl_1_mem, "l2-intl-1");
    probe_node(pdev, dev_data, &dev_data->l2_cont_1_mem, "l2-cont-1");

    // DMA uffer list
    INIT_LIST_HEAD(&dev_data->test_head);

    // Char device
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
