#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/export.h> //used to import EXPORT_SYMBOL() functions
#include <linux/workqueue.h>

MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("Virtual PCIe Queue and Scheduler Driver");
MODULE_LICENSE("GPL");

extern struct file_operations tpci_fops;


// Define variables
#define DRIVER_NAME "cobi_chip_vdriver64"
#define DEVICE_FILE_TEMPLATE "/dev/cobi_chip_testdriver64%d"
#define MAX_DEVICES 1

static int major;
static struct class *vpci_class;
static struct device *vpci_device = NULL;
static int device_number;

//store device data 
struct device_data {
    struct file *fd_val;
    int value;
    bool id_used[17];  // Array to track which FIFO slots need to be read
};

//store the devices in an array
struct device_data *device_array[MAX_DEVICES];

//problem data
struct write_data_t {
    off_t offset;
    uint64_t value;  // Change to 64-bit
};
typedef struct write_data_t write_data_t;

//store the users problems and solutions 
struct user_data {
    int problem_count;           
    int solved;
    int first_problem;
    int *card_id;
    int *problem_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
    struct write_data_t **write_data;
};
typedef struct user_data user_data;

//make a queue to store and work on user_data structures
static struct workqueue_struct *user_data_wq;
struct user_data_work {
    struct work_struct work;
    struct user_data *data;
};

//helper functions
//reverse the 8-bit data
uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

// Add the worker function
static void process_user_data(struct work_struct *work)
{
    struct user_data_work *ud_work = container_of(work, struct user_data_work, work);
    struct user_data *data = ud_work->data;
    int i;
    int ret;
    int k;
    int device_count = 0;
    int problems_submitted = 0;
    uint32_t read_data;
    uint64_t best_spins;
    uint64_t best_ham;
    int problem_id = 0;
    int card_id = 0;
    int read_flag = 0 ;
    loff_t offset = 0;
    bool found = false;
    int found_counter = 0;
    bool first_problem_tracker;
    //if the data is NULL then return
    if (!data)
        goto out;

    //Check if there are any free chips avaiable

    // Count non-NULL entries
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (device_array[i] != NULL) {
            device_count++;
        }
    }

    //keep trying to read and write to the chips until all the problems are solved
    while(data->solved != data->problem_count){

        //WRITE
        //go through the devices and check if they are avaiable to be written to
        for (i = 0; i < device_count; i++) {

            //check if device "i" is avaiable to be written to
            if (device_array[i] && device_array[i]->value < 17) {
                
                //submit as many problems to the device as avaiable slots
                for (k = device_array[i]->value; k < 17; k++) {
                    
                    //find an open problem id to use
                    if(!(device_array[i]->id_used[k])){

                        //check if there exists data to write and if all problems have been submitted
                        if (data->write_data[problems_submitted] && problems_submitted != data->problem_count) {
                            // Send the problem to the PCIe device
                            ret = tpci_fops.write(device_array[i]->fd_val, (char *)&data->write_data[problems_submitted], sizeof(write_data_t), &offset);
                            if (ret < 0) {
                                printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                                return;
                            }
                            
                            //increment the problems_submitted to sumbit the next problem in the problem set
                            problems_submitted++;
                            
                            //set the ids that will identify which problem is being solved on what chip
                            data->card_id[problems_submitted] = i;
                            data->problem_id[problems_submitted] = k;
                            printk(KERN_INFO "Processing problem %d\n", i);
                        }
                        
                        //indicate that the problem id is being used
                        device_array[i]->id_used[k] = true;
                    }

                }

                //set the value of the device to the number of problems that have been submitted
                if (k == 17) {
                    device_array[i]->value = 16;
                    break;
                }

            }
        }
        
        //READ 
        //check if any of the devices need to be read from
        for (i = 0; i < device_count; i++) {
            read_flag = 0;
            //check if device "i" is avaiable to be read from by checking the read flag
            offset = 10 * sizeof(uint32_t); //fifo flag offset
            ret = tpci_fops.read(device_array[i]->fd_val, (char *)&read_data, sizeof(read_data), &offset);
            if (ret < 0) {
                printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                goto out;
            }
            //set the read flag from the returned read data 
            read_flag = read_data;

            //read the data from the device
            if(read_flag == 1){
                //read the data from the device. 128-bits need to be read from the device, so 4 reads need to be sent 
                offset = 1 * sizeof(uint32_t); //fifo flag offset
                for( k = 0 ; k <3; k++){

                    ret = tpci_fops.read(device_array[i]->fd_val, (char *)&read_data, sizeof(read_data), &offset);
                    if (ret < 0) {
                        printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                        goto out;
                    }

                    if(k == 0){
                        problem_id = inverse_data(read_data);
                        card_id = inverse_data(read_data >> 4);
                        best_spins = read_data;
                    }
                    if(k == 1){
                        best_spins = ((uint64_t)read_data << 32) | (best_spins & 0xFFFFFFFF);
                        best_ham = read_data;
                    }
                    if(k == 2){
                        best_ham = ((uint64_t)read_data << 32) | (best_ham & 0xFFFFFFFF);
                    }
                }
            }

            //Store the data in the user_data struct
            found_counter = data->first_problem; // used to search through the problems that have been submitted
            first_problem_tracker = false; //track which problems have been solved so they do not need to be searched through
            found = false; //track if the problem has been found to break the while loop

            while(found){
                if(data->card_id[found_counter] == card_id && data->problem_id[found_counter] == problem_id){
                    data->best_spins[found_counter] = best_spins;
                    data->best_ham[found_counter] = best_ham;
                    data->solved++;
                    found = true;
                }
                found_counter++;
                
                if(data->best_spins[found_counter] == NULL && !first_problem_tracker){
                    data->first_problem = found_counter;
                    first_problem_tracker = true;
                }
                
            }
            

        }
    }
    

