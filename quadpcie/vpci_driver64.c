#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/export.h> //used to import EXPORT_SYMBOL() functions
#include <linux/workqueue.h>
#include <linux/delay.h>  // Include this header for msleep

MODULE_AUTHOR("William Moy");
MODULE_DESCRIPTION("Virtual PCIe Queue and Scheduler Driver");
MODULE_VERSION(".1"); 
MODULE_LICENSE("GPL");
extern struct file_operations tpci_fops;

//// Function to log time to a file that is stored in /var/log/pci_driver.log
static void log_action(const char *message, size_t size) {
    struct file *file;
    loff_t pos = 0;
    char *buf;
    size_t len;
    
    // Allocate buffer with space for newline and null terminator
    buf = kmalloc(size + 2, GFP_KERNEL);
    if (!buf)
        return;

    // Copy message and append newline
    len = scnprintf(buf, size + 2, "%s\n", message);

    // Open and write to log file
    file = filp_open("/tmp/pci_driver.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(file)) {
        kfree(buf);
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
static uint32_t user_id_counter = 0;
static bool busy = false;

//Define mutexes
static DEFINE_MUTEX(open_lock);
static DEFINE_MUTEX(process_lock);


//store device data 
struct device_data {
    struct file *fd_val;
    int problems_stored;
    bool id_used[16];  // Array to track which FIFO slots need to be read
};

//store the devices in an array
struct device_data *device_array[MAX_DEVICES];

//store the users problems and solutions 
struct user_data {
    uint64_t user_id;        // Remove pointer
    uint32_t problem_count;  // Remove pointer
    uint32_t *card_id;
    uint32_t *problem_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
    uint64_t **write_data;
};
typedef struct user_data user_data;

//array to store the read data which are lists of uint64_t values
static uint64_t *processed_data[MAX_USER_DATA];

//make a queue to store write data and work on user_data structures
static struct workqueue_struct *user_data_wq;

//user_data_work struct to store the user_data and work_struct
struct user_data_work {
    struct work_struct work;
    struct user_data *data;
};
typedef struct user_data_work user_data_work;

///////////////////////////HELPER FUNCTIONS///////////////////////////
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
    uint64_t problem_id = 0;
    //Set the problem id 
    problem_id =  new_pid << 24;
    rawData[164] = rawData[164] & 0xFFFFFFFF0000FFFF;
    //OR the problem id with the write data that contains the problem id
    rawData[164] = (problem_id | rawData[164]);
}

//swaps the right byte with the left byte -> ABCD -> BADC
uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

//clean up the user data struct
static void cleanup_user_data(struct user_data *data, size_t problem_count) {
    if (!data) return;

    // Free write_data arrays first
    if (data->write_data) {
        for (int i = 0; i < problem_count; i++) {
            if(data->write_data[i]) kfree(data->write_data[i]);
        }
        if(data->write_data) kfree(data->write_data);
    }

    // Free other arrays
    if(data->card_id) kfree(data->card_id);
    if(data->problem_id) kfree(data->problem_id);
    if(data->best_spins) kfree(data->best_spins);
    if(data->best_ham) kfree(data->best_ham);

    // Finally free the main structure
    if(data) kfree(data);
}

// Store the processed data in an array
static void store_user_data_read(struct user_data *data) {
    //store user_id and problem_count at the front of the array followed by the spins and ham 
    uint64_t *read_data = (uint64_t *)kmalloc((2 + 2 * data->problem_count) * sizeof(uint64_t),GFP_KERNEL);
    int i;
    
    if (!read_data)
        return;
    
    //set the user_id
    read_data[0] = data->user_id;
    read_data[1] = data->problem_count;

    //initialize the read_data arary with the best_spins and best_ham values
    for(i = 0; i < data->problem_count; i++){
        read_data[2 + i] = data->best_spins[i];
        read_data[2 + data->problem_count + i] = data->best_ham[i];
    }
    

    for (int i = 0; i < MAX_USER_DATA; i++) {
        if (processed_data[i] == NULL) {
            processed_data[i] = read_data;
            //printk(KERN_WARNING "VPCI: Stored User Data\n");
            return;
        }
    }
    
    //printk(KERN_WARNING "VPCI: Failed to Store User Data\n");

}

static void bulk_write(struct user_data *data, int device_count, int *problems_submitted){
    loff_t offset = 0;
    int i;
    int k;
    uint16_t problem_id_16;
    int ret;
    //WRITE
    //go through the devices and check if they are avaiable to be written to
    for (i = 0; i < device_count; i++) {

        //check if device "i" is avaiable to be written to
        if (device_array[i] && device_array[i]->problems_stored < 16) {
            
            //submit as many problems to the device as avaiable slots
            for (k = 0; k < 17; k++) {
                //find an open problem id to use and check if there are problems to be submitted
                if( (*problems_submitted != data->problem_count)){
                    
                    //DEBUG
                    //printk(KERN_WARNING "VPCI: problems_submitted=%d, problem_count=%d\n", *problems_submitted, data->problem_count);
                    //printk(KERN_WARNING "VPCI: id_used[%d]=%d, data->write_data[%d]=%p\n", k, device_array[i]->id_used[k], *problems_submitted, data->write_data[*problems_submitted]);

                    if((device_array[i]->id_used[k] == false) && (data->write_data[*problems_submitted] != NULL)){
                        //DEBUG
                        //printk(KERN_WARNING "Found an open chip slot\n");

                        //set offset
                        offset = 9 * sizeof(uint64_t);
                        
                        //set the problem id in the chip data
                        problem_id_16 = (uint16_t)k;
 
                        set_pid_in_rawData(data->write_data[*problems_submitted],problem_id_16 );
     

                        //swap the bytes of the 
                        for (size_t i = 0; i < RAW_BYTE_CNT; ++i) {
                            data->write_data[*problems_submitted][i] =  swap_bytes(data->write_data[*problems_submitted][i]);   // 64-bit value
                        }

                        // for (int j = 0; j < RAW_BYTE_CNT; j++) {
                        //     printk(KERN_WARNING "VPCI: write_data[%d][%d] = 0x%llx\n", *problems_submitted, j, data->write_data[problems_submitted][j]);
                        // }

                        //printk(KERN_WARNING "VPCI: Size of write_data[%d] = 0x%lx\n", *problems_submitted, sizeof(data->write_data[problems_submitted][0]));
                        
                        // Send the problem to the PCIe device
                        ret = tpci_fops.write((struct file *)device_array[i]->fd_val, (const char *)data->write_data[*problems_submitted], RAW_BYTE_CNT * sizeof(uint64_t), &offset);
                        if (ret < 0) {
                            printk(KERN_ERR "Failed to write to underlying device: %d\n", ret);
                            return;
                        }
                        
                        //set the ids that will identify which problem is being solved on what chip
                        data->card_id[*problems_submitted] = i;
                        data->problem_id[*problems_submitted] = k;
                        //printk(KERN_WARNING "Processing problem %d\n", i);

                        //increment the problems_submitted to sumbit the next problem in the problem set
                        *problems_submitted = *problems_submitted + 1;                        
                        
                        //indicate how many problems are being stored in the chip
                        device_array[i]->problems_stored++;

                        //indicate that the problem id is being used
                        device_array[i]->id_used[k] = true;
                    }
                }

            }

            //set the value of the device to the number of problems that have been submitted
            if (k == 16) {
                device_array[i]->problems_stored = 16;
                break;
            }

        }
    }
}

static void bulk_read(struct user_data *data, int device_count, int *solved, int *solved_array){
    uint32_t read_flag = 0 ;
    loff_t offset = 0;

    int problem_id = 0;
    int card_id = 0;
    uint64_t best_spins;
    uint64_t best_ham;

    int ret;
    uint32_t *read_data = kmalloc(sizeof(uint32_t), GFP_KERNEL);
    int first_problem = 0;
    bool first_problem_tracker;

    int i;
    int k;
    int j;

    for (i = 0; i < device_count; i++) {
        
        read_flag = 0;
        //check if device "i" is avaiable to be read from by checking the read flag
        offset = 10 * sizeof(uint32_t); //fifo flag offset
        
        ret = tpci_fops.read(device_array[i]->fd_val, (char *)read_data, sizeof(read_data), &offset);
        
        if (ret < 0) {
            printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
            kfree(read_data);
            return;
        }
        
        //set the read flag from the returned read data 
        read_flag = *read_data;
        //read the data from the device
        if(read_flag == 1){                
            //read the data from the device. 96-bits need to be read from the device, so 3 reads need to be sent 
            offset = 4 * sizeof(uint32_t); //fifo flag offset
            for( k = 0 ; k <3; k++){

                ret = tpci_fops.read(device_array[i]->fd_val, (char *)read_data, sizeof(read_data), &offset);
                if (ret < 0) {
                    printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                    kfree(read_data);
                    return;
                }

                if(k == 0){
                    problem_id = inverse_data(*read_data);
                    card_id = inverse_data(*read_data >> 4);
                    best_spins = *read_data;
                }
                if(k == 1){
                    best_spins = ((uint64_t)*read_data << 32) | (best_spins & 0xFFFFFFFF);
                    best_ham = *read_data;
                }
                if(k == 2){
                    best_ham = ((uint64_t)*read_data << 32) | (best_ham & 0xFFFFFFFF);
                    device_array[i]->problems_stored = device_array[i]->problems_stored - 1 ;
                }
            }

            //clear id spot for a new problem
            device_array[i]->id_used[problem_id] = false;
            
            j = first_problem; // used to search through the problems that have been solved
            first_problem_tracker = false; //track which problems have been solved so they do not need to be searched through
            
            for(j = 0; j < data->problem_count; j++){
                //printk(KERN_WARNING "VPCI: card_id[%d]=%d, card_id=%d, problem_id[%d]=%d, problem_id=%d\n", j, data->card_id[j], card_id, j, data->problem_id[j], problem_id);
                if(data->card_id[j] == card_id && data->problem_id[j] == problem_id){
                    //store the problem solution in the data struct
                    data->best_spins[j] = best_spins;
                    data->best_ham[j] = best_ham;
                    *solved = *solved + 1;

                    //indicate that the problem has been solved and see if we can reduce the number of problems that need to be iterated through in the for loop
                    solved_array[j] = 1;
                    if(!first_problem_tracker && (j-1) == first_problem)
                        first_problem = j;
                    
                    break;
                }
                
                //if the problem has been solved then set the first problem to the problem that has been solved to avoid going over it again
                if(solved_array[j] == 1 && !first_problem_tracker && (j-1) == first_problem){
                    first_problem = j;
                    first_problem_tracker = true;
                }
                
            }
            
        }
        
    }

}

// Add the worker function
static void process_user_data(struct work_struct *work){
    struct user_data_work *ud_work = container_of(work, struct user_data_work, work);
    struct user_data *data = kmalloc(sizeof(user_data), GFP_KERNEL);
    int i;
    int device_count = 0;
    int *problems_submitted = kmalloc(sizeof(int), GFP_KERNEL);
    int *solved = kmalloc(sizeof(int), GFP_KERNEL);
    int *solved_array; 

    //won't be used in offical version of driver
    int timeout = 0;

    
    //intialize variables
    //set the pointers from the work struct to the data struct 
    data = ud_work->data;
    *problems_submitted = 0;
    *solved = 0;
    //array to keep track of what problems have been solved
    solved_array = (int *)kmalloc(data->problem_count * sizeof(int), GFP_KERNEL);
    for (i = 0; i < data->problem_count; i++) {
        solved_array[i] = 0;
    }

    mutex_lock(&process_lock);
    
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
    

    while(*solved != data->problem_count){

        //WRITE
        bulk_write(data, device_count, problems_submitted);

        
        //READ 
        //check if any of the devices need to be read from
        bulk_read(data, device_count, solved, solved_array);
       
        msleep(100);
        timeout++;
        if ( timeout  == 10) {
            printk(KERN_WARNING "VPCI: Timeout occurred after 10 seconds\n");
            goto out;
        }
    }


    //transfer the data that is solved to a queue that will store the problems until they are ready to be sent back to the user
    store_user_data_read(data);

    // Free the user_data structure
    if(ud_work) kfree(ud_work);
    cleanup_user_data(data, data->problem_count);
    mutex_unlock(&process_lock);
    return;

out:
    if(ud_work) kfree(ud_work);
    cleanup_user_data(data, data->problem_count);
    mutex_unlock(&process_lock);
}

// Add function to queue work
int queue_user_data_work(struct user_data *data){
    char log_message[512]; //used to log messgages
    int ret;
    struct user_data_work *workq = kmalloc(sizeof(user_data_work), GFP_KERNEL);

    snprintf(log_message, sizeof(log_message), "Entering queue_user_data_work\n");
    log_action(log_message, strlen(log_message));

    if (!workq)
        return -ENOMEM;

    //copy the pointer of data to the workq struct data pointer
    workq->data = data;

    //create a work struct to be stored in the queue
    INIT_WORK(&workq->work, process_user_data);

    //log queueing work has been entered
    snprintf(log_message, sizeof(log_message), "Queueing work\n");
    log_action(log_message, strlen(log_message));

    //queue the work
    ret = queue_work(user_data_wq, &workq->work);
    if (!ret) {
        // Work was already queued or has failed
        snprintf(log_message, sizeof(log_message), "Queued user data failed\n");
        log_action(log_message, strlen(log_message));
        kfree(workq);
        return -EBUSY;
    }

    return 0;
}

//intailize the user data struct
static struct user_data* init_user_data(struct file *file, struct user_data *data, const char __user *buf, size_t problem_count) {
    //declare funciton variables
    char log_message[512];
    int i;    
    int ret;
    uint64_t *temp_data_storage;
    if (!data || !buf || problem_count == 0 || problem_count > 1000) {
        printk(KERN_ERR "VPCI: Unitialized Internal data storage or user buffer or problem_count = 0 or over 1000 \n");
        return NULL;
    }

    // Allocate memory for the user_data structure
    data->card_id = (uint32_t *)kmalloc( problem_count * sizeof(uint32_t), GFP_KERNEL);
    data->problem_id = (uint32_t *)kmalloc( problem_count * sizeof(uint32_t), GFP_KERNEL);
    data->best_spins = (uint64_t *)kmalloc( problem_count * sizeof(uint64_t), GFP_KERNEL);
    data->best_ham = (uint64_t *)kmalloc( problem_count * sizeof(uint64_t), GFP_KERNEL);
    data->write_data = (uint64_t **)kmalloc(problem_count * sizeof(uint64_t *), GFP_KERNEL);
    
    temp_data_storage = kmalloc(problem_count * RAW_BYTE_CNT * sizeof(uint64_t), GFP_KERNEL);

    if(!temp_data_storage){
        printk(KERN_ERR "VPCI: Failed to allocate space for temp_data_storage \n");
    }  

    if (!access_ok(buf, RAW_BYTE_CNT * sizeof(uint64_t) * problem_count)) {
        printk(KERN_ERR "VPCI: Failed to access user_buffer \n");
        goto cleanup;
    }

    ret = copy_from_user(temp_data_storage, buf, RAW_BYTE_CNT * sizeof(uint64_t) * problem_count);
    if (ret != 0) {
        printk(KERN_ERR "VPCI: Copy ret = %d\n", ret);
        printk(KERN_ERR "VPCI: Failed to copy user data \n");
        goto cleanup;
    } else {
        printk(KERN_WARNING "VPCI: Copied user data\n");
    }

    printk(KERN_ERR "VPCI: Problem count = %zu\n", problem_count);
    (uint64_t *)kmalloc( problem_count * sizeof(uint64_t), GFP_KERNEL);
    //divide the problems from the user space into the user_data structs 
    for (i = 0; i < problem_count; i++) {
        // Allocate memory for the write_data array
        data->write_data[i] = (uint64_t*)kmalloc(RAW_BYTE_CNT * sizeof(uint64_t), GFP_KERNEL);

        //check if allocation failed
        if (!data->write_data[i]) {
            // Handle allocation failure
            printk(KERN_ERR "VPCI: Unitialized write_data[i] \n");
            goto cleanup;
        }

        // Before access_ok check:
        if (IS_ERR(buf + ((size_t)i * sizeof(uint64_t) * RAW_BYTE_CNT))) {
            printk(KERN_ERR "VPCI: Invalid user buffer pointer\n");
            goto cleanup;
        }

        //check if buffer is reachable 
        if (!access_ok(buf + ((size_t)i * sizeof(uint64_t) * RAW_BYTE_CNT), RAW_BYTE_CNT * sizeof(uint64_t))) {
            printk(KERN_ERR "VPCI: Failed to access user_buffer \n");
            goto cleanup;
        }

        //copy the data from the user space to the kernel space
        ret = copy_from_user(data->write_data[i], buf + (i * sizeof(uint64_t) * RAW_BYTE_CNT), RAW_BYTE_CNT * sizeof(uint64_t));

        if (ret != 0) {
            printk(KERN_ERR "VPCI: Copy ret = %d\n", ret);
            printk(KERN_ERR "VPCI: Failed to copy user data \n");
            goto cleanup;
        } else {
            printk(KERN_WARNING "VPCI: Copied user data\n");
        }

        //intialize the problem id to NULL
        data->problem_id[i] = 0;
    }

    
    //intialize the probem count and solved count that will be used to know when to return the solution to the user
    user_id_counter = user_id_counter + 1;
    data->user_id = user_id_counter;
    data->problem_count = problem_count;
    if(temp_data_storage) kfree(temp_data_storage);
    return data;
    
cleanup:
    snprintf(log_message, sizeof(log_message), "Cleanup error \n");
    log_action(log_message, strlen(log_message));
    if(temp_data_storage) kfree(temp_data_storage);
    cleanup_user_data(data, problem_count); 
    return NULL;
}

//read function of the virtual PCIe device
static ssize_t vpci_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
    int i;
    int j;
    int ret;
    size_t problem_count;
    //create a temp data structure to store user_data_read
    uint64_t  *user_data_read_temp = (uint64_t *)kmalloc(2 * sizeof(uint64_t), GFP_KERNEL);

    //copy the data from the user that contains the user_id and problem_count 
    if (copy_from_user(user_data_read_temp, buf, 2 * sizeof(uint64_t)) != 0) {
        return -EFAULT;
    }


    // Print the user_data_read_temp structure contents
    // printk(KERN_INFO "VPCI: user_id = %llu\n", user_data_read_temp[0] );
    // printk(KERN_INFO "VPCI: problem_count = %llu\n", user_data_read_temp[1] );

    for(i = 0; i < MAX_USER_DATA; i++){

        // Check if entry exists and data was intialized
       if (processed_data[i] != NULL && user_data_read_temp != NULL) {
            problem_count = processed_data[i][1];

            // debug print
            printk(KERN_INFO "VPCI: Found stored problem User ID: %llu\n", processed_data[i][0]);
            printk(KERN_INFO "VPCI: Found stored problem problem count: %llu\n", processed_data[i][1]);

            // DEBUG: Print the best_spins and best_ham values
            for(j = 0; j < (2 + 2 * problem_count); j++){
                printk(KERN_INFO "VPCI: read_data: %llu\n", processed_data[i][j]);
            }

            if(processed_data[i][0] == user_data_read_temp[0] && processed_data[i][1] == user_data_read_temp[1]){
                // Copy the processed data to the user

                ret = copy_to_user(buf, processed_data[i], (2 * problem_count + 2) * sizeof(uint64_t));
                //printk(KERN_INFO "VPCI: Return value from copy_to_user: %d\n", ret); //DEBUG
                if (ret != 0) {
                    printk(KERN_INFO "VPCI: Copy Failed \n");
                    return -EFAULT;
                }



                
                // Free the processed data
                kfree(processed_data[i]);
                kfree(user_data_read_temp);
                processed_data[i] = NULL;
                

                return 0;
            }
        } else{
            //printk(KERN_INFO "VPCI: processed_data[%d] null \n",i);
            return -EFAULT;
        }
    }
    kfree(user_data_read_temp);

    return -1;
}

