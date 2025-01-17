#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/export.h> //used to export file operations
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/kfifo.h>

MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("Virtual PCIe Queue and Scheduler Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1"); 
// Define variables
#define DRIVER_NAME "cobi_chip_testdriver640"
static int major;
static struct class *tpci_class;
static struct device *tpci_device = NULL;
static int device_number;
static bool busy = false;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static DEFINE_MUTEX(open_lock);
static DEFINE_MUTEX(problem_lock);


// sturutre to hold the data important to the problem. Will generate random data for the solution etc.
struct fake_data {
    int problem_id;
    int time_value;
    bool done;
};

//problem data
typedef struct {
    off_t offset;
    uint64_t value;  // Change to 64-bit
} write_data_t;

//store the users problem and fake solution timing data
static struct fake_data *data_array[16];

//store the user solution data in a fifo
static DEFINE_MUTEX(fifo_lock);
static DECLARE_KFIFO(fake_data_fifo, struct fake_data, 32);

static void init_fifo(void)
{
    INIT_KFIFO(fake_data_fifo);
    mutex_lock(&fifo_lock);
    kfifo_reset(&fake_data_fifo);
    mutex_unlock(&fifo_lock);
}
//simulate the read flag of the real PCIe device
static int read_flag = 0;


//
static struct timer_list timer;
static int counter = 0;

static void timer_callback(struct timer_list *t)
{
    int i;
    counter++;
    
    for (i = 0; i < 16; i++) {
        if (data_array[i] != NULL && !data_array[i]->done) {
            if (counter == data_array[i]->time_value) {
                data_array[i] = NULL;
                read_flag++;
            }
        }
    }
    
    mod_timer(&timer, jiffies + usecs_to_jiffies(1));
}

static void init_timer_setup(void)
{
    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + usecs_to_jiffies(1));
}


static int pci_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    mutex_lock(&open_lock);
    if (busy) {
        ret = -EBUSY;
    } else {
        // claim device
        busy = true;
        printk( KERN_WARNING "PCI:  opened device");

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
    int i;
    //create a new struct to store the passed in problem
    struct fake_data *new_data = kmalloc(sizeof(struct fake_data), GFP_KERNEL);
    write_data_t write_data;

    if (!new_data)
        return -ENOMEM;

    //intialize the data
    if (copy_from_user(&write_data, buf, sizeof(write_data)) != 0)
        return -EFAULT;
    
//TODO set the problem ID value to its value from the write_data struct

    //intialize the solution timing data
    new_data->time_value = counter + (get_random_int() % 51 + 50); // set the time value to a random number between 50 and 100 in the future from counter to simulate a COBI solve time of between 50-100us
    new_data->done = false;

    //store the problem in the data array
    for (i = 0; i < 16; i++) {
        if (data_array[i] == NULL) {
            mutex_lock(&problem_lock);
            data_array[i] = new_data;
            mutex_unlock(&problem_lock);
            break;
        }
    }

    if (i == 16) {
        kfree(new_data);
        return -ENOSPC;
    }

    return 0;
}

//read function of the virtual PCIe device
static ssize_t pci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int ret;
    int i;  

    if (*offset == 0) {
        int result = (read_flag > 0) ? 1 : 0;
        ret = copy_to_user(buf, &result, sizeof(result));
        if (ret != 0) {
            return -EFAULT;
        }
        return sizeof(result);
    }

    if (*offset == 1) {
        
        for (i = 0; i < 16; i++) {
            if (data_array[i] != NULL && data_array[i]->done) {
                uint64_t problem_id = data_array[i]->problem_id;
                ret = copy_to_user(buf, &problem_id, sizeof(problem_id));
                if (ret != 0) {
                    return -EFAULT;
                }
                mutex_lock(&problem_lock);
                kfree(data_array[i]);
                data_array[i] = NULL;
                read_flag--;
                mutex_unlock(&problem_lock);
                return sizeof(problem_id);
            }
        }
        return -EAGAIN;
    }

    /*
    printk(KERN_INFO "PCI: Starting read operation\n");
    
    ret = copy_to_user(buf, &read_data, sizeof(read_data));
    if (ret != 0) {
        printk(KERN_ERR "PCI: copy_to_user failed with %d\n", ret);
        return -EFAULT;
    }

    printk(KERN_INFO "PCI: Read successful, value=%d\n", read_data);
    return sizeof(read_data);
    */
    return 0;

}

//file operations for the virtual PCIe device
struct file_operations tpci_fops = {
    .owner = THIS_MODULE,
    .open = pci_open,
    .release = pci_release,
    .read = pci_read, 
    .write = pci_write
};
EXPORT_SYMBOL(tpci_fops);

static int __init vpci_init(void) {
    //Virtual PCIe device registration and initialization

    // Register the character device with the Linux kernel
    int result = register_chrdev(0, DRIVER_NAME, &tpci_fops);
    //check if the character device was successfully registered
    if( result < 0 )
    {
            printk( KERN_WARNING "VPCI:  can't register character device with error code = %i\n", result );
            return result;
    }
    //store the major number
    major = result;

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
    
    //set up the timer to check for done problems
    init_timer_setup();

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

    //delete the timer simulating problem completion
    del_timer_sync(&timer);     
}

module_init(vpci_init);
module_exit(vpci_exit);