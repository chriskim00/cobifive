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
static int user_id_counter = 0;
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
    uint32_t user_id;        // Remove pointer
    uint32_t problem_count;  // Remove pointer
    uint32_t solved;        // Already not a pointer
    uint32_t first_problem; // Already not a pointer
    uint32_t *card_id;
    uint32_t *problem_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
    uint64_t **write_data;
};
typedef struct user_data user_data;

//read data structure
struct user_data_read {
    int user_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
};
typedef struct user_data_read user_data_read;

//array to store the user_data_read
static struct user_data_read *processed_data[MAX_USER_DATA];


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
            kfree(data->write_data[i]);
        }
        kfree(data->write_data);
    }

    // Free other arrays
    kfree(data->card_id);
    kfree(data->problem_id);
    kfree(data->best_spins);
    kfree(data->best_ham);

    // Finally free the main structure
    kfree(data);
}

// Store the processed data in an array
static void store_user_data_read(struct user_data *data) {
    struct user_data_read *new_data = kmalloc(sizeof(struct user_data_read), GFP_KERNEL);

    if (!new_data)
        return;
    
    new_data->user_id = data->user_id;
    *new_data->best_spins = *data->best_spins;
    *new_data->best_ham = *data->best_ham;

    for (int i = 0; i < MAX_USER_DATA; i++) {
        if (processed_data[i] == NULL) {
            processed_data[i] = new_data;
            cleanup_user_data(data, data->problem_count);
            break;
        }
    }

}