static ssize_t vpci_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
    //initialize variables 
    size_t problem_count;
    struct user_data *data = kmalloc(sizeof(user_data), GFP_KERNEL);

    //printk( KERN_WARNING "VPCI:  Entered write");

    // Determine number of uint64_t structures
    // Calculate problem count and ensure it's a whole number
    problem_count = len / (RAW_BYTE_CNT * sizeof(uint64_t));
    if (len % (RAW_BYTE_CNT * sizeof(uint64_t)) != 0) {
        printk(KERN_ERR "VPCI: Invalid data length - must be multiple of %zu bytes\n", RAW_BYTE_CNT * sizeof(uint64_t));
        return -EINVAL;
    }
    
    // Further processing of the user data into a structure user_data that seperates out the problems and ids them
    if(len != sizeof(buf)){
        //check if data was created
        if (!data){
            printk( KERN_WARNING "VPCI:  Data Intializing Failed");
            return -EFAULT;
        }

        //intialize the data struct with the user data
        if(init_user_data(file, data, buf, problem_count) == NULL){
            printk( KERN_WARNING "VPCI:  Data Intializing Failed");
            return -EFAULT;
        }
        


    }

    /*
    //queue the user data to be processed
    if(queue_user_data_work(data)){
        printk( KERN_WARNING "VPCI:  Queue work failed");
        cleanup_user_data(data, problem_count);
        return -ENOMEM;
    }
    */

    printk(KERN_INFO "VPCI: User ID: %llu\n", data->user_id);
    return data->user_id;
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
        //printk( KERN_WARNING "VPCI:  opened device");
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
    struct file *fd;
    int result;
    char device_file[256];
    int i;
    int k;

    //printk(KERN_WARNING "VPCI: Checking for PCI devices\n");
    
    //open all PCI devices with COBI chips to prevent others from using
    for (i = 0; i < MAX_DEVICES; i++) {
        //create a device file string
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        //open the device file
        fd = filp_open(device_file, O_RDONLY, 0);
        //check if opening the file failed
        if (!IS_ERR(fd)) {
            //create a struct to store the device data
            struct device_data *dev_data = kmalloc(sizeof(struct device_data), GFP_KERNEL);
            if (!dev_data) {
                return -ENOMEM;
            }

            //intialize the dev_data struct
            dev_data->fd_val = fd;
            dev_data->problems_stored = 0;
            for(k = 0; k < 16; k++){
            dev_data->id_used[k] = false;
            }

            //store the device data in the device array
            device_array[i] = dev_data;
            
            //printk(KERN_WARNING "VPCI: Opened device %s\n", device_file);

        } 

    }
    
    //create a queue to process the users data
    user_data_wq = create_singlethread_workqueue("user_data_worker");

    if (!user_data_wq) {
        // Handle log_message
        //printk(KERN_WARNING "VPCI: Created  Queue Failed\n");
        return -ENOMEM;
    }

    //printk(KERN_WARNING "VPCI: Created  Queue \n");

    //Virtual PCIe device registration and initialization
    // Register the character device of the virtual driver with the Linux kernel
    result = register_chrdev(0, DRIVER_NAME, &vpci_fops);
    //check if the character device was successfully registered
    if( result < 0 )
    {
        //printk( KERN_WARNING "VPCI:  can't register character device with error code = %i\n", result );
        return result;
    }
    //store the major number
    major = result;

    //create a class for the vitual driver/device
    vpci_class = class_create(THIS_MODULE, DRIVER_NAME);
    if( IS_ERR( vpci_class ) )
    {
        //printk( KERN_WARNING "VPCI:  can\'t create device class with errorcode = %ld\n", PTR_ERR( vpci_class ) );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( vpci_class );
    }

    //make a device number using the MAJOR macro for the virtual driver
    device_number = MKDEV( major, 0 );

    //create a device for the virtual drivr class
    vpci_device = device_create( vpci_class, NULL, device_number, NULL, DRIVER_NAME );
    if ( IS_ERR(vpci_device ) )
    {
        //printk( KERN_WARNING "can\'t create device with errorcode = %ld\n", PTR_ERR( vpci_device ) );
        class_destroy( vpci_class );
        unregister_chrdev( major, DRIVER_NAME );
        return PTR_ERR( vpci_device );
    }
    //printk(KERN_WARNING "VPCI: Module loaded\n");
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
    flush_workqueue(user_data_wq);
    destroy_workqueue(user_data_wq);
    log_action("Workqueue destroyed\n", sizeof("Workqueue destroyed\n"));

    
    printk(KERN_WARNING "VPCI: Module unloaded\n");



}

module_init(vpci_init);
module_exit(vpci_exit);