out:
    kfree(ud_work);
}

// Add function to queue work
int queue_user_data_work(struct user_data *data)
{
    struct user_data_work *work;

    work = kmalloc(sizeof(*work), GFP_KERNEL);
    if (!work)
        return -ENOMEM;

    INIT_WORK(&work->work, process_user_data);
    work->data = data;

    return queue_work(user_data_wq, &work->work);
}

//intailize the user data struct
struct user_data* init_user_data(struct file *file, struct user_data *data, const char __user *buf, size_t size) {
    int i;
    
    // Allocate memory for the user_data structure based on the amount of problem (size) sent from the userspace
    data->problem_id = kmalloc(size * sizeof(int), GFP_KERNEL);
    data->best_spins = kmalloc(size * sizeof(char __user *), GFP_KERNEL);
    data->best_ham = kmalloc(size * sizeof(char __user *), GFP_KERNEL);
    data->write_data = kmalloc(size * sizeof(write_data_t *), GFP_KERNEL);

    //divide the problems from the user space into the user_data structs 
    for (i = 0; i < size; i++) {
        // Allocate memory for each write_data_t structure
        data->write_data[i] = kmalloc(sizeof(write_data_t), GFP_KERNEL);

        //check if allocation failed
        if (!data->write_data[i]) {
            // Handle allocation failure
            return NULL;
        }

        //copy the data from the user space to the kernel space
        if (copy_from_user(&(data->write_data[i]), data + (i * sizeof(write_data_t)), sizeof(write_data_t))) {
            return NULL;
        }

//TODO: set the problem ID value of the variable and also in the problem passed to the PCIe device
        data->problem_id[i] = 0;

    }

    //intialize the probem count and solved count that will be used to know when to return the solution to the user
    data->first_problem = 0;
    data->problem_count = size;
    data->solved = 0;

    return data;
};

// Function to free user_data
void free_user_data(struct user_data *data) {
    kfree(data);
}

//read function of the virtual PCIe device
static ssize_t vpci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int i = 0;
    int ret = 0;

   // Check if we have valid file descriptors
    if (!device_array[i]->value || !(device_array[i]->fd_val)) {
        printk(KERN_ERR "No valid device file descriptor\n");
        return -ENODEV;
    }

    // Check buffer size
    if (len < sizeof(unsigned int)) {
        printk(KERN_ERR "Buffer too small\n");
        return -EINVAL;
    }

    
    ret = tpci_fops.read(device_array[i]->fd_val, buf, len, offset);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
        return ret;
    }

    return 0;
}

static ssize_t vpci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
    int ret = 0;
    size_t data_count;
    struct user_data *data = kmalloc(sizeof(*data), GFP_KERNEL);


    // Determine number of write_data_t structures
    data_count = len / sizeof(write_data_t);  

    // Further processing of the user data into a structure user_data that seperates out the problems and ids them
    if(len != sizeof(buf)){
        //check if data was created
        if (!data)
            return -ENOMEM;

        //intialize the data struct with the user data
        data = init_user_data(file, data, buf, data_count);
        if (!data)
            return -EFAULT;
    }

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
    char device_file[256];

    printk(KERN_INFO "VPCI Driver: Checking for PCI devices\n");
    
    //open all PCI devices with COBI chips to prevent others from using
    for (int i = 0; i < MAX_DEVICES; i++) {
        //create a device file string
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        //open the device file
        file = filp_open(device_file, O_RDONLY, 0);
        //check if opening the file failed
        if (!IS_ERR(file)) {
            //create a struct to store the device data
            struct device_data *dev_data = kmalloc(sizeof(struct device_data), GFP_KERNEL);
            if (!dev_data) {
                return -ENOMEM;
            }

            //intialize the dev_data struct
            dev_data->value = 0;
            dev_data->fd_val = file;
            dev_data->id_used = {[0 ... 17] = false};

            //store the device data in the device array
            device_array[i] = dev_data;
            
            printk(KERN_INFO "VPCI Driver: Opened device %s\n", device_file);

        } else {
            printk(KERN_INFO "VPCI Driver: No device found at %s\n", device_file);
        }
    }
    
    //create a queue to process the users data
    user_data_wq = create_singlethread_workqueue("user_data_worker");
    if (!user_data_wq) {
        // Handle error
        return -ENOMEM;
    }

    //Virtual PCIe device registration and initialization

    // Register the character device of the virtual driver with the Linux kernel
    result = register_chrdev(0, DRIVER_NAME, &vpci_fops);
    //check if the character device was successfully registered
    if( result < 0 )
    {
        printk( KERN_WARNING "VPCI:  can't register character device with error code = %i\n", result );
        return result;
    }
    //store the major number
    major = result;


    //create a class for the vitual driver/device
    vpci_class = class_create(THIS_MODULE, DRIVER_NAME);
    if( IS_ERR( vpci_class ) )
    {
        printk( KERN_WARNING "VPCI:  can\'t create device class with errorcode = %ld\n", PTR_ERR( vpci_class ) );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( vpci_class );
    }
    printk( KERN_NOTICE "VPCI: Device class created\n" );

    //make a device number using the MAJOR macro for the virtual driver
    device_number = MKDEV( major, 0 );

    //create a device for the virtual drivr class
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

    // close the devices
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (device_array[i]  != NULL) {
            filp_close(device_array[i]->fd_val, NULL);
            kfree(device_array[i]);
            device_array[i] = NULL;
        }
    }

    // Destroy the workqueue
    if (user_data_wq) {
        flush_workqueue(user_data_wq);
        destroy_workqueue(user_data_wq);
    }
    printk(KERN_INFO "VPCI: Module unloaded\n");

}

module_init(vpci_init);
module_exit(vpci_exit);