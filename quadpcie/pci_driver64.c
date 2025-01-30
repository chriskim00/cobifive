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
#define SOLVED_ARRAY_SIZE 16
#define RAW_BYTE_CNT 166

static int major;
static struct class *tpci_class;
static struct device *tpci_device = NULL;
static int device_number;
static bool busy;
int read_counter;

//define mutexes
static DEFINE_MUTEX(open_lock);
static DEFINE_MUTEX(problem_lock);


// sturutre to hold the data important to the problem. Will generate random data for the solution etc.
struct fake_data {
    uint32_t problem_id;
    int time_value;
    bool done;
};

//store the users problem and fake solution timing data
static struct fake_data *data_array[SOLVED_ARRAY_SIZE];

//problem data
typedef struct {
    off_t offset;
    uint64_t value;  // Change to 64-bit
} write_data_t;

//simulate the read flag of the real PCIe device
static int read_flag = 0;

//helper functions
uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

//simulate the timing of the real cobi chips
static struct timer_list timer;
static unsigned int counter = 0;

static void timer_callback(struct timer_list *t){
    int i;
    counter++;
    //printk(KERN_WARNING "PCI: Time Counter = %u\n", counter);
    for (i = 0; i < SOLVED_ARRAY_SIZE; i++) {
        //printk(KERN_DEBUG "PCI: data_array[%d]: %p, done: %s\n", i, data_array[i], (data_array[i] != NULL) ? (data_array[i]->done ? "true" : "false") : "NULL");
        if (data_array[i] != NULL && !(data_array[i]->done)) {
            if (counter == data_array[i]->time_value) {
                mutex_lock(&problem_lock);
                //printk(KERN_WARNING "PCI: Problem %u Finished\n", data_array[i]->problem_id);
                data_array[i]->done = true;
                mutex_unlock(&problem_lock);
                read_flag++;
            }
        }
    }


    if(counter == 4294967294){
        printk(KERN_WARNING "PCI: Time Counter Reset\n");
        counter = 0;
    }

    mod_timer(&timer, jiffies + usecs_to_jiffies(1));
}

static void init_timer_setup(void)
{
    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + usecs_to_jiffies(1));
}

//write function of the virtual PCIe device
static ssize_t pci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
    int ret = -EINVAL;
    struct fake_data *new_data = (struct fake_data*)kmalloc(sizeof(struct fake_data), GFP_KERNEL);
    uint64_t *raw_write_data = (uint64_t*)kmalloc(RAW_BYTE_CNT * sizeof(uint64_t), GFP_KERNEL);
    int i;


    // Allocate memory
    if (!raw_write_data || !new_data) {
        ret = -ENOMEM;
        goto cleanup;
    }

    //printk(KERN_WARNING "PCI: Write data len = %zu\n", len);

    // Copy from user space
    memcpy(raw_write_data, buf, len);

    if(*offset == 9 * sizeof(uint64_t)){
        for (i = 0; i < SOLVED_ARRAY_SIZE; i++) {
            //printk(KERN_WARNING "PCI: Searching for a open problem index\n");
            if (data_array[i] == NULL) {


                //store the problem data in the data_array
                //intialize the fake data
                new_data->problem_id = (0x00000000000F0000 & raw_write_data[164]) >> 16;
                //printk(KERN_WARNING "PCI: Raw Write Data[164] = %llx\n", raw_write_data[164]);
                //printk(KERN_WARNING "PCI: Problem ID = %u\n", new_data->problem_id);
                new_data->time_value = counter + (get_random_int() % 3 + 1); // set the time value to a random number between 50 and 100 in the future from counter to simulate a COBI solve time of between 50-100us
                new_data->done = false;
                mutex_lock(&problem_lock);
                data_array[i] = new_data;
                mutex_unlock(&problem_lock);


                //write was successful, clean up the structures in memory
                kfree(raw_write_data);
                return 0;
            }
        }
    }
    
    printk(KERN_WARNING "PCI: Failed to find an open problem index\n");
    //write has failed, free up the structures in memory
    kfree(new_data);
    kfree(raw_write_data);
    printk(KERN_WARNING "PCI: Wiped data structures after failure\n");
    return -ENOSPC;
    ret = len;  // Success case

    out:
        kfree(new_data);
        kfree(raw_write_data);
        printk(KERN_WARNING "PCI: Wiped data structures after failure\n");
        return -EFAULT;
    cleanup:
        if (ret < 0) {
            kfree(raw_write_data);
            kfree(new_data);
        }
        return ret;
}

