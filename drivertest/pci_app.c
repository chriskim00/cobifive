#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <cerrno>
#include <stdarg.h>
#include <stdio.h>
//#include "cobi_input_data.h"

#define DEVICE_FILE_TEMPLATE "/dev/cobi_chip_testdriver64%d"
#define RAW_BYTE_CNT 166

/* 
THIS APPLICATION IS FOR the virtual pcie driver. 
Created by: RPJ
Date: Dec. 6, 2024
*/

void log_message(const char* format, ...) {
    FILE* output = fopen("output_logs", "a");
    if (!output) {
        output = stdout;  // Fallback to stdout if file can't be opened
    }
    
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    if (output != stdout) {
        fclose(output);
    }
    fflush(output);
}

uint64_t  rawData1[166] = {
0xF1F2F3F4F5F5ABC1, 0xA1A2A3A4A5A64433, 0xB1B2B3B4B5B6ABD1, 0x0030700020000000, 0x000B000000000209, 0x0980000090070000, 0x0063500000000000, 0x07080700000CB000, 0xD070228000000006, 0x000000000000E060, 0x5020002A00000000, 0x70CE0000000B2D00, 0x00003000500C0040, 0x0000E00750A00CA0, 0x0006C0000C008000, 0x000040E020C02E00, 
0x0000700008000000, 0x0000D0D0000007B0, 0x000000060AA50030, 0x00000000000200C0, 0xB60000B0030D0000, 0x00A00030070A0009, 0x9000000000000000, 0x0000000700300000, 0x0000000500000000, 0x0000000000000000, 0x002D0200B5800000, 0x7000000000088006, 0x0D00000004700A00, 0xD000000A00000000, 0x00000000000F0070, 0x00D07BC0000E0B00, 
0x0000B02000000000, 0x0407C0000C000000, 0x09D60042700B0000, 0x04000000F0800009, 0x0B00000000A0000F, 0x5C3B020080020060, 0x000900002000000D, 0x9E3000000E460090, 0x000040000C006000, 0x0000CE0902000000, 0x000000C860000000, 0x00000DD000000E00, 0x0600C09000740070, 0x0000040000090300, 0x0040A70030000000, 0x00570A0424F00000, 
0x0500000000000D0B, 0x000000000B000000, 0x000FF00B000000F0, 0x000CA00000D00000, 0x0000020680000000, 0x092000000E000070, 0xC000000300E0C00E, 0x0000E0E0B0000000, 0x000C000A0000000B, 0x0000C04000000000, 0x080000000000282D, 0x00800005420020D0, 0xF00D00000F000000, 0xC0000000B00009B0, 0x00C0500700008500, 0x0040400000000000, 
0x000B00D006000400, 0x0000000000000000, 0x7305000000800040, 0x05D0700300000000, 0xC0B20F0000000000, 0x00030000066000C0, 0x9060000A00000000, 0x0000000000020000, 0x00700F80E0E06E00, 0x900000060500000A, 0x0000000000C200E0, 0x0000B06000000090, 0x00500000000006B0, 0x000000000F000C00, 0x0000E00000000000, 0xD00A000000E00200, 
0x0000090039080000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0A00000000000000, 0xF006EA0000070000, 0x000004CB00000000, 0x00000502040000B0, 0x00B0000809000000, 0x00A00000F0000000, 0x0000800500000000, 0x00000DD00000000B, 0x0000E0D0E9C000C0, 
0xBF000E0E00030000, 0x0008AC000000E000, 0x00D0040C60AE0095, 0xFA00700000000000, 0x0000000000082002, 0x00908000030090D0, 0xC000006230000305, 0x00000056080A0090, 0x000B008D00000700, 0x0000000500002000, 0x0EE0000000700000, 0x0900000004000006, 0x0000000B0E0000D4, 0x00002B0000000400, 0x000D355000000000, 0x0EC09400000040E0, 
0x0A00800000000005, 0x8000000000E00000, 0x05000000D0090040, 0x0D090CB00000009C, 0x00003E0400000000, 0x000B0390F0600500, 0x000A000000000C00, 0x000000000000E000, 0x050003000000000C, 0xD0000E0000200B00, 0x0008000000F70000, 0x0006950000000000, 0x0000000A00000000, 0x0000000090000000, 0x000000005B000002, 0x90F0009D00006000, 
0xD00B004000000303, 0x00000A0000B00005, 0x0000000600000000, 0x9000000040000A09, 0x80000300000A0009, 0x220000B030700000, 0x0000900200F62000, 0x6000D00000080002, 0x0E000EB00009C400, 0x00007C000040000C, 0x00900903900F2000, 0x00A000050000D000, 0x000000800A090000, 0x000B000FC00E0700, 0x00000004000E0609, 0x0000000A070C0000, 
0x5000500A6D009500, 0x0000040070007000, 0x0000000008000000, 0x000877F090000000, 0x00300006060040D7, 0x00300000000D0B00, 0x0030050D00000000, 0x000B000200000000, 0x0000000000000B00, 0x5AE9030000000070, 0x0C0E040000007300, 0x0006000000F00C00, 0xB0000000060D0E00, 0x00080000C0F00B02, 0x300500000B800040, 0x0000000000000000, 
0x0000000000000000, 0x0000000000000000, 0x000300FD00000000, 0x00FF001F0003000F, 0xAAAAAAAA01110000, 0x000000AAAAAAAAAA

};

typedef struct {
    off_t offset;
    uint64_t value;  // Change to 64-bit
} write_data_t;

struct user_read_data {
    int user_id;
    uint64_t *best_spins;
    uint64_t *best_ham;
};
typedef struct user_read_data user_read_data;

