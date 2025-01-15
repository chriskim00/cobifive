#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/export.h> //used to import EXPORT_SYMBOL() functions

MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("Virtual PCIe Queue and Scheduler Driver");
MODULE_LICENSE("GPL");

extern struct file_operations tpci_fops;


// Define variables
#define DRIVER_NAME "cobi_chip_vdriver64"
#define DEVICE_FILE_TEMPLATE "/dev/cobi_pcie_card%d"
#define MAX_DEVICES 1
#define PROBLEM_BYTE_SIZE 1

static int major;
static struct class *vpci_class;
static struct device *vpci_device = NULL;
static int device_number;
struct file **fd_val;



//read function of the virtual PCIe device
static ssize_t vpci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int ret = 0;

   // Check if we have valid file descriptors
    if (!fd_val || !fd_val[0]) {
        printk(KERN_ERR "No valid device file descriptor\n");
        return -ENODEV;
    }

    // Check buffer size
    if (len < sizeof(unsigned int)) {
        printk(KERN_ERR "Buffer too small\n");
        return -EINVAL;
    }

    
    ret = tpci_fops.read(fd_val[0], buf, len, offset);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
        return ret;
    }

    return 0;
}

static ssize_t vpci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){

    //copy to the user from the kernel sapces
    if (copy_to_user((void __user *)buf, &len, len) != 0)
        return -EFAULT;

    return 0;
}

//file operations for the virtual PCIe device
static struct file_operations vpci_fops = {
    .owner = THIS_MODULE,
    .read = vpci_read, 
    .write = vpci_write
};

static int __init vpci_init(void) {

    //variables
    struct file *file;
    int result;

    fd_val = kmalloc(MAX_DEVICES * sizeof(struct file *), GFP_KERNEL);
    if (!fd_val) {
        return -ENOMEM;
    }

    printk(KERN_INFO "VPCI Driver: Checking for PCI devices\n");
    
    //open all PCI devices with COBI chips to prevent others from using

    /*
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
    */

    
    // Allocate memory for the file descriptor array if not already allocated
    
    // Open file and store in array
    file = filp_open("/dev/cobi_chip_testdriver64", O_RDONLY, 0);
    fd_val[0] = file;
    /*
    if (!IS_ERR(file)) {
        fd_val[0] = file;
        printk(KERN_INFO "VPCI Driver: Opened device %s\n", device_file);
    } else {
        printk(KERN_INFO "VPCI Driver: No device found at %s\n", device_file);
        fd_val[0] = NULL;
    }
    */

    //Virtual PCIe device registration and initialization

    // Register the character device with the Linux kernel
    result = register_chrdev(0, DRIVER_NAME, &vpci_fops);
    //check if the character device was successfully registered
    if( result < 0 )
    {
        printk( KERN_WARNING "VPCI:  can't register character device with error code = %i\n", result );
        return result;
    }
    //store the major number
    major = result;


    //create a class for the device
    vpci_class = class_create(THIS_MODULE, DRIVER_NAME);
    if( IS_ERR( vpci_class ) )
    {
        printk( KERN_WARNING "VPCI:  can\'t create device class with errorcode = %ld\n", PTR_ERR( vpci_class ) );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( vpci_class );
    }
    printk( KERN_NOTICE "VPCI: Device class created\n" );

    //make a device number macro
    device_number = MKDEV( major, 0 );

    //create a device for the class
    vpci_device = device_create( vpci_class, NULL, device_number, NULL, DRIVER_NAME );
    if ( IS_ERR(vpci_device ) )
    {
        printk( KERN_WARNING "can\'t create device with errorcode = %ld\n", PTR_ERR( vpci_device ) );
        class_destroy( vpci_class );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( vpci_device );
    }
    printk(KERN_INFO "VPCI: Module loaded\n");
    return 0;
}

static void __exit vpci_exit(void) {
    // Unregister the character device with the Linux kernel
    printk( KERN_NOTICE "Unregister_device() is called\n" );

    if( !IS_ERR( vpci_class ) )
    {
        device_destroy( vpci_class, device_number );
    }
    if( !IS_ERR( vpci_class ) && !IS_ERR( vpci_device ) )
    {
        class_destroy( vpci_class );
    }
    if( major != 0 )
    {
        unregister_chrdev( major, DRIVER_NAME );
    }
    printk(KERN_INFO "VPCI: Module unloaded\n");

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (fd_val[i] != NULL) {
            filp_close(fd_val[i], NULL);
        }
    }
}

module_init(vpci_init);
module_exit(vpci_exit);