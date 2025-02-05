#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
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

void perform_operations(const char* device_file) {
    int fd, i;
    uint32_t read_data;
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
    write_data.offset = 8 * sizeof(uint64_t);
    write_data.value = 0x0;    // Initialize AXI interface control
    if (write(fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        close(fd);
        return;
   }
    usleep(5);

  //  printf("Writing Problem 1.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

  //  usleep(1);
    printf("Writing Problem 2.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

  //  usleep(1);
    printf("Writing Problem 3.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

//    printf("Done writing data to cobi chip. Now reading responses.\n");
 //   usleep(50);
/*
    for (i = 0; i < READ_COUNT_COBI_CHIP_1; ++i) {
        read_offset = 4 * sizeof(uint32_t);  // Adjust read offset for 64-bit

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
*/
   // usleep(1);
    printf("Writing Problem 4.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

   // usleep(1);
    printf("Writing Problem 5.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

   // usleep(1);
    printf("Writing Problem 6.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

   // usleep(1);
    printf("Writing Problem 7.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit

   // usleep(1);
    printf("Writing Problem 8.\n");
    perform_bulk_write(fd, (const uint64_t*)test, RAW_BYTE_CNT);  // Assume rawData is updated to 64-bit
    // Wait time for cobi response
    usleep(3000);
    printf("Done writing data to cobi chip. Now reading responses.\n");
    /*
  int j = 1;

   while (1) {
        read_offset = 10 * sizeof(uint32_t); // read fifo empty status

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
        printf("COBIFIVE status succeeding read: 0x%x\n", read_data);
        if (!(read_data & (1 << 1))) {

            if(j <8){
            j++;
            printf("COBIFIVE Response: %d \n", j);
                for (i = 0; i < READ_COUNT_COBI_CHIP_1; ++i)
                {
                    read_offset = 4 * sizeof(uint32_t);  // Adjust read offset for 64-bit

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
            else
                break;


        }
    }
    */
        read_offset = 10 * sizeof(uint32_t); // read fifo empty status

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
        printf("COBIFIVE status succeeding read: 0x%x\n", read_data);

    for (int j = 0; j < 8; ++j) {
        for (i = 0; i < READ_COUNT_COBI_CHIP_1; ++i)
        {
            read_offset = 4 * sizeof(uint32_t);  // Adjust read offset for 64-bit

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

    // End timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate and print elapsed time
    double elapsed_time = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time Lapsed: %.6f seconds\n\n", elapsed_time);

    // Close device file
    close(fd);
}

int main()
{
    char device_file[256];
    for (int i = 0; i < MAX_DEVICES; ++i) {
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        if (access(device_file, F_OK) == 0) {
            printf("Accessing device file: %s\n", device_file);
            perform_operations(device_file);
        } else {
            printf("No device found at %s\n", device_file);
            break;
        }
    }
    return 0;
}

/*
int main()
{
    char device_file[256];
    snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, 1); // Access device file 1

    if (access(device_file, F_OK) == 0) {
        printf("Accessing device file: %s\n", device_file);
        perform_operations(device_file);
    } else {
        printf("No device found at %s\n", device_file);
    }

    return 0;
}
*/
