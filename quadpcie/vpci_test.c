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
            printf("\nAccessing device file: %s\n", device_file);
            //for (int k = 0; k < 5; ++k) {
            //    printf("Outer loop : %d\n", k);
                perform_operations(device_file);
            //}
        } else {
            printf("No device found at %s\n", device_file);
            break;
        }
    }
    
    fclose(log_file); // Close the file when done
    return 0;
}
