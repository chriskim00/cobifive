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

#define DEVICE_FILE_TEMPLATE "/dev/cobi_chip_vdriver64"
#define READ_COUNT_COBI_CHIP_1 3
#define RAW_BYTE_CNT 166
#define ROW_SIZE 7
#define MAX_DEVICES 10

/* 
THIS APPLICATION IS FOR Quad COBIFIVE. 
Created by: RPJ
Date: Dec. 6, 2024
*/
void perform_operations(const char* device_file) {
    int fd;

    fd = open(device_file, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return;
    }


    off_t read_offset = 1 * sizeof(uint32_t); // Read FIFO empty status

    // Read data from device
    uint32_t read_data = 12345;
    ssize_t bytes_read = read(fd, &read_data, sizeof(read_data));
    printf("Bytes read: %zd\n", bytes_read);

    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
    }
    
/*
    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        return -1;
    }
*/


    printf("Value: %u\n", read_data);

    if(fd){
        close(fd);
    }

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
    snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE);

    if (access(device_file, F_OK) == 0) {
        printf("\nAccessing device file: %s\n", device_file);
        //for (int k = 0; k < 5; ++k) {
        //    printf("Outer loop : %d\n", k);
            perform_operations(device_file);
        //}
    } else {
        printf("No device found at %s\n", device_file);
    }
    
    
    fclose(log_file); // Close the file when done
    return 0;
}