static void bulk_write(struct *user_data, int device_count, int problems_submitted){
    //WRITE
    //go through the devices and check if they are avaiable to be written to
    for (i = 0; i < device_count; i++) {

        //check if device "i" is avaiable to be written to
        if (device_array[i] && device_array[i]->problems_stored < 16) {
            
            //submit as many problems to the device as avaiable slots
            for (k = 0; k < 17; k++) {
                //find an open problem id to use and check if there are problems to be submitted
                if( (problems_submitted != data->problem_count)){

                    printk(KERN_WARNING "VPCI: problems_submitted=%d, problem_count=%d\n", problems_submitted, data->problem_count);
                    printk(KERN_WARNING "VPCI: id_used[%d]=%d, data->write_data[%d]=%p\n", k, device_array[i]->id_used[k], problems_submitted, data->write_data[problems_submitted]);

                    if((device_array[i]->id_used[k] == false) && (data->write_data[problems_submitted] != NULL)){
                        timeout = 0;
                        printk(KERN_WARNING "Found an open chip slot\n");
                        //set offset
                        offset = 9 * sizeof(uint64_t);
                        
                        //set the problem id in the chip data
                        problem_id_16 = (uint16_t)k;
                        printk(KERN_WARNING "VPCI: problem_id_16=%d, k=%d\n", problem_id_16, k);
                        set_pid_in_rawData(data->write_data[problems_submitted],problem_id_16 );
                        printk(KERN_WARNING "VPCI: write_data[%d][%d] = 0x%llx\n", problems_submitted, 164, data->write_data[problems_submitted][164]);

                        //swap the bytes of the 
                        for (size_t i = 0; i < RAW_BYTE_CNT; ++i) {
                            data->write_data[problems_submitted][i] =  swap_bytes(data->write_data[problems_submitted][i]);   // 64-bit value
                        }

                        // for (int j = 0; j < RAW_BYTE_CNT; j++) {
                        //     printk(KERN_WARNING "VPCI: write_data[%d][%d] = 0x%llx\n", problems_submitted, j, data->write_data[problems_submitted][j]);
                        // }

                        //printk(KERN_WARNING "VPCI: Size of write_data[%d] = 0x%lx\n", problems_submitted, sizeof(data->write_data[problems_submitted][0]));
                        
                        // Send the problem to the PCIe device
                        ret = tpci_fops.write((struct file *)device_array[i]->fd_val, (const char *)data->write_data[problems_submitted], RAW_BYTE_CNT * sizeof(uint64_t), &offset);
                        if (ret < 0) {
                            printk(KERN_ERR "Failed to write to underlying device: %d\n", ret);
                            return;
                        }
                        
                        //set the ids that will identify which problem is being solved on what chip
                        data->card_id[problems_submitted] = i;
                        data->problem_id[problems_submitted] = k;
                        printk(KERN_WARNING "Processing problem %d\n", i);

                        //increment the problems_submitted to sumbit the next problem in the problem set
                        problems_submitted++;                        
                        
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

// Add the worker function
static void process_user_data(struct work_struct *work){
    struct user_data_work *ud_work = container_of(work, struct user_data_work, work);
    struct user_data *data = kmalloc(sizeof(user_data), GFP_KERNEL);
    
    int i;
    int ret;
    int k;
    int j;
    int device_count = 0;
    int problems_submitted = 0;
    uint32_t *read_data = kmalloc(sizeof(uint32_t), GFP_KERNEL);
    uint64_t best_spins;
    uint64_t best_ham;
    int problem_id = 0;
    uint16_t problem_id_16;
    int card_id = 0;
    uint32_t read_flag = 0 ;
    loff_t offset = 0;
    bool found = false;
    int found_counter = 0;
    bool first_problem_tracker;
    int timeout = 0;

    //set the pointers from the work struct to the data struct 
    data = ud_work->data;

    printk(KERN_WARNING "VPCI: problem_count = %d\n", data->problem_count);
    printk(KERN_WARNING "VPCI: ud_work problem_count = %d\n", ud_work->data->problem_count);

    printk(KERN_WARNING "VPCI: Entered process_user_data \n");

    mutex_lock(&process_lock);
    
    printk( KERN_WARNING "VPCI: Queue has started to process user data \n");
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

        //set a timeout to leave the loop
        if(timeout > 50){
            printk(KERN_WARNING "VPCI: Timeout\n");
            goto out;
        }
        msleep(100);  // .1 second delay
        timeout++;

        //WRITE
        //go through the devices and check if they are avaiable to be written to
        for (i = 0; i < device_count; i++) {

            //check if device "i" is avaiable to be written to
            if (device_array[i] && device_array[i]->problems_stored < 16) {
                
                //submit as many problems to the device as avaiable slots
                for (k = 0; k < 17; k++) {
                    //find an open problem id to use and check if there are problems to be submitted
                    if( (problems_submitted != data->problem_count)){

                        printk(KERN_WARNING "VPCI: problems_submitted=%d, problem_count=%d\n", problems_submitted, data->problem_count);
                        printk(KERN_WARNING "VPCI: id_used[%d]=%d, data->write_data[%d]=%p\n", k, device_array[i]->id_used[k], problems_submitted, data->write_data[problems_submitted]);

                        if((device_array[i]->id_used[k] == false) && (data->write_data[problems_submitted] != NULL)){
                            timeout = 0;
                            printk(KERN_WARNING "Found an open chip slot\n");
                            //set offset
                            offset = 9 * sizeof(uint64_t);
                            
                            //set the problem id in the chip data
                            problem_id_16 = (uint16_t)k;
                            printk(KERN_WARNING "VPCI: problem_id_16=%d, k=%d\n", problem_id_16, k);
                            set_pid_in_rawData(data->write_data[problems_submitted],problem_id_16 );
                            printk(KERN_WARNING "VPCI: write_data[%d][%d] = 0x%llx\n", problems_submitted, 164, data->write_data[problems_submitted][164]);

                            //swap the bytes of the 
                            for (size_t i = 0; i < RAW_BYTE_CNT; ++i) {
                                data->write_data[problems_submitted][i] =  swap_bytes(data->write_data[problems_submitted][i]);   // 64-bit value
                            }

                            // for (int j = 0; j < RAW_BYTE_CNT; j++) {
                            //     printk(KERN_WARNING "VPCI: write_data[%d][%d] = 0x%llx\n", problems_submitted, j, data->write_data[problems_submitted][j]);
                            // }

                            //printk(KERN_WARNING "VPCI: Size of write_data[%d] = 0x%lx\n", problems_submitted, sizeof(data->write_data[problems_submitted][0]));
                            
                            // Send the problem to the PCIe device
                            ret = tpci_fops.write((struct file *)device_array[i]->fd_val, (const char *)data->write_data[problems_submitted], RAW_BYTE_CNT * sizeof(uint64_t), &offset);
                            if (ret < 0) {
                                printk(KERN_ERR "Failed to write to underlying device: %d\n", ret);
                                return;
                            }
                            
                            //set the ids that will identify which problem is being solved on what chip
                            data->card_id[problems_submitted] = i;
                            data->problem_id[problems_submitted] = k;
                            printk(KERN_WARNING "Processing problem %d\n", i);

                            //increment the problems_submitted to sumbit the next problem in the problem set
                            problems_submitted++;                        
                            
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

        //READ 
        //check if any of the devices need to be read from
        for (i = 0; i < device_count; i++) {
            
            read_flag = 0;
            //check if device "i" is avaiable to be read from by checking the read flag
            offset = 10 * sizeof(uint32_t); //fifo flag offset
            
            ret = tpci_fops.read(device_array[i]->fd_val, (char *)read_data, sizeof(read_data), &offset);
            
            if (ret < 0) {
                printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                goto out;
            }
            
            //set the read flag from the returned read data 
            read_flag = *read_data;
            //read the data from the device
            if(read_flag == 1){
                timeout = 0;
                
                //read the data from the device. 96-bits need to be read from the device, so 3 reads need to be sent 
                offset = 4 * sizeof(uint32_t); //fifo flag offset
                for( k = 0 ; k <3; k++){

                    ret = tpci_fops.read(device_array[i]->fd_val, (char *)read_data, sizeof(read_data), &offset);
                    if (ret < 0) {
                        printk(KERN_ERR "Failed to read from underlying device: %d\n", ret);
                        goto out;
                    }

                    if(k == 0){
                        printk(KERN_WARNING "VPCI: read_data=0x%x\n", *read_data);
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
                printk(KERN_WARNING "VPCI: problem_id = %d\n", problem_id);
                printk(KERN_WARNING "VPCI: card_id = %d\n", card_id); 
                printk(KERN_WARNING "VPCI: best_spins = 0x%llx\n", best_spins);
                printk(KERN_WARNING "VPCI: best_ham = 0x%llx\n", best_ham);
                
                for(j = 0; j < data->problem_count; j++){
                    if(data->card_id[j] == card_id && data->problem_id[j] == problem_id){
                        printk(KERN_WARNING "VPCI: data->card_id[%d]=%d, card_id=%d, data->problem_id[%d]=%d, problem_id=%d\n", j, data->card_id[j], card_id, j, data->problem_id[j], problem_id);
                        msleep(100); 
                        data->best_spins[k] = best_spins;
                        data->best_ham[k] = best_ham;
                        data->solved++;
                        break;
                    }
                }
                
            }
            

            /*
            //Store the data in the user_data struct
            found_counter = data->first_problem; // used to search through the problems that have been submitted
            first_problem_tracker = false; //track which problems have been solved so they do not need to be searched through
            found = false; //track if the problem has been found to break the while loop
           
           for(k = 0; k < data->problem_count; k++){
                if(data->card_id[k] == card_id && data->problem_id[k] == problem_id){
                    data->best_spins[k] = best_spins;
                    data->best_ham[k] = best_ham;
                    data->solved++;
                    found = true;
                    found_counter++;
                }
           }
            
            while(!found){
                if(data->card_id[found_counter] == card_id && data->problem_id[found_counter] == problem_id){
                    data->best_spins[found_counter] = best_spins;
                    data->best_ham[found_counter] = best_ham;
                    data->solved++;
                    found = true;
                    found_counter++;
                }
            
                
//TODO: track the first problem that has not been solved
                // if(data->best_spins[found_counter] == 0  && !first_problem_tracker){
                //     data->first_problem = found_counter;
                //     first_problem_tracker = true;
                // }
                
                
            }
            */
        }
    }

    printk( KERN_WARNING "VPCI: Exiting process_user_data \n");

    //transfer the data that is solved to a queue that will store the problems until they are ready to be sent back to the user
    store_user_data_read(data);

    // Free the user_data structure
    kfree(ud_work);
    kfree(data);
    mutex_unlock(&process_lock);

out:
    kfree(ud_work);
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

    if (!data || !buf || problem_count == 0 || problem_count > 1000) {
        snprintf(log_message, sizeof(log_message), "VPCI: Invalid parameters\n");
        log_action(log_message, strlen(log_message));
        return NULL;
    }

    // Allocate memory for the user_data structure
    data->card_id = (uint32_t *)kmalloc( problem_count * sizeof(uint32_t), GFP_KERNEL);
    data->problem_id = (uint32_t *)kmalloc( problem_count * sizeof(uint32_t), GFP_KERNEL);
    data->best_spins = (uint64_t *)kmalloc( problem_count * sizeof(uint64_t), GFP_KERNEL);
    data->best_ham = (uint64_t *)kmalloc( problem_count * sizeof(uint64_t), GFP_KERNEL);
    data->write_data = (uint64_t **)kmalloc(problem_count * sizeof(uint64_t *), GFP_KERNEL);

    if (!data || !buf || problem_count == 0) {
        snprintf(log_message, sizeof(log_message), "Invalid parameters in init_user_data");
        log_action(log_message, strlen(log_message));
        return NULL;
    }

    snprintf(log_message, sizeof(log_message), "Initalized regular vars\n");
    log_action(log_message, strlen(log_message));
    
    //divide the problems from the user space into the user_data structs 
    for (i = 0; i < problem_count; i++) {
        // Allocate memory for each write_data structure
        snprintf(log_message, sizeof(log_message), "Intialized single write_data value\n");
        log_action(log_message, strlen(log_message));
        data->write_data[i] = (uint64_t*)kmalloc(RAW_BYTE_CNT * sizeof(uint64_t), GFP_DMA);


        //check if allocation failed
        if (!data->write_data[i]) {
            // Handle allocation failure
            snprintf(log_message, sizeof(log_message), "Failed to initalize single write_data value\n");
            log_action(log_message, strlen(log_message));
            goto cleanup;
        }
        

        //copy the data from the user space to the kernel space
        if (copy_from_user(data->write_data[i], buf + (i * sizeof(uint64_t) * RAW_BYTE_CNT), RAW_BYTE_CNT * sizeof(uint64_t))) {
            snprintf(log_message, sizeof(log_message), "Failed to copy single write_data value\n");
            log_action(log_message, strlen(log_message));
            goto cleanup;
        }

        //intialize the problem id to NULL
        data->problem_id[i] = 0;

    }

    
    //intialize the probem count and solved count that will be used to know when to return the solution to the user
    data->user_id = user_id_counter++;
    data->first_problem = 0;
    data->problem_count = problem_count;
    data->solved = 0;
    
    
    snprintf(log_message, sizeof(log_message), "User ID: %d, First Problem: %d, Problem Count: %d, Solved: %d \n", data->user_id, data->first_problem, data->problem_count, data->solved);
    log_action(log_message, strlen(log_message));

    /*
    // Log write_data values
    int k;
    for (i = 0; i < problem_count; i++) {
        if (data->write_data[i]) {
            for (k = 0; k < RAW_BYTE_CNT; k++){
                snprintf(log_message, sizeof(log_message), "Write Data[%d][%d]: 0x%llx", i, k, (unsigned long long)(data->write_data[i][k]));
                log_action(log_message, strlen(log_message));
            }
        }
    }
    */
    return data;
    
cleanup:
    snprintf(log_message, sizeof(log_message), "Cleanup error \n");
    log_action(log_message, strlen(log_message));
    cleanup_user_data(data, problem_count); 
    return NULL;
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
    size_t problem_count;
    struct user_data *data = kmalloc(sizeof(user_data), GFP_KERNEL);
    char log_message[256];

    printk( KERN_WARNING "VPCI:  Entered write");
    if(enable_logging){
        log_action("PCI write started\n", sizeof("PCI write started\n"));
    }

    // Determine number of uint64_t structures
    // Calculate problem count and ensure it's a whole number
    problem_count = len / (RAW_BYTE_CNT * sizeof(uint64_t));
    if (len % (RAW_BYTE_CNT * sizeof(uint64_t)) != 0) {
        printk(KERN_ERR "VPCI: Invalid data length - must be multiple of %zu bytes\n", RAW_BYTE_CNT * sizeof(uint64_t));
        return -EINVAL;
    }

    // Log the number of problems passed
    if(enable_logging){
        snprintf(log_message, sizeof(log_message), "Problems passed: %zu\n", problem_count);
        log_action(log_message, sizeof(log_message));
    }
    
    // Further processing of the user data into a structure user_data that seperates out the problems and ids them
    if(len != sizeof(buf)){
        //check if data was created
        if (!data)
            return -ENOMEM;

        //intialize the data struct with the user data
        init_user_data(file, data, buf, problem_count);
        
        if (!data){
            printk( KERN_WARNING "VPCI:  Data Intializing Failed");
            return -EFAULT;
        }

    }

    //queue the user data to be processed
    if(queue_user_data_work(data)){
        snprintf(log_message, sizeof(log_message), "Queueing Failed\n");
        log_action(log_message, sizeof(log_message));
        printk( KERN_WARNING "VPCI:  Queue work failed");
        cleanup_user_data(data, problem_count);
        return -ENOMEM;
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
        printk( KERN_WARNING "VPCI:  opened device");
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
    char log_message[512];
    int i;
    int k;

    if(enable_logging){
        log_action("PCI Intialized", sizeof("PCI Intialized"));
    }

    printk(KERN_WARNING "VPCI: Checking for PCI devices\n");
    
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
            
            printk(KERN_WARNING "VPCI: Opened device %s\n", device_file);

        } 
        else {
            printk(KERN_WARNING "VPCI: No device found at %s\n", device_file);
        }
    }
    
    //create a queue to process the users data
    printk(KERN_WARNING "VPCI: Create  Queue \n" );

    user_data_wq = create_singlethread_workqueue("user_data_worker");

    snprintf(log_message, sizeof(log_message),"user_data_wq = %p\n", user_data_wq);
    log_action(log_message, strlen(log_message));

    if (!user_data_wq) {
        // Handle log_message
        printk(KERN_WARNING "VPCI: Created  Queue Failed\n");
        return -ENOMEM;
    }

    printk(KERN_WARNING "VPCI: Created  Queue \n");



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
    printk(KERN_WARNING "VPCI: Module loaded\n");
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