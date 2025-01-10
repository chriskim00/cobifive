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

uint64_t swap_bytes(uint64_t val) {

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

    if (!(read_data & (1 << 3))) {
        printf("S_READY IS LOW.\n");
    }
    *read_status = read_data;
    return 0;
}


void fwid_read(int fd) {
    off_t read_offset = 1 * sizeof(uint32_t); // Read FIFO empty status

    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        return;
    }

    // Read data from device
    uint32_t read_data;
    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        return;
    }

    printf("FWID: %x\n", read_data);
}


void fwrev_read(int fd) {
    off_t read_offset = 2 * sizeof(uint32_t); // Read FIFO empty status

    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        return;
    }

    // Read data from device
    uint32_t read_data;
    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        return;
    }

    printf("FWREV: %x\n", read_data);
}



void spad_cmd(int fd) {
    write_data_t write_data;
    write_data.offset = 3 * sizeof(uint64_t);
    write_data.value = 0xAA55AA55AA55AA55;    // Initialize AXI interface control
    if (write(fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        close(fd);
        return;
    }


    off_t read_offset = 3 * sizeof(uint32_t); // Read FIFO empty status

    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        return;
    }

    // Read data from device
    uint32_t read_data;
    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        return;
    }

    printf("SPAD: %x \n", read_data);
}

uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

void perform_operations(const char* device_file) {
    int fd, i;
    uint32_t read_data, read_status; 
    write_data_t write_data;
    off_t read_offset, read_mem_check;
    struct timespec start, end;

    // Open device file
    fd = open(device_file, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return;
    }

    printf("PCIe Card Initial Test\n");
    fwid_read(fd);
    fwrev_read(fd);
    spad_cmd(fd);

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
                perform_operations(device_file);
        } else {
            printf("No device found at %s\n", device_file);
            break;
        }
    }
    
    fclose(log_file); // Close the file when done
    return 0;
}