//Helper Functions
//reverse the 8-bit data -> Takes in: 0b00001011 and outputs: 0b00001101
uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

static int perform_read(int fd, int* problem_id, uint64_t* best_spins, uint64_t* best_ham) {
    int read_flag = 0;
    int *card_id;
    __off_t offset;
    uint32_t read_data;
    int ret;
    int k;

    //check if device "i" is avaiable to be read from by checking the read flag
    offset = 10 * sizeof(uint32_t); //fifo flag offset
    ret = pread(fd, (char *)&read_data, sizeof(read_data), offset);
    if (ret < 0) {
        log_message("Failed to read from underlying device: %d\n", ret);
        return -1;
    }
    //set the read flag from the returned read data 
    read_flag = read_data;

    //read the data from the device
    if(read_flag == 1){
        //read the data from the device. 96-bits need to be read from the device, so 3 reads need to be sent 
        offset = 4 * sizeof(uint32_t); //fifo flag offset
        for( k = 0 ; k <3; k++){

            ret = pread(fd, (char *)&read_data, sizeof(read_data), offset);
            if (ret < 0) {
                log_message("Failed to read from underlying device: %d\n", ret);
                return -1;
            }

            if(k == 0){
                *problem_id = inverse_data(read_data);
                *card_id = inverse_data(read_data >> 4);
                *best_spins = read_data;
            }
            if(k == 1){
                *best_spins = ((uint64_t)read_data << 32) | (*best_spins & 0xFFFFFFFF);
                *best_ham = read_data;
            }
            if(k == 2){
                *best_ham = ((uint64_t)read_data << 32) | (*best_ham & 0xFFFFFFFF);
            }
        }
    }
    else{
        log_message("No data to read\n");
        return -1;
    }

    log_message("Read logged\n");
    return 0;
}

static int perform_bulk_write(int fd, const uint64_t* data, size_t count) {
    int user_id; //used to track the problems sent to the device
    int k;
    //create a temp structure to store the problem data
    write_data_t *bulk_write_data = (write_data_t*)malloc(count * sizeof(write_data_t));

    
    //check if the sturcture was created
    if (!bulk_write_data) {
        log_message("Failed to allocate memory for bulk write data");
        return -1;
    }
    
    //loop through the data and intialize the bulk_write_data struct
    for (size_t i = 0; i < count; ++i) {
        bulk_write_data[i].offset = 9 * sizeof(uint64_t);  // Memory Map Address
        bulk_write_data[i].value =  swap_bytes(data[i]);   // 64-bit value
    }
    
    //write the data to the device
    user_id = write(fd, bulk_write_data, count * sizeof(write_data_t));
    
    free(bulk_write_data);

    return user_id;
}

void perform_operations(const char* device_file) {
    log_message("Entered perform_operations\n");

    int fd; // File descriptor for using the virtual driver
    size_t problem_count = 1; //how many problems were sent to the device


    // Open device file
    fd = open(device_file, O_RDWR);
    if (fd < 0) {
        log_message("Failed to open device file '%s': %s (errno: %d)\n", device_file, strerror(errno), errno);
        return;
    }

    // Allocate main structure
    struct user_read_data *read_data = (struct user_read_data *)malloc(sizeof(struct user_read_data));
    if (read_data == NULL) {
        log_message("Failed to allocate read_data\n");
        return;
    }

    // Initialize pointers to NULL
    read_data->best_spins = NULL;
    read_data->best_ham = NULL;

    // Allocate arrays
    read_data->best_spins = (uint64_t *)malloc(problem_count * sizeof(uint64_t));
    read_data->best_ham = (uint64_t *)malloc(problem_count * sizeof(uint64_t));

    if (!read_data->best_spins || !read_data->best_ham) {
        log_message("Failed to allocate arrays\n");
        free(read_data->best_spins);
        free(read_data->best_ham);
        free(read_data);
        return;
    }


    //send data to the vdriver
    read_data->user_id = perform_bulk_write(fd, (const uint64_t*)rawData1, problem_count*RAW_BYTE_CNT);  
    close(fd);
    /*

    //retrieve data from the vdriver
    time_t start_time_wr = time(NULL); // Record the start time
    
    // Wait for up to 2 seconds before breaking the loop
    while (difftime(time(NULL), start_time_wr) < 2) {
        //open the device file
        fd = open(device_file, O_RDWR);

        //check if the r
        if (!perform_read(fd, &read_data->user_id, read_data->best_spins, read_data->best_ham) != sizeof(read_data->user_id)) {
            log_message("Read Data:\n");
            log_message("Data read - User ID: %d\n", read_data->user_id);
            log_message("Best Spins: %" PRIu64 "\n", read_data->best_spins[0]); 
            log_message("Best Ham: %" PRIu64 "\n", read_data->best_ham[0]);
            break; // Exit the loop after successful read
        }

 
        //close the device file
        close(fd);

        // Check every 100ms
        usleep(100000); 
    }
*/

}

int main() {
    // Open the output_logs file for writing
    FILE *log_file = freopen("output_logs", "w", stdout);
    if (log_file == NULL) {
        log_message("Failed to redirect stdout");
        return 1;
    }

    // Redirect stderr as well
    if (freopen("output_logs", "a", stderr) == NULL) {
        log_message("Failed to redirect stderr");
        return 1;
    }

    char device_file[256];
    snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, 0);
    if (access(device_file, F_OK) == 0) {
        log_message("\nAccessing device file: %s\n", device_file);
        //for (int k = 0; k < 5; ++k) {
        //    printf("Outer loop : %d\n", k);
            perform_operations(device_file);
        //}
    } else {
        log_message("No device found at %s\n", device_file);
    }
    
    
    fclose(log_file); // Close the file when done
    return 0;
}