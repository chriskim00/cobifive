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
#include "cobi_input_data.h"

#define DEVICE_FILE_TEMPLATE "/dev/cobi_pcie_card%d"
#define READ_COUNT_COBI_CHIP_1 3
#define RAW_BYTE_CNT 166
#define ROW_SIZE 7
#define MAX_DEVICES 10

/* 
THIS APPLICATION IS FOR COBIFIVE. THIS WILL ONLY WORK
WITH PCI DRIVER 64bit.
Created by: RPJ
Date: Nov. 6, 2024
*/

typedef struct {
    off_t offset;
    uint64_t value;  // Change to 64-bit
} write_data_t;

uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

void perform_bulk_write(int fd, const uint64_t* data, size_t count) {
    write_data_t *bulk_write_data = (write_data_t*)malloc(count * sizeof(write_data_t));
    if (!bulk_write_data) {
        perror("Failed to allocate memory for bulk write data");
        return;
    }
    
    for (size_t i = 0; i < count; ++i) {
        bulk_write_data[i].offset = 9 * sizeof(uint64_t);  // Memory Map Address
        bulk_write_data[i].value =  swap_bytes(data[i]);   // 64-bit value
    }

    if (write(fd, bulk_write_data, count * sizeof(write_data_t)) != count * sizeof(write_data_t)) {
        perror("Failed to write bulk data to device");
    }

    free(bulk_write_data);
}

int read_empty_status(int fd, uint32_t* read_status) {
    off_t read_offset = 10 * sizeof(uint32_t); // Read FIFO empty status

    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        return -1;
    }

    // Read data from device
    uint32_t read_data;
    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        return -1;
    }
        printf("Read Status: 0x%x\n", read_data);

    if (!(read_data & (1 << 3))) {
        printf("S_READY IS LOW.\n");
    }
    *read_status = read_data;
    return 0;
}

void perform_operations(const char* device_file) {
    int fd, i;
    uint32_t read_data, read_status; 
    write_data_t write_data;
    off_t read_offset;
    struct timespec start, end;

    // Open device file
    fd = open(device_file, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return;
    }

    printf("Writing Data to COBIFIVE\n");
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    // COBIFIVE Initialization
    
    printf("initialize\n");

    write_data.offset = 8 * sizeof(uint64_t);
    write_data.value = 0x0;    // Initialize AXI interface control
    if (write(fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        close(fd);
        return;
    }
    usleep(5); 

       // Data writing loop
    int expected_reply = 0;
    for (int problem = 1; problem <= 15; ++problem) {
        printf("Writing Problem %d.\n", problem);
        switch (problem) {
            case 1:
                perform_bulk_write(fd, (const uint64_t*)rawData1, RAW_BYTE_CNT);
                break;
            case 2:
                perform_bulk_write(fd, (const uint64_t*)rawData2, RAW_BYTE_CNT);
                break;
            case 3:
                perform_bulk_write(fd, (const uint64_t*)rawData3, RAW_BYTE_CNT);
                break;
            case 4:
                perform_bulk_write(fd, (const uint64_t*)rawData4, RAW_BYTE_CNT);
                break;
            case 5:
                perform_bulk_write(fd, (const uint64_t*)rawData5, RAW_BYTE_CNT);
                break;
            case 6:
                perform_bulk_write(fd, (const uint64_t*)rawData6, RAW_BYTE_CNT);
                break;
            case 7:
                perform_bulk_write(fd, (const uint64_t*)rawData7, RAW_BYTE_CNT);
                break;
            case 8:
                perform_bulk_write(fd, (const uint64_t*)rawData8, RAW_BYTE_CNT);
                break;
            case 9:
                perform_bulk_write(fd, (const uint64_t*)rawData9, RAW_BYTE_CNT);
                break;
            case 10:
                perform_bulk_write(fd, (const uint64_t*)rawData10, RAW_BYTE_CNT);
                break;
                
            case 11:
                perform_bulk_write(fd, (const uint64_t*)rawData11, RAW_BYTE_CNT);
                break;
            case 12:
                perform_bulk_write(fd, (const uint64_t*)rawData12, RAW_BYTE_CNT);
                break;
            case 13:
                perform_bulk_write(fd, (const uint64_t*)rawData13, RAW_BYTE_CNT);
                break;
            case 14:
                perform_bulk_write(fd, (const uint64_t*)rawData14, RAW_BYTE_CNT);
                break;
            case 15:
                perform_bulk_write(fd, (const uint64_t*)rawData15, RAW_BYTE_CNT);
                break;
            /*case 16:
                perform_bulk_write(fd, (const uint64_t*)rawData6, RAW_BYTE_CNT);
                break;
            case 17:
                perform_bulk_write(fd, (const uint64_t*)rawData7, RAW_BYTE_CNT);
                break;
            case 18:
                perform_bulk_write(fd, (const uint64_t*)rawData8, RAW_BYTE_CNT);
                break;
            case 19:
                perform_bulk_write(fd, (const uint64_t*)rawData9, RAW_BYTE_CNT);
                break;
            case 20:
                perform_bulk_write(fd, (const uint64_t*)rawData10, RAW_BYTE_CNT);
                break; */
            default:
                printf("Invalid problem number.\n");
                break;
        }
        read_empty_status(fd, &read_status);
        expected_reply = problem;
        if (!(read_status & (1 << 3))) {
            break;
            printf("S_READY IS LOW. STOP WRITING AT: %d\n", problem);
        }
        printf("wait for 1us.\n");
        usleep(1);
    }

    usleep(10000);
    // Wait time for cobi response
    printf("Done writing data to cobi chip. Now reading responses.\n");
    
int j = 0; // Initialize counter
printf("Expected reply: %d\n", expected_reply);
time_t start_time = time(NULL); // Record the start time
while (1) {

    if (difftime(time(NULL), start_time) >= 0.5) { // Check if 12 second has passed
        printf("Timeout occurred! Exiting loop after 2 second.\n");
        break;
    }
   
    read_empty_status(fd, &read_status);

    if (j < expected_reply) {
        if (!(read_status & (1 << 1))) {
            j++;
            printf("COBIFIVE Response: %d \n", j);
            for (i = 0; i < READ_COUNT_COBI_CHIP_1; ++i) {
                read_offset = 4 * sizeof(uint32_t);

                // Write read offset to device
                if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
                    perror("Failed to set read offset in device");
                    close(fd);
                    return;
                }

                // Read data from device
                if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
                    perror("Failed to read from device");
                    close(fd);
                    return;
                }
                printf("COBIFIVE Response: 0x%x\n", read_data);
            }
        }
    
    } else {
        printf("fifo is empty\n");
        break;
    }
}

    // End timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate and print elapsed time
    double elapsed_time = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time Lapsed: %.6f seconds\n\n", elapsed_time);

    // Close device file
    close(fd);
}
int main() {
    // Open the output_logs file for writing
    FILE *log_file = freopen("output_logs", "w", stdout);
    if (log_file == NULL) {
        perror("Failed to redirect stdout");
        return 1;
    }

    // Redirect stderr as well
    if (freopen("output_logs", "a", stderr) == NULL) {
        perror("Failed to redirect stderr");
        return 1;
    }

    char device_file[256];
    for (int i = 0; i < MAX_DEVICES; ++i) {
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        if (access(device_file, F_OK) == 0) {
            printf("Accessing device file: %s\n", device_file);
            for (int k = 0; k < 50; ++k) {
                printf("loop : %d\n", k);
                perform_operations(device_file);
            }
        } else {
            printf("No device found at %s\n", device_file);
            break;
        }
    }
    
    fclose(log_file); // Close the file when done
    return 0;
}
