#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/unistd.h>
#include <linux/version.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("A simple Linux kernel module");
MODULE_VERSION("0.1");

#define DEVICE_NAME "cobi_vpci_device"
#define CLASS_NAME "vpci"
#define MAX_DEVICES 10
#define DEVICE_FILE_TEMPLATE "/dev/cobi_pcie_card%d"


static int major;
static struct class *vpci_class = NULL;
static struct device *vpci_device = NULL;
static struct cdev vpci_cdev;
struct file **fd_val;    //used to store the file descriptors of the registered PCIe devices
static DEFINE_MUTEX(vpci_mutex); //lock for the virutal pcie device

//queue for the virtual pcie device
struct vpci_data {
    struct list_head list; // Link in the vpci queue
    char *data;            // Data pointer
    size_t count;            // Size of the data
};

static LIST_HEAD(vpci_queue);                // Linked list head for VPCI queue
static struct workqueue_struct *vpci_wq;     // Pointer to the workqueue for VPCI
static void vpci_work_handler(struct work_struct *work); // Work handler function prototype
static DECLARE_WORK(vpci_work, vpci_work_handler); //declare the queue 

static ssize_t vpci_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    kernel_read(fd_val[0], buf, len, offset);
    return 0;
}

//TODO: create a queue to store asynchrounous data from the PCIe device that will correlate with an vpci driver-assigned problem id. Allow the data to be deleted after it has been sent to the user. 

//LIMIT of 32 problems at a time
static ssize_t vpci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    
    
    kernel_write(fd_val[0], buf, len,offset);

    /*
    struct vpci_data *entry;

    if (len == 0)
        return 0;

    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return -ENOMEM;

    entry->data = kmalloc(len, GFP_KERNEL);
    if (!entry->data) {
        kfree(entry);
        return -ENOMEM;
    }

    if (copy_from_user(entry->data, buf, len)) {
        kfree(entry->data);
        kfree(entry);
        return -EFAULT;
    }

    //TODO assign a problem ID from 0-15 and PCI card 1-2. Don't overfill queue with problems 

    entry->count = len;

    mutex_lock(&vpci_mutex);
    list_add_tail(&entry->list, &vpci_queue);
    mutex_unlock(&vpci_mutex);

    queue_work(vpci_wq, &vpci_work);

    printk(KERN_INFO "VPCI Driver: Write to device\n");

    //TODO Assign ID and pass user point to read function in a work queue 
*/
    return len;
}

//read queue will poll and return all problems once it has finished

/*
static void vpci_work_handler(struct work_struct *work)
{

    //take the data from the user and add problem ids

    //record the starting problem id and total problem ids

    //send the data to the PCIe device



    mutex_lock(&vpci_mutex);
    list_for_each_entry_safe(entry, tmp, &vpci_queue, list) {
        // Process the data and write it to the PCI device
        // For example, you can call a function in pci_driver64.c to handle the data
        // pci_driver64_write(entry->data, entry->len);

        // Simulate response from PCI device
        printk(KERN_INFO "VPCI Driver: Processing data: %s\n", entry->data);

        list_del(&entry->list);
        kfree(entry->data);
        kfree(entry);
    }
    mutex_unlock(&vpci_mutex);
}
*/

//create an interupt based mmio check of the read FIFOs that will be used to continusouly readout data from the chips


//create a multi-thread workqueue with a function of checking the read buffer to see if the problems it is assinged are completed


static struct file_operations fops = {
    .open = NULL,
    .release = NULL,
    .read = vpci_read,
    .write = vpci_write,
};

static int __init vpci_driver_init(void)
{
    struct pci_dev *pdev = NULL;
    struct file *file;
    int ret;
    char device_file[256]; //used to copy the file path of registered PCIe deices

    printk(KERN_INFO "VPCI Driver: Checking for PCI devices\n");

    // Iterate over PCI devices to check if any device is managed by pci_driver64
    for_each_pci_dev(pdev) {
        if (pdev->driver && strcmp(pdev->driver->name, "cobi_chip_driver64") == 0) {
            printk(KERN_INFO "VPCI Driver: Found PCI device managed by pci_driver64\n");
            break;
        }
    }

    //if there are no COBI chips plugged in, return an error
    if (!pdev) {
        printk(KERN_ERR "VPCI Driver: No PCI devices managed by pci_driver64 found\n");
        return -ENODEV;
    }

    

    //open all PCI devices with COBI chips to prevent others from using

    for (int i = 0; i < MAX_DEVICES; i++) {
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        file = filp_open(device_file, O_RDONLY, 0);
        if (!IS_ERR(file)) {
            fd_val[i] = file;
            printk(KERN_INFO "VPCI Driver: Opened device %s\n", device_file);
        } else {
            printk(KERN_INFO "VPCI Driver: No device found at %s\n", device_file);
            fd_val[i] = NULL;
        }
    }

    //create a single-threaded workqueue for the virtual pcie device
    vpci_wq = create_workqueue("vpci_wq");
    if (!vpci_wq) {
        pr_err("Failed to create workqueue\n");
        return -ENOMEM;
    }

    // Register character device of the VPCI driver
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "VPCI Driver: Failed to register a major number\n");
        return major;
    }
    printk(KERN_INFO "VPCI Driver: Registered with major number %d\n", major);

    // Register the device class of the VPCI driver
    vpci_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(vpci_class)) {
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "VPCI Driver: Failed to register device class\n");
        return PTR_ERR(vpci_class);
    }
    printk(KERN_INFO "VPCI Driver: Device class registered\n");


    //intialize the character device structure
    cdev_init(&vpci_cdev, &fops);
    vpci_cdev.owner = THIS_MODULE;
    ret = cdev_add(&vpci_cdev, MKDEV(major, 0), 1);
    if (ret) {
        device_destroy(vpci_class, MKDEV(major, 0));
        class_destroy(vpci_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "VPCI Driver: Failed to add cdev\n");
        return ret;
    }

    // Register the device driver of the VPCI driver
    vpci_device = device_create(vpci_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(vpci_device)) {
        class_destroy(vpci_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "VPCI Driver: Failed to create the device\n");
        return PTR_ERR(vpci_device);
    }
    printk(KERN_INFO "VPCI Driver: Device class created\n");

    printk(KERN_INFO "VPCI Driver: Module initialized\n");
    return 0;
}

static void __exit vpci_driver_exit(void)
{
    //close the opened PCI cobi devicess
    for( int i = 0; i < MAX_DEVICES; i++) {
        filp_close(fd_val[i], NULL);
    }

    flush_workqueue(vpci_wq);
    destroy_workqueue(vpci_wq);
    cdev_del(&vpci_cdev);
    device_destroy(vpci_class, MKDEV(major, 0));
    class_unregister(vpci_class);
    class_destroy(vpci_class);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "VPCI Driver: Module removed\n");

}

module_init(vpci_driver_init);
module_exit(vpci_driver_exit);