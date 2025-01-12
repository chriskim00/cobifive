#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>


MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("Virtual PCIe Queue and Scheduler Driver");
MODULE_LICENSE("GPL");

// Define variables
#define DRIVER_NAME "cobi_chip_testdriver64"
static int major;
static struct class *tpci_class;
static struct device *tpci_device = NULL;
static int device_number;
static bool busy = false;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static DEFINE_MUTEX(open_lock);

static int pci_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    mutex_lock(&open_lock);
    if (busy) {
        ret = -EBUSY;
    } else {
        // claim device
        busy = true;
    }
    mutex_unlock(&open_lock);

    return ret;
}

static int pci_release(struct inode *inode, struct file *file)
{
    busy = false;
    return 0;
}


//read function of the virtual PCIe device
static ssize_t pci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){

    if( copy_to_user(buf, &len, len) != 0 )
        return -EFAULT;

    return 0;
}

//read function of the virtual PCIe device
static ssize_t pci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){

    if( copy_to_user(buf, &len, len) != 0 )
        return -EFAULT;

    return 0;
}

//file operations for the virtual PCIe device
static struct file_operations vpci_fops = {
    .owner = THIS_MODULE,
    .open = pci_open,
    .release = pci_release,
    .read = pci_read, 
    .write = pci_write
};

static int __init vpci_init(void) {

    //Virtual PCIe device registration and initialization

    // Register the character device with the Linux kernel
    int result = register_chrdev(0, DRIVER_NAME, &vpci_fops);
    //check if the character device was successfully registered
    if( result < 0 )
    {
            printk( KERN_WARNING "VPCI:  can't register character device with error code = %i\n", result );
            return result;
    }
    //store the major number
    major = result;
    if(major != 0)
    {
        unregister_chrdev(major, DRIVER_NAME);
    }

    //create a class for the device
    tpci_class = class_create(THIS_MODULE, DRIVER_NAME);
    if( IS_ERR( tpci_class ) )
    {
        printk( KERN_WARNING "VPCI:  can\'t create device class with errorcode = %ld\n", PTR_ERR( tpci_class ) );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( tpci_class );
    }
    printk( KERN_NOTICE "VPCI: Device class created\n" );

    //make a device number macro
    device_number = MKDEV( major, 0 );

    //create a device for the class
    tpci_device = device_create( tpci_class, NULL, device_number, NULL, DRIVER_NAME );
    if ( IS_ERR(tpci_device ) )
    {
        printk( KERN_WARNING "can\'t create device with errorcode = %ld\n", PTR_ERR( tpci_device ) );
        class_destroy( tpci_class );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( tpci_device );
    }
    printk(KERN_INFO "VPCI: Module loaded\n");
    return 0;
}

static void __exit vpci_exit(void) {
    // Unregister the character device with the Linux kernel
    printk( KERN_NOTICE "Unregister_device() is called\n" );

    if( !IS_ERR( tpci_class ) )
    {
        device_destroy( tpci_class, device_number );
    }
    if( !IS_ERR( tpci_class ) && !IS_ERR( tpci_device ) )
    {
        class_destroy( tpci_class );
    }
    if( major != 0 )
    {
        unregister_chrdev( major, DRIVER_NAME );
    }
    printk(KERN_INFO "VPCI: Module unloaded\n");
}

module_init(vpci_init);
module_exit(vpci_exit);