//read function of the virtual PCIe device
static ssize_t pci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int i;
    uint32_t problem_id;
    //printk(KERN_WARNING "PCI: Entered read\n");
    if (*offset == 10 * sizeof(uint32_t)) {
        uint32_t result = (read_flag > 0) ? 1 : 0;
        memcpy(buf, &result, sizeof(result));

        return sizeof(result);
    }

    
    if (*offset == 4 * sizeof(uint32_t)) {
        if(read_counter == 0 && read_flag > 0){
            for (i = 0; i < SOLVED_ARRAY_SIZE; i++) {
                if (data_array[i] != NULL && data_array[i]->done) {
                    //printk(KERN_WARNING "PCI: Entered Offset 4 or Send data loop\n");

                    //copy the problem id to the user
                    problem_id = inverse_data(data_array[i]->problem_id);
                    memcpy(buf, &problem_id, sizeof(problem_id));

                    //free the data_array
                    mutex_lock(&problem_lock);
                    kfree(data_array[i]);
                    data_array[i] = NULL;
                    mutex_unlock(&problem_lock);
                    
                    //increment read counter to simulate the 4 reads required by the PCIe device
                    read_counter++;

                    //printk(KERN_WARNING "PCI: Sent problem ID, read step 1 \n");
                    return 0;
                }
            }

            return -EAGAIN; //try again message
        }

        if(read_counter == 1){       
            //send over a random 32 bit value
            uint32_t random_value;
            get_random_bytes(&random_value, sizeof(random_value));
            memcpy(buf, &random_value, sizeof(random_value));
           
            read_counter++;
            //printk(KERN_WARNING "Second Read of single solution, read step 2  \n");
            return 0;
        }

        if(read_counter > 1 ){       
            //send over a random 32 bit value
            uint32_t random_value;
            get_random_bytes(&random_value, sizeof(random_value));
            memcpy(buf, &random_value, sizeof(random_value));
            
            read_counter = 0;
            read_flag--;
            //printk(KERN_WARNING "Third Read of single solution, read step 3 \n");
            return 0;
        }

    }

    //failed to read from the device
    return -EFAULT;

}

//take control of the pci device
static int pci_open(struct inode *inode, struct file *file){
    int ret = 0;
    mutex_lock(&open_lock);
    if (busy) {
        printk( KERN_WARNING "PCI:  couldn't open device");
        ret = -EBUSY;
    } else {
        // claim device
        busy = true;
        //printk( KERN_WARNING "PCI:  opened device");

    }
    mutex_unlock(&open_lock);

    return ret;
}

//release control of the pci device
static int pci_release(struct inode *inode, struct file *file){
    //printk( KERN_WARNING "PCI:  closed device");
    busy = false;
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
EXPORT_SYMBOL(tpci_fops); //export the functions to be used by the virtual driver

static int __init pci_init(void) {
    
    // Register the character device with the Linux kernel
    int result = register_chrdev(0, DRIVER_NAME, &tpci_fops);
    read_counter = 0;
    busy = false;
    //check if the character device was successfully registered
    if( result < 0 )
    {
            printk( KERN_WARNING "PCI:  can't register character device with error code = %i\n", result );
            return result;
    }
    //store the major number
    major = result;

    //create a class for the device
    tpci_class = class_create(THIS_MODULE, DRIVER_NAME);
    if( IS_ERR( tpci_class ) )
    {
        printk( KERN_WARNING "PCI:  can\'t create device class with errorcode = %ld\n", PTR_ERR( tpci_class ) );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( tpci_class );
    }

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

    //set up the timer to check for done problems
    init_timer_setup();
    printk( KERN_WARNING "PCI:  Module loaded");
    return 0;
}

static void __exit pci_exit(void) {
    // Unregister the character device with the Linux kernel

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
    printk(KERN_WARNING "PCI: Module unloaded\n");

    //delete the timer simulating problem completion
    del_timer_sync(&timer);     
}

module_init(pci_init);
module_exit(pci_exit);