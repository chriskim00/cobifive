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

//// Function to log time to a file that is stored in /var/log/pci_driver.log
static void log_action(const char *message, size_t size) {
    struct file *file;
    loff_t pos = 0;
    char *buf;
    size_t len;
    const char *filename;

    buf = kmalloc(size, GFP_KERNEL);
    if (!buf)
        return;

    snprintf(buf, size, "%s", message);
    len = strlen(buf);

    filename = "/tmp/pci_driver.log";
    file = filp_open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(file)) {
        kfree(buf);
        printk(KERN_ERR "Failed to open log file2: %ld\n", PTR_ERR(file));

        return;
    }

    kernel_write(file, buf, len, &pos);

    filp_close(file, NULL);
    kfree(buf);
}

// Define log
bool enable_logging = true;

// Define variables
#define DRIVER_NAME "cobi_chip_vdriver64"
#define DEVICE_FILE_TEMPLATE "/dev/cobi_chip_testdriver64%d"
#define MAX_DEVICES 1
#define RAW_BYTE_CNT 166
#define MAX_USER_DATA 15

static int major;
static struct class *vpci_class;
static struct device *vpci_device = NULL;
static int device_number;
static int user_id_counter = 0;
static bool busy = false;

//Define mutexes
static DEFINE_MUTEX(open_lock);


//store device data 
struct device_data {
    struct file *fd_val;
    int *value;
    bool *id_used[16];  // Array to track which FIFO slots need to be read
};

//store the devices in an array
struct device_data *device_array[MAX_DEVICES];

//store the users problems and solutions 
struct user_data {
    int *user_id;
    int *problem_count;           
    int *solved;
    int *first_problem;
    int *card_id;
    uint32_t *problem_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
    uint64_t **write_data;
};
typedef struct user_data user_data;

//read data structure
struct user_data_read {
    int *user_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
};
typedef struct user_data_read user_data_read;

//array to store the user_data_read
static struct user_data_read *processed_data[MAX_USER_DATA];

// Store the processed data in an array
static void store_user_data_read(struct user_data *data) {
    struct user_data_read *new_data = kmalloc(sizeof(struct user_data_read), GFP_KERNEL);

    if (!new_data)
        return;
    
    new_data->user_id = data->user_id;
    new_data->best_spins = data->best_spins;
    new_data->best_ham = data->best_ham;

    while(1){
        for (int i = 0; i < MAX_USER_DATA; i++) {
            if (processed_data[i] == NULL) {
                processed_data[i] = new_data;
                break;
            }
        }
    }

}

//make a queue to store write data and work on user_data structures
static struct workqueue_struct *user_data_wq;
struct user_data_work {
    struct work_struct work;
    struct user_data *data;
};

//helper functions
//reverse the 8-bit data -> Takes in: 0b00001011 and outputs: 0b00001101
uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

//change the problem ID 
void set_pid_in_rawData(uint64_t *rawData, uint16_t new_pid){
    //change the MSB 16 bits of the rawData to the new_pid
    // Clear the MSB 16 bits
    *rawData &= 0x0000FFFFFFFFFFFF;

    // Set the new_pid in the MSB 16 bits
    *rawData |= ((uint64_t)new_pid << 48);
}

// Add the worker function
static void process_user_data(struct work_struct *work){
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
    uint16_t pid = 0xFFFF;

    //if the data is NULL then return
    if (!data)
        goto out;

    //Check if there are any free chips avaiable

    // Count non-NULL entries in the device array
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
            if (device_array[i] && *device_array[i]->value < 16) {
                
                //submit as many problems to the device as avaiable slots
                for (k = 0; k < 17; k++) {
                    
                    //find an open problem id to use
                    if(*(device_array[i]->id_used[k])){

                        //check if there exists data to write and if all problems have been submitted
                        if (data->write_data[problems_submitted] && problems_submitted != *(data->problem_count)) {


                            set_pid_in_rawData(data->write_data[problems_submitted], pid);
                            // Send the problem to the PCIe device
                            ret = tpci_fops.write(device_array[i]->fd_val, (char *)&data->write_data[problems_submitted], sizeof(uint64_t), &offset);
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
                        *device_array[i]->id_used[k] = true;
                    }

                }

                //set the value of the device to the number of problems that have been submitted
                if (k == 16) {
                    *device_array[i]->value = 16;
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
                //read the data from the device. 96-bits need to be read from the device, so 3 reads need to be sent 
                offset = 4 * sizeof(uint32_t); //fifo flag offset
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
                        *device_array[i]->value = *device_array[i]->value - 1 ;
                    }
                }
            }

            //Store the data in the user_data struct
            found_counter = *data->first_problem; // used to search through the problems that have been submitted
            first_problem_tracker = false; //track which problems have been solved so they do not need to be searched through
            found = false; //track if the problem has been found to break the while loop

            while(!found){
                if(data->card_id[found_counter] == card_id && data->problem_id[found_counter] == problem_id){
                    data->best_spins[found_counter] = best_spins;
                    data->best_ham[found_counter] = best_ham;
                    data->solved++;
                    found = true;
                    found_counter++;
                }
                
                /*
//TODO: track the first problem that has not been solved
                if(data->best_spins[found_counter] == 0  && !first_problem_tracker){
                    data->first_problem = found_counter;
                    first_problem_tracker = true;
                }
                */
                
            }
            

        }
    }

    //transfer the data that is solved to a queue that will store the problems until they are ready to be sent back to the user
    store_user_data_read(data);

    // Free the user_data structure
    kfree(ud_work);

out:
    kfree(ud_work);
}

// Add function to queue work
int queue_user_data_work(struct user_data *data){
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
    //declare funciton variables
    int i;
    uint64_t *bulk_write_data = (uint64_t*)kmalloc(RAW_BYTE_CNT * sizeof(uint64_t), GFP_KERNEL);
    
    if(enable_logging){   
        log_action("PCI write intialize started", sizeof("PCI write intialize started"));
    }

    

    // Allocate memory for the user_data structure based on the amount of problem (size) sent from the userspace
    data->problem_id = (uint32_t*)kmalloc(size * sizeof(int), GFP_KERNEL);
    data->best_spins = (uint64_t*)kmalloc(size * sizeof(char __user *), GFP_KERNEL);
    data->best_ham = (uint64_t*)kmalloc(size * sizeof(char __user *), GFP_KERNEL);
    data->write_data = (uint64_t**)kmalloc(size * sizeof(uint64_t *), GFP_KERNEL);
    
    //divide the problems from the user space into the user_data structs 
    for (i = 0; i < size; i++) {
        // Allocate memory for each write_data structure
        data->write_data[i] = (uint64_t*)kmalloc(RAW_BYTE_CNT * sizeof(uint64_t), GFP_KERNEL);
        /*
        //check if allocation failed
        if (!data->write_data[i]) {
            // Handle allocation failure
            return NULL;
        }

        //copy the data from the user space to the kernel space
        if (copy_from_user(bulk_write_data, data + (i * sizeof(uint64_t) * RAW_BYTE_CNT), sizeof(uint64_t))) {
            kfree(bulk_write_data);
            return NULL;
        }
        //copy the data to the user_data struct
        data->write_data[i] = bulk_write_data;
        //intialize the problem id to NULL
        data->problem_id[i] = 0;

    }

    //intialize the probem count and solved count that will be used to know when to return the solution to the user
    data->user_id = user_id_counter++;
    data->first_problem = 0;
    data->problem_count = size;
    data->solved = 0;
    */
    kfree(bulk_write_data);

    if(enable_logging){
        log_action("PCI write intialize ended", sizeof("PCI write intialize ended"));
    }
    
    return data;
};


//read function of the virtual PCIe device
static ssize_t vpci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int i;
    //create a temp data structure to store user_data_read
    struct user_data_read *user_data_read_temp = kmalloc(sizeof(user_data_read), GFP_KERNEL);

    if(enable_logging){
        log_action("PCI read started", sizeof("PCI read started"));
    }


    //intialize the user_data_read_temp struct

    if (copy_from_user(user_data_read_temp, buf, sizeof(*user_data_read_temp)) != 0) {
        return -EFAULT;
    }

    for(i = 0; i < MAX_USER_DATA; i++){
        
        if(processed_data[i]->user_id == user_data_read_temp->user_id){
            //allocate the memory space to store the solution data
            if (copy_to_user(buf, processed_data[i], sizeof(processed_data[i])) != sizeof(processed_data[i])) {
                return -EFAULT;
            }

            // Free the processed data
            kfree(processed_data[i]);
            kfree(user_data_read_temp);
            processed_data[i] = NULL;

                if(enable_logging){
                    log_action("PCI read sucessful", sizeof("PCI read sucessful"));
                }

            return 0;
        }
    }

    kfree(user_data_read_temp);

    if(enable_logging){
        log_action("PCI read unsucessful", sizeof("PCI read unsucessful"));
    }

    return -1;
}

static ssize_t vpci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
    //initialize variables 
    size_t data_count;
    struct user_data *data = kmalloc(sizeof(*data), GFP_KERNEL);
    
    if(enable_logging){
        log_action("PCI write started", sizeof("PCI write started"));
    }



    // Determine number of uint64_t structures
    data_count = len / sizeof(uint64_t);  
    
    // Log the number of problems passed
    if(enable_logging){
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "Problems passed: %zu", data_count);
        log_action(log_message, sizeof(log_message));
    }

    
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

    if(enable_logging){
        log_action("PCI write ended", sizeof("PCI write ended"));
    }
    
    return 0;
}

static int vpci_open(struct inode *inode, struct file *file){
    
    int ret = 0;
    mutex_lock(&open_lock);
    if (busy) {
        printk( KERN_WARNING "PCI:  device busy");
        ret = -EBUSY;
    } else {
        // claim device
        busy = true;
        printk( KERN_WARNING "PCI:  opened device");
    }
    mutex_unlock(&open_lock);

    return ret;
}

static int vpci_release(struct inode *inode, struct file *file){
    busy = false;
    return 0;
}

//file operations for the virtual PCIe device
static struct file_operations vpci_fops = {
    .owner = THIS_MODULE,
    .read = vpci_read, 
    .write = vpci_write,
    .open = vpci_open,
    .release = vpci_release
};

static int __init vpci_init(void) {
    //variables
    struct file *file;
    int result;
    char device_file[256];

    if(enable_logging){
        log_action("PCI Intialized", sizeof("PCI Intialized"));
    }

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
            memset(dev_data->id_used, false, sizeof(dev_data->id_used));

            //store the device data in the device array
            device_array[i] = dev_data;
            
            printk(KERN_INFO "VPCI Driver: Opened device %s\n", device_file);

        } 
        else {